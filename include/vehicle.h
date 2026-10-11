#ifndef ATG_ENGINE_SIM_VEHICLE_H
#define ATG_ENGINE_SIM_VEHICLE_H

#include "scs.h"
#include "drivetrain_spring.h"
#include "units.h"

class Vehicle {
    public:
        struct Parameters {
            double mass;
            double dragCoefficient;
            double crossSectionArea;
            double diffRatio;
            double tireRadius;
            double rollingResistance;

            // Drivetrain compliance (axles, tires) between the transmission
            // output and the wheels
            double stiffness = units::torque(50.0, units::ft_lb) / units::angle(1.0, units::deg);
            double damping = 1.0;
            double maxFlex = units::angle(30.0, units::deg);
            bool limitFlex = true;
            bool simulateFlex = false;

            double maxBrakeForce = units::force(5000.0, units::N);
        };

    public:
        Vehicle();
        ~Vehicle();

        void initialize(const Parameters &params);
        void update(double dt);

        // `driveshaft` is the body driven by the transmission when drivetrain
        // flex is simulated (otherwise null)
        void addToSystem(
            atg_scs::RigidBodySystem *system,
            atg_scs::RigidBody *rotatingMass,
            atg_scs::RigidBody *driveshaft = nullptr);

        inline bool isFlexSimulated() const { return m_simulateFlex; }
        inline DrivetrainSpring *getDrivetrainSpring() { return &m_drivetrainSpring; }

        // Brake pedal position in [0, 1]
        inline void setBrakeLevel(double level) { m_brakeLevel = level < 0 ? 0 : (level > 1 ? 1 : level); }
        inline double getBrakeLevel() const { return m_brakeLevel; }
        inline double getMaxBrakeForce() const { return m_maxBrakeForce; }
        inline double getBrakeForce() const { return m_brakeLevel * m_maxBrakeForce; }
        inline double getMass() const { return m_mass; }
        inline double getRollingResistance() const { return m_rollingResistance; }
        inline double getDragCoefficient() const { return m_dragCoefficient; }
        inline double getCrossSectionArea() const { return m_crossSectionArea; }
        inline double getDiffRatio() const { return m_diffRatio; }
        inline double getTireRadius() const { return m_tireRadius; }
        double getSpeed() const;
        inline double getTravelledDistance() const { return m_travelledDistance; }
        inline void resetTravelledDistance() { m_travelledDistance = 0; }
        double linearForceToVirtualTorque(double force) const;

    protected:
        atg_scs::RigidBody *m_rotatingMass;

        double m_mass;
        double m_dragCoefficient;
        double m_crossSectionArea;
        double m_diffRatio;
        double m_tireRadius;
        double m_travelledDistance;
        double m_rollingResistance;

        double m_maxBrakeForce;
        double m_brakeLevel;

        bool m_simulateFlex;
        DrivetrainSpring m_drivetrainSpring;
};

#endif /* ATG_ENGINE_SIM_VEHICLE_H */
