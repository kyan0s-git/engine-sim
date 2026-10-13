#ifndef ATG_ENGINE_SIM_ENGINE_FRICTION_H
#define ATG_ENGINE_SIM_ENGINE_FRICTION_H

#include <string>

// Mechanical friction of a four-stroke piston engine expressed as friction
// mean effective pressure (FMEP). Pumping losses are not included because
// the simulation computes them directly.
//
// Patton, Nitschke & Heywood (SAE 890836), as given in Heywood, Internal
// Combustion Engine Fundamentals, section 13.6: crankshaft, reciprocating,
// valvetrain and auxiliary terms as functions of geometry, speed and intake
// pressure. Chen & Flynn (SAE 650733): A + B p_max + C Sp + D Sp^2.
//
// The coefficients are quoted from memory in the research notes and are
// labelled [R] there; they must be checked against the primary source.
class EngineFriction {
    public:
        enum class Model {
            Legacy,                     // constant crankshaft friction torque
            PattonNitschkeHeywood,
            ChenFlynn
        };

        enum class Valvetrain {
            DohcBucket,
            SohcFinger,
            SohcRocker,
            Pushrod
        };

        struct Parameters {
            Model model = Model::Legacy;
            Valvetrain valvetrain = Valvetrain::DohcBucket;
            bool rollerFollowers = false;
            int valvesPerCylinder = 4;
            double maxValveLift = 0.010;            // m

            // 0 = derived from the bore
            double mainBearingDiameter = 0.0;       // m
            double mainBearingLength = 0.0;         // m
            int mainBearingCount = 0;

            double multiplier = 1.0;

            // Chen-Flynn coefficients
            double chenFlynnConstant = 0.4E5;               // Pa
            double chenFlynnPeakPressure = 0.005;           // Pa / Pa
            double chenFlynnPistonSpeed = 0.08E5;           // Pa / (m/s)
            double chenFlynnPistonSpeedSquared = 0.0009E5;  // Pa / (m/s)^2
        };

        struct Geometry {
            double bore = 0;                // m
            double stroke = 0;              // m
            int cylinders = 0;
            int banks = 1;
            int crankJournals = 0;          // rod journals on all crankshafts
            double compressionRatio = 10;
            double displacement = 0;        // m^3
        };

        struct Breakdown {
            double crankshaft = 0;          // Pa
            double reciprocating = 0;
            double valvetrain = 0;
            double auxiliary = 0;

            double total() const { return crankshaft + reciprocating + valvetrain + auxiliary; }
        };

    public:
        EngineFriction();

        static bool parseModel(const std::string &name, bool engineering, Model *model);
        static bool parseValvetrain(const std::string &name, Valvetrain *valvetrain, bool *roller);

        void initialize(const Parameters &params, const Geometry &geometry);

        Model getModel() const { return m_params.model; }
        const Parameters &getParameters() const { return m_params; }
        const Geometry &getGeometry() const { return m_geometry; }

        // Friction mean effective pressure [Pa] at the given speed [rad/s],
        // intake manifold pressure relative to ambient and last cycle's peak
        // cylinder pressure [Pa]
        Breakdown pattonNitschkeHeywood(double speed, double intakePressureRatio) const;
        double chenFlynn(double speed, double peakPressure) const;
        double fmep(double speed, double intakePressureRatio, double peakPressure) const;

        // Mean friction torque [N m] for an FMEP [Pa]
        double torque(double fmep) const;

        double getMainBearingDiameter() const { return m_mainBearingDiameter; }
        double getMainBearingLength() const { return m_mainBearingLength; }
        int getMainBearingCount() const { return m_mainBearingCount; }
        double getRodBearingDiameter() const { return m_rodBearingDiameter; }
        double getRodBearingLength() const { return m_rodBearingLength; }
        int getCamBearingCount() const { return m_camBearingCount; }

    protected:
        Parameters m_params;
        Geometry m_geometry;

        double m_mainBearingDiameter;
        double m_mainBearingLength;
        int m_mainBearingCount;
        double m_rodBearingDiameter;
        double m_rodBearingLength;
        int m_camBearingCount;
};

#endif /* ATG_ENGINE_SIM_ENGINE_FRICTION_H */
