#include "../include/ecu.h"

#include "../include/engine.h"
#include "../include/forced_induction.h"
#include "../include/ignition_module.h"
#include "../include/transmission.h"
#include "../include/units.h"
#include "../include/vehicle.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>

namespace {

    constexpr double Deg = constants::pi / 180.0;
    constexpr double AtmosphereKpa = 101.325;

    double smoothstep(double a, double b, double x) {
        const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
        return t * t * (3 - 2 * t);
    }

    std::vector<double> linspace(double a, double b, int n) {
        std::vector<double> v(n);
        for (int i = 0; i < n; ++i) v[i] = a + (b - a) * i / std::max(n - 1, 1);
        return v;
    }

    std::string join(const std::vector<double> &values) {
        std::ostringstream ss;
        ss.precision(6);
        for (size_t i = 0; i < values.size(); ++i) ss << (i ? "," : "") << values[i];
        return ss.str();
    }

    std::vector<double> split(const std::string &text) {
        std::vector<double> values;
        std::stringstream ss(text);
        std::string item;
        while (std::getline(ss, item, ',')) values.push_back(std::atof(item.c_str()));
        return values;
    }

} /* namespace */

void EcuTable::resize(const std::vector<double> &xAxis, const std::vector<double> &yAxis, double value) {
    x = xAxis;
    y = yAxis;
    v.assign(static_cast<size_t>(cols()) * rows(), value);
}

double EcuTable::position(const std::vector<double> &axis, double value) {
    const int n = static_cast<int>(axis.size());
    if (n <= 1) return 0;
    if (value <= axis.front()) return 0;
    if (value >= axis.back()) return n - 1;

    int i = 0;
    while (i < n - 2 && value > axis[i + 1]) ++i;
    const double span = axis[i + 1] - axis[i];
    return i + ((span > 0) ? (value - axis[i]) / span : 0.0);
}

double EcuTable::sample(double xq, double yq) const {
    if (v.empty() || x.empty()) return 0;

    const double fx = position(x, xq);
    const int c0 = std::min(static_cast<int>(fx), cols() - 1);
    const int c1 = std::min(c0 + 1, cols() - 1);
    const double tx = fx - c0;

    if (rows() == 1) {
        return at(0, c0) * (1 - tx) + at(0, c1) * tx;
    }

    const double fy = position(y, yq);
    const int r0 = std::min(static_cast<int>(fy), rows() - 1);
    const int r1 = std::min(r0 + 1, rows() - 1);
    const double ty = fy - r0;

    const double a = at(r0, c0) * (1 - tx) + at(r0, c1) * tx;
    const double b = at(r1, c0) * (1 - tx) + at(r1, c1) * tx;
    return a * (1 - ty) + b * ty;
}

Ecu::Ecu() {
    m_engine = nullptr;
    m_transmission = nullptr;
    m_vehicle = nullptr;
    m_scriptAdvanceOffset = 0;
    m_idleAir = 0;
    m_shiftTimer = 0;
    m_shiftTarget = -1;
    m_manualHold = 0;
    m_overboostTimer = 0;
    m_boostFiltered = 0;
    m_launchClutch = 0;
    m_dashpot = 0;
    m_lastThrottle = 0;
    m_timeInGear = 0;
}

void Ecu::initialize(Engine *engine, Transmission *transmission, Vehicle *vehicle) {
    m_engine = engine;
    m_transmission = transmission;
    m_vehicle = vehicle;
    m_state = State();
    m_idleAir = 0;
    m_shiftTimer = 0;
    m_manualHold = 0;
    m_overboostTimer = 0;
    m_boostFiltered = 0;
    m_launchClutch = 0;
    m_dashpot = 0;
    m_lastThrottle = 0;

    Settings s;
    IgnitionModule *ignition = engine->getIgnitionModule();
    const double redline = units::toRpm(engine->getRedline());
    const double revLimit = units::toRpm(ignition->getRevLimit());
    const ForcedInduction *fi = engine->getForcedInduction();
    const double boostKpa = (fi != nullptr) ? fi->getBoostTarget() / 1000.0 : 0.0;
    const double scriptLambda = (engine->getIntakeCount() > 0) ? engine->getIntake(0)->getLambda() : 1.0;

    // Axes: engine speed up to past the limiter, manifold pressure up to the
    // boost the engine can make
    const double rpmMax = std::ceil(std::max(redline, revLimit) * 1.1 / 500.0) * 500.0;
    const std::vector<double> rpmAxis = linspace(0, rpmMax, 16);
    const double maxLoad = std::max(110.0, AtmosphereKpa + 1.4 * boostKpa + ((fi != nullptr && boostKpa <= 0) ? 120.0 : 0.0));
    const std::vector<double> loadAxis = linspace(20, std::ceil(maxLoad / 10.0) * 10.0, 12);

    // The stock maps reproduce the script: its timing curve at every load and
    // its mixture everywhere
    s.ignition.name = "Ignition advance";
    s.ignition.xLabel = "Engine speed [rpm]";
    s.ignition.yLabel = "Manifold pressure [kPa]";
    s.ignition.unit = "deg BTDC";
    s.ignition.resize(rpmAxis, loadAxis, 0.0);
    for (int r = 0; r < s.ignition.rows(); ++r) {
        for (int c = 0; c < s.ignition.cols(); ++c) {
            s.ignition.at(r, c) = ignition->sampleCurve(units::rpm(rpmAxis[c])) / Deg;
        }
    }

    s.lambda.name = "Air-fuel ratio (lambda)";
    s.lambda.xLabel = s.ignition.xLabel;
    s.lambda.yLabel = s.ignition.yLabel;
    s.lambda.unit = "lambda";
    s.lambda.resize(rpmAxis, loadAxis, scriptLambda);

    // Boost by engine speed
    s.boost.name = "Boost target";
    s.boost.xLabel = "Engine speed [rpm]";
    s.boost.unit = "kPa";
    s.boost.resize(linspace(0, rpmMax, 12), {}, boostKpa);

    s.revLimit = revLimit;
    s.cutTime = ignition->getLimiterDuration();
    s.launchRpm = std::round(0.55 * redline / 100.0) * 100.0;
    s.useBoostMap = (fi != nullptr && fi->isTurbocharger());
    s.antiLagMinRpm = std::round(0.35 * redline / 100.0) * 100.0;
    s.idleRpm = 0;      // set below

    // Gearbox: upshift early at light throttle, near the limit at full
    s.upshift.name = "Upshift";
    s.upshift.xLabel = "Throttle [%]";
    s.upshift.unit = "rpm";
    s.upshift.resize(linspace(0, 100, 5), {}, 0);
    s.downshift = s.upshift;
    s.downshift.name = "Downshift";
    const double up[] = { 0.38, 0.45, 0.58, 0.75, 0.97 };
    const double down[] = { 0.18, 0.20, 0.26, 0.34, 0.50 };
    for (int i = 0; i < 5; ++i) {
        s.upshift.at(0, i) = std::round(std::max(up[i] * redline, 1800.0) / 50.0) * 50.0;
        s.downshift.at(0, i) = std::round(std::max(down[i] * redline, 1000.0) / 50.0) * 50.0;
    }
    s.wotUpshift.assign((transmission != nullptr) ? transmission->getGearCount() : 0, 0.0);

    // Idle control only ever adds air, so engines whose own idle setting
    // keeps them above the target are unaffected
    s.idleControl = true;
    s.idleRpm = std::clamp(std::round(0.11 * redline / 50.0) * 50.0, 700.0, 2500.0);
    s.launchClutchRpm = std::clamp(0.2 * redline, 1000.0, 4000.0);

    m_settings = s;
    m_stock = s;
}

double Ecu::launchClutch(double dt, double rpm, double throttle) {
    // Slips the clutch to hold the engine near a launch speed, like a torque
    // converter's stall speed (higher with more throttle): the engine stays
    // where it makes torque, whatever its size or the clutch's capacity
    double target = m_settings.idleRpm + m_settings.launchClutchRpm * (0.3 + 0.7 * std::clamp(throttle, 0.0, 1.0));
    if (m_state.launchActive) {
        // Below the launch speed, or the limiter would hold the engine
        // under the point where the clutch bites and the car never moves
        target = std::min(target, 0.9 * m_settings.launchRpm);
    }
    // It lets go faster than it bites, so that slowing to a stop doesn't
    // stall the engine
    const double gain = (rpm < target) ? 6.0 : 3.0;
    m_launchClutch = std::clamp(m_launchClutch + gain * (rpm - target) / target * dt, 0.0, 1.0);

    // Anti-stall: near idle (braking hard to a stop) the clutch opens at
    // once rather than at the controller's pace
    const double idle = m_settings.idleRpm;
    m_launchClutch = std::min(m_launchClutch, smoothstep(0.9 * idle, 1.25 * idle, rpm));
    return m_launchClutch;
}

void Ecu::requestShift(int direction) {
    if (m_transmission == nullptr) return;

    const int gear = m_transmission->getGear();
    const int target = std::clamp(gear + direction, -1, m_transmission->getGearCount() - 1);
    if (target == gear) return;

    if (m_settings.automatic && m_settings.enabled) {
        if (target < 0) {
            m_transmission->changeGear(-1);
        }
        else if (m_shiftTimer <= 0) {
            m_shiftTarget = target;
            m_shiftTimer = m_settings.shiftTime;
            m_state.shifting = true;
            m_state.lastShift = (direction > 0) ? "manual up" : "manual down";
        }

        m_manualHold = 3.0;
    }
    else {
        m_transmission->changeGear(target);
    }
}

void Ecu::updateGearbox(double dt, double rpm, double throttle, double vehicleSpeed) {
    Transmission *t = m_transmission;
    const int gearCount = t->getGearCount();
    m_manualHold = std::max(m_manualHold - dt, 0.0);

    // Shift in progress: cut the power and open the clutch, change gear half
    // way, then bring the clutch back in
    if (m_shiftTimer > 0) {
        const double half = 0.5 * m_settings.shiftTime;
        const bool firstHalf = (m_shiftTimer > half);
        m_shiftTimer -= dt;

        if (firstHalf && m_shiftTimer <= half && t->getGear() != m_shiftTarget) {
            t->changeGear(m_shiftTarget);
        }

        // Near idle or at a standstill the clutch comes back only as far as
        // the engine can take without stalling
        const double idle = m_settings.idleRpm;
        const double allowed = (vehicleSpeed < 1.0) ? 0.0 : smoothstep(0.9 * idle, 1.25 * idle, rpm);
        if (m_shiftTimer > half) {
            t->setClutchPressure(0.0);
        }
        else {
            t->setClutchPressure((1.0 - std::max(m_shiftTimer, 0.0) / half) * allowed);
        }
        m_launchClutch = std::min(m_launchClutch, allowed);

        if (m_shiftTimer <= 0) {
            m_shiftTimer = 0;
            m_state.shifting = false;
        }
        return;
    }

    m_state.shifting = false;
    m_timeInGear += dt;
    int gear = t->getGear();

    // Pressing the throttle in neutral selects first gear
    if (gear < 0) {
        const bool running = m_engine->getIgnitionModule()->m_enabled && rpm > 0.8 * m_settings.idleRpm;
        if (running && throttle > 0.05 && vehicleSpeed < 3.0) {
            t->changeGear(0);
            gear = 0;
        }
        else {
            t->setClutchPressure(1.0);
            return;
        }
    }

    // Engine speed the wheels would turn the engine at in this gear
    const double wheelRpm = units::toRpm(
        vehicleSpeed / std::max(m_vehicle->getTireRadius(), 0.05)
        * m_vehicle->getDiffRatio() * t->getGearRatio(gear));

    // Held on the brakes at a standstill (staging a launch): clutch out
    if (m_vehicle->getBrakeLevel() > 0 && vehicleSpeed < 1.0) {
        m_launchClutch = 0;
    m_dashpot = 0;
    m_lastThrottle = 0;
        t->setClutchPressure(0.0);
        return;
    }

    // Clutch: slips to launch the car, locked once the wheels keep the
    // engine above idle.
    const double lockRpm = m_settings.idleRpm + 0.6 * m_settings.launchClutchRpm;
    double clutch = 1.0;
    if (wheelRpm > lockRpm) m_launchClutch = 1.0;
    else clutch = launchClutch(dt, rpm, throttle);
    t->setClutchPressure(clutch);

    if (m_manualHold > 0) return;

    const double throttlePercent = throttle * 100.0;
    auto upshiftRpm = [&](int g) {
        if (g >= 0 && g < static_cast<int>(m_settings.wotUpshift.size())
            && m_settings.wotUpshift[g] > 0 && throttle > 0.9)
        {
            return m_settings.wotUpshift[g];
        }
        return std::min(m_settings.upshift.sample(throttlePercent), m_settings.revLimit - 50);
    };

    auto startShift = [&](int target, const char *what) {
        m_shiftTarget = target;
        m_shiftTimer = m_settings.shiftTime;
        m_timeInGear = 0;
        m_state.shifting = true;
        m_state.lastShift = std::string(what) + " at " + std::to_string(static_cast<int>(rpm)) + " rpm";
    };

    // Upshifts need the engine at the shift point and the road speed close
    // to it as well (a gearbox reads its output shaft): shifting on the
    // engine speed alone during wheelspin lands in a gear too tall to pull.
    // Downshifts follow the road speed so that the engine speed sagging
    // during a shift or recovering from wheelspin doesn't trigger one
    const bool braking = m_vehicle->getBrakeLevel() > 0;
    if (gear < gearCount - 1 && rpm > upshiftRpm(gear) && wheelRpm > 0.85 * upshiftRpm(gear)
        && clutch > 0.95 && m_timeInGear > 0.3 && !braking)
    {
        startShift(gear + 1, "up");
        return;
    }

    if (gear > 0 && m_timeInGear > 1.0) {
        const double after = wheelRpm * t->getGearRatio(gear - 1) / t->getGearRatio(gear);
        const bool stopped = vehicleSpeed < 1.0;
        if (stopped) {
            startShift(0, "down (stopped)");
        }
        else if (wheelRpm < m_settings.downshift.sample(throttlePercent) && after < 0.9 * upshiftRpm(gear - 1)) {
            startShift(gear - 1, "down");
        }
    }
}

void Ecu::update(double dt) {
    if (m_engine == nullptr) return;

    IgnitionModule *ignition = m_engine->getIgnitionModule();
    ForcedInduction *fi = m_engine->getForcedInduction();
    const Settings &s = m_settings;

    const double rpm = m_engine->getRpm();
    const double load = m_engine->getManifoldPressure() / 1000.0;
    const double throttle = m_engine->getSpeedControl();
    const double vehicleSpeed = (m_vehicle != nullptr) ? std::abs(m_vehicle->getSpeed()) : 0.0;

    m_state.rpm = rpm;
    m_state.load = load;
    m_state.throttle = throttle;

    if (!s.enabled) {
        ignition->setEcuControl(false);
        ignition->setCut(false);
        ignition->setRevLimit(units::rpm(m_stock.revLimit));
        m_engine->setSpeedControlLimits(0.0, 1.0);
        for (int i = 0; i < m_engine->getIntakeCount(); ++i) {
            m_engine->getIntake(i)->setLambda(m_stock.lambda.sample(rpm, 100.0));
        }
        m_state.advance = ignition->getTimingAdvance() / Deg;
        m_state.limiting = ignition->isLimiting();
        m_state.softLimiting = m_state.launchActive = m_state.antiLagActive = m_state.overboost = false;
        m_state.shifting = false;
        return;
    }

    // Ignition
    double advance = s.useIgnitionMap
        ? s.ignition.sample(rpm, load)
        : ignition->sampleCurve(units::rpm(rpm)) / Deg;

    // Launch control and the limiters
    const bool clutchOpen = (m_transmission == nullptr)
        || m_transmission->getGear() < 0
        || m_transmission->getClutchPressure() < 0.5;
    m_state.launchActive = s.launchControl && vehicleSpeed < 1.5 && throttle > 0.5
        && (clutchOpen || s.automatic);
    const double limit = m_state.launchActive ? s.launchRpm : s.revLimit;

    m_state.softLimiting = false;
    if (s.limiter == Limiter::Soft && s.softBand > 0 && rpm > limit - s.softBand) {
        const double t = std::clamp((rpm - (limit - s.softBand)) / s.softBand, 0.0, 1.0);
        advance -= 15.0 * t;
        m_state.softLimiting = true;
    }

    ignition->setRevLimit(units::rpm(limit));
    // Launch control cuts briefly so that light engines don't swing far below
    // the launch speed (the clutch needs a steady engine speed to bite)
    ignition->setLimiterDuration(m_state.launchActive ? std::min(s.cutTime, 0.02) : s.cutTime);

    // Anti-lag
    const bool turbo = (fi != nullptr && fi->isTurbocharger());
    m_state.antiLagActive = s.antiLag && turbo && ignition->m_enabled
        && throttle < 0.05 && rpm > s.antiLagMinRpm;
    if (m_state.antiLagActive) {
        advance = -s.antiLagRetard;
    }

    ignition->setEcuControl(true);
    ignition->setEcuAdvance(advance * Deg);

    // Fuel
    m_state.lambda = s.useFuelMap ? s.lambda.sample(rpm, load) : m_stock.lambda.sample(rpm, 100.0);
    for (int i = 0; i < m_engine->getIntakeCount(); ++i) {
        m_engine->getIntake(i)->setLambda(m_state.lambda);
    }

    // Boost and overboost protection
    m_state.overboost = false;
    if (fi != nullptr) {
        if (s.useBoostMap) {
            m_state.boostTarget = std::max(s.boost.sample(rpm), 0.0);
            fi->setBoostTarget(m_state.boostTarget * 1000.0);
        }
        else {
            m_state.boostTarget = fi->getBoostTarget() / 1000.0;
        }

        // The manifold pressure pulses with every intake event (strongly on
        // engines with few cylinders or rotors), so the protection reads it
        // through a filter like a real pressure sensor and ignores the
        // spike when the throttle stays open through a shift
        const double filter = dt / (dt + 0.1);
        m_boostFiltered += (fi->getBoost() / 1000.0 - m_boostFiltered) * filter;

        m_overboostTimer = std::max(m_overboostTimer - dt, 0.0);
        // Only a turbocharger has a boost target (a supercharger's boost is
        // set by its drive ratio)
        if (s.overboostCut && fi->isTurbocharger() && m_state.boostTarget > 0 && !m_state.shifting
            && m_boostFiltered > m_state.boostTarget + s.overboostMargin)
        {
            m_overboostTimer = 0.25;
        }
        m_state.overboost = (m_overboostTimer > 0);
    }

    // Idle speed control: proportional-integral on the engine speed error,
    // adding air through the idle bypass with the throttle closed (it only
    // adds air, so engines that idle above the target are unaffected)
    // It arms once the engine has run above the target, so it doesn't add
    // air while cranking (a bigger start flare empties the manifold)
    double idleAir = 0;
    if (!ignition->m_enabled || rpm < 150) m_idleArmed = false;
    else if (rpm > 1.2 * s.idleRpm) m_idleArmed = true;

    if (s.idleControl && m_idleArmed) {
        const double error = s.idleRpm - rpm;
        if (throttle < 0.03) {
            m_idleAir = std::clamp(m_idleAir + 1.5e-5 * error * dt, 0.0, 0.12);
        }
        // Dashpot: off throttle at speed the bypass holds extra air and
        // lets it go slowly, so that the engine coming down from a run (or
        // with the clutch let go when braking to a stop) settles at idle
        // instead of falling through it
        // It is set when the throttle closes (never while it is closed, or
        // the extra air could hold the engine up) and bleeds off near idle
        if (throttle < 0.03 && m_lastThrottle >= 0.03 && rpm > 1.5 * s.idleRpm) m_dashpot = 0.015;
        else if (rpm < 1.3 * s.idleRpm) m_dashpot = std::max(m_dashpot - 0.0075 * dt, 0.0);
        if (throttle >= 0.03) m_dashpot = 0;

        // Well below the target the engine is about to stall: catch it hard
        const double sag = std::max(0.85 * s.idleRpm - rpm, 0.0);
        idleAir = std::clamp(m_idleAir + m_dashpot + 5e-5 * std::max(error, 0.0) + 2e-4 * sag, 0.0, 0.15);
    }
    else if (!ignition->m_enabled) {
        m_idleAir = std::max(m_idleAir - dt * 0.05, 0.0);
    }
    m_state.idleAir = s.idleControl ? idleAir : 0.0;
    m_lastThrottle = throttle;

    double floor = m_state.idleAir;
    if (m_state.antiLagActive) floor = std::max(floor, s.antiLagAir);

    // Gearbox
    if (s.automatic && m_transmission != nullptr && m_vehicle != nullptr) {
        updateGearbox(dt, rpm, throttle, vehicleSpeed);
    }
    else {
        m_state.shifting = false;
    }

    // Shifts cut the power only under throttle: off throttle near idle the
    // cut would stall the engine
    const bool cut = m_state.overboost
        || (m_state.shifting && m_shiftTimer > 0.5 * s.shiftTime && throttle > 0.2);
    ignition->setCut(cut);
    m_engine->setSpeedControlLimits(floor, m_state.shifting ? 0.0 : 1.0);

    m_state.advance = advance;
    m_state.limiting = ignition->isLimiting();
    m_state.gear = (m_transmission != nullptr) ? m_transmission->getGear() : -1;
}

void Ecu::optimizeShiftPoints(const std::vector<double> &rpm, const std::vector<double> &torque) {
    if (m_transmission == nullptr || rpm.size() < 4 || rpm.size() != torque.size()) return;

    auto torqueAt = [&](double r) {
        if (r <= rpm.front()) return torque.front();
        if (r >= rpm.back()) return 0.0;
        size_t i = 0;
        while (i + 2 < rpm.size() && r > rpm[i + 1]) ++i;
        const double t = (r - rpm[i]) / std::max(rpm[i + 1] - rpm[i], 1e-9);
        return torque[i] * (1 - t) + torque[i + 1] * t;
    };

    const int gears = m_transmission->getGearCount();
    m_settings.wotUpshift.assign(gears, 0.0);
    const double limit = m_settings.revLimit - 100;
    for (int g = 0; g + 1 < gears; ++g) {
        const double r0 = m_transmission->getGearRatio(g);
        const double r1 = m_transmission->getGearRatio(g + 1);
        double shift = limit;

        // Shift where the next gear gives more wheel torque at the same
        // road speed
        for (double r = 0.5 * limit; r <= limit; r += 25) {
            if (torqueAt(r) * r0 < torqueAt(r * r1 / r0) * r1) {
                shift = r;
                break;
            }
        }

        m_settings.wotUpshift[g] = std::round(shift / 25.0) * 25.0;
    }
}

std::string Ecu::serialize() const {
    const Settings &s = m_settings;
    std::ostringstream o;
    o << "# Engine Simulator ECU tune\n"
      << "enabled=" << s.enabled << "\n"
      << "use_ignition_map=" << s.useIgnitionMap << "\n"
      << "use_fuel_map=" << s.useFuelMap << "\n"
      << "use_boost_map=" << s.useBoostMap << "\n"
      << "limiter=" << (s.limiter == Limiter::Soft ? "soft" : "cut") << "\n"
      << "rev_limit=" << s.revLimit << "\n"
      << "soft_band=" << s.softBand << "\n"
      << "cut_time=" << s.cutTime << "\n"
      << "launch_control=" << s.launchControl << "\n"
      << "launch_rpm=" << s.launchRpm << "\n"
      << "overboost_cut=" << s.overboostCut << "\n"
      << "overboost_margin=" << s.overboostMargin << "\n"
      << "idle_control=" << s.idleControl << "\n"
      << "idle_rpm=" << s.idleRpm << "\n"
      << "anti_lag=" << s.antiLag << "\n"
      << "anti_lag_retard=" << s.antiLagRetard << "\n"
      << "anti_lag_air=" << s.antiLagAir << "\n"
      << "anti_lag_min_rpm=" << s.antiLagMinRpm << "\n"
      << "automatic=" << s.automatic << "\n"
      << "shift_time=" << s.shiftTime << "\n"
      << "launch_clutch_rpm=" << s.launchClutchRpm << "\n"
      << "wot_upshift=" << join(s.wotUpshift) << "\n";

    for (const EcuTable *t : { &s.ignition, &s.lambda, &s.boost, &s.upshift, &s.downshift }) {
        const std::string key =
            (t == &s.ignition) ? "ignition" : (t == &s.lambda) ? "lambda"
            : (t == &s.boost) ? "boost" : (t == &s.upshift) ? "upshift" : "downshift";
        o << key << ".x=" << join(t->x) << "\n";
        if (!t->y.empty()) o << key << ".y=" << join(t->y) << "\n";
        for (int r = 0; r < t->rows(); ++r) {
            std::vector<double> row(t->v.begin() + static_cast<size_t>(r) * t->cols(),
                t->v.begin() + static_cast<size_t>(r + 1) * t->cols());
            o << key << ".row" << r << "=" << join(row) << "\n";
        }
    }

    return o.str();
}

bool Ecu::deserialize(const std::string &text, std::string *error) {
    std::map<std::string, std::string> values;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        values[line.substr(0, eq)] = line.substr(eq + 1);
    }

    Settings s = m_settings;
    auto number = [&](const char *key, double *target) {
        auto it = values.find(key);
        if (it != values.end()) *target = std::atof(it->second.c_str());
    };
    auto flag = [&](const char *key, bool *target) {
        auto it = values.find(key);
        if (it != values.end()) *target = (it->second == "1");
    };

    flag("enabled", &s.enabled);
    flag("use_ignition_map", &s.useIgnitionMap);
    flag("use_fuel_map", &s.useFuelMap);
    flag("use_boost_map", &s.useBoostMap);
    if (values.count("limiter")) s.limiter = (values["limiter"] == "soft") ? Limiter::Soft : Limiter::SparkCut;
    number("rev_limit", &s.revLimit);
    number("soft_band", &s.softBand);
    number("cut_time", &s.cutTime);
    flag("launch_control", &s.launchControl);
    number("launch_rpm", &s.launchRpm);
    flag("overboost_cut", &s.overboostCut);
    number("overboost_margin", &s.overboostMargin);
    flag("idle_control", &s.idleControl);
    number("idle_rpm", &s.idleRpm);
    flag("anti_lag", &s.antiLag);
    number("anti_lag_retard", &s.antiLagRetard);
    number("anti_lag_air", &s.antiLagAir);
    number("anti_lag_min_rpm", &s.antiLagMinRpm);
    flag("automatic", &s.automatic);
    number("shift_time", &s.shiftTime);
    number("launch_clutch_rpm", &s.launchClutchRpm);
    if (values.count("wot_upshift")) {
        std::vector<double> w = split(values["wot_upshift"]);
        w.resize(s.wotUpshift.size(), 0.0);
        s.wotUpshift = w;
    }

    for (EcuTable *t : { &s.ignition, &s.lambda, &s.boost, &s.upshift, &s.downshift }) {
        const std::string key =
            (t == &s.ignition) ? "ignition" : (t == &s.lambda) ? "lambda"
            : (t == &s.boost) ? "boost" : (t == &s.upshift) ? "upshift" : "downshift";
        if (!values.count(key + ".x")) continue;

        EcuTable table = *t;
        table.x = split(values[key + ".x"]);
        table.y = values.count(key + ".y") ? split(values[key + ".y"]) : std::vector<double>();
        table.v.clear();
        for (int r = 0; r < table.rows(); ++r) {
            const std::vector<double> row = split(values[key + ".row" + std::to_string(r)]);
            if (static_cast<int>(row.size()) != table.cols()) {
                if (error != nullptr) *error = "Row " + std::to_string(r) + " of the " + key + " map has the wrong length";
                return false;
            }
            table.v.insert(table.v.end(), row.begin(), row.end());
        }

        if (table.x.size() < 2) {
            if (error != nullptr) *error = "The " + key + " map needs at least two columns";
            return false;
        }
        *t = table;
    }

    s.revLimit = std::clamp(s.revLimit, 1000.0, 25000.0);
    s.idleRpm = std::clamp(s.idleRpm, 300.0, 4000.0);
    s.shiftTime = std::clamp(s.shiftTime, 0.05, 2.0);
    s.cutTime = std::clamp(s.cutTime, 0.005, 1.0);
    m_settings = s;
    return true;
}

void Ecu::applyPreset(const std::string &name) {
    m_settings = m_stock;
    Settings &s = m_settings;
    const bool boosted = (m_engine != nullptr && m_engine->getForcedInduction() != nullptr);

    if (name == "economy") {
        // Lean cruise with the extra advance a dilute charge needs
        for (int r = 0; r < s.lambda.rows(); ++r) {
            for (int c = 0; c < s.lambda.cols(); ++c) {
                if (s.lambda.y[r] < 75) s.lambda.at(r, c) = 1.08;
                s.ignition.at(r, c) += 6.0 * (1.0 - smoothstep(35, 95, s.ignition.y[r]));
            }
        }
        for (double &v : s.upshift.v) v *= 0.85;
        for (double &v : s.downshift.v) v *= 0.85;
        for (double &v : s.boost.v) v *= 0.8;
        s.limiter = Limiter::Soft;
    }
    else if (name == "performance" || name == "race") {
        // Full-load enrichment and part-load advance
        for (int r = 0; r < s.ignition.rows(); ++r) {
            for (int c = 0; c < s.ignition.cols(); ++c) {
                s.ignition.at(r, c) += 4.0 * (1.0 - smoothstep(35, 95, s.ignition.y[r]));
            }
        }
        for (int r = 0; r < s.ignition.rows(); ++r) {
            for (int c = 0; c < s.ignition.cols(); ++c) {
                if (s.ignition.y[r] >= 80) s.ignition.at(r, c) += 2.0;
            }
        }
        for (int r = 0; r < s.lambda.rows(); ++r) {
            for (int c = 0; c < s.lambda.cols(); ++c) {
                if (s.lambda.y[r] >= 90) s.lambda.at(r, c) = std::min(s.lambda.at(r, c), boosted ? 0.82 : 0.87);
            }
        }
        for (double &v : s.boost.v) v *= 1.2;
        s.upshift.at(0, s.upshift.cols() - 1) = s.revLimit - 100;
        s.launchControl = true;
        s.shiftTime = 0.18;
        s.idleControl = true;

        if (name == "race") {
            s.revLimit += 300;
            s.antiLag = boosted;
            s.shiftTime = 0.1;
            s.launchRpm = std::round(0.65 * s.revLimit / 100.0) * 100.0;
            s.cutTime = 0.03;
        }
    }
}
