#include "../include/combustion_chamber.h"

#include "../include/constants.h"
#include "../include/units.h"
#include "../include/piston.h"
#include "../include/connecting_rod.h"
#include "../include/utilities.h"
#include "../include/exhaust_system.h"
#include "../include/cylinder_bank.h"
#include "../include/engine.h"

#include <cmath>

CombustionChamber::CombustionChamber() {
    m_crankcasePressure = 0.0;
    m_heatTransferCoefficient = 100.0;
    m_blockTemperature = units::celcius(90.0);
    m_piston = nullptr;
    m_head = nullptr;
    m_engine = nullptr;
    m_pistonSpeed = nullptr;
    m_pressure = nullptr;
    m_lit = false;
    m_litLastFrame = false;
    m_peakTemperature = 0;

    m_meanPistonSpeedToTurbulence = nullptr;
    m_nBurntFuel = 0;

    m_manifoldToRunnerFlowRate = 0;
    m_primaryToCollectorFlowRate = 0;
    m_cylinderWidthApproximation = 0;
    m_cylinderCrossSectionSurfaceArea = 0;

    m_lastTimestepTotalExhaustFlow = 0;
    m_lastTimestepTotalIntakeFlow = 0;
    m_exhaustFlow = 0;
    m_exhaustFlowRate = 0;
    m_intakeFlowRate = 0;

    m_fuel = nullptr;

    m_engineering = false;
    m_pistonFriction = true;
    m_wallTemperature = 450.0;
    m_heatTransferMultiplier = 1.0;
    m_currentHeatTransferCoefficient = 0.0;
    m_productFractionStoichiometric = 0.27;

    m_intakeOpen = false;
    m_combustionPhase = false;
    m_ivcPressure = 0;
    m_ivcVolume = 0;
    m_ivcTemperature = 0;
    m_ivcGamma = 1.35;

    m_randomState = 0x9E3779B97F4A7C15ull;
}

CombustionChamber::~CombustionChamber() {
    assert(m_pistonSpeed == nullptr);
    assert(m_pressure == nullptr);
}

void CombustionChamber::initialize(const Parameters &params) {
    m_piston = params.Piston;
    m_head = params.Head;
    m_fuel = params.Fuel;
    m_crankcasePressure = params.CrankcasePressure;
    m_heatTransferCoefficient = params.HeatTransferCoefficient;
    m_blockTemperature = params.BlockTemperature;
    m_meanPistonSpeedToTurbulence = params.MeanPistonSpeedToTurbulence;
    m_wallTemperature = params.WallTemperature;
    m_heatTransferMultiplier = params.HeatTransferMultiplier;
    m_pistonFriction = params.PistonFriction;

    m_engineering =
        m_engine != nullptr
        && m_engine->getPhysicsModel() == GasSystem::Model::Engineering;

    GasSystem::Mix exhaustMix;
    if (m_engineering) {
        const thermo::FuelDefinition *fuel = &m_fuel->getDefinition();
        m_system.setModel(GasSystem::Model::Engineering, fuel);
        m_intakeRunnerAndManifold.setModel(GasSystem::Model::Engineering, fuel);
        m_exhaustRunnerAndPrimary.setModel(GasSystem::Model::Engineering, fuel);

        // Mole fraction of combustion products in stoichiometric burned gas,
        // used to estimate the residual gas fraction
        const thermo::Composition charge = thermo::freshCharge(*fuel, 1.0);
        thermo::Composition delta;
        thermo::burn(*fuel, charge.n[thermo::Fuel], charge.n[thermo::O2], &delta);

        double products = 0, total = 0;
        for (int i = 0; i < thermo::SpeciesCount; ++i) {
            const double n = charge.n[i] + delta.n[i];
            total += n;
            if (i == thermo::CO2 || i == thermo::H2O || i == thermo::CO || i == thermo::H2) {
                products += n;
            }
        }

        m_productFractionStoichiometric = products / total;

        // The exhaust starts out filled with air
        exhaustMix = GasSystem::Mix::fromComposition(thermo::freshCharge(*fuel, 0.0));
    }

    m_pistonSpeed = new double[StateSamples];
    m_pressure = new double[StateSamples];
    for (int i = 0; i < StateSamples; ++i) {
        m_pistonSpeed[i] = 0;
        m_pressure[i] = 0;
    }

    Intake *intake = m_head->getIntake(m_piston->getCylinderIndex());
    ExhaustSystem *exhaust = m_head->getExhaustSystem(m_piston->getCylinderIndex());

    m_manifoldToRunnerFlowRate = intake->getRunnerFlowRate();
    m_primaryToCollectorFlowRate = exhaust->getPrimaryFlowRate();

    const double bore_r = m_head->getCylinderBank()->getBore() / 2.0;
    m_cylinderCrossSectionSurfaceArea = constants::pi * bore_r * bore_r;
    m_cylinderWidthApproximation = std::sqrt(m_cylinderCrossSectionSurfaceArea);

    const double height = getVolume() / m_cylinderCrossSectionSurfaceArea;
    m_system.setGeometry(
        m_cylinderWidthApproximation,
        height,
        1.0,
        0.0);

    const double intakeRunnerCrossSection = m_head->getIntakeRunnerCrossSectionArea();
    const double intakeRunnerWidth = std::sqrt(intakeRunnerCrossSection);
    const double manifoldRunnerLength = intake->getRunnerLength();
    const double manifoldRunnerVolume = intakeRunnerCrossSection * manifoldRunnerLength;
    const double totalIntakeRunnerVolume = m_head->getIntakeRunnerVolume() + manifoldRunnerVolume;
    const double overallIntakeRunnerLength = totalIntakeRunnerVolume / intakeRunnerCrossSection;
    m_intakeRunnerAndManifold.initialize(
        units::pressure(1.0, units::atm),
        totalIntakeRunnerVolume,
        units::celcius(25.0),
        intake->getFuelAirMix());
    m_intakeRunnerAndManifold.setGeometry(
        overallIntakeRunnerLength,
        intakeRunnerWidth,
        1.0,
        0.0);

    const double exhaustRunnerCrossSection = m_head->getExhaustRunnerCrossSectionArea();
    const double exhaustRunnerWidth = std::sqrt(exhaustRunnerCrossSection);
    const double exhaustTubeLength =
        exhaust->getPrimaryTubeLength() + m_head->getHeaderPrimaryLength(m_piston->getCylinderIndex());
    const double exhaustTubeVolume = exhaustRunnerCrossSection * exhaustTubeLength;
    const double totalExhaustRunnerVolume = m_head->getExhaustRunnerVolume() + exhaustTubeVolume;
    const double overallExhaustRunnerLength = totalExhaustRunnerVolume / exhaustRunnerCrossSection;
    m_exhaustRunnerAndPrimary.initialize(
        units::pressure(1.0, units::atm),
        totalExhaustRunnerVolume,
        units::celcius(25.0),
        exhaustMix);
    m_exhaustRunnerAndPrimary.setGeometry(
        overallExhaustRunnerLength,
        exhaustRunnerWidth,
        1.0,
        0.0);
}

void CombustionChamber::destroy() {
    if (m_pistonSpeed != nullptr) delete[] m_pistonSpeed;
    if (m_pressure != nullptr) delete[] m_pressure;

    m_pistonSpeed = nullptr;
    m_pressure = nullptr;
}

double CombustionChamber::getVolume() const {
    const double combustionPortVolume = m_head->getCombustionChamberVolume();
    const CylinderBank *bank = m_head->getCylinderBank();

    const double area = bank->boreSurfaceArea();
    const double s =
        m_piston->relativeX() * bank->getDx()
        + m_piston->relativeY() * bank->getDy();
    const double sweep =
        area * (bank->getDeckHeight() - s - m_piston->getCompressionHeight());

    return sweep + combustionPortVolume - m_piston->getDisplacement();
}

double CombustionChamber::pistonSpeed() const {
    const CylinderBank *bank = m_head->getCylinderBank();
    return
        m_piston->m_body.v_x * bank->getDx()
        + m_piston->m_body.v_y * bank->getDy();
}

double CombustionChamber::calculateMeanPistonSpeed() const {
    double avg = 0;
    for (int i = 0; i < StateSamples; ++i) {
        avg += m_pistonSpeed[i];
    }

    avg /= StateSamples;
    return avg;
}

double CombustionChamber::calculateFiringPressure() const {
    double firingPressure = 0;
    for (int i = 0; i < StateSamples; ++i) {
        if (m_pressure[i] > firingPressure) {
            firingPressure = m_pressure[i];
        }
    }

    return firingPressure;
}

bool CombustionChamber::popLitLastFrame() {
    const bool lit = m_litLastFrame;
    m_litLastFrame = false;

    return lit;
}

void CombustionChamber::setRandomSeed(unsigned int seed) {
    // splitmix64 of the seed so that neighbouring seeds give unrelated
    // sequences
    unsigned long long z = 0x9E3779B97F4A7C15ull * (seed + 1ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    m_randomState = (z ^ (z >> 31)) | 1ull;
}

double CombustionChamber::random() {
    // xorshift64*
    m_randomState ^= m_randomState >> 12;
    m_randomState ^= m_randomState << 25;
    m_randomState ^= m_randomState >> 27;
    const unsigned long long r = m_randomState * 0x2545F4914F6CDD1Dull;

    return static_cast<double>(r >> 11) * (1.0 / 9007199254740991.0);
}

double CombustionChamber::getSweptVolume() const {
    if (m_engine == nullptr || m_engine->getCylinderCount() == 0) return 0.0;
    else return m_engine->getDisplacement() / m_engine->getCylinderCount();
}

double CombustionChamber::crankAngle() const {
    double angle = m_engine->getOutputCrankshaft()->getCycleAngle();
    if (std::isnan(angle) || std::isinf(angle)) {
        angle = 0.0;
    }

    return angle;
}

double CombustionChamber::degreesAfter(double angle, double reference) const {
    const double cycleLength = m_engine->getOutputCrankshaft()->getCycleLength();
    double d = std::fmod(angle - reference, cycleLength);
    if (d >= cycleLength / 2) d -= cycleLength;
    else if (d < -cycleLength / 2) d += cycleLength;

    return d * (180.0 / constants::pi);
}

double CombustionChamber::airMolarMass() const {
    return m_engineering
        ? thermo::air::molarMass
        : units::AirMolecularMass;
}

double CombustionChamber::fuelMolarMass() const {
    return m_engineering
        ? m_fuel->getDefinition().molarMass
        : m_fuel->getMolecularMass();
}

double CombustionChamber::airMoles(const GasSystem::Mix &mix, double n) const {
    // Air is identified by its oxygen (the legacy model's air is 25 % O2)
    return n * mix.p_o2 / (m_engineering ? thermo::air::x_O2 : 0.25);
}

bool CombustionChamber::igniteEngineering() {
    const thermo::FuelDefinition &fuel = m_fuel->getDefinition();
    const thermo::Composition c = m_system.composition();
    if (c.n[thermo::Fuel] <= 0) return false;

    const double phi = thermo::equivalenceRatio(fuel, c);
    if (phi < fuel.leanLimit || phi > fuel.richLimit) return false;

    const double total = c.total();
    const double products =
        c.n[thermo::CO2] + c.n[thermo::H2O] + c.n[thermo::CO] + c.n[thermo::H2];

    m_flameEvent.equivalenceRatio = phi;
    m_flameEvent.residualFraction =
        clamp((products / total) / m_productFractionStoichiometric, 0.0, 1.0);
    m_flameEvent.ignitionTemperature = m_system.temperature();
    m_flameEvent.ignitionPressure = m_system.pressure();
    m_flameEvent.unburnedGamma = m_system.heatCapacityRatio();
    m_flameEvent.turbulence =
        m_meanPistonSpeedToTurbulence->sampleTriangle(calculateMeanPistonSpeed());
    m_flameEvent.efficiency = m_fuel->getCombustionEfficiency();
    m_flameEvent.flameSpeed = engineeringFlameSpeed();

    // Properties of the unburned charge and of the products it burns to,
    // used to split the charge into an unburned and a burned zone
    thermo::Composition unburned = m_system.mix().composition();
    const double n_unburned = unburned.total();
    thermo::Composition delta;
    thermo::burn(
        fuel,
        unburned.n[thermo::Fuel] * m_flameEvent.efficiency,
        unburned.n[thermo::O2],
        &delta);

    thermo::Composition burned = unburned;
    for (int i = 0; i < thermo::SpeciesCount; ++i) {
        burned.n[i] = std::fmax(burned.n[i] + delta.n[i], 0.0);
    }

    // Per mole of unburned gas; the burned polynomial includes the heat of
    // reaction so that both are on the stored energy's scale
    m_flameEvent.unburnedEnergy.clear();
    m_flameEvent.burnedEnergy.clear();
    for (int i = 0; i < thermo::SpeciesCount; ++i) {
        const thermo::EnergyPolynomial &e =
            (i == thermo::Fuel) ? fuel.energy : thermo::species(i).energy;
        m_flameEvent.unburnedEnergy.add(e, unburned.n[i] / n_unburned);
        m_flameEvent.burnedEnergy.add(e, burned.n[i] / n_unburned);
    }

    m_flameEvent.burnedMolesPerMole = burned.total() / n_unburned;
    m_flameEvent.burnedTemperature = 2000.0;

    return true;
}

double CombustionChamber::engineeringFlameSpeed() const {
    // Unburned gas compressed isentropically from the state at ignition
    const double P = m_system.pressure();
    const double gamma = m_flameEvent.unburnedGamma;
    const double T_u = (m_flameEvent.ignitionPressure > 0)
        ? m_flameEvent.ignitionTemperature
            * std::pow(P / m_flameEvent.ignitionPressure, (gamma - 1) / gamma)
        : m_system.temperature();

    const double S_L = m_fuel->laminarBurningVelocityEngineering(
        m_flameEvent.equivalenceRatio,
        T_u,
        P,
        m_flameEvent.residualFraction);

    return m_fuel->turbulentFlameSpeed(m_flameEvent.turbulence, S_L);
}

void CombustionChamber::ignite() {
    if (!m_lit && m_engineering) {
        if (!igniteEngineering()) return;

        m_flameEvent.lastVolume = getVolume();
        m_flameEvent.travel_x = 0;
        m_flameEvent.travel_y = 0;
        m_flameEvent.lit_n = 0;
        m_flameEvent.total_n = m_system.n();
        m_flameEvent.percentageLit = 0;
        m_flameEvent.globalMix = m_system.mix();
        m_lit = true;
        m_litLastFrame = true;
        m_combustionPhase = true;

        m_cycle.sparked = true;
        m_cycle.sparkAngle = crankAngle();
        return;
    }

    if (!m_lit) {
        if (m_system.mix().p_fuel == 0) return;

        const double afr = m_system.mix().p_o2 / m_system.mix().p_fuel;
        const double equivalenceRatio = afr / m_fuel->getMolecularAfr();
        if (equivalenceRatio < 0.5) return;
        else if (equivalenceRatio > 1.9) return;

        const double idealInert = m_system.mix().p_o2 / 0.7;
        const double dilution = (m_system.mix().p_inert / idealInert) - 1;

        m_flameEvent.lastVolume = getVolume();
        m_flameEvent.travel_x = 0;
        m_flameEvent.travel_y = 0;
        m_flameEvent.lit_n = 0;
        m_flameEvent.total_n = m_system.n();
        m_flameEvent.percentageLit = 0;
        m_flameEvent.globalMix = m_system.mix();
        m_lit = true;
        m_litLastFrame = true;
        m_combustionPhase = true;

        m_cycle.sparked = true;
        m_cycle.sparkAngle = crankAngle();

        const double randomness =
            m_fuel->getBurningEfficiencyRandomness();
        const double lowEfficiencyAttenuation =
            m_fuel->getLowEfficiencyAttenuation();
        const double maxBurningEfficiency =
            m_fuel->getMaxBurningEfficiency();
        const double maxTurbulenceEffect =
            m_fuel->getMaxTurbulenceEffect();
        const double maxDilutionEffect =
            m_fuel->getMaxDilutionEffect();

        const double turbulence =
            m_meanPistonSpeedToTurbulence->sampleTriangle(
                calculateMeanPistonSpeed());
        const double mixingFactor =
            1.0 - (
                clamp(turbulence / maxTurbulenceEffect)
                * clamp(1 - dilution / maxDilutionEffect));
        const double rand_s =
            lowEfficiencyAttenuation
            * ((1 - randomness) + randomness * random());
        const double efficiencyAttenuation =
            (mixingFactor * rand_s + (1 - mixingFactor));
        m_flameEvent.efficiency =
            efficiencyAttenuation * maxBurningEfficiency;
        m_flameEvent.flameSpeed = m_fuel->flameSpeed(
            turbulence,
            afr,
            m_system.temperature(),
            m_system.pressure(),
            calculateFiringPressure(),
            units::pressure(160, units::psi));
    }
}

void CombustionChamber::update(double dt) {
    const double volume = getVolume();
    trackCycle(volume - m_system.volume());
    m_system.setVolume(volume);

    updateCycleStates();

    m_intakeFlowRate = calculateIntakeFlowRate();
    m_exhaustFlowRate = calculateExhaustFlowRate();

    if (m_engineering) {
        updateHeatTransferCoefficient(calculateMeanPistonSpeed());

        if (m_lit) {
            m_flameEvent.flameSpeed = engineeringFlameSpeed();
        }
    }

    const double T = m_system.temperature();
    if (T > m_cycle.peakTemperature) {
        m_cycle.peakTemperature = T;
    }

    // Burn progress for the burn angles
    if (m_cycle.sparked
        && m_cycle.burnSamples < CycleTracker::MaxBurnSamples
        && (m_cycle.burnSamples == 0
            || m_cycle.burnAmount[m_cycle.burnSamples - 1] != m_cycle.fuelBurned))
    {
        m_cycle.burnAngle[m_cycle.burnSamples] = crankAngle();
        m_cycle.burnAmount[m_cycle.burnSamples] = m_cycle.fuelBurned;
        ++m_cycle.burnSamples;
    }
}

void CombustionChamber::updateHeatTransferCoefficient(double meanPistonSpeed) {
    // Woschni (1967):
    //   h = 3.26 B^-0.2 p^0.8 T^-0.55 w^0.8   [W/(m^2 K)], B [m], p [kPa]
    //   w = C1 Sp + C2 (V_d T_r / (p_r V_r)) (p - p_m)
    // with C1 = 6.18 during gas exchange and 2.28 otherwise, C2 = 3.24e-3
    // during combustion and expansion, and p_m the motored pressure from the
    // state at intake valve closing.
    const double V = m_system.volume();
    const double P = m_system.pressure();
    const double T = m_system.temperature();

    const bool intakeOpen = getIntakeOpening() > 0;
    const bool exhaustOpen = getExhaustOpening() > 0;
    if (m_intakeOpen && !intakeOpen) {
        m_ivcPressure = P;
        m_ivcVolume = V;
        m_ivcTemperature = T;
        m_ivcGamma = m_system.heatCapacityRatio();
    }

    m_intakeOpen = intakeOpen;
    if (exhaustOpen) {
        m_combustionPhase = false;
    }

    double w = ((intakeOpen || exhaustOpen) ? 6.18 : 2.28) * meanPistonSpeed;
    if (m_combustionPhase && m_ivcPressure > 0 && V > 0) {
        const double P_motored = m_ivcPressure * std::pow(m_ivcVolume / V, m_ivcGamma);
        w += 3.24E-3
            * (getSweptVolume() * m_ivcTemperature / (m_ivcPressure * m_ivcVolume))
            * std::fmax(P - P_motored, 0.0);
    }

    const double bore = m_head->getCylinderBank()->getBore();
    if (P <= 0 || T <= 0 || w <= 0 || bore <= 0) {
        m_currentHeatTransferCoefficient = 0;
        return;
    }

    m_currentHeatTransferCoefficient =
        m_heatTransferMultiplier
        * 3.26
        * std::pow(bore, -0.2)
        * std::pow(P / units::kPa, 0.8)
        * std::pow(T, -0.55)
        * std::pow(w, 0.8);
}

void CombustionChamber::trackCycle(double dV) {
    // Work done by the gas, as computed by GasSystem::changeVolume()
    const double P = m_system.pressure();
    const double V = m_system.volume() + dV;
    const double W = P * dV;
    m_cycle.strokeWork += W;
    m_cycle.workSinceExtreme += W;

    if (P > m_cycle.peakPressure) {
        m_cycle.peakPressure = P;
        m_cycle.peakPressureAngle = crankAngle();
    }

    if (m_cycle.direction == 0) {
        if (dV == 0) return;

        m_cycle.direction = (dV > 0) ? 1 : -1;
        m_cycle.extremeVolume = V;
        m_cycle.workSinceExtreme = 0;
        m_cycle.strokeWork = 0;
        return;
    }

    // Volume extremes (TDC and BDC) are confirmed once the volume has moved
    // away from them by a small fraction of the swept volume
    const double sweptVolume = getSweptVolume();
    const double threshold = (sweptVolume > 0)
        ? 0.002 * sweptVolume
        : 1E-3 * V;

    if (m_cycle.direction > 0) {
        if (V > m_cycle.extremeVolume) {
            m_cycle.extremeVolume = V;
            m_cycle.workSinceExtreme = 0;
        }
        else if (V < m_cycle.extremeVolume - threshold) {
            onStrokeEnd(false);
            m_cycle.direction = -1;
            m_cycle.extremeVolume = V;
        }
    }
    else {
        if (V < m_cycle.extremeVolume) {
            m_cycle.extremeVolume = V;
            m_cycle.extremePressure = P;
            m_cycle.extremeAngle = crankAngle();
            m_cycle.workSinceExtreme = 0;
        }
        else if (V > m_cycle.extremeVolume + threshold) {
            onStrokeEnd(true);
            m_cycle.direction = 1;
            m_cycle.extremeVolume = V;
        }
    }
}

void CombustionChamber::onStrokeEnd(bool tdc) {
    Stroke stroke;
    stroke.work = m_cycle.strokeWork - m_cycle.workSinceExtreme;
    stroke.endsAtTdc = tdc;
    stroke.tdcPressure = m_cycle.extremePressure;
    stroke.tdcAngle = m_cycle.extremeAngle;

    for (int i = 0; i < 3; ++i) {
        m_cycle.strokes[i] = m_cycle.strokes[i + 1];
    }

    m_cycle.strokes[3] = stroke;
    ++m_cycle.strokeCount;

    m_cycle.strokeWork = m_cycle.workSinceExtreme;
    m_cycle.workSinceExtreme = 0;

    // A cycle is complete at the BDC that ends the expansion stroke:
    // exhaust (-> TDC), intake (-> BDC), compression (-> firing TDC, the
    // TDC with the higher pressure), expansion (-> BDC)
    const Stroke *s = m_cycle.strokes;
    if (!tdc
        && m_cycle.strokeCount >= 4
        && s[0].endsAtTdc && !s[1].endsAtTdc && s[2].endsAtTdc
        && s[2].tdcPressure > s[0].tdcPressure)
    {
        closeCycle();
    }
}

void CombustionChamber::closeCycle() {
    const Stroke *s = m_cycle.strokes;
    const double tdcAngle = s[2].tdcAngle;
    const double sweptVolume = getSweptVolume();

    CycleMetrics metrics;
    metrics.valid = true;
    metrics.cycleCount = m_lastCycle.cycleCount + 1;
    metrics.firingTdcAngle = tdcAngle;
    metrics.grossWork = s[2].work + s[3].work;
    metrics.pumpingWork = s[0].work + s[1].work;
    metrics.netWork = metrics.grossWork + metrics.pumpingWork;
    if (sweptVolume > 0) {
        metrics.imepGross = metrics.grossWork / sweptVolume;
        metrics.imepNet = metrics.netWork / sweptVolume;
        metrics.pmep = -metrics.pumpingWork / sweptVolume;
    }

    metrics.peakPressure = m_cycle.peakPressure;
    metrics.peakPressureAngle = degreesAfter(m_cycle.peakPressureAngle, tdcAngle);
    metrics.peakTemperature = m_cycle.peakTemperature;
    metrics.fuelInducted = m_cycle.fuelInducted * fuelMolarMass();
    metrics.airInducted = m_cycle.airInducted * airMolarMass();
    metrics.fuelBurned = m_cycle.fuelBurned;
    metrics.heatLoss = m_cycle.heatLoss;

    if (m_cycle.sparked) {
        metrics.sparkAdvance = -degreesAfter(m_cycle.sparkAngle, tdcAngle);
    }

    // Burn angles from the cumulative burned fuel
    const int n = m_cycle.burnSamples;
    const double total = m_cycle.fuelBurned;
    if (n > 0 && total > 0) {
        metrics.burned = true;

        double *targets[] = { &metrics.ca10, &metrics.ca50, &metrics.ca90 };
        const double fractions[] = { 0.1, 0.5, 0.9 };
        for (int k = 0; k < 3; ++k) {
            const double target = fractions[k] * total;
            for (int i = 0; i < n; ++i) {
                if (m_cycle.burnAmount[i] < target) continue;

                double angle = degreesAfter(m_cycle.burnAngle[i], tdcAngle);
                if (i > 0) {
                    const double a0 = degreesAfter(m_cycle.burnAngle[i - 1], tdcAngle);
                    const double b0 = m_cycle.burnAmount[i - 1];
                    const double b1 = m_cycle.burnAmount[i];
                    if (b1 > b0) {
                        angle = a0 + (angle - a0) * (target - b0) / (b1 - b0);
                    }
                }

                *targets[k] = angle;
                break;
            }
        }
    }

    m_lastCycle = metrics;

    m_cycle.peakPressure = 0;
    m_cycle.peakTemperature = 0;
    m_cycle.fuelInducted = 0;
    m_cycle.airInducted = 0;
    m_cycle.heatLoss = 0;
    m_cycle.fuelBurned = 0;
    m_cycle.burnSamples = 0;
    m_cycle.sparked = false;
}

double CombustionChamber::calculateIntakeFlowRate() const {
    return m_head->intakeFlowRate(m_piston->getCylinderIndex());
}

double CombustionChamber::calculateExhaustFlowRate() const {
    return m_head->exhaustFlowRate(m_piston->getCylinderIndex());
}

double CombustionChamber::getIntakeOpening() const {
    return m_head->intakeValveLift(m_piston->getCylinderIndex());
}

double CombustionChamber::getExhaustOpening() const {
    return m_head->exhaustValveLift(m_piston->getCylinderIndex());
}

void CombustionChamber::flow(double dt) {
    if (m_system.temperature() > m_peakTemperature) {
        m_peakTemperature = m_system.temperature();
    }

    const double volume = getVolume();
    const double cylinderHeight = volume / m_cylinderCrossSectionSurfaceArea;
    const double cylinderSurfaceArea =
        cylinderHeight * constants::pi * m_head->getCylinderBank()->getBore()
        + m_cylinderCrossSectionSurfaceArea * 2;

    // Heat exchange with the cylinder walls (Newton's law of cooling). The
    // exact exponential solution is used instead of an explicit Euler step
    // so that large coefficients or small charges can't overshoot the wall
    // temperature and destabilize the simulation. The engineering model
    // uses Woschni's correlation for the coefficient.
    const double heatCapacity = m_system.heatCapacity();
    if (heatCapacity > 0) {
        const double h = m_engineering
            ? m_currentHeatTransferCoefficient
            : m_heatTransferCoefficient;
        const double T_wall = m_engineering
            ? m_wallTemperature
            : m_blockTemperature;
        const double T = m_system.temperature();
        const double decay =
            std::exp(-h * cylinderSurfaceArea * dt / heatCapacity);
        const double T_next = T_wall + (T - T_wall) * decay;
        const double dE = heatCapacity * (T_next - T);
        m_system.changeEnergy(dE);
        m_cycle.heatLoss -= dE;
    }
    m_system.flow(m_piston->getBlowbyK(), dt, m_crankcasePressure, units::celcius(25.0));

    Intake *intake = m_head->getIntake(m_piston->getCylinderIndex());
    ExhaustSystem *exhaust = m_head->getExhaustSystem(m_piston->getCylinderIndex());

    const double start_n = m_system.n();

    GasSystem::FlowParameters flowParams;
    flowParams.dt = dt;

    flowParams.k_flow = m_manifoldToRunnerFlowRate;
    flowParams.crossSectionArea_0 = intake->getPlenumCrossSectionArea();
    flowParams.crossSectionArea_1 = m_head->getIntakeRunnerCrossSectionArea();
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.system_0 = &intake->m_system;
    flowParams.system_1 = &m_intakeRunnerAndManifold;
    GasSystem::flow(flowParams);

    m_intakeRunnerAndManifold.dissipateExcessVelocity();

    flowParams.k_flow = m_intakeFlowRate;
    flowParams.crossSectionArea_0 = m_head->getIntakeRunnerCrossSectionArea();
    flowParams.crossSectionArea_1 = volume / cylinderHeight;
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.system_0 = &m_intakeRunnerAndManifold;
    flowParams.system_1 = &m_system;
    const double intakeFlow = GasSystem::flow(flowParams);

    // Fresh charge inducted (backflow is subtracted)
    {
        const GasSystem::Mix inductedMix = (intakeFlow > 0)
            ? m_intakeRunnerAndManifold.mix()
            : m_system.mix();
        m_cycle.fuelInducted += intakeFlow * inductedMix.p_fuel;
        m_cycle.airInducted += airMoles(inductedMix, intakeFlow);
    }

    m_intakeRunnerAndManifold.dissipateExcessVelocity();
    m_system.dissipateExcessVelocity();

    flowParams.k_flow = m_exhaustFlowRate;
    flowParams.crossSectionArea_0 = volume / cylinderHeight;
    flowParams.crossSectionArea_1 = m_head->getExhaustRunnerCrossSectionArea();
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.system_0 = &m_system;
    flowParams.system_1 = &m_exhaustRunnerAndPrimary;
    const double exhaustFlow = GasSystem::flow(flowParams);

    m_system.dissipateExcessVelocity();
    m_exhaustRunnerAndPrimary.dissipateExcessVelocity();

    flowParams.k_flow = m_primaryToCollectorFlowRate;
    flowParams.crossSectionArea_0 = m_head->getExhaustRunnerCrossSectionArea();
    flowParams.crossSectionArea_1 = exhaust->getCollectorCrossSectionArea();
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.system_0 = &m_exhaustRunnerAndPrimary;
    flowParams.system_1 = exhaust->getSystem();
    GasSystem::flow(flowParams);

    m_intakeRunnerAndManifold.updateVelocity(dt, intake->getVelocityDecay());
    m_system.updateVelocity(dt, 0.5);
    m_exhaustRunnerAndPrimary.updateVelocity(dt, exhaust->getVelocityDecay());

    if (std::abs(intakeFlow) > 1E-9 && m_lit) {
        m_lit = false;
    }

    m_exhaustFlow = exhaustFlow;
    m_lastTimestepTotalExhaustFlow += exhaustFlow;
    m_lastTimestepTotalIntakeFlow += intakeFlow;

    if (m_lit && m_engineering) {
        propagateFlame(dt, volume);
    }
    else if (m_lit) {
        CylinderBank *bank = m_head->getCylinderBank();
        const double totalTravel_x = bank->getBore() / 2;
        const double totalTravel_y = volume / bank->boreSurfaceArea();
        const double expansion = volume / m_flameEvent.lastVolume;
        const double lastTravel_x = m_flameEvent.travel_x;
        const double lastTravel_y = m_flameEvent.travel_y * expansion;
        const double flameSpeed = m_flameEvent.flameSpeed;

        m_flameEvent.travel_x =
            std::fmin(lastTravel_x + dt * flameSpeed, totalTravel_x);
        m_flameEvent.travel_y =
            std::fmin(lastTravel_y + dt * flameSpeed, totalTravel_y);

        if (lastTravel_x < m_flameEvent.travel_x || lastTravel_y < m_flameEvent.travel_y) {
            const double burnedVolume =
                m_flameEvent.travel_x * m_flameEvent.travel_x
                * constants::pi * m_flameEvent.travel_y;
            const double prevBurnedVolume =
                lastTravel_x * lastTravel_x * constants::pi * lastTravel_y;
            const double litVolume = burnedVolume - prevBurnedVolume;
            const double n = (litVolume / volume) * m_system.n();

            const double fuelBurned =
                m_system.react(n * m_flameEvent.efficiency, m_flameEvent.globalMix);
            const double massFuelBurned = fuelBurned * m_fuel->getMolecularMass();
            m_system.changeEnergy(
                massFuelBurned * m_fuel->getEnergyDensity());

            m_flameEvent.lit_n += n;
            m_flameEvent.percentageLit += litVolume / volume;

            m_nBurntFuel += massFuelBurned;
            m_cycle.fuelBurned += massFuelBurned;
        }
        else {
            m_lit = false;
        }

        m_flameEvent.lastVolume = volume;
    }
}

void CombustionChamber::propagateFlame(double dt, double volume) {
    // Quasi-dimensional entrainment: the flame consumes unburned gas at
    //     dn/dt = rho_u A_f S_T
    // where rho_u is the molar density of the unburned gas (compressed
    // isentropically since ignition) and A_f the area of the flame front.
    // The burned gas occupies the rest of the chamber; it is shaped like the
    // legacy model's flame (a cylinder centered below the spark plug that
    // grows radially and downwards at the same rate until it reaches the
    // cylinder wall or the piston), which gives the front's area.
    const double R_bore = m_head->getCylinderBank()->getBore() / 2;
    const double H = volume / (constants::pi * R_bore * R_bore);
    const double P = m_system.pressure();
    const double gamma = m_flameEvent.unburnedGamma;
    const double T_u = m_flameEvent.ignitionTemperature
        * std::pow(P / m_flameEvent.ignitionPressure, (gamma - 1) / gamma);

    const double n_unburned = std::fmax(m_flameEvent.total_n - m_flameEvent.lit_n, 0.0);
    const double rho_u = P / (constants::R * T_u);

    // Burned zone at the constant-pressure adiabatic flame temperature of
    // the unburned gas (enthalpy balance per mole of unburned gas, including
    // the chemical energy that the stored sensible energy leaves out):
    //     e_b(T_b) + n_b R T_b + chem_b = e_u(T_u) + R T_u + chem_u
    double V_burned = 0;
    if (m_flameEvent.lit_n > 0) {
        const double n_b = m_flameEvent.burnedMolesPerMole;
        const double h_u =
            m_flameEvent.unburnedEnergy.e(T_u) + constants::R * T_u
            + m_flameEvent.unburnedEnergy.chem - m_flameEvent.burnedEnergy.chem;

        double T_b = m_flameEvent.burnedTemperature;
        for (int i = 0; i < 8; ++i) {
            const double f = m_flameEvent.burnedEnergy.e(T_b) + n_b * constants::R * T_b - h_u;
            const double df = m_flameEvent.burnedEnergy.cv(T_b) + n_b * constants::R;
            const double step = f / df;
            T_b = clamp(T_b - step, T_u, thermo::T_max);
            if (std::abs(step) < 0.01) break;
        }

        m_flameEvent.burnedTemperature = T_b;
        V_burned = clamp(
            m_flameEvent.lit_n * n_b * constants::R * T_b / P,
            0.0,
            volume);
    }

    // Flame kernel created by the spark
    constexpr double KernelRadius = 1.0E-3;
    double r, y;
    const double s = std::cbrt(V_burned / constants::pi);
    if (s <= std::fmin(R_bore, H)) {
        r = y = std::fmax(s, KernelRadius);
    }
    else if (H < R_bore) {
        y = H;
        r = std::fmin(std::sqrt(V_burned / (constants::pi * H)), R_bore);
    }
    else {
        r = R_bore;
        y = std::fmin(V_burned / (constants::pi * R_bore * R_bore), H);
    }

    double frontArea = 0;
    if (r < R_bore) frontArea += 2 * constants::pi * r * y;
    if (y < H) frontArea += constants::pi * r * r;

    // Once the front reaches the walls the remaining gas burns out with the
    // time scale of the turbulent flame crossing a fraction of the bore
    const double flameSpeed = m_flameEvent.flameSpeed;
    double dn;
    if (frontArea > 0) {
        dn = rho_u * frontArea * flameSpeed * dt;
    }
    else {
        const double burnOutRate = flameSpeed / (0.2 * R_bore);
        dn = n_unburned * (1 - std::exp(-burnOutRate * dt));
    }

    dn = std::fmin(dn, n_unburned);

    m_flameEvent.travel_x = r;
    m_flameEvent.travel_y = y;
    m_flameEvent.lastVolume = volume;

    if (dn > 0) {
        // The heat of reaction is added by the gas model
        const double fuelBurned =
            m_system.burn(dn, m_flameEvent.globalMix, m_flameEvent.efficiency);
        const double massFuelBurned = fuelBurned * m_fuel->getDefinition().molarMass;

        m_flameEvent.lit_n += dn;
        m_flameEvent.percentageLit = m_flameEvent.lit_n / m_flameEvent.total_n;

        m_nBurntFuel += massFuelBurned;
        m_cycle.fuelBurned += massFuelBurned;
    }

    if (m_flameEvent.total_n - m_flameEvent.lit_n <= 1E-4 * m_flameEvent.total_n) {
        m_lit = false;
    }
}

double CombustionChamber::lastEventAfr() const {
    if (m_engineering) {
        const double fuel = m_flameEvent.globalMix.p_fuel * fuelMolarMass();
        return (fuel > 0)
            ? airMoles(m_flameEvent.globalMix, 1.0) * airMolarMass() / fuel
            : 0.0;
    }

    const double totalFuel = m_flameEvent.globalMix.p_fuel * m_flameEvent.total_n;
    const double totalOxygen = m_flameEvent.globalMix.p_o2 * m_flameEvent.total_n;
    const double totalInert = m_flameEvent.globalMix.p_inert * m_flameEvent.total_n;

    constexpr double octaneMolarMass = units::mass(114.23, units::g);
    constexpr double oxygenMolarMass = units::mass(31.9988, units::g);
    constexpr double nitrogenMolarMass = units::mass(28.014, units::g);

    if (totalFuel == 0) return 0;
    else {
        return
            (oxygenMolarMass * totalOxygen + totalInert * nitrogenMolarMass)
            / (totalFuel * octaneMolarMass);
    }
}

double CombustionChamber::calculateFrictionForce(double v_s) const {
    if (!m_pistonFriction) return 0.0;

    const double cylinderWallForce = m_piston->calculateCylinderWallForce();

    const double F_coul = m_frictionModel.frictionCoeff * cylinderWallForce;
    const double v_st = m_frictionModel.breakawayFrictionVelocity * constants::root_2;
    const double v_coul = m_frictionModel.breakawayFrictionVelocity / 10;
    const double F_brk = m_frictionModel.breakawayFriction;
    const double v = std::abs(v_s);

    const double F_0 = constants::root_2 * constants::e * (F_brk - F_coul);
    const double F_1 = v / v_st;
    const double F_2 = std::exp(-F_1 * F_1) * F_1;
    const double F_3 = F_coul * std::tanh(v / v_coul);
    const double F_4 = m_frictionModel.viscousFrictionCoefficient * v;

    return F_0 * F_2 + F_3 + F_4;
}

void CombustionChamber::updateCycleStates() {
    double crankAngle = m_engine->getOutputCrankshaft()->getCycleAngle();
    if (std::isnan(crankAngle) || std::isinf(crankAngle)) {
        crankAngle = 0.0;
    }

    const double cycleLength = m_engine->getOutputCrankshaft()->getCycleLength();
    const int i = (int)std::round((crankAngle / cycleLength) * (StateSamples - 1.0));

    m_pistonSpeed[i] = std::abs(pistonSpeed());
    m_pressure[i] = m_system.pressure();
}

void CombustionChamber::apply(atg_scs::SystemState *system) {
    CylinderBank *bank = m_head->getCylinderBank();
    const double area = (bank->getBore() * bank->getBore() / 4.0) * constants::pi;
    const double v_x = system->v_x[m_piston->m_body.index];
    const double v_y = system->v_y[m_piston->m_body.index];

    const double v_s =
        v_x * bank->getDx() + v_y * bank->getDy();

    const double pressureDifferential = m_system.pressure() - m_crankcasePressure;
    const double force = -area * pressureDifferential;

    if (std::isnan(force) || std::isinf(force)) {
        assert(false);
    }

    constexpr double limit = 1E-3;
    const double abs_v_s = std::fmin(std::abs(v_s), limit);
    const double attenuation = abs_v_s / limit;

    const double F = calculateFrictionForce(v_s) * attenuation;
    const double F_fric = (v_s > 0)
        ? -F
        : F;

    system->applyForce(
        0.0,
        0.0,
        (force + F_fric) * bank->getDx(),
        (force + F_fric) * bank->getDy(),
        m_piston->m_body.index);
}

double CombustionChamber::getFrictionForce() const {
    CylinderBank *bank = m_head->getCylinderBank();
    const double v_x = m_piston->m_body.v_x;
    const double v_y = m_piston->m_body.v_y;

    const double v_s =
        v_x * bank->getDx() + v_y * bank->getDy();

    return calculateFrictionForce(v_s);
}
