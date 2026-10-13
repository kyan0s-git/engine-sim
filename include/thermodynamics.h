#ifndef ATG_ENGINE_SIM_THERMODYNAMICS_H
#define ATG_ENGINE_SIM_THERMODYNAMICS_H

#include <string>
#include <vector>

// Thermodynamic properties of ideal gas mixtures for the "engineering"
// physics model.
//
// Species properties use NASA 7-coefficient polynomials (GRI-Mech 3.0 for the
// combustion species, refits of the NASA CEA database for liquid fuels); see
// research/research_notes/.../thermodynamics_gas_properties.md.
//
// Energy convention: a gas system stores a "sensible" internal energy that is
// always positive so that the rest of the simulation can treat it like the
// legacy model's thermal energy. Per mole of species i:
//
//     e_i(T) = cv_i(T_ref) * T                                  (T <  T_ref)
//     e_i(T) = h_i(T) - h_i(T_ref) - R (T - T_ref) + cv_i(T_ref) T_ref
//                                                               (T >= T_ref)
//
// with T_ref = 298.15 K. The chemical (formation) energy that is left out of
// e_i is chem_i = h_i(T_ref) - R T_ref - cv_i(T_ref) T_ref, so a reaction that
// conserves the total internal energy changes the stored energy by
// sum(n_reactants * chem) - sum(n_products * chem).
namespace thermo {

    constexpr double T_ref = 298.15;
    constexpr double T_max = 5000.0;

    enum Species {
        Fuel = 0,
        O2,
        N2,
        Ar,
        CO2,
        H2O,
        CO,
        H2,
        SpeciesCount
    };

    const char *speciesName(int species);

    // NASA 7-coefficient polynomial with a common 1000 K midpoint
    struct NasaPolynomial {
        double low[7];
        double high[7];

        double cp_R(double T) const;
        double h_RT(double T) const;
        double s_R(double T) const;

        // Molar enthalpy including the enthalpy of formation [J/mol]
        double h(double T) const;

        NasaPolynomial scaled(double s) const;
    };

    constexpr double T_mid = 1000.0;

    // Polynomial representation of the sensible energy e(T) (see above):
    //     e(T) / R = c0 + c1 T + c2 T^2 + c3 T^3 + c4 T^4 + c5 T^5
    // in each temperature range, plus the constant heat capacity used below
    // T_ref. All members are linear in the species amounts so mixtures are
    // represented by mole-weighted sums.
    struct EnergyPolynomial {
        double low[6] = { 0, 0, 0, 0, 0, 0 };
        double high[6] = { 0, 0, 0, 0, 0, 0 };
        double cv_ref_R = 0;    // cv(T_ref) / R
        double chem = 0;        // chemical energy left out of e [J/mol]

        void clear();
        void add(const EnergyPolynomial &p, double weight);

        // Sensible energy per mole [J/mol] and heat capacity [J/(mol K)]
        double e(double T) const;
        double cv(double T) const;

        // Solves e(T) = e_target; T_guess is used as the starting point
        double temperature(double e_target, double T_guess) const;
    };

    EnergyPolynomial makeEnergyPolynomial(const NasaPolynomial &poly);

    struct SpeciesData {
        const char *name;
        double molarMass;           // kg/mol
        NasaPolynomial poly;
        EnergyPolynomial energy;
    };

    // Properties of the fixed (non-fuel) species
    const SpeciesData &species(int s);

    // Formation enthalpies at T_ref of the main product species [J/mol]
    double formationEnthalpy(int s);

    // Dry air (CEA "Air": N2 78.084 %, O2 20.9476 %, Ar 0.9365 %,
    // CO2 0.0319 %)
    namespace air {
        constexpr double x_N2 = 0.78084;
        constexpr double x_O2 = 0.209476;
        constexpr double x_Ar = 0.009365;
        constexpr double x_CO2 = 0.000319;
        constexpr double molarMass = 28.9651159e-3;    // kg/mol

        // Moles of air per mole of O2
        constexpr double molesPerO2 = 1.0 / x_O2;
    }

    struct FlameSpeedParameters {
        enum class Correlation {
            RhodesKeck,         // gasoline (Rhodes & Keck, SAE 850047)
            MetghalchiKeck      // Metghalchi & Keck 1982 form
        };

        Correlation correlation = Correlation::RhodesKeck;
        double phi_m = 1.21;        // equivalence ratio of the peak
        double B_m = 0.305;         // m/s
        double B_phi = -0.549;      // m/s
    };

    struct FuelDefinition {
        std::string name;

        // Atoms per molecule (C_x H_y O_z)
        double C = 0;
        double H = 0;
        double O = 0;

        double molarMass = 0;       // kg/mol
        double lhv = 0;             // J/kg, as usually quoted (liquid fuel)
        double latentHeat = 0;      // J/kg, 0 for gaseous fuels
        double density = 0;         // kg/m^3 (liquid or gas at 1 atm, 15 C)

        FlameSpeedParameters flameSpeed;

        NasaPolynomial poly;
        EnergyPolynomial energy;

        // Moles of O2 needed to burn one mole of fuel
        double stoichiometricO2() const { return C + H / 4 - O / 2; }

        // Stoichiometric air-fuel ratio (mass) with real air
        double stoichiometricAfr() const;

        // Lower heating value of fuel vapour [J/kg]
        double vapourLhv() const { return lhv + latentHeat; }
    };

    // Creates a fuel with the given formula and heating value. The heat
    // capacity of the vapour is taken from `shape` (a polynomial of a similar
    // molecule with molar mass `shapeMolarMass`, scaled per unit mass), and
    // the enthalpy of formation is chosen so that complete combustion releases
    // exactly the given LHV.
    FuelDefinition makeFuel(
        const std::string &name,
        double C, double H, double O,
        double lhv,
        double latentHeat,
        double density,
        const NasaPolynomial &shape,
        double shapeMolarMass,
        const FlameSpeedParameters &flameSpeed);

    // Mixes fuels by liquid volume fraction (e.g. E10 = 90 % gasoline and
    // 10 % ethanol)
    FuelDefinition blendFuels(
        const std::string &name,
        const FuelDefinition &a,
        const FuelDefinition &b,
        double volumeFractionB);

    // Built-in fuels: "gasoline", "iso-octane", "n-heptane", "ethanol",
    // "methanol", "e10", "e85", "methane" (natural gas), "propane" (LPG) and
    // "hydrogen". Returns false if the name is unknown.
    bool findFuel(const std::string &name, FuelDefinition *fuel);
    std::vector<std::string> fuelNames();

    // The gasoline surrogate used by default
    const FuelDefinition &defaultFuel();

    // Species amounts [mol]
    struct Composition {
        double n[SpeciesCount] = { 0, 0, 0, 0, 0, 0, 0, 0 };

        double total() const;
    };

    // Fresh charge of fuel vapour and dry air with the given equivalence
    // ratio, as mole fractions
    Composition freshCharge(const FuelDefinition &fuel, double phi);

    // Equivalence ratio of a mixture
    double equivalenceRatio(const FuelDefinition &fuel, const Composition &c);

    // Burns `fuel` moles of fuel with `o2` moles of oxygen. Lean and
    // stoichiometric mixtures burn completely to CO2 and H2O; rich mixtures
    // produce CO and H2 in water-gas shift equilibrium (K = 3.5, about 1730 K
    // freezing temperature). If there isn't enough oxygen to oxidize all
    // carbon to CO, the amount of fuel burned is reduced.
    //
    // Returns the change of each species amount in `delta` and the amount of
    // fuel actually burned.
    double burn(
        const FuelDefinition &fuel,
        double fuelMoles,
        double o2Moles,
        Composition *delta,
        double waterGasShiftK = 3.5);

    // Change of the stored sensible energy caused by a change of composition
    // that conserves total internal energy (i.e. the heat released) [J]
    double heatRelease(const FuelDefinition &fuel, const Composition &delta);

} /* namespace thermo */

#endif /* ATG_ENGINE_SIM_THERMODYNAMICS_H */
