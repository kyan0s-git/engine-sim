#ifndef ATG_ENGINE_SIM_ENGINE_METRICS_H
#define ATG_ENGINE_SIM_ENGINE_METRICS_H

class Engine;

// Engine performance figures computed from the last complete cycle of each
// cylinder (see CombustionChamber::CycleMetrics) and the brake torque
// measured by the dynamometer. Mean effective pressures are per unit of
// displacement; for rotary engines each chamber's swept volume is used.
struct EngineMetrics {
    bool valid = false;
    int cylinders = 0;              // cylinders with a complete cycle
    int firingCylinders = 0;        // ... that burned fuel

    double speed = 0;               // [rad/s]

    double imepGross = 0;           // [Pa]
    double imepNet = 0;
    double pmep = 0;                // pumping loss
    double bmep = 0;
    double fmep = 0;                // imepNet - bmep (total mechanical loss)
    double fmepModel = 0;           // from the friction model (if used)

    double indicatedTorque = 0;     // [N m], from the net indicated work
    double brakeTorque = 0;
    double indicatedPower = 0;      // [W]
    double brakePower = 0;

    double mechanicalEfficiency = 0;
    double indicatedEfficiency = 0;     // net indicated work / fuel energy (LHV)
    double brakeEfficiency = 0;

    double fuelFlow = 0;            // [kg/s]
    double airFlow = 0;             // [kg/s]
    double isfc = 0;                // [kg/J]
    double bsfc = 0;                // [kg/J]

    double volumetricEfficiency = 0;    // air only, 1 atm and 25 C reference
    double afr = 0;
    double lambda = 0;

    double sparkAdvance = 0;        // [deg BTDC]
    double ca10 = 0;                // [deg ATDC]
    double ca50 = 0;
    double ca90 = 0;
    double peakPressure = 0;        // [Pa]
    double peakPressureAngle = 0;   // [deg ATDC]
    double peakTemperature = 0;     // [K]
    double heatLossFraction = 0;    // wall heat loss / fuel energy
};

EngineMetrics calculateEngineMetrics(const Engine &engine, double brakeTorque);

// SAE J1349 (2004) power correction factor to 99 kPa dry air and 25 C:
//     CF = 1.18 (99 / p_d) sqrt(T / 298) - 0.18
// with the dry air pressure p_d [Pa] and the intake air temperature T [K]
double saeJ1349CorrectionFactor(double dryAirPressure, double temperature);

// Ambient conditions of the simulation
constexpr double SimulationAmbientPressure = 101325.0;
constexpr double SimulationAmbientTemperature = 298.15;

#endif /* ATG_ENGINE_SIM_ENGINE_METRICS_H */
