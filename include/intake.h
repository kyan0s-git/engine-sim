#ifndef ATG_ENGINE_SIM_INTAKE_H
#define ATG_ENGINE_SIM_INTAKE_H

#include "part.h"

#include "gas_system.h"

class ForcedInduction;

class Intake : public Part {
    public:
        struct Parameters {
            // Plenum volume
            double volume;

            // Plenum dimensions
            double CrossSectionArea;

            // Input flow constant
            double InputFlowK;

            // Idle-circuit flow constant
            double IdleFlowK;

            // Flow rate from plenum to runner
            double RunnerFlowRate;

            // Molecular air fuel ratio (defaults to ideal for octane)
            double MolecularAfr = (25.0 / 2.0);

            // Throttle plate position at idle
            double IdleThrottlePlatePosition = 0.975;

            // Runner volume
            double RunnerLength = units::distance(4.0, units::inch);

            // Velocity decay factor
            double VelocityDecay = 0.5;

            // Engineering physics model: metered air-fuel ratio relative to
            // stoichiometric and the fraction of the fuel's latent heat of
            // vaporization taken from the air
            GasSystem::Model Model = GasSystem::Model::Legacy;
            const thermo::FuelDefinition *Fuel = nullptr;
            double Lambda = 1.0;
            double EvaporativeCooling = 0.5;
        };

    public:
        Intake();
        virtual ~Intake();

        void initialize(Parameters &params);
        virtual void destroy();

        // Mixture supplied through the throttle
        GasSystem::Mix getFuelAirMix() const;

        void process(double dt);

        inline double getRunnerFlowRate() const { return m_runnerFlowRate; }
        inline double getThrottlePlatePosition() const { return m_idleThrottlePlatePosition * m_throttle; }
        inline double getRunnerLength() const { return m_runnerLength; }
        inline double getPlenumCrossSectionArea() const { return m_crossSectionArea; }
        inline double getVelocityDecay() const { return m_velocityDecay; }
        inline double getLambda() const { return m_lambda; }
        inline void setLambda(double lambda) { m_lambda = (lambda > 0) ? lambda : 1.0; }

        // The throttle draws from the compressor's charge air instead of the
        // atmosphere when the engine is boosted
        void setForcedInduction(ForcedInduction *forcedInduction) { m_forcedInduction = forcedInduction; }
        ForcedInduction *getForcedInduction() const { return m_forcedInduction; }

        // Temperature of the fresh charge after fuel evaporation [K]
        inline double getChargeTemperature() const { return m_chargeTemperature; }

        GasSystem m_system;
        double m_throttle;

        double m_flow;
        double m_flowRate;
        double m_totalFuelInjected;

    protected:
        double m_crossSectionArea;
        double m_inputFlowK;
        double m_idleFlowK;
        double m_runnerFlowRate;
        double m_molecularAfr;
        double m_idleThrottlePlatePosition;
        double m_runnerLength;
        double m_velocityDecay;

        GasSystem::Model m_model;
        const thermo::FuelDefinition *m_fuel;
        double m_lambda;
        double m_chargeTemperature;

        GasSystem m_atmosphere;
        ForcedInduction *m_forcedInduction;
};

#endif /* ATG_ENGINE_SIM_INTAKE_H */
