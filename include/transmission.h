#ifndef ATG_ENGINE_SIM_TRANSMISSION_H
#define ATG_ENGINE_SIM_TRANSMISSION_H

#include "vehicle.h"
#include "engine.h"
#include "scs.h"
#include "drivetrain_spring.h"

class Transmission {
    public:
        struct Parameters {
            int GearCount;
            const double *GearRatios;
            double MaxClutchTorque;

            // Clutch disc damper springs
            double MaxClutchFlex = units::angle(5.0, units::deg);
            bool LimitClutchFlex = true;
            double ClutchStiffness = units::torque(100.0, units::ft_lb) / units::angle(1.0, units::deg);
            double ClutchDamping = 1.0;
            bool SimulateFlex = false;
        };

    public:
        Transmission();
        ~Transmission();

        void initialize(const Parameters &params);
        void update(double dt);
        // `rotatingMass` represents the vehicle's inertia and `output` is the
        // body that the clutch drives (the same body unless drivetrain flex
        // is simulated)
        void addToSystem(
            atg_scs::RigidBodySystem *system,
            atg_scs::RigidBody *rotatingMass,
            Vehicle *vehicle,
            Engine *engine,
            atg_scs::RigidBody *output = nullptr);

        inline int getGearCount() const { return m_gearCount; }
        inline double getGearRatio(int gear) const { return m_gearRatios[gear]; }
        inline bool isFlexSimulated() const { return m_simulateFlex; }
        void changeGear(int newGear);
        inline int getGear() const { return m_gear; }
        inline void setClutchPressure(double pressure) { m_clutchPressure = pressure; }
        inline double getClutchPressure() const { return m_clutchPressure; }

    protected:
        atg_scs::ClutchConstraint m_clutchConstraint;
        DrivetrainSpring m_clutchSpring;
        bool m_simulateFlex;

        atg_scs::RigidBody *m_rotatingMass;
        atg_scs::RigidBody *m_output;
        Vehicle *m_vehicle;

        int m_gear;
        int m_newGear;
        int m_gearCount;
        double *m_gearRatios;
        double m_maxClutchTorque;
        double m_clutchPressure;
};

#endif /* ATG_ENGINE_SIM_TRANSMISSION_H */
