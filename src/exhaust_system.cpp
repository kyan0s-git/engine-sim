#include "../include/exhaust_system.h"

#include "../include/units.h"

ExhaustSystem::ExhaustSystem() {
    m_primaryFlowRate = 0;
    m_outletFlowRate = 0;
    m_collectorCrossSectionArea = 0;
    m_length = 0;
    m_primaryTubeLength = 0;
    m_audioVolume = 0;
    m_velocityDecay = 0;
    m_backflowAtmosphericMixing = 0;
    m_flow = 0;
    m_index = -1;
    m_impulseResponse = nullptr;
    m_engineering = false;
}

ExhaustSystem::~ExhaustSystem() {
    /* void */
}

void ExhaustSystem::initialize(const Parameters &params) {
    const double systemWidth = std::sqrt(params.collectorCrossSectionArea);
    const double volume = params.collectorCrossSectionArea * params.length;
    const double systemLength = params.length;

    m_engineering = (params.model == GasSystem::Model::Engineering);
    GasSystem::Mix initialMix;
    if (m_engineering) {
        m_system.setModel(params.model, params.fuel);
        m_atmosphere.setModel(params.model, params.fuel);

        // Starts out filled with air
        initialMix = GasSystem::Mix::fromComposition(
            thermo::freshCharge(thermo::defaultFuel(), 0.0));
    }

    m_system.initialize(
            units::pressure(1.0, units::atm),
            volume,
            units::celcius(25.0),
            initialMix);
    m_system.setGeometry(
        systemLength,
        systemWidth,
        1.0,
        0.0);

    m_atmosphere.initialize(
        units::pressure(1.0, units::atm),
        units::volume(1000.0, units::m3),
        units::celcius(25.0));
    m_atmosphere.setGeometry(
        units::distance(10.0, units::m),
        units::distance(10.0, units::m),
        1.0,
        0.0);

    m_primaryFlowRate = params.primaryFlowRate;
    m_audioVolume = params.audioVolume;
    m_outletFlowRate = params.outletFlowRate;
    m_collectorCrossSectionArea = params.collectorCrossSectionArea;
    m_velocityDecay = params.velocityDecay;
    m_backflowAtmosphericMixing = params.backflowAtmosphericMixing;
    m_impulseResponse = params.impulseResponse;
    m_length = params.length;
    m_primaryTubeLength = params.primaryTubeLength;
}

void ExhaustSystem::destroy() {
    /* void */
}

void ExhaustSystem::process(double dt) {
    // Air in the model is 25% oxygen (see Intake). By default gas pulled back
    // in through the outlet is treated as inert exhaust gas.
    const double o2 = 0.25 * m_backflowAtmosphericMixing;

    GasSystem::Mix airMix;
    airMix.p_fuel = 0;
    airMix.p_inert = 1.0 - o2;
    airMix.p_o2 = o2;

    if (m_engineering) {
        // Real air mixed with exhaust gas (treated as nitrogen)
        const double x = m_backflowAtmosphericMixing;
        airMix.p_o2 = x * thermo::air::x_O2;
        airMix.p_ar = x * thermo::air::x_Ar;
        airMix.p_co2 = x * thermo::air::x_CO2;
        airMix.p_n2 = 1.0 - airMix.p_o2 - airMix.p_ar - airMix.p_co2;
        airMix.p_inert = 1.0 - airMix.p_o2;
    }

    m_atmosphere.reset(units::pressure(1.0, units::atm), units::celcius(25.0), airMix);
    GasSystem::FlowParameters flowParams;
    flowParams.crossSectionArea_0 = m_collectorCrossSectionArea;
    flowParams.crossSectionArea_1 = units::area(10, units::m2);
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.dt = dt;
    flowParams.system_0 = &m_atmosphere;
    flowParams.system_1 = &m_system;
    flowParams.k_flow = m_outletFlowRate;

    m_flow = m_system.flow(flowParams);

    m_system.dissipateExcessVelocity();
    m_system.updateVelocity(dt, m_velocityDecay);
}
