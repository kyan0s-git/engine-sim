#ifndef ATG_ENGINE_SIM_FORCED_INDUCTION_H
#define ATG_ENGINE_SIM_FORCED_INDUCTION_H

#include "part.h"

#include "gas_system.h"

#include "scs.h"

#include <string>

class Crankshaft;
class Intake;

// Turbochargers and superchargers.
//
// The compressor fills a charge-air volume (pipes and intercooler) that the
// intakes' throttles draw from instead of the atmosphere.
//
//  - Centrifugal compressors (turbochargers and centrifugal superchargers)
//    are modelled as a pressure source behind a flow restriction. The source
//    pressure follows from the Euler work of the impeller,
//        PR = (1 + eta * sigma * U^2 / (c_p * T1))^(gamma / (gamma - 1)),
//    so the delivered boost depends on the wheel's tip speed U and drops as
//    the flow approaches the compressor's capacity. The impeller absorbs
//    sigma * U^2 of work per kilogram.
//  - Positive displacement superchargers (Roots and twin-screw) deliver their
//    displacement every revolution; some of it leaks back to the inlet
//    through the clearances.
//
// A turbocharger's turbine is the restriction at the exhaust outlet. It
// extracts eta_t * c_p * T * (1 - (p_out / p_in)^((gamma - 1) / gamma)) per
// unit of flow; the shaft's kinetic energy integrates the turbine power minus
// the compressor and bearing power. A wastegate bypasses the turbine once the
// boost reaches its set point. Superchargers are driven by the crankshaft
// through their drive ratio and load it with their drive power.
class ForcedInduction : public Part, public atg_scs::ForceGenerator {
    public:
        enum class Type {
            Turbocharger,
            Roots,
            TwinScrew,
            Centrifugal
        };

        // Turbochargers only
        enum class Arrangement {
            Single,
            Twin,           // two turbochargers in parallel
            Sequential      // the second turbocharger joins at high speed
        };

        struct Parameters {
            Type type = Type::Turbocharger;
            Arrangement arrangement = Arrangement::Single;

            // Compressor (centrifugal: per unit)
            double compressorDiameter = 0.060;      // impeller tip diameter [m]
            double compressorFlow = 0.30;           // maximum flow [kg/s]
            double compressorEfficiency = 0.75;
            double maxSpeed = 2 * 3.14159265358979 * 3000;  // shaft limit [rad/s]

            // Turbine (turbochargers)
            double turbineAr = 0.64;                // housing A/R
            double turbineEfficiency = 0.70;
            double inertia = 3.0e-5;                // rotating assembly [kg m^2]
            bool ballBearings = false;

            // Boost control
            double boost = 1.0e5;                   // wastegate set point (gauge) [Pa]
            bool blowoffValve = true;

            // Superchargers
            double driveRatio = 1.0;                // compressor speed / crank speed
            double displacement = 1.9e-3;           // positive displacement, per rev [m^3]
            bool bypassValve = true;                // vents the charge at part throttle

            // Charge air
            double intercoolerEffectiveness = 0.7;
            double chargeVolume = 6.0e-3;           // [m^3]

            // Sequential turbochargers: engine speed at which the second
            // unit comes in [rad/s]
            double sequentialSpeed = 2 * 3.14159265358979 * 4000 / 60;

            GasSystem::Model model = GasSystem::Model::Legacy;
            const thermo::FuelDefinition *fuel = nullptr;
        };

        struct Unit {
            double omega = 0;           // shaft speed [rad/s]
            double energy = 0;          // shaft kinetic energy [J]
            double compressorFlow = 0;  // [mol/s]
            double turbinePower = 0;    // [W]
            double compressorPower = 0;
            bool active = true;
        };

    public:
        ForcedInduction();
        virtual ~ForcedInduction();

        void initialize(const Parameters &params);
        virtual void destroy();

        // Connections (made by the engine)
        void setCrankshaft(Crankshaft *crankshaft) { m_crankshaft = crankshaft; }
        void addIntake(Intake *intake);
        void setExhaustSystemCount(int count) { m_exhaustSystemCount = count; }

        // Called by each exhaust system to get its outlet restriction and to
        // report its outlet flow (moles leaving through the outlet)
        double getExhaustOutletFlowK(double mufflerK) const;
        void addExhaustFlow(double n, double T, double P, double hcr, double mufflerK);

        // One fluid step (after the exhausts, before the intakes)
        void process(double dt);

        // Applies the supercharger's drive torque to the crankshaft
        virtual void apply(atg_scs::SystemState *system) override;

        GasSystem *getChargeSystem() { return &m_charge; }

        Type getType() const { return m_type; }
        Arrangement getArrangement() const { return m_arrangement; }
        bool isTurbocharger() const { return m_type == Type::Turbocharger; }
        bool isPositiveDisplacement() const { return m_type == Type::Roots || m_type == Type::TwinScrew; }
        std::string getDescription() const;

        int getUnitCount() const { return m_unitCount; }
        const Unit &getUnit(int i) const { return m_units[i]; }

        // Gauge pressure of the charge air [Pa]
        double getBoost() const;
        double getChargePressure() const { return m_charge.pressure(); }
        double getChargeTemperature() const { return m_charge.temperature(); }
        double getCompressorOutletTemperature() const { return m_compressorOutletTemperature; }

        // Shaft speed of the (first) compressor [rad/s]
        double getShaftSpeed() const;
        double getMaxSpeed() const { return m_maxSpeed; }

        double getWastegateOpening() const { return m_wastegate; }
        double getBlowoffOpening() const { return m_blowoff; }
        double getBlowoffFlow() const { return m_blowoffFlowRate; }    // [mol/s]
        double getCompressorFlow() const;                               // [mol/s]
        double getDrivePower() const { return m_drivePower; }           // superchargers [W]
        double getTurbinePower() const;

        // Share of the exhaust flow going through the turbine(s) [0, 1]
        double getTurbineFlowFraction() const { return m_turbineFraction; }

        // Runtime adjustments
        void setBoostTarget(double boost) { m_boostTarget = boost; }
        double getBoostTarget() const { return m_boostTarget; }
        void setBlowoffValve(bool enabled) { m_blowoffEnabled = enabled; }
        bool hasBlowoffValve() const { return m_blowoffEnabled; }
        void setIntercoolerEffectiveness(double e);
        double getIntercoolerEffectiveness() const { return m_intercoolerEffectiveness; }
        void setDriveRatio(double ratio) { m_driveRatio = ratio; }
        double getDriveRatio() const { return m_driveRatio; }

        // Audio: compressor whine and blow-off noise
        double getWhineFrequency() const;   // [Hz]
        double getWhineLevel() const;       // [0, 1]
        double getBlowoffLevel() const;     // [0, 1]

    protected:
        double centrifugalSourcePressureRatio(double omega) const;
        void processCentrifugal(Unit &unit, double dt, double scale, const GasSystem::Mix &mix, double T1);
        void processPositiveDisplacement(double dt, const GasSystem::Mix &mix, double T1);
        double plenumPressure() const;
        double crankSpeed() const;

        Type m_type;
        Arrangement m_arrangement;

        Unit m_units[2];
        int m_unitCount;

        GasSystem m_charge;

        Crankshaft *m_crankshaft;
        Intake *m_intakes[16];
        int m_intakeCount;
        int m_exhaustSystemCount;

        // Compressor
        double m_tipRadius;
        double m_compressorK;
        double m_compressorEfficiency;
        double m_maxSpeed;
        double m_slip;

        // Positive displacement
        double m_displacement;
        double m_leakK;
        double m_bypassK;
        bool m_bypassValve;
        double m_bypass;
        double m_driveRatio;
        double m_driveTorque;
        double m_drivePower;

        // Turbine
        double m_turbineK;
        double m_wastegateK;
        double m_turbineEfficiency;
        double m_inertia;
        double m_bearingLoss;
        double m_turbineFraction;
        double m_sequentialSpeed;
        double m_turbineEnergy;     // turbine work since the last process() [J]

        // Control
        double m_boostTarget;
        double m_wastegate;
        bool m_blowoffEnabled;
        double m_blowoff;
        double m_blowoffK;
        double m_blowoffFlowRate;

        double m_intercoolerEffectiveness;
        double m_compressorOutletTemperature;
};

#endif /* ATG_ENGINE_SIM_FORCED_INDUCTION_H */
