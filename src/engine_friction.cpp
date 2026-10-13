#include "../include/engine_friction.h"

#include "../include/constants.h"
#include "../include/units.h"

#include <algorithm>
#include <cmath>

EngineFriction::EngineFriction() {
    m_mainBearingDiameter = 0;
    m_mainBearingLength = 0;
    m_mainBearingCount = 0;
    m_rodBearingDiameter = 0;
    m_rodBearingLength = 0;
    m_camBearingCount = 0;
}

bool EngineFriction::parseModel(const std::string &name, bool engineering, Model *model) {
    if (name == "auto" || name.empty()) {
        *model = engineering ? Model::PattonNitschkeHeywood : Model::Legacy;
    }
    else if (name == "legacy") *model = Model::Legacy;
    else if (name == "patton_nitschke_heywood" || name == "pnh") *model = Model::PattonNitschkeHeywood;
    else if (name == "chen_flynn") *model = Model::ChenFlynn;
    else {
        *model = engineering ? Model::PattonNitschkeHeywood : Model::Legacy;
        return false;
    }

    return true;
}

bool EngineFriction::parseValvetrain(const std::string &name_, Valvetrain *valvetrain, bool *roller) {
    std::string name = name_;
    *roller = false;

    const std::string suffix = "_roller";
    if (name.size() > suffix.size()
        && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        *roller = true;
        name = name.substr(0, name.size() - suffix.size());
    }

    if (name == "dohc_bucket" || name == "dohc") *valvetrain = Valvetrain::DohcBucket;
    else if (name == "sohc_finger") *valvetrain = Valvetrain::SohcFinger;
    else if (name == "sohc_rocker" || name == "sohc") *valvetrain = Valvetrain::SohcRocker;
    else if (name == "pushrod" || name == "ohv" || name == "cam_in_block") *valvetrain = Valvetrain::Pushrod;
    else {
        *valvetrain = Valvetrain::DohcBucket;
        return false;
    }

    return true;
}

void EngineFriction::initialize(const Parameters &params, const Geometry &geometry) {
    m_params = params;
    m_geometry = geometry;

    const double B = geometry.bore;
    const int nc = std::max(geometry.cylinders, 1);
    const int banks = std::max(geometry.banks, 1);

    // Typical proportions of automotive engines when not specified
    m_mainBearingDiameter = (params.mainBearingDiameter > 0)
        ? params.mainBearingDiameter
        : 0.65 * B;
    m_mainBearingLength = (params.mainBearingLength > 0)
        ? params.mainBearingLength
        : 0.4 * m_mainBearingDiameter;
    m_mainBearingCount = (params.mainBearingCount > 0)
        ? params.mainBearingCount
        : std::max(geometry.crankJournals, 1) + 1;

    m_rodBearingDiameter = 0.55 * B;
    m_rodBearingLength = 0.35 * m_rodBearingDiameter;

    const int cylindersPerBank = (nc + banks - 1) / banks;
    switch (params.valvetrain) {
    case Valvetrain::DohcBucket:
        m_camBearingCount = banks * 2 * (cylindersPerBank + 1);
        break;
    case Valvetrain::SohcFinger:
    case Valvetrain::SohcRocker:
        m_camBearingCount = banks * (cylindersPerBank + 1);
        break;
    case Valvetrain::Pushrod:
    default:
        m_camBearingCount = cylindersPerBank + 1;
        break;
    }
}

EngineFriction::Breakdown EngineFriction::pattonNitschkeHeywood(
    double speed,
    double intakePressureRatio) const
{
    // PNH units: kPa, mm, rev/min, m/s
    // The mixed-lubrication terms grow without bound towards zero speed;
    // below 500 rpm (cranking) the friction is held at its 500 rpm value
    const double N = std::fmax(units::toRpm(std::abs(speed)), 500.0);
    const double B = units::convert(m_geometry.bore, units::mm);
    const double S = units::convert(m_geometry.stroke, units::mm);
    const double nc = std::max(m_geometry.cylinders, 1);
    const double Sp = 2 * m_geometry.stroke * N / 60.0;
    const double rc = std::fmax(m_geometry.compressionRatio, 1.0);

    if (B <= 0 || S <= 0) return Breakdown();

    const double Db = units::convert(m_mainBearingDiameter, units::mm);
    const double Lb = units::convert(m_mainBearingLength, units::mm);
    const double nb = m_mainBearingCount;
    const double Dr = units::convert(m_rodBearingDiameter, units::mm);
    const double Lr = units::convert(m_rodBearingLength, units::mm);
    const double Lv = units::convert(m_params.maxValveLift, units::mm);
    const double nv = m_params.valvesPerCylinder * nc;

    const double B2_S_nc = B * B * S * nc;

    Breakdown result;

    // Crankshaft: main bearing seals, main bearings (hydrodynamic) and
    // turbulent dissipation
    result.crankshaft =
        1.22E5 * Db / B2_S_nc
        + 3.03E-4 * N * Db * Db * Db * Lb * nb / B2_S_nc
        + 1.35E-10 * Db * Db * N * N * nb / nc;

    // Reciprocating: piston skirt, ring tension, connecting rod bearings and
    // gas pressure loading of the rings
    result.reciprocating =
        2.94E2 * Sp / B
        + 4.06E4 * (1 + 500 / N) / (B * B)
        + 3.03E-4 * N * Dr * Dr * Dr * Lr * nc / B2_S_nc
        + 6.89 * std::fmax(intakePressureRatio, 0.0)
            * (0.088 * rc + 0.182 * std::pow(rc, 1.33 - 2.38E-2 * Sp));

    // Valvetrain: camshaft bearings, followers, oscillating parts
    double C_ff, C_rf, C_oh, C_om;
    switch (m_params.valvetrain) {
    case Valvetrain::SohcFinger:
        C_ff = 600; C_rf = 0.0227; C_oh = 0.2; C_om = 42.8;
        break;
    case Valvetrain::SohcRocker:
        C_ff = 400; C_rf = 0.0151; C_oh = 0.5; C_om = 21.4;
        break;
    case Valvetrain::Pushrod:
        C_ff = 400; C_rf = 0.0151; C_oh = 0.5; C_om = 32.1;
        break;
    case Valvetrain::DohcBucket:
    default:
        C_ff = 200; C_rf = 0.0076; C_oh = 0.5; C_om = 10;
        break;
    }

    const double follower = m_params.rollerFollowers
        ? C_rf * N * nv / (S * nc)
        : C_ff * (1 + 1000 / N) * nv / (S * nc);

    result.valvetrain =
        244 * N * m_camBearingCount / B2_S_nc
        + follower
        + C_oh * std::pow(Lv, 1.5) * std::sqrt(N) * nv / (B * S * nc)
        + C_om * (1 + 500 / N) * Lv * nv / (S * nc);

    // Accessories (oil pump, water pump, non-charging alternator)
    result.auxiliary = 8.32 + 1.86E-3 * N + 7.45E-7 * N * N;

    const double toPa = units::kPa * m_params.multiplier;
    result.crankshaft *= toPa;
    result.reciprocating *= toPa;
    result.valvetrain *= toPa;
    result.auxiliary *= toPa;

    return result;
}

double EngineFriction::chenFlynn(double speed, double peakPressure) const {
    const double N = units::toRpm(std::abs(speed));
    const double Sp = 2 * m_geometry.stroke * N / 60.0;

    return m_params.multiplier * (
        m_params.chenFlynnConstant
        + m_params.chenFlynnPeakPressure * peakPressure
        + m_params.chenFlynnPistonSpeed * Sp
        + m_params.chenFlynnPistonSpeedSquared * Sp * Sp);
}

double EngineFriction::fmep(double speed, double intakePressureRatio, double peakPressure) const {
    switch (m_params.model) {
    case Model::PattonNitschkeHeywood:
        return pattonNitschkeHeywood(speed, intakePressureRatio).total();
    case Model::ChenFlynn:
        return chenFlynn(speed, peakPressure);
    case Model::Legacy:
    default:
        return 0.0;
    }
}

double EngineFriction::torque(double fmep) const {
    // W_f = FMEP * V_d per cycle of 4 pi
    return fmep * m_geometry.displacement / (4 * constants::pi);
}
