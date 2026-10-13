#include "../include/engine.h"

#include "../include/constants.h"
#include "../include/units.h"
#include "../include/fuel.h"
#include "../include/piston_engine_simulator.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <assert.h>

Engine::Engine() {
    m_name = "";

    m_crankshafts = nullptr;
    m_cylinderBanks = nullptr;
    m_heads = nullptr;
    m_pistons = nullptr;
    m_connectingRods = nullptr;
    m_exhaustSystems = nullptr;
    m_intakes = nullptr;
    m_combustionChambers = nullptr;

    m_crankshaftCount = 0;
    m_cylinderBankCount = 0;
    m_cylinderCount = 0;
    m_intakeCount = 0;
    m_exhaustSystemCount = 0;
    m_starterSpeed = 0;
    m_starterTorque = 0;
    m_dynoMinSpeed = 0;
    m_dynoMaxSpeed = 0;
    m_dynoHoldStep = 0;
    m_redline = 0;

    m_throttle = nullptr;
    m_throttleValue = 0.0;

    m_initialSimulationFrequency = 10000.0;
    m_initialHighFrequencyGain = 0.01;
    m_initialJitter = 0.5;
    m_initialNoise = 1.0;
    m_initialConvolution = 1.0;
    m_maxSleSolverSteps = 128;
    m_fluidSimulationSteps = 8;
    m_heatTransferCoefficient = 100.0;
    m_blockTemperature = units::celcius(90.0);

    m_displacement = 0.0;
    m_compressionRatio = 0.0;
    m_physicsModel = GasSystem::Model::Legacy;
    m_wallTemperature = 450.0;
    m_heatTransferMultiplier = 1.0;
    m_frictionTorque = 0.0;
    m_fmep = 0.0;
}

Engine::~Engine() {
    assert(m_crankshafts == nullptr);
    assert(m_cylinderBanks == nullptr);
    assert(m_pistons == nullptr);
    assert(m_connectingRods == nullptr);
    assert(m_heads == nullptr);
    assert(m_exhaustSystems == nullptr);
    assert(m_intakes == nullptr);
    assert(m_combustionChambers == nullptr);
}

void Engine::initialize(const Parameters &params) {
    m_crankshaftCount = params.crankshaftCount;
    m_cylinderCount = params.cylinderCount;
    m_cylinderBankCount = params.cylinderBanks;
    m_exhaustSystemCount = params.exhaustSystemCount;
    m_intakeCount = params.intakeCount;
    m_starterTorque = params.starterTorque;
    m_starterSpeed = params.starterSpeed;
    m_dynoMinSpeed = params.dynoMinSpeed;
    m_dynoMaxSpeed = params.dynoMaxSpeed;
    m_dynoHoldStep = params.dynoHoldStep;
    m_redline = params.redline;
    m_name = params.name;
    m_throttle = params.throttle;
    m_initialHighFrequencyGain = params.initialHighFrequencyGain;
    m_initialSimulationFrequency = params.initialSimulationFrequency;
    m_initialJitter = params.initialJitter;
    m_initialNoise = params.initialNoise;
    m_initialConvolution = params.initialConvolution;
    m_maxSleSolverSteps = std::max(1, params.maxSleSolverSteps);
    m_fluidSimulationSteps = std::max(1, params.fluidSimulationSteps);
    m_heatTransferCoefficient = params.heatTransferCoefficient;
    m_blockTemperature = params.blockTemperature;
    m_physicsModel = params.physicsModel;
    m_wallTemperature = params.wallTemperature;
    m_heatTransferMultiplier = params.heatTransferMultiplier;
    m_frictionParameters = params.friction;

    m_crankshafts = new Crankshaft[m_crankshaftCount];
    m_cylinderBanks = new CylinderBank[m_cylinderBankCount];
    m_heads = new CylinderHead[m_cylinderBankCount];
    m_pistons = new Piston[m_cylinderCount];
    m_connectingRods = new ConnectingRod[m_cylinderCount];
    m_exhaustSystems = new ExhaustSystem[m_exhaustSystemCount];
    m_intakes = new Intake[m_intakeCount];
    m_combustionChambers = new CombustionChamber *[m_cylinderCount];

    for (int i = 0; i < m_exhaustSystemCount; ++i) {
        m_exhaustSystems[i].m_index = i;
    }

    for (int i = 0; i < m_cylinderCount; ++i) {
        m_combustionChambers[i] = createCombustionChamber(i);
        m_combustionChambers[i]->setEngine(this);
        m_combustionChambers[i]->setRandomSeed(static_cast<unsigned int>(i));
    }
}

CombustionChamber *Engine::createCombustionChamber(int index) {
    return new CombustionChamber;
}

void Engine::destroy() {
    for (int i = 0; i < m_crankshaftCount; ++i) {
        m_crankshafts[i].destroy();
    }

    for (int i = 0; i < m_cylinderCount; ++i) {
        m_pistons[i].destroy();
        m_connectingRods[i].destroy();
        m_combustionChambers[i]->destroy();
        delete m_combustionChambers[i];
    }

    for (int i = 0; i < m_cylinderBankCount; ++i) {
        m_heads[i].destroy();
        m_cylinderBanks[i].destroy();
    }

    for (int i = 0; i < m_exhaustSystemCount; ++i) {
        m_exhaustSystems[i].destroy();
    }

    for (int i = 0; i < m_intakeCount; ++i) {
        m_intakes[i].destroy();
    }

    m_ignitionModule.destroy();

    if (m_throttle != nullptr) delete m_throttle;
    if (m_crankshafts != nullptr) delete[] m_crankshafts;
    if (m_cylinderBanks != nullptr) delete[] m_cylinderBanks;
    if (m_heads != nullptr) delete[] m_heads;
    if (m_pistons != nullptr) delete[] m_pistons;
    if (m_connectingRods != nullptr) delete[] m_connectingRods;
    if (m_exhaustSystems != nullptr) delete[] m_exhaustSystems;
    if (m_intakes != nullptr) delete[] m_intakes;
    if (m_combustionChambers != nullptr) delete[] m_combustionChambers;

    m_crankshafts = nullptr;
    m_cylinderBanks = nullptr;
    m_pistons = nullptr;
    m_connectingRods = nullptr;
    m_heads = nullptr;
    m_exhaustSystems = nullptr;
    m_intakes = nullptr;
    m_combustionChambers = nullptr;
    m_throttle = nullptr;

    for (auto it = m_ownedObjects.rbegin(); it != m_ownedObjects.rend(); ++it) {
        it->second(it->first);
    }

    m_ownedObjects.clear();
}

Crankshaft *Engine::getOutputCrankshaft() const {
    return &m_crankshafts[0];
}

void Engine::setSpeedControl(double s) {
    m_throttle->setSpeedControl(s);
}

double Engine::getSpeedControl() {
    return m_throttle->getSpeedControl();
}

void Engine::setThrottle(double throttle) {
    for (int i = 0; i < m_intakeCount; ++i) {
        m_intakes[i].m_throttle = throttle;
    }

    m_throttleValue = throttle;
}

double Engine::getThrottle() const {
    return m_throttleValue;
}

double Engine::getThrottlePlateAngle() const {
    return (1 - m_intakes[0].getThrottlePlatePosition()) * (constants::pi / 2);
}

bool placeRod(
    const ConnectingRod &rod,
    const CylinderBank &bank,
    const Crankshaft &crankshaft,
    double crankshaftAngle,
    double *p_x,
    double *p_y,
    double *theta,
    double *s)
{
    double p_x_0, p_y_0, l_x, l_y, theta_0;
    if (rod.getMasterRod() != nullptr) {
        double s;
        const bool succeeded = placeRod(
            *rod.getMasterRod(),
            *rod.getMasterRod()->getPiston()->getCylinderBank(),
            *rod.getCrankshaft(),
            crankshaftAngle,
            &p_x_0,
            &p_y_0,
            &theta_0,
            &s);

        if (!succeeded) {
            return false;
        }

        rod.getMasterRod()->getRodJournalPositionLocal(rod.getPiston()->getCylinderIndex(), &l_x, &l_y);
    }
    else {
        theta_0 = crankshaftAngle;
        p_x_0 = rod.getCrankshaft()->getPosX();
        p_y_0 = rod.getCrankshaft()->getPosY();
        rod.getCrankshaft()->getRodJournalPositionLocal(rod.getPiston()->getCylinderIndex(), &l_x, &l_y);
    }

    const double dx = std::cos(theta_0);
    const double dy = std::sin(theta_0);
    *p_x = p_x_0 + (dx * l_x - dy * l_y);
    *p_y = p_y_0 + (dy * l_x + dx * l_y);

    // (bank->m_x + bank->m_dx * s - p_x)^2 + (bank->m_y + bank->m_dy * s - p_y)^2 = (rod->m_length)^2
    const double a = bank.getDx() * bank.getDx() + bank.getDy() * bank.getDy();
    const double b = -2 * bank.getDx() * ((*p_x) - bank.getX()) - 2 * bank.getDy() * ((*p_y) - bank.getY());
    const double c =
        ((*p_x) - bank.getX()) * ((*p_x) - bank.getX())
        + ((*p_y) - bank.getY()) * ((*p_y) - bank.getY())
        - rod.getLength() * rod.getLength();

    const double det = b * b - 4 * a * c;
    if (det < 0) return false;

    const double sqrt_det = std::sqrt(det);
    const double s0 = (-b + sqrt_det) / (2 * a);
    const double s1 = (-b - sqrt_det) / (2 * a);

    *s = std::max(s0, s1);
    if (*s < 0) return false;
   
    if (s != nullptr) {
        const double dx = (bank.getX() + bank.getDx() * (*s)) - (*p_x);
        const double dy = (bank.getY() + bank.getDy() * (*s)) - (*p_y);

        *theta = (dy > 0)
            ? std::acos(dx)
            : -std::acos(dx);
    }

    return true;
}

void Engine::calculateDisplacement() {
    // There is a closed-form/correct way to do this which I really
    // don't feel like deriving right now, so I'm just going with this
    // numerical approximation.
    constexpr int Resolution = 1000;

    std::vector<double> min_s(m_cylinderCount, DBL_MAX);
    std::vector<double> max_s(m_cylinderCount, -DBL_MAX);

    for (int j = 0; j < Resolution; ++j) {
        const double crankshaftAngle = 2 * (j / static_cast<double>(Resolution)) * constants::pi;

        for (int i = 0; i < m_cylinderCount; ++i) {
            const Piston &piston = m_pistons[i];
            const CylinderBank &bank = *piston.getCylinderBank();
            const ConnectingRod &rod = *piston.getRod();
            const Crankshaft &shaft = *rod.getCrankshaft();

            double p_x, p_y;
            double theta;
            double s;
            if (!placeRod(
                rod,
                bank,
                shaft,
                crankshaftAngle,
                &p_x,
                &p_y,
                &theta,
                &s))
            {
                continue;
            }

            min_s[i] = std::min(min_s[i], s);
            max_s[i] = std::max(max_s[i], s);
        }
    }

    double displacement = 0;
    double compressionRatio = 0;
    int compressionRatioSamples = 0;
    for (int i = 0; i < m_cylinderCount; ++i) {
        const Piston &piston = m_pistons[i];
        const CylinderBank &bank = *piston.getCylinderBank();

        if (min_s[i] < max_s[i]) {
            const double r = bank.getBore() / 2.0;
            const double area = constants::pi * r * r;
            const double swept = area * (max_s[i] - min_s[i]);
            displacement += swept;

            // Clearance volume at TDC (see CombustionChamber::getVolume())
            const CylinderHead &head = m_heads[bank.getIndex()];
            const double clearance =
                area * (bank.getDeckHeight() - max_s[i] - piston.getCompressionHeight())
                + head.getCombustionChamberVolume()
                - piston.getDisplacement();
            if (clearance > 0) {
                compressionRatio += (swept + clearance) / clearance;
                ++compressionRatioSamples;
            }
        }
    }

    m_displacement = displacement;
    m_compressionRatio = (compressionRatioSamples > 0)
        ? compressionRatio / compressionRatioSamples
        : 0.0;
}

void Engine::initializeFriction() {
    calculateDisplacement();

    EngineFriction::Geometry geometry;
    geometry.cylinders = m_cylinderCount;
    geometry.banks = m_cylinderBankCount;
    geometry.displacement = m_displacement;
    geometry.compressionRatio = (m_compressionRatio > 0) ? m_compressionRatio : 10.0;

    double bore = 0;
    for (int i = 0; i < m_cylinderCount; ++i) {
        bore += m_pistons[i].getCylinderBank()->getBore();
    }

    geometry.bore = (m_cylinderCount > 0) ? bore / m_cylinderCount : 0.0;
    geometry.stroke = (geometry.bore > 0 && m_cylinderCount > 0)
        ? m_displacement / (m_cylinderCount * constants::pi * geometry.bore * geometry.bore / 4)
        : 0.0;

    for (int i = 0; i < m_crankshaftCount; ++i) {
        geometry.crankJournals += m_crankshafts[i].getRodJournalCount();
    }

    EngineFriction::Parameters params = m_frictionParameters;
    if (geometry.bore <= 0 || geometry.stroke <= 0) {
        params.model = EngineFriction::Model::Legacy;
    }

    m_friction.initialize(params, geometry);
}

double Engine::updateFriction() {
    if (!usesFrictionModel()) {
        m_fmep = 0;
        m_frictionTorque = 0;
        return 0;
    }

    double peakPressure = 0;
    int samples = 0;
    for (int i = 0; i < m_cylinderCount; ++i) {
        const CombustionChamber::CycleMetrics &metrics =
            m_combustionChambers[i]->getLastCycleMetrics();
        if (metrics.valid) {
            peakPressure += metrics.peakPressure;
            ++samples;
        }
    }

    if (samples > 0) peakPressure /= samples;

    const double intakePressureRatio =
        getManifoldPressure() / units::pressure(1.0, units::atm);
    m_fmep = m_friction.fmep(getSpeed(), intakePressureRatio, peakPressure);
    m_frictionTorque = m_friction.torque(m_fmep);

    return m_frictionTorque;
}

double Engine::getIntakeFlowRate() const {
    double airIntake = 0;
    for (int i = 0; i < m_intakeCount; ++i) {
        airIntake += m_intakes[i].m_flowRate;
    }

    return airIntake;
}

void Engine::update(double dt) {
    m_throttle->update(dt, this);
}

double Engine::getManifoldPressure() const {
    double pressureSum = 0.0;
    for (int i = 0; i < m_intakeCount; ++i) {
        pressureSum += m_intakes[i].m_system.pressure();
    }

    return pressureSum / m_intakeCount;
}

double Engine::getIntakeAfr() const {
    if (isEngineering()) {
        double air = 0, fuel = 0;
        for (int i = 0; i < m_intakeCount; ++i) {
            const thermo::Composition c = m_intakes[i].m_system.composition();
            air += c.n[thermo::O2] / thermo::air::x_O2 * thermo::air::molarMass;
            fuel += c.n[thermo::Fuel] * m_fuel.getDefinition().molarMass;
        }

        return (fuel > 0) ? air / fuel : 0.0;
    }

    double totalOxygen = 0.0;
    double totalFuel = 0.0;
    for (int i = 0; i < m_intakeCount; ++i) {
        totalOxygen += m_intakes[i].m_system.n_o2();
        totalFuel += m_intakes[i].m_system.n_fuel();
    }

    constexpr double octaneMolarMass = units::mass(114.23, units::g);
    constexpr double oxygenMolarMass = units::mass(31.9988, units::g);

    if (totalFuel == 0) return 0;
    else {
        return
            (oxygenMolarMass * totalOxygen / 0.21)
            / (totalFuel * octaneMolarMass);
    }
}

double Engine::getExhaustO2() const {
    if (isEngineering()) {
        // Mass fraction of oxygen
        double o2 = 0, mass = 0;
        for (int i = 0; i < m_exhaustSystemCount; ++i) {
            const GasSystem &system = m_exhaustSystems[i].m_system;
            o2 += system.n_o2() * thermo::species(thermo::O2).molarMass;
            mass += system.mass();
        }

        return (mass > 0) ? o2 / mass : 0.0;
    }

    double totalInert = 0.0;
    double totalOxygen = 0.0;
    double totalFuel = 0.0;
    for (int i = 0; i < m_exhaustSystemCount; ++i) {
        totalInert += m_exhaustSystems[i].m_system.n_inert();
        totalOxygen += m_exhaustSystems[i].m_system.n_o2();
        totalFuel += m_exhaustSystems[i].m_system.n_fuel();
    }

    constexpr double octaneMolarMass = units::mass(114.23, units::g);
    constexpr double oxygenMolarMass = units::mass(31.9988, units::g);
    constexpr double nitrogenMolarMass = units::mass(28.014, units::g);

    if (totalFuel == 0) return 0;
    else {
        return
            (oxygenMolarMass * totalOxygen)
            / (
                totalFuel * octaneMolarMass
                + nitrogenMolarMass * totalInert
                + oxygenMolarMass * totalOxygen);
    }
}

void Engine::resetFuelConsumption() {
    for (int i = 0; i < m_intakeCount; ++i) {
        m_intakes[i].m_totalFuelInjected = 0;
    }
}

double Engine::getTotalFuelMassConsumed() const {
    double n_fuelConsumed = 0;
    for (int i = 0; i < m_intakeCount; ++i) {
        n_fuelConsumed += m_intakes[i].m_totalFuelInjected;
    }

    return n_fuelConsumed * (isEngineering()
        ? m_fuel.getDefinition().molarMass
        : m_fuel.getMolecularMass());
}

double Engine::getTotalVolumeFuelConsumed() const {
    return getTotalFuelMassConsumed() / (isEngineering()
        ? m_fuel.getDefinition().density
        : m_fuel.getDensity());
}

int Engine::getMaxDepth() const {
    int maxDepth = 0;
    for (int i = 0; i < m_crankshaftCount; ++i) {
        maxDepth = std::max(m_crankshafts[i].getRodJournalCount(), maxDepth);
    }

    return maxDepth;
}

Simulator *Engine::createSimulator(Vehicle *vehicle, Transmission *transmission) {
    PistonEngineSimulator *simulator = new PistonEngineSimulator;
    Simulator::Parameters simulatorParams;
    simulatorParams.systemType = Simulator::SystemType::NsvOptimized;
    simulator->initialize(simulatorParams);

    simulator->loadSimulation(this, vehicle, transmission);
    simulator->setFluidSimulationSteps(m_fluidSimulationSteps);
    simulator->setMaxSleSolverSteps(m_maxSleSolverSteps);

    return static_cast<Simulator *>(simulator);
}

double Engine::getRpm() const {
    if (m_crankshaftCount == 0) return 0;
    else return std::abs(units::toRpm(getCrankshaft(0)->m_body.v_theta));
}

double Engine::getSpeed() const {
    if (m_crankshaftCount == 0) return 0;
    else return std::abs(getCrankshaft(0)->m_body.v_theta);
}

bool Engine::isSpinningCw() const {
    return getOutputCrankshaft()->m_body.v_theta <= 0;
}
