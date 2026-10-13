#include "../include/fuel.h"

#include "../include/units.h"

#include <cmath>

Fuel::Fuel() {
    m_molecularMass = 0.0;
    m_energyDensity = 0.0;
    m_density = 0.0;
    m_turbulenceToFlameSpeedRatio = nullptr;
    m_molecularAfr = 0.0;
    m_maxBurningEfficiency = 0.0;
    m_maxDilutionEffect = 0.0;
    m_maxTurbulenceEffect = 0.0;
    m_burningEfficiencyRandomness = 0.0;
    m_lowEfficiencyAttenuation = 0.0;
    m_combustionEfficiency = 1.0;
    m_knownType = true;
    m_definition = thermo::defaultFuel();
}

Fuel::~Fuel() {
    /* void */
}

void Fuel::initialize(const Parameters &params) {
    m_molecularMass = params.molecularMass;
    m_energyDensity = params.energyDensity;
    m_density = params.density;
    m_turbulenceToFlameSpeedRatio = params.turbulenceToFlameSpeedRatio;
    m_molecularAfr = params.molecularAfr;
    m_burningEfficiencyRandomness = params.burningEfficiencyRandomness;
    m_maxBurningEfficiency = params.maxBurningEfficiency;
    m_maxDilutionEffect = params.maxDilutionEffect;
    m_maxTurbulenceEffect = params.maxTurbulenceEffect;
    m_lowEfficiencyAttenuation = params.lowEfficiencyAttenuation;
    m_combustionEfficiency = params.combustionEfficiency;

    m_knownType = params.type.empty() || thermo::findFuel(params.type, &m_definition);
    if (params.type.empty() || !m_knownType) {
        m_definition = thermo::defaultFuel();
    }
}

double Fuel::flameSpeed(
    double turbulence,
    double molecularAfr,
    double T,
    double P,
    double firingPressure,
    double motoringPressure) const
{
    const double S_L = laminarBurningVelocity(molecularAfr, T, P);
    const double p_adjustment = 1.0;

    return m_turbulenceToFlameSpeedRatio->sampleTriangle((turbulence / S_L) * p_adjustment) * S_L;
}

double Fuel::turbulentFlameSpeed(double turbulence, double S_L) const {
    return m_turbulenceToFlameSpeedRatio->sampleTriangle(turbulence / S_L) * S_L;
}

double Fuel::laminarBurningVelocity(double molecularAfr, double T, double P) const {
    // Assuming fuel is gasoline
    constexpr double er_m = 1.21;
    constexpr double B_m = units::distance(30.5, units::cm) / units::sec;
    constexpr double B_er = -units::distance(54.9, units::cm) / units::sec;
    const double er = molecularAfr / m_molecularAfr;
    const double alpha = 2.4 - 0.271 * std::pow(er, 3.51);
    const double beta = -0.357 + 0.14 * std::pow(er, 2.77);

    const double S_L_0 = B_m + B_er * (er - er_m) * (er - er_m);
    const double T_ratio = T / units::kelvin(298);
    const double P_ratio = P / units::pressure(1.0, units::atm);

    return S_L_0 * std::pow(T_ratio, alpha) * std::pow(P_ratio, beta);
}

double Fuel::laminarBurningVelocityEngineering(
    double phi,
    double T_u,
    double P,
    double residualFraction) const
{
    // S_L = S_L0(phi) (T_u / 298 K)^alpha (P / 1 atm)^beta (1 - c x_r^d)
    // with S_L0 = B_m + B_phi (phi - phi_m)^2 (Metghalchi & Keck form)
    const thermo::FlameSpeedParameters &params = m_definition.flameSpeed;
    const double phi_c = std::fmin(std::fmax(phi, 0.4), 2.0);

    double alpha, beta, dilution;
    if (params.correlation == thermo::FlameSpeedParameters::Correlation::RhodesKeck) {
        alpha = 2.4 - 0.271 * std::pow(phi_c, 3.51);
        beta = -0.357 + 0.14 * std::pow(phi_c, 2.77);
        dilution = 1 - 2.06 * std::pow(std::fmax(residualFraction, 0.0), 0.77);
    }
    else {
        alpha = 2.18 - 0.8 * (phi_c - 1);
        beta = -0.16 + 0.22 * (phi_c - 1);
        dilution = 1 - 2.1 * std::fmax(residualFraction, 0.0);
    }

    const double S_L_0 = params.B_m + params.B_phi * (phi_c - params.phi_m) * (phi_c - params.phi_m);
    const double T_ratio = std::fmax(T_u, 200.0) / units::kelvin(298);
    const double P_ratio = std::fmax(P, 1000.0) / units::pressure(1.0, units::atm);

    // A small floor keeps very dilute or off-stoichiometric flames moving
    // so that the burn always completes
    return std::fmax(S_L_0 * std::fmax(dilution, 0.1), 0.02)
        * std::pow(T_ratio, alpha) * std::pow(P_ratio, beta);
}
