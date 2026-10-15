#include "../include/forced_induction.h"

#include "../include/crankshaft.h"
#include "../include/intake.h"
#include "../include/units.h"
#include "../include/utilities.h"

#include <algorithm>
#include <cmath>

namespace {

    constexpr double Ambient = units::pressure(1.0, units::atm);
    constexpr double AmbientTemperature = units::celcius(25.0);

    // Air properties for the compressor
    constexpr double Gamma = 1.4;
    constexpr double Cp_mass = 1005.0;                          // [J/(kg K)]
    constexpr double Cp_molar = constants::R * Gamma / (Gamma - 1);
    constexpr double Exponent = (Gamma - 1) / Gamma;

    // Tip speed limit of aluminium compressor wheels [m/s]
    constexpr double MaxTipSpeed = 520.0;

    double approach(double value, double target, double dt, double timeConstant) {
        return value + (target - value) * std::min(1.0, dt / timeConstant);
    }

    GasSystem::Mix airMix(bool engineering) {
        GasSystem::Mix mix;
        if (engineering) {
            return GasSystem::Mix::fromComposition(
                thermo::freshCharge(thermo::defaultFuel(), 0.0));
        }

        mix.p_fuel = 0;
        mix.p_o2 = 0.25;
        mix.p_inert = 0.75;
        return mix;
    }

} /* namespace */

ForcedInduction::ForcedInduction() {
    m_type = Type::Turbocharger;
    m_arrangement = Arrangement::Single;
    m_unitCount = 1;

    m_crankshaft = nullptr;
    for (Intake *&intake : m_intakes) intake = nullptr;
    m_intakeCount = 0;
    m_exhaustSystemCount = 1;

    m_tipRadius = 0;
    m_compressorK = 0;
    m_compressorEfficiency = 0.75;
    m_maxSpeed = 0;
    m_slip = 0.9;

    m_displacement = 0;
    m_leakK = 0;
    m_bypassK = 0;
    m_bypassValve = false;
    m_bypass = 0;
    m_driveRatio = 1;
    m_driveTorque = 0;
    m_drivePower = 0;

    m_turbineK = 0;
    m_wastegateK = 0;
    m_turbineEfficiency = 0.7;
    m_inertia = 3e-5;
    m_bearingLoss = 0;
    m_turbineFraction = 0;
    m_sequentialSpeed = 0;
    m_turbineEnergy = 0;

    m_boostTarget = 0;
    m_wastegate = 0;
    m_blowoffEnabled = true;
    m_blowoff = 0;
    m_blowoffK = 0;
    m_blowoffFlowRate = 0;

    m_intercoolerEffectiveness = 0.7;
    m_compressorOutletTemperature = AmbientTemperature;
}

ForcedInduction::~ForcedInduction() {
    /* void */
}

void ForcedInduction::initialize(const Parameters &params) {
    m_type = params.type;
    m_arrangement = isTurbocharger() ? params.arrangement : Arrangement::Single;
    m_unitCount = (m_arrangement == Arrangement::Single) ? 1 : 2;

    const double d = clamp(params.compressorDiameter, 0.02, 0.2);
    m_tipRadius = d / 2;
    m_compressorEfficiency = clamp(params.compressorEfficiency, 0.4, 0.85);
    m_maxSpeed = (params.maxSpeed > 0)
        ? params.maxSpeed
        : MaxTipSpeed / m_tipRadius;

    // The compressor delivers its maximum flow when the source pressure
    // (about 3 bar at the design speed) drops by a quarter across it
    const double compressorFlow = clamp(params.compressorFlow, 0.01, 3.0) / units::AirMolecularMass;
    m_compressorK = GasSystem::flowConstant(
        compressorFlow, 3.0 * Ambient, 0.75 * Ambient, units::celcius(50.0), Gamma);

    m_displacement = std::max(params.displacement, 0.0);
    m_driveRatio = std::max(params.driveRatio, 0.0);
    m_bypassValve = params.bypassValve;

    // Rotor clearances leak back about 40% (Roots) or 20% (twin screw) of
    // the delivery at 1 bar of boost and 6000 rpm rotor speed
    const double leakFraction = (m_type == Type::Roots) ? 0.4 : 0.2;
    const double pdFlow = Ambient * m_displacement * 100.0 / (constants::R * AmbientTemperature);
    m_leakK = GasSystem::flowConstant(
        std::max(leakFraction * pdFlow, 1e-6), 2.0 * Ambient, 1.0 * Ambient, units::celcius(60.0), Gamma);
    m_bypassK = GasSystem::flowConstant(
        std::max(pdFlow, 1e-6), Ambient, units::pressure(1.5, units::inHg), AmbientTemperature, Gamma);

    const double ar = clamp(params.turbineAr, 0.3, 1.6);
    // Turbine flow capacity scales with the housing's A/R. Turbines are
    // sized to spool well below the engine's maximum flow (about half the
    // compressor's maximum flow at an expansion ratio of 2.2 for a 0.64 A/R
    // housing); the wastegate bypasses the excess.
    const double turbineFlow = 0.5 * compressorFlow * std::pow(ar / 0.64, 0.9);
    m_turbineK = GasSystem::flowConstant(
        turbineFlow, 2.2 * Ambient, 1.2 * Ambient, 1000.0, 1.33);
    m_wastegateK = 4.0 * m_turbineK;
    m_turbineEfficiency = clamp(params.turbineEfficiency, 0.3, 0.85);
    m_inertia = std::max(params.inertia, 1e-6);

    // Bearing and windage losses (P = c * omega^2) scale with the wheel
    // size; ball bearings lose a little over half as much as journal
    // bearings
    m_bearingLoss = (params.ballBearings ? 1.1e-6 : 2.0e-6) * std::pow(d / 0.06, 3.0);
    m_sequentialSpeed = params.sequentialSpeed;

    m_boostTarget = std::max(params.boost, 0.0);
    m_blowoffEnabled = params.blowoffValve;
    m_blowoffK = GasSystem::flowConstant(
        0.5 * compressorFlow, 2.0 * Ambient, 1.0 * Ambient, units::celcius(50.0), Gamma);
    if (isPositiveDisplacement()) {
        m_blowoffK = m_bypassK;
    }

    m_intercoolerEffectiveness = clamp(params.intercoolerEffectiveness, 0.0, 1.0);

    for (Unit &unit : m_units) unit = Unit();
    m_units[1].active = (m_arrangement == Arrangement::Twin);

    // Charge pipes of about 2.5" diameter
    const double area = units::area(32.0, units::cm2);
    const double volume = std::max(params.chargeVolume, 0.5e-3);
    m_charge.setModel(params.model, params.fuel);
    m_charge.initialize(Ambient, volume, AmbientTemperature, airMix(params.model == GasSystem::Model::Engineering));
    m_charge.setGeometry(volume / area, std::sqrt(area), 1.0, 0.0);
}

void ForcedInduction::destroy() {
    /* void */
}

void ForcedInduction::addIntake(Intake *intake) {
    if (m_intakeCount >= 16) return;
    m_intakes[m_intakeCount++] = intake;

    if (m_intakeCount == 1) {
        // The charge air carries the intake's mixture (the model meters the
        // fuel at the compressor inlet)
        m_charge.reset(Ambient, intake->getChargeTemperature(), intake->getFuelAirMix());
    }
}

std::string ForcedInduction::getDescription() const {
    switch (m_type) {
    case Type::Roots: return "Roots supercharger";
    case Type::TwinScrew: return "Twin-screw supercharger";
    case Type::Centrifugal: return "Centrifugal supercharger";
    case Type::Turbocharger:
    default:
        switch (m_arrangement) {
        case Arrangement::Twin: return "Twin turbochargers";
        case Arrangement::Sequential: return "Sequential turbochargers";
        case Arrangement::Single:
        default: return "Turbocharger";
        }
    }
}

double ForcedInduction::getExhaustOutletFlowK(double mufflerK) const {
    if (!isTurbocharger()) return mufflerK;

    double k = m_wastegateK * m_wastegate;
    for (int i = 0; i < m_unitCount; ++i) {
        if (m_units[i].active) k += m_turbineK;
    }

    // Each exhaust system feeds its share of the turbines; the turbine and
    // the muffler are restrictions in series
    k /= std::max(m_exhaustSystemCount, 1);
    if (k <= 0) return 0;
    if (mufflerK <= 0) return k;

    return 1.0 / std::sqrt(1.0 / (k * k) + 1.0 / (mufflerK * mufflerK));
}

void ForcedInduction::addExhaustFlow(double n, double T, double P, double hcr, double mufflerK) {
    if (!isTurbocharger() || n <= 0 || P <= Ambient) return;

    double turbineK = 0;
    for (int i = 0; i < m_unitCount; ++i) {
        if (m_units[i].active) turbineK += m_turbineK;
    }

    const double totalK = turbineK + m_wastegateK * m_wastegate;
    if (totalK <= 0) return;

    const double share = totalK / std::max(m_exhaustSystemCount, 1);
    m_turbineFraction = turbineK / totalK;

    // Pressure drop across the turbine (restrictions in series share the
    // drop in proportion to 1 / k^2)
    double turbineDropFraction = 1.0;
    if (mufflerK > 0) {
        const double rt = 1.0 / (share * share);
        const double rm = 1.0 / (mufflerK * mufflerK);
        turbineDropFraction = rt / (rt + rm);
    }

    const double P_out = P - (P - Ambient) * turbineDropFraction;
    const double g = std::max(hcr, 1.05);
    const double cp = constants::R * g / (g - 1);
    const double isentropicWork = cp * T * (1.0 - std::pow(P_out / P, (g - 1) / g));

    m_turbineEnergy += m_turbineEfficiency * m_turbineFraction * n * isentropicWork;
}

double ForcedInduction::centrifugalSourcePressureRatio(double omega) const {
    const double U = omega * m_tipRadius;
    const double pr = std::pow(
        1.0 + m_compressorEfficiency * m_slip * U * U / (Cp_mass * AmbientTemperature),
        1.0 / Exponent);

    return std::min(pr, 6.0);
}

double ForcedInduction::plenumPressure() const {
    if (m_intakeCount == 0) return Ambient;

    double sum = 0;
    for (int i = 0; i < m_intakeCount; ++i) {
        sum += m_intakes[i]->m_system.pressure();
    }

    return sum / m_intakeCount;
}

double ForcedInduction::crankSpeed() const {
    return (m_crankshaft != nullptr) ? std::abs(m_crankshaft->m_body.v_theta) : 0.0;
}

void ForcedInduction::processCentrifugal(
    Unit &unit,
    double dt,
    double scale,
    const GasSystem::Mix &mix,
    double T1)
{
    const double pr0 = centrifugalSourcePressureRatio(unit.omega);
    const double pr = std::max(m_charge.pressure() / Ambient, 1.0);

    // Compressor outlet temperature, then the intercooler
    const double T2 = AmbientTemperature
        * (1.0 + (std::pow(pr, Exponent) - 1.0) / m_compressorEfficiency);
    m_compressorOutletTemperature = T2;
    const double T_in = T2 - m_intercoolerEffectiveness * (T2 - AmbientTemperature)
        + (T1 - AmbientTemperature);

    const double flow = m_charge.flow(scale * m_compressorK, dt, pr0 * Ambient, T_in, mix);
    const double dn = -flow;   // moles into the charge air

    unit.compressorFlow = dn / dt;

    // Euler work of the impeller; reverse flow (surge) brakes the wheel too
    const double U = unit.omega * m_tipRadius;
    const double massFlow = std::abs(dn) / dt * units::AirMolecularMass;
    unit.compressorPower = massFlow * m_slip * U * U;
}

void ForcedInduction::processPositiveDisplacement(double dt, const GasSystem::Mix &mix, double T1) {
    const double omega = crankSpeed() * m_driveRatio;
    m_units[0].omega = omega;

    const double pr = std::max(m_charge.pressure() / Ambient, 1.0);
    const double efficiency = (m_type == Type::Roots) ? 0.55 : 0.72;
    const double T2 = AmbientTemperature
        * (1.0 + (std::pow(pr, Exponent) - 1.0) / efficiency);
    m_compressorOutletTemperature = T2;
    const double T_in = T2 - m_intercoolerEffectiveness * (T2 - AmbientTemperature)
        + (T1 - AmbientTemperature);

    // Displacement delivered this step
    const double volumeFlow = m_displacement * omega / (2 * constants::pi);
    const double n = 0.95 * Ambient * volumeFlow * dt / (constants::R * AmbientTemperature);
    if (n > 0) {
        m_charge.inject(n, T_in, mix);
    }

    // Leakage back to the inlet
    const double leak = m_charge.flow(m_leakK, dt, Ambient, AmbientTemperature, mix);

    const double netFlow = (n - leak) / dt;
    m_units[0].compressorFlow = netFlow;

    // Drive power: the work to heat the delivered air plus 10% mechanical
    // losses
    m_units[0].compressorPower = (n / dt) * Cp_molar * (T2 - AmbientTemperature) / 0.9;
}

void ForcedInduction::process(double dt) {
    if (dt <= 0) return;

    const bool engineering = m_charge.isEngineering();
    const GasSystem::Mix mix = (m_intakeCount > 0)
        ? m_intakes[0]->getFuelAirMix()
        : airMix(engineering);
    const double T1 = (m_intakeCount > 0)
        ? m_intakes[0]->getChargeTemperature()
        : AmbientTemperature;

    const double p_plenum = plenumPressure();
    const double boost = m_charge.pressure() - Ambient;

    if (isTurbocharger()) {
        // Sequential turbochargers: the second unit joins above the
        // transition speed (with some hysteresis)
        if (m_arrangement == Arrangement::Sequential) {
            const double speed = crankSpeed();
            if (!m_units[1].active && speed > m_sequentialSpeed) m_units[1].active = true;
            else if (m_units[1].active && speed < 0.9 * m_sequentialSpeed) m_units[1].active = false;
        }

        // Wastegate: a proportional actuator that starts to open 15% below
        // the set point and is fully open at it
        const double band = std::max(0.15 * m_boostTarget, units::pressure(0.5, units::psi));
        const double wastegateTarget = (m_boostTarget > 0)
            ? clamp((boost - (m_boostTarget - band)) / band, 0.0, 1.0)
            : 1.0;
        m_wastegate = approach(m_wastegate, wastegateTarget, dt, 0.03);

        int activeUnits = 0;
        for (int i = 0; i < m_unitCount; ++i) {
            if (m_units[i].active) ++activeUnits;
        }

        for (int i = 0; i < m_unitCount; ++i) {
            Unit &unit = m_units[i];
            unit.turbinePower = 0;
            unit.compressorFlow = 0;
            unit.compressorPower = 0;

            if (unit.active) {
                processCentrifugal(unit, dt, 1.0, mix, T1);
                const double turbineEnergy = m_turbineEnergy / std::max(activeUnits, 1);
                unit.turbinePower = turbineEnergy / dt;
                unit.energy += turbineEnergy;
            }

            unit.energy -= (unit.compressorPower + m_bearingLoss * unit.omega * unit.omega) * dt;

            // Overspeed: the wastegate can't stop a badly mismatched turbo
            // but the wheel is limited to its maximum speed
            const double maxEnergy = 0.5 * m_inertia * m_maxSpeed * m_maxSpeed;
            unit.energy = clamp(unit.energy, 0.0, maxEnergy);
            unit.omega = std::sqrt(2 * unit.energy / m_inertia);
        }

        m_turbineEnergy = 0;
        m_drivePower = 0;
        m_driveTorque = 0;
    }
    else {
        if (m_type == Type::Centrifugal) {
            Unit &unit = m_units[0];
            unit.omega = std::min(crankSpeed() * m_driveRatio, m_maxSpeed);
            processCentrifugal(unit, dt, 1.0, mix, T1);
            unit.compressorPower += m_bearingLoss * unit.omega * unit.omega;
        }
        else {
            processPositiveDisplacement(dt, mix, T1);

            // Bypass valve: recirculates the delivery at part throttle
            if (m_bypassValve) {
                const double target =
                    clamp((0.9 * Ambient - p_plenum) / (0.2 * Ambient), 0.0, 1.0);
                m_bypass = approach(m_bypass, target, dt, 0.05);
                if (m_bypass > 0) {
                    m_charge.flow(m_bypassK * m_bypass, dt, Ambient, AmbientTemperature, mix);
                }
            }
        }

        m_drivePower = m_units[0].compressorPower;
        m_driveTorque = m_drivePower / std::max(crankSpeed(), 20.0);
    }

    // Blow-off valve: vents the charge when the throttle snaps shut
    double blowoffTarget = 0;
    if (m_blowoffEnabled && !isPositiveDisplacement()) {
        const double dp = m_charge.pressure() - p_plenum;
        blowoffTarget = clamp((dp - units::pressure(4.0, units::psi)) / units::pressure(3.0, units::psi), 0.0, 1.0);
    }

    m_blowoff = approach(m_blowoff, blowoffTarget, dt, 0.01);
    double vented = 0;
    if (m_blowoff > 0) {
        vented = m_charge.flow(m_blowoffK * m_blowoff, dt, Ambient, AmbientTemperature, mix);
    }

    m_blowoffFlowRate = approach(m_blowoffFlowRate, std::max(vented, 0.0) / dt, dt, 0.02);

    m_charge.dissipateExcessVelocity();
    m_charge.updateVelocity(dt, 0.5);
}

void ForcedInduction::apply(atg_scs::SystemState *system) {
    if (m_crankshaft == nullptr || m_driveTorque == 0) return;

    const int index = m_crankshaft->m_body.index;
    const double v = system->v_theta[index];
    if (v > 0) system->t[index] -= m_driveTorque;
    else if (v < 0) system->t[index] += m_driveTorque;
}

double ForcedInduction::getBoost() const {
    return m_charge.pressure() - Ambient;
}

double ForcedInduction::getShaftSpeed() const {
    return m_units[0].omega;
}

double ForcedInduction::getCompressorFlow() const {
    double flow = 0;
    for (int i = 0; i < m_unitCount; ++i) flow += m_units[i].compressorFlow;
    return flow;
}

double ForcedInduction::getTurbinePower() const {
    double power = 0;
    for (int i = 0; i < m_unitCount; ++i) power += m_units[i].turbinePower;
    return power;
}

void ForcedInduction::setIntercoolerEffectiveness(double e) {
    m_intercoolerEffectiveness = clamp(e, 0.0, 1.0);
}

double ForcedInduction::getWhineFrequency() const {
    // Shaft order of the compressor; the blade pass frequency is far above
    // the simulation's audio bandwidth
    return m_units[0].omega / (2 * constants::pi);
}

double ForcedInduction::getWhineLevel() const {
    if (isPositiveDisplacement()) {
        // Supercharger whine follows rotor speed and load
        const double speed = clamp(m_units[0].omega / (2 * constants::pi * 250.0), 0.0, 1.0);
        const double load = clamp(getBoost() / units::pressure(10.0, units::psi), 0.0, 1.0);
        return speed * (0.3 + 0.7 * load);
    }

    const double s = clamp(m_units[0].omega / m_maxSpeed, 0.0, 1.0);
    const double flow = clamp(
        std::abs(getCompressorFlow()) * units::AirMolecularMass / 0.3, 0.0, 1.0);
    return s * s * (0.25 + 0.75 * flow);
}

double ForcedInduction::getBlowoffLevel() const {
    return clamp(m_blowoffFlowRate * units::AirMolecularMass / 0.15, 0.0, 1.0);
}
