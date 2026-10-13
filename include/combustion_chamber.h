#ifndef ATG_ENGINE_SIM_COMBUSTION_CHAMBER_H
#define ATG_ENGINE_SIM_COMBUSTION_CHAMBER_H

#include "scs.h"

#include "piston.h"
#include "gas_system.h"
#include "cylinder_head.h"
#include "units.h"
#include "fuel.h"

class Engine;
class CombustionChamber : public atg_scs::ForceGenerator {
    public:
        struct Parameters {
            ::Piston *Piston;
            CylinderHead *Head;
            ::Fuel *Fuel;
            Function *MeanPistonSpeedToTurbulence;

            double StartingPressure;
            double StartingTemperature;
            double CrankcasePressure;

            double HeatTransferCoefficient = 100.0;
            double BlockTemperature = units::celcius(90.0);

            // Engineering physics model: Woschni heat transfer to walls at
            // this mean temperature, scaled by the multiplier
            double WallTemperature = 450.0;
            double HeatTransferMultiplier = 1.0;

            // False if the engine's friction model already includes piston
            // friction
            bool PistonFriction = true;
        };

        struct FlameEvent {
            double lit_n = 0;
            double total_n = 0;
            double percentageLit = 0;
            double efficiency = 1.0;
            double flameSpeed = 0.0;

            double lastVolume = 0.0;
            double travel_x = 0.0;
            double travel_y = 0.0;
            GasSystem::Mix globalMix;

            // Engineering model: state at ignition used to estimate the
            // unburned gas temperature as the burn progresses
            double turbulence = 0.0;
            double equivalenceRatio = 1.0;
            double residualFraction = 0.0;
            double ignitionTemperature = 0.0;
            double ignitionPressure = 0.0;
            double unburnedGamma = 1.35;
            double burnedTemperature = 2000.0;

            // Energy of the unburned and burned gas per mole of unburned gas
            thermo::EnergyPolynomial unburnedEnergy;
            thermo::EnergyPolynomial burnedEnergy;
            double burnedMolesPerMole = 1.0;
        };

        // Results of the last complete four-stroke cycle. Angles are crank
        // angles in degrees after the firing TDC.
        struct CycleMetrics {
            bool valid = false;
            int cycleCount = 0;

            double grossWork = 0;           // compression + expansion [J]
            double pumpingWork = 0;         // exhaust + intake [J]
            double netWork = 0;             // [J]
            double imepGross = 0;           // [Pa]
            double imepNet = 0;             // [Pa]
            double pmep = 0;                // [Pa], positive = loss

            double peakPressure = 0;        // [Pa]
            double peakPressureAngle = 0;
            double peakTemperature = 0;     // [K]

            bool burned = false;
            double sparkAdvance = 0;        // degrees before TDC
            double ca10 = 0;
            double ca50 = 0;
            double ca90 = 0;

            double fuelBurned = 0;          // [kg]
            double fuelInducted = 0;        // [kg]
            double airInducted = 0;         // [kg]
            double heatLoss = 0;            // to the walls [J]
        };

        struct FrictionModelParams {
            double frictionCoeff = 0.06;
            double breakawayFriction = units::force(50, units::N);
            double breakawayFrictionVelocity = units::distance(0.1, units::m);
            double viscousFrictionCoefficient = units::force(20, units::N);
        };

    public:
        CombustionChamber();
        virtual ~CombustionChamber();

        void initialize(const Parameters &params);
        void destroy();
        void setEngine(Engine *engine) { m_engine = engine; }
        virtual void apply(atg_scs::SystemState *system) override;

        CylinderHead *getCylinderHead() const { return m_head; }
        Piston *getPiston() const { return m_piston; }

        // Geometry dependent functions; overridden by the chambers of engines
        // that aren't reciprocating piston engines (e.g. rotary engines)
        virtual double getFrictionForce() const;
        virtual double getVolume() const;
        virtual double pistonSpeed() const;

        // Flow coefficients of the intake and exhaust openings
        virtual double calculateIntakeFlowRate() const;
        virtual double calculateExhaustFlowRate() const;

        // Current intake/exhaust opening (valve lift for piston engines)
        virtual double getIntakeOpening() const;
        virtual double getExhaustOpening() const;
        double calculateMeanPistonSpeed() const;
        double calculateFiringPressure() const;

        bool isLit() const { return m_lit; }
        bool popLitLastFrame();

        void ignite();
        void update(double dt);
        void flow(double dt);

        double lastEventAfr() const;

        const CycleMetrics &getLastCycleMetrics() const { return m_lastCycle; }

        // Swept volume of this cylinder [m^3]
        virtual double getSweptVolume() const;

        // Current wall heat transfer coefficient [W/(m^2 K)]
        double getHeatTransferCoefficient() const { return m_currentHeatTransferCoefficient; }

        bool isEngineering() const { return m_engineering; }

        void setRandomSeed(unsigned int seed);

        double getLastIterationExhaustFlow() const { return m_exhaustFlow; }

        void resetLastTimestepExhaustFlow() { m_lastTimestepTotalExhaustFlow = 0; }
        double getLastTimestepExhaustFlow() const { return m_lastTimestepTotalExhaustFlow; }

        void resetLastTimestepIntakeFlow() { m_lastTimestepTotalIntakeFlow = 0; }
        double getLastTimestepIntakeFlow() const { return m_lastTimestepTotalIntakeFlow; }

        Function *m_meanPistonSpeedToTurbulence;
        GasSystem m_system;
        GasSystem m_intakeRunnerAndManifold;
        GasSystem m_exhaustRunnerAndPrimary;
        FlameEvent m_flameEvent;
        bool m_lit;

        FrictionModelParams m_frictionModel;

        double m_peakTemperature;
        double m_nBurntFuel;

    protected:
        double calculateFrictionForce(double v) const;
        void updateCycleStates();

        // Uniform random number in [0, 1]
        double random();

        bool igniteEngineering();
        double engineeringFlameSpeed() const;
        void propagateFlame(double dt, double volume);
        void updateHeatTransferCoefficient(double meanPistonSpeed);
        void trackCycle(double dV);
        void onStrokeEnd(bool tdc);
        void closeCycle();
        double crankAngle() const;
        double degreesAfter(double angle, double reference) const;
        double airMoles(const GasSystem::Mix &mix, double n) const;
        double airMolarMass() const;
        double fuelMolarMass() const;

        struct Stroke {
            double work = 0;
            bool endsAtTdc = false;
            double tdcPressure = 0;
            double tdcAngle = 0;
        };

        struct CycleTracker {
            int direction = 0;          // +1 expanding, -1 compressing
            double extremeVolume = 0;
            double extremePressure = 0;
            double extremeAngle = 0;
            double workSinceExtreme = 0;
            double strokeWork = 0;

            Stroke strokes[4];
            int strokeCount = 0;

            double peakPressure = 0;
            double peakPressureAngle = 0;
            double peakTemperature = 0;
            double fuelInducted = 0;    // [mol]
            double airInducted = 0;     // [mol]
            double heatLoss = 0;
            double fuelBurned = 0;      // [kg]

            bool sparked = false;
            double sparkAngle = 0;

            static constexpr int MaxBurnSamples = 1024;
            double burnAngle[MaxBurnSamples];
            double burnAmount[MaxBurnSamples];
            int burnSamples = 0;
        };

        CycleTracker m_cycle;
        CycleMetrics m_lastCycle;

        bool m_engineering;
        bool m_pistonFriction;
        double m_wallTemperature;
        double m_heatTransferMultiplier;
        double m_currentHeatTransferCoefficient;
        double m_productFractionStoichiometric;

        // State at intake valve closing (reference for the Woschni motored
        // pressure) and combustion phase flag
        bool m_intakeOpen;
        bool m_combustionPhase;
        double m_ivcPressure;
        double m_ivcVolume;
        double m_ivcTemperature;
        double m_ivcGamma;

        unsigned long long m_randomState;

        double m_intakeFlowRate;
        double m_exhaustFlowRate;

        double m_manifoldToRunnerFlowRate;
        double m_primaryToCollectorFlowRate;
        double m_cylinderCrossSectionSurfaceArea;
        double m_cylinderWidthApproximation;

        double m_lastTimestepTotalExhaustFlow;
        double m_lastTimestepTotalIntakeFlow;
        double m_exhaustFlow;

        double m_crankcasePressure;
        double m_heatTransferCoefficient;
        double m_blockTemperature;

        double *m_pressure;
        double *m_pistonSpeed;
        static constexpr int StateSamples = 256;

        bool m_litLastFrame;

        Piston *m_piston;
        CylinderHead *m_head;
        Engine *m_engine;
        Fuel *m_fuel;
};

#endif /* ATG_ENGINE_SIM_COMBUSTION_CHAMBER_H */
