#ifndef ATG_ENGINE_SIM_IGNITION_MODULE_H
#define ATG_ENGINE_SIM_IGNITION_MODULE_H

#include "part.h"

#include "crankshaft.h"
#include "function.h"
#include "units.h"

#include <vector>

class IgnitionModule : public Part {
    public:
        struct Parameters {
            int cylinderCount;
            Crankshaft *crankshaft;
            Function *timingCurve;
            double revLimit = units::rpm(6000.0);
            double limiterDuration = 0.5 * units::sec;
        };

        struct SparkPlug {
            // A plug can fire more than once per cycle (e.g. a rotary engine's
            // plug fires once per eccentric shaft revolution)
            std::vector<double> angles;
            bool ignitionEvent = false;
            bool enabled = false;
        };

    public:
        IgnitionModule();
        virtual ~IgnitionModule();

        virtual void destroy();

        void initialize(const Parameters &params);
        // Adds a firing angle (crankshaft cycle angle) for the given plug
        void setFiringOrder(int cylinderIndex, double angle);
        void reset();
        void update(double dt);

        bool getIgnitionEvent(int index) const;
        void resetIgnitionEvents();

        double getTimingAdvance();

        // Added to the timing curve (positive = more advance) [rad]
        void setTimingOffset(double offset) { m_timingOffset = offset; }
        double getTimingOffset() const { return m_timingOffset; }

        void setRevLimit(double revLimit) { m_revLimit = revLimit; }

        // True while the rev limiter is cutting the spark
        bool isLimiting() const { return m_revLimitTimer > 0; }
        double getRevLimit() const { return m_revLimit; }

        bool m_enabled;

    protected:
        SparkPlug *getPlug(int i);

        Function *m_timingCurve;
        SparkPlug *m_plugs;
        Crankshaft *m_crankshaft;
        int m_cylinderCount;

        double m_lastCrankshaftAngle;
        double m_revLimit;
        double m_revLimitTimer;
        double m_limiterDuration;
        double m_timingOffset = 0.0;
};

#endif /* ATG_ENGINE_SIM_IGNITION_MODULE_H */
