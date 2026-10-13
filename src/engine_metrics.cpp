#include "../include/engine_metrics.h"

#include "../include/engine.h"
#include "../include/constants.h"
#include "../include/units.h"

#include <cmath>

EngineMetrics calculateEngineMetrics(const Engine &engine, double brakeTorque) {
    EngineMetrics m;
    m.speed = engine.getSpeed();
    m.brakeTorque = brakeTorque;

    const bool engineering = engine.isEngineering();
    Fuel *fuel = const_cast<Engine &>(engine).getFuel();
    const double lhv = engineering
        ? fuel->getDefinition().vapourLhv()
        : fuel->getEnergyDensity();
    const double airMolarMass = engineering
        ? thermo::air::molarMass
        : units::AirMolecularMass;
    const double stoichiometricAfr = engineering
        ? fuel->getDefinition().stoichiometricAfr()
        : (fuel->getMolecularAfr() / 0.25) * units::AirMolecularMass / fuel->getMolecularMass();
    const double referenceAirDensity =
        SimulationAmbientPressure * airMolarMass
        / (constants::R * SimulationAmbientTemperature);

    const double cycleLength = engine.getOutputCrankshaft()->getCycleLength();

    double swept = 0, netWork = 0, grossWork = 0, pumpingWork = 0;
    double fuel_kg = 0, air_kg = 0, heatLoss = 0;
    double totalSwept = 0;
    for (int i = 0; i < engine.getCylinderCount(); ++i) {
        const CombustionChamber *chamber = engine.getChamber(i);
        totalSwept += chamber->getSweptVolume();

        const CombustionChamber::CycleMetrics &c = chamber->getLastCycleMetrics();
        if (!c.valid) continue;

        ++m.cylinders;
        swept += chamber->getSweptVolume();
        netWork += c.netWork;
        grossWork += c.grossWork;
        pumpingWork += c.pumpingWork;
        fuel_kg += c.fuelInducted;
        air_kg += c.airInducted;
        heatLoss += c.heatLoss;

        if (c.peakPressure > m.peakPressure) {
            m.peakPressure = c.peakPressure;
        }

        m.peakTemperature += c.peakTemperature;

        if (c.burned) {
            ++m.firingCylinders;
            m.sparkAdvance += c.sparkAdvance;
            m.ca10 += c.ca10;
            m.ca50 += c.ca50;
            m.ca90 += c.ca90;
            m.peakPressureAngle += c.peakPressureAngle;
        }
    }

    if (m.cylinders == 0 || swept <= 0) {
        return m;
    }

    m.valid = true;
    m.peakTemperature /= m.cylinders;
    if (m.firingCylinders > 0) {
        m.sparkAdvance /= m.firingCylinders;
        m.ca10 /= m.firingCylinders;
        m.ca50 /= m.firingCylinders;
        m.ca90 /= m.firingCylinders;
        m.peakPressureAngle /= m.firingCylinders;
    }

    m.imepGross = grossWork / swept;
    m.imepNet = netWork / swept;
    m.pmep = -pumpingWork / swept;

    // Scale the cylinders that reported to the whole engine
    const double scale = totalSwept / swept;
    m.indicatedTorque = scale * netWork / cycleLength;
    m.bmep = brakeTorque * cycleLength / totalSwept;
    m.fmep = m.imepNet - m.bmep;
    m.fmepModel = engine.usesFrictionModel()
        ? engine.getFrictionMeanEffectivePressure()
        : 0.0;

    m.indicatedPower = m.indicatedTorque * m.speed;
    m.brakePower = brakeTorque * m.speed;
    m.mechanicalEfficiency = (m.imepNet > 0) ? m.bmep / m.imepNet : 0.0;

    const double cyclesPerSecond = m.speed / cycleLength;
    m.fuelFlow = scale * fuel_kg * cyclesPerSecond;
    m.airFlow = scale * air_kg * cyclesPerSecond;

    if (fuel_kg > 0) {
        const double fuelEnergy = fuel_kg * lhv;
        m.indicatedEfficiency = netWork / fuelEnergy;
        m.brakeEfficiency = m.indicatedEfficiency * m.mechanicalEfficiency;
        m.heatLossFraction = heatLoss / fuelEnergy;
        m.afr = air_kg / fuel_kg;
        m.lambda = m.afr / stoichiometricAfr;
    }

    if (m.indicatedPower > 0) m.isfc = m.fuelFlow / m.indicatedPower;
    if (m.brakePower > 0) m.bsfc = m.fuelFlow / m.brakePower;

    m.volumetricEfficiency = air_kg / (referenceAirDensity * swept);

    return m;
}

double saeJ1349CorrectionFactor(double dryAirPressure, double temperature) {
    return 1.18 * (99000.0 / dryAirPressure) * std::sqrt(temperature / 298.0) - 0.18;
}
