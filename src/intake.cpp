#include "../include/intake.h"

#include "../include/units.h"
#include "../include/utilities.h"

#include <cmath>

Intake::Intake() {
    m_inputFlowK = 0;
    m_idleFlowK = 0;
    m_flow = 0;
    m_throttle = 1.0;
    m_idleThrottlePlatePosition = 0.0;
    m_crossSectionArea = 0.0;
    m_flowRate = 0;
    m_totalFuelInjected = 0;
    m_molecularAfr = 0;
    m_runnerLength = 0;
    m_velocityDecay = 0;
    m_model = GasSystem::Model::Legacy;
    m_fuel = nullptr;
    m_lambda = 1.0;
    m_chargeTemperature = units::celcius(25.0);
}

Intake::~Intake() {
    /* void */
}

void Intake::initialize(Parameters &params) {
    const double width = std::sqrt(params.CrossSectionArea);
    m_molecularAfr = params.MolecularAfr;
    m_model = params.Model;
    m_fuel = params.Fuel;
    m_lambda = (params.Lambda > 0) ? params.Lambda : 1.0;
    m_chargeTemperature = units::celcius(25.0);

    if (m_model == GasSystem::Model::Engineering) {
        m_system.setModel(m_model, m_fuel);
        m_atmosphere.setModel(m_model, m_fuel);

        // Charge cooling by fuel evaporation:
        // dT = (fuel mass fraction) * h_fg / c_p,mixture
        const thermo::FuelDefinition &fuel =
            (m_fuel != nullptr) ? *m_fuel : thermo::defaultFuel();
        const thermo::Composition charge = thermo::freshCharge(fuel, 1.0 / m_lambda);

        double mass = 0, cp = 0;
        for (int i = 0; i < thermo::SpeciesCount; ++i) {
            const bool isFuel = (i == thermo::Fuel);
            const double M = isFuel ? fuel.molarMass : thermo::species(i).molarMass;
            const thermo::EnergyPolynomial &energy =
                isFuel ? fuel.energy : thermo::species(i).energy;
            mass += charge.n[i] * M;
            cp += charge.n[i] * (energy.cv(m_chargeTemperature) + constants::R);
        }

        const double fuelMassFraction = charge.n[thermo::Fuel] * fuel.molarMass / mass;
        const double cp_mass = cp / mass;
        m_chargeTemperature -= clamp(params.EvaporativeCooling, 0.0, 1.0)
            * fuelMassFraction * fuel.latentHeat / cp_mass;
    }

    // The manifold starts out filled with the charge the carburetor supplies
    m_system.initialize(
        units::pressure(1.0, units::atm),
        params.volume,
        m_chargeTemperature,
        getFuelAirMix());
    m_system.setGeometry(
        width,
        params.volume / params.CrossSectionArea,
        1.0,
        0.0);

    m_atmosphere.initialize(
        units::pressure(1.0, units::atm),
        units::volume(1000.0, units::m3),
        units::celcius(25.0));
    m_atmosphere.setGeometry(
        units::distance(100.0, units::m),
        units::distance(100.0, units::m),
        1.0,
        0.0);

    m_inputFlowK = params.InputFlowK;
    m_idleFlowK = params.IdleFlowK;
    m_idleThrottlePlatePosition = params.IdleThrottlePlatePosition;
    m_runnerLength = params.RunnerLength;
    m_crossSectionArea = params.CrossSectionArea;
    m_velocityDecay = params.VelocityDecay;
    m_runnerFlowRate = params.RunnerFlowRate;
}

void Intake::destroy() {
    /* void */
}

GasSystem::Mix Intake::getFuelAirMix() const {
    if (m_model == GasSystem::Model::Engineering) {
        const thermo::FuelDefinition &fuel =
            (m_fuel != nullptr) ? *m_fuel : thermo::defaultFuel();
        return GasSystem::Mix::fromComposition(thermo::freshCharge(fuel, 1.0 / m_lambda));
    }

    const double ideal_afr = 0.8 * m_molecularAfr * 4;
    const double p_air = ideal_afr / (1 + ideal_afr);

    GasSystem::Mix fuelAirMix;
    fuelAirMix.p_fuel = 1 - p_air;
    fuelAirMix.p_inert = p_air * 0.75;
    fuelAirMix.p_o2 = p_air * 0.25;

    return fuelAirMix;
}

void Intake::process(double dt) {
    const GasSystem::Mix fuelAirMix = getFuelAirMix();
    const bool engineering = (m_model == GasSystem::Model::Engineering);

    // The legacy idle circuit supplies a very rich mixture; the engineering
    // model meters the same air-fuel ratio as through the throttle
    GasSystem::Mix fuelMix = fuelAirMix;
    if (!engineering) {
        const double idle_afr = 2.0;
        const double p_idle_air = idle_afr / (1 + idle_afr);
        fuelMix.p_fuel = (1.0 - p_idle_air);
        fuelMix.p_inert = p_idle_air * 0.75;
        fuelMix.p_o2 = p_idle_air * 0.25;
    }

    const double T_charge = engineering ? m_chargeTemperature : units::celcius(25.0);

    const double throttle = getThrottlePlatePosition();
    const double flowAttenuation = std::cos(throttle * constants::pi / 2);

    GasSystem::FlowParameters flowParams;
    flowParams.crossSectionArea_0 = units::area(10, units::m2);
    flowParams.crossSectionArea_1 = m_crossSectionArea;
    flowParams.direction_x = 0.0;
    flowParams.direction_y = -1.0;
    flowParams.dt = dt;

    m_atmosphere.reset(units::pressure(1.0, units::atm), T_charge, fuelAirMix);
    flowParams.system_0 = &m_atmosphere;
    flowParams.system_1 = &m_system;
    flowParams.k_flow = flowAttenuation * m_inputFlowK;
    m_flow = m_system.flow(flowParams);

    m_atmosphere.reset(units::pressure(1.0, units::atm), T_charge, fuelMix);
    flowParams.system_0 = &m_atmosphere;
    flowParams.system_1 = &m_system;
    flowParams.k_flow = m_idleFlowK;
    const double idleCircuitFlow = m_system.flow(flowParams);

    m_system.dissipateExcessVelocity();
    m_system.updateVelocity(dt, m_velocityDecay);

    if (m_flow > 0) {
        m_totalFuelInjected += fuelAirMix.p_fuel * m_flow;
    }

    if (idleCircuitFlow > 0) {
        m_totalFuelInjected += fuelMix.p_fuel * idleCircuitFlow;
    }
}
