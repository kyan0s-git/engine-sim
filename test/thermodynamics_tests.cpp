#include <gtest/gtest.h>

#include "../include/gas_system.h"
#include "../include/thermodynamics.h"
#include "../include/units.h"

#include <cmath>
#include <functional>

// Verification tests for the engineering physics model. Reference values are
// either closed-form solutions computed here or the values tabulated in
// research/research_notes/.../thermodynamics_gas_properties.md (computed with
// Cantera from GRI-Mech 3.0 and NASA CEA data).

namespace {

    constexpr double R = constants::R;

    thermo::Composition airComposition() {
        thermo::Composition c;
        c.n[thermo::N2] = thermo::air::x_N2;
        c.n[thermo::O2] = thermo::air::x_O2;
        c.n[thermo::Ar] = thermo::air::x_Ar;
        c.n[thermo::CO2] = thermo::air::x_CO2;
        return c;
    }

    // Standard-state entropy of a composition per mole of mixture (s / R)
    double entropy_R(const thermo::FuelDefinition &fuel, const thermo::Composition &c, double T) {
        const double total = c.total();
        double s = 0;
        for (int i = 0; i < thermo::SpeciesCount; ++i) {
            if (c.n[i] == 0) continue;
            const thermo::NasaPolynomial &poly =
                (i == thermo::Fuel) ? fuel.poly : thermo::species(i).poly;
            s += (c.n[i] / total) * poly.s_R(T);
        }

        return s;
    }

    double cp_R(const thermo::FuelDefinition &fuel, const thermo::Composition &c, double T) {
        const double total = c.total();
        double cp = 0;
        for (int i = 0; i < thermo::SpeciesCount; ++i) {
            if (c.n[i] == 0) continue;
            const thermo::NasaPolynomial &poly =
                (i == thermo::Fuel) ? fuel.poly : thermo::species(i).poly;
            cp += (c.n[i] / total) * poly.cp_R(T);
        }

        return cp;
    }

    double bisect(const std::function<double(double)> &f, double a, double b) {
        double fa = f(a);
        for (int i = 0; i < 200; ++i) {
            const double m = 0.5 * (a + b);
            const double fm = f(m);
            if ((fm < 0) == (fa < 0)) {
                a = m;
                fa = fm;
            }
            else {
                b = m;
            }
        }

        return 0.5 * (a + b);
    }

    GasSystem::Mix airMix() {
        return GasSystem::Mix::fromComposition(airComposition());
    }

    void initializeEngineering(
        GasSystem *system,
        double P,
        double V,
        double T,
        const GasSystem::Mix &mix,
        const thermo::FuelDefinition *fuel = nullptr)
    {
        system->setModel(GasSystem::Model::Engineering, fuel);
        system->initialize(P, V, T, mix);
        system->setGeometry(
            std::cbrt(V),
            std::cbrt(V),
            1.0,
            0.0);
    }

} /* namespace */

TEST(ThermodynamicsTests, EnergyPolynomialIsInvertible) {
    const thermo::EnergyPolynomial &energy = thermo::species(thermo::CO2).energy;
    for (double T = 50; T < 6000; T += 37.0) {
        const double e = energy.e(T);
        EXPECT_NEAR(energy.temperature(e, 1000.0), T, 1E-5);
        EXPECT_NEAR(energy.temperature(e, 300.0), T, 1E-5);
    }

    // Continuous at the range boundaries
    for (const double T : { thermo::T_ref, thermo::T_mid, thermo::T_max }) {
        EXPECT_NEAR(energy.e(T - 1E-6), energy.e(T + 1E-6), 1E-3);
    }
}

TEST(ThermodynamicsTests, EnergyMatchesEnthalpyDifference) {
    // e(T2) - e(T1) = h(T2) - h(T1) - R (T2 - T1) for T >= T_ref
    for (int s = 1; s < thermo::SpeciesCount; ++s) {
        const thermo::SpeciesData &data = thermo::species(s);
        const double T1 = 400, T2 = 2500;
        EXPECT_NEAR(
            data.energy.e(T2) - data.energy.e(T1),
            data.poly.h(T2) - data.poly.h(T1) - R * (T2 - T1),
            0.5) << data.name;
    }
}

TEST(ThermodynamicsTests, AirProperties) {
    const thermo::Composition air = airComposition();
    const thermo::FuelDefinition &fuel = thermo::defaultFuel();

    // CEA "Air": cp/R = 3.500 (300 K), 3.975 (1000 K), 4.356 (2000 K)
    EXPECT_NEAR(cp_R(fuel, air, 300.0), 3.500, 0.005);
    EXPECT_NEAR(cp_R(fuel, air, 1000.0), 3.975, 0.01);
    EXPECT_NEAR(cp_R(fuel, air, 2000.0), 4.356, 0.01);

    GasSystem system;
    initializeEngineering(&system, 101325.0, 1E-3, 300.0, airMix());
    EXPECT_NEAR(system.heatCapacityRatio(), 1.400, 0.002);
    EXPECT_NEAR(system.molarMass(), thermo::air::molarMass, 1E-6);
    EXPECT_NEAR(system.temperature(), 300.0, 1E-6);
    EXPECT_NEAR(system.pressure(), 101325.0, 1E-3);
    EXPECT_NEAR(system.c(), 347.2, 0.5);
}

TEST(ThermodynamicsTests, FuelLibrary) {
    struct Expected {
        const char *name;
        double afr;
    };

    // Stoichiometric AFR with real air (research notes, fuel properties)
    const Expected expected[] = {
        { "iso-octane", 15.13 },
        { "n-heptane", 15.18 },
        { "ethanol", 9.00 },
        { "methanol", 6.47 },
        { "methane", 17.24 },
        { "propane", 15.68 },
        { "hydrogen", 34.29 }
    };

    for (const Expected &e : expected) {
        thermo::FuelDefinition fuel;
        ASSERT_TRUE(thermo::findFuel(e.name, &fuel)) << e.name;
        EXPECT_NEAR(fuel.stoichiometricAfr(), e.afr, 0.03) << e.name;
    }

    thermo::FuelDefinition gasoline, e85, fuel;
    ASSERT_TRUE(thermo::findFuel("Gasoline", &gasoline));
    EXPECT_NEAR(gasoline.stoichiometricAfr(), 14.6, 0.1);

    // E85 is about 9.8:1 (research notes, blends)
    ASSERT_TRUE(thermo::findFuel("e85", &e85));
    EXPECT_NEAR(e85.stoichiometricAfr(), 9.8, 0.2);

    EXPECT_FALSE(thermo::findFuel("kerosene-x", &fuel));

    // The formation enthalpy derived from the heating value matches the CEA
    // value for iso-octane gas (-224.01 kJ/mol)
    ASSERT_TRUE(thermo::findFuel("iso-octane", &fuel));
    EXPECT_NEAR(fuel.poly.h(thermo::T_ref), -224010.0, 1500.0);
}

TEST(ThermodynamicsTests, HeatReleaseMatchesLhv) {
    for (const std::string &name : thermo::fuelNames()) {
        thermo::FuelDefinition fuel;
        ASSERT_TRUE(thermo::findFuel(name, &fuel));

        thermo::Composition delta;
        const double burned = thermo::burn(fuel, 1.0, 100.0, &delta);
        EXPECT_NEAR(burned, 1.0, 1E-12);

        // Burning at constant volume and returning the products to T_ref
        // releases the (constant pressure) heating value plus the work of the
        // change in moles. The stored sensible energy at T_ref differs
        // between reactants and products, which is accounted for here.
        double dn = 0, dE_ref = 0;
        for (int i = 0; i < thermo::SpeciesCount; ++i) {
            dn += delta.n[i];
            const thermo::EnergyPolynomial &energy =
                (i == thermo::Fuel) ? fuel.energy : thermo::species(i).energy;
            dE_ref += delta.n[i] * energy.e(thermo::T_ref);
        }

        const double released = thermo::heatRelease(fuel, delta) - dE_ref;
        const double expected = fuel.vapourLhv() * fuel.molarMass + dn * R * thermo::T_ref;
        EXPECT_NEAR(released / expected, 1.0, 1E-9) << name;
    }
}

TEST(ThermodynamicsTests, RichCombustionWaterGasShift) {
    thermo::FuelDefinition fuel;
    ASSERT_TRUE(thermo::findFuel("iso-octane", &fuel));

    // phi = 1.2, complete combustion + WGS (K = 3.5), mole fractions of
    // the products: CO2 0.0905, H2O 0.1375, CO 0.0516, H2 0.0224
    const thermo::Composition charge = thermo::freshCharge(fuel, 1.2);
    EXPECT_NEAR(thermo::equivalenceRatio(fuel, charge), 1.2, 1E-12);

    thermo::Composition delta;
    thermo::burn(fuel, charge.n[thermo::Fuel], charge.n[thermo::O2], &delta);

    thermo::Composition products = charge;
    for (int i = 0; i < thermo::SpeciesCount; ++i) products.n[i] += delta.n[i];
    const double total = products.total();

    EXPECT_NEAR(products.n[thermo::CO2] / total, 0.0905, 0.0005);
    EXPECT_NEAR(products.n[thermo::H2O] / total, 0.1375, 0.0005);
    EXPECT_NEAR(products.n[thermo::CO] / total, 0.0516, 0.0005);
    EXPECT_NEAR(products.n[thermo::H2] / total, 0.0224, 0.0005);
    EXPECT_NEAR(products.n[thermo::O2], 0.0, 1E-15);
    EXPECT_NEAR(products.n[thermo::Fuel], 0.0, 1E-15);

    // Elements are conserved
    const double C0 = charge.n[thermo::Fuel] * fuel.C + charge.n[thermo::CO2];
    const double C1 = products.n[thermo::CO2] + products.n[thermo::CO];
    EXPECT_NEAR(C0, C1, 1E-12);

    const double O0 = 2 * charge.n[thermo::O2] + 2 * charge.n[thermo::CO2];
    const double O1 =
        2 * products.n[thermo::CO2] + products.n[thermo::CO] + products.n[thermo::H2O];
    EXPECT_NEAR(O0, O1, 1E-12);
}

TEST(ThermodynamicsTests, UnburnedAndBurnedGamma) {
    thermo::FuelDefinition fuel;
    ASSERT_TRUE(thermo::findFuel("iso-octane", &fuel));

    // Unburned stoichiometric iso-octane/air at 300 K: gamma = 1.356
    GasSystem system;
    const GasSystem::Mix charge =
        GasSystem::Mix::fromComposition(thermo::freshCharge(fuel, 1.0));
    initializeEngineering(&system, 101325.0, 1E-3, 300.0, charge, &fuel);
    EXPECT_NEAR(system.heatCapacityRatio(), 1.356, 0.003);

    // Burned at about 2000 K: gamma = 1.25-1.26
    system.burn(system.n(), system.mix());
    system.changeTemperature(2000.0 - system.temperature());
    EXPECT_NEAR(system.temperature(), 2000.0, 1E-6);
    EXPECT_GT(system.heatCapacityRatio(), 1.24);
    EXPECT_LT(system.heatCapacityRatio(), 1.27);
}

TEST(ThermodynamicsTests, ConstantVolumeCombustionTemperature) {
    thermo::FuelDefinition fuel;
    ASSERT_TRUE(thermo::findFuel("iso-octane", &fuel));

    // Adiabatic constant-volume complete combustion of iso-octane vapour/air
    // from 700 K and 20 bar (SI-like TDC conditions)
    struct Case {
        double phi;
        double T_ad;
    };

    for (const Case &c : { Case{ 0.8, 2821.0 }, Case{ 1.0, 3206.0 }, Case{ 1.2, 3010.0 } }) {
        GasSystem system;
        const GasSystem::Mix charge =
            GasSystem::Mix::fromComposition(thermo::freshCharge(fuel, c.phi));
        initializeEngineering(&system, 20E5, 1E-4, 700.0, charge, &fuel);

        const double n0 = system.n();
        const double burned = system.burn(system.n(), system.mix());
        EXPECT_NEAR(burned, charge.p_fuel * n0, 1E-12);

        // The rich case keeps CO and H2 (WGS) so it is a little less
        // accurate against the tabulated value
        EXPECT_NEAR(system.temperature(), c.T_ad, (c.phi > 1) ? 60.0 : 25.0) << c.phi;
    }

    // Stoichiometric final pressure is about 97 bar
    GasSystem system;
    initializeEngineering(
        &system, 20E5, 1E-4, 700.0,
        GasSystem::Mix::fromComposition(thermo::freshCharge(fuel, 1.0)),
        &fuel);
    system.burn(system.n(), system.mix());
    EXPECT_NEAR(system.pressure() / 1E5, 97.0, 2.0);
}

TEST(ThermodynamicsTests, IsentropicCompression) {
    // Air compressed 10:1 in small reversible steps follows the
    // variable-cp isentrope: [s(T) - R ln T] - [s(T0) - R ln T0] = R ln(V0/V)
    const double V0 = 500E-6, T0 = 300.0;
    GasSystem system;
    initializeEngineering(&system, 101325.0, V0, T0, airMix());

    const int steps = 200000;
    const double V1 = V0 / 10;
    for (int i = 1; i <= steps; ++i) {
        system.setVolume(V0 + (V1 - V0) * i / static_cast<double>(steps));
    }

    const thermo::Composition air = airComposition();
    const thermo::FuelDefinition &fuel = thermo::defaultFuel();
    const double target = std::log(V0 / V1);
    const double T_expected = bisect([&](double T) {
        return (entropy_R(fuel, air, T) - std::log(T))
            - (entropy_R(fuel, air, T0) - std::log(T0)) - target;
    }, T0, 2000.0);

    EXPECT_NEAR(system.temperature(), T_expected, 0.2);

    // About 730 K rather than the 753.6 K of a constant gamma = 1.4 gas
    EXPECT_LT(system.temperature(), 745.0);
    EXPECT_GT(system.temperature(), 715.0);
}

TEST(ThermodynamicsTests, BlowdownCoolsIsentropically) {
    // A rigid tank discharging into the atmosphere: the gas left in the tank
    // expands isentropically, s(T) - R ln p = const
    const double P0 = 5E5, T0 = 1500.0;
    GasSystem tank;
    initializeEngineering(&tank, P0, 1E-3, T0, airMix());

    const double k = 1E-6;
    while (tank.pressure() > 2.5E5) {
        tank.flow(k, 1E-6, 101325.0, 300.0, airMix());
    }

    const thermo::Composition air = airComposition();
    const thermo::FuelDefinition &fuel = thermo::defaultFuel();
    const double P1 = tank.pressure();
    const double T_expected = bisect([&](double T) {
        return (entropy_R(fuel, air, T) - entropy_R(fuel, air, T0)) - std::log(P1 / P0);
    }, 300.0, T0);

    EXPECT_NEAR(tank.temperature(), T_expected, 1.0);
    EXPECT_LT(tank.temperature(), 0.9 * T0);
}

TEST(ThermodynamicsTests, FillingHeatsTheCharge) {
    // Filling a rigid tank from the atmosphere: the incoming gas brings its
    // enthalpy, so n1 e(T1) = n0 e(T0) + (n1 - n0) h(T_env). Starting from a
    // near vacuum the temperature approaches gamma * T_env (about 420 K).
    const double T_env = 300.0;
    const double P_env = 101325.0;
    GasSystem tank;
    initializeEngineering(&tank, 1000.0, 1E-3, T_env, airMix());

    const double n0 = tank.n();
    const double E0 = tank.kineticEnergy();
    while (tank.pressure() < 0.95 * P_env) {
        tank.flow(1E-6, 1E-6, P_env, T_env, airMix());
    }

    thermo::EnergyPolynomial airEnergy;
    const thermo::Composition air = airComposition();
    for (int i = 1; i < thermo::SpeciesCount; ++i) {
        airEnergy.add(thermo::species(i).energy, air.n[i] / air.total());
    }

    const double h_env = airEnergy.e(T_env) + R * T_env;
    const double E_expected = E0 + (tank.n() - n0) * h_env;
    EXPECT_NEAR(tank.kineticEnergy() / E_expected, 1.0, 1E-9);

    const double T_expected = airEnergy.temperature(E_expected / tank.n(), 400.0);
    EXPECT_NEAR(tank.temperature(), T_expected, 1E-6);
    EXPECT_GT(tank.temperature(), 395.0);
    EXPECT_LT(tank.temperature(), 421.0);
}

TEST(ThermodynamicsTests, ChokedFlowRate) {
    // Choked molar flow: n' = k p0 sqrt(gamma) (2 / (gamma + 1))^((gamma + 1) / (2 (gamma - 1)))
    //                         / sqrt(R T0) * sqrt(M_air / M)
    const double P0 = 10E5, T0 = 600.0, k = 1E-6, dt = 1E-9;
    GasSystem tank;
    initializeEngineering(&tank, P0, 1E-3, T0, airMix());

    const double gamma = tank.heatCapacityRatio();
    const double M = tank.molarMass();
    const double expected =
        k * P0 * std::sqrt(gamma)
        * std::pow(2 / (gamma + 1), (gamma + 1) / (2 * (gamma - 1)))
        / std::sqrt(R * T0)
        * std::sqrt(units::AirMolecularMass / M);

    const double flow = tank.flow(k, dt, 101325.0, 300.0, airMix());
    EXPECT_NEAR(flow / dt / expected, 1.0, 1E-3);
}

TEST(ThermodynamicsTests, FlowConservesMassAndEnergy) {
    GasSystem a, b;
    initializeEngineering(&a, 3E5, 1E-3, 900.0, airMix());
    initializeEngineering(&b, 1E5, 2E-3, 300.0, airMix());

    const double n0 = a.n() + b.n();
    const double m0 = a.mass() + b.mass();
    const double E0 = a.totalEnergy() + b.totalEnergy();

    GasSystem::FlowParameters params;
    params.k_flow = 5E-4;
    params.dt = 1E-5;
    params.direction_x = 1.0;
    params.direction_y = 0.0;
    params.crossSectionArea_0 = 1E-3;
    params.crossSectionArea_1 = 1E-3;
    params.system_0 = &a;
    params.system_1 = &b;

    for (int i = 0; i < 5000; ++i) {
        GasSystem::flow(params);
        a.dissipateVelocity(params.dt, 1E-3);
        b.dissipateVelocity(params.dt, 1E-3);
    }

    EXPECT_NEAR(a.n() + b.n(), n0, 1E-12 * n0);
    EXPECT_NEAR((a.mass() + b.mass()) / m0, 1.0, 1E-12);
    EXPECT_NEAR((a.totalEnergy() + b.totalEnergy()) / E0, 1.0, 1E-9);
    EXPECT_NEAR(a.pressure() / b.pressure(), 1.0, 0.05);
}

TEST(ThermodynamicsTests, LegacyReactDoesNotConsumeOxygenWithoutFuel) {
    GasSystem system;
    GasSystem::Mix mix;
    mix.p_fuel = 0.0;
    mix.p_o2 = 0.25;
    mix.p_inert = 0.75;
    system.initialize(101325.0, 1E-3, 300.0, mix);

    const double o2 = system.n_o2();
    const double n = system.n();

    GasSystem::Mix flameMix;
    flameMix.p_fuel = 0.02;
    flameMix.p_o2 = 0.24;
    flameMix.p_inert = 0.74;
    EXPECT_EQ(system.react(n * 0.5, flameMix), 0.0);
    EXPECT_NEAR(system.n_o2(), o2, 1E-15);
    EXPECT_NEAR(system.n(), n, 1E-15);
}
