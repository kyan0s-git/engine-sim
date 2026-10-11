#ifndef ATG_ENGINE_SIM_DRIVETRAIN_SPRING_H
#define ATG_ENGINE_SIM_DRIVETRAIN_SPRING_H

#include "scs.h"

// Torsional spring/damper connecting two rotating bodies. Used to model
// compliance in the drivetrain (axles, tires) and in the clutch disc's damper
// springs.
//
//  - The wind-up angle is limited to +/- max flex by a stiff end stop
//  - Optionally the transmitted torque is capped, in which case the spring
//    slips once the cap is exceeded (models a slipping clutch)
//
// The wind-up angle is tracked by integrating the relative angular velocity
// since the absolute angles of the crankshaft wrap around.
class DrivetrainSpring : public atg_scs::ForceGenerator {
    public:
        DrivetrainSpring();
        virtual ~DrivetrainSpring();

        void connect(atg_scs::RigidBody *body1, atg_scs::RigidBody *body2);
        void disconnect();

        virtual void apply(atg_scs::SystemState *state) override;

        // Called once per simulation step after integration
        void update(double dt);

        void reset() { m_windup = 0; }

        double getWindup() const { return m_windup; }
        double getLastTorque() const { return m_lastTorque; }

        // Spring stiffness [N*m/rad] and damping [N*m*s/rad]
        double m_ks;
        double m_kd;

        // Maximum wind-up before hitting the end stop [rad]
        double m_maxFlex;
        bool m_limitFlex;

        // Maximum transmitted torque; negative means unlimited
        double m_maxTorque;

    protected:
        double calculateTorque(double windup, double relativeVelocity) const;

        atg_scs::RigidBody *m_body1;
        atg_scs::RigidBody *m_body2;

        double m_windup;
        double m_lastTorque;
};

#endif /* ATG_ENGINE_SIM_DRIVETRAIN_SPRING_H */
