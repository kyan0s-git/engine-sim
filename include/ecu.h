#ifndef ATG_ENGINE_SIM_ECU_H
#define ATG_ENGINE_SIM_ECU_H

#include <string>
#include <vector>

class Engine;
class Transmission;
class Vehicle;

// A table of values over one or two axes with bilinear interpolation
// (clamped at the edges). Values are stored row by row: v[row * cols + col]
// where columns follow x and rows follow y.
struct EcuTable {
    std::string name;
    std::string xLabel, yLabel, unit;
    std::vector<double> x;
    std::vector<double> y;          // empty: a 1D table over x
    std::vector<double> v;

    int cols() const { return static_cast<int>(x.size()); }
    int rows() const { return y.empty() ? 1 : static_cast<int>(y.size()); }
    double &at(int row, int col) { return v[static_cast<size_t>(row) * cols() + col]; }
    double at(int row, int col) const { return v[static_cast<size_t>(row) * cols() + col]; }

    void resize(const std::vector<double> &xAxis, const std::vector<double> &yAxis, double value);
    double sample(double xq, double yq = 0.0) const;

    // Fractional index of a value along an axis (for drawing the operating
    // point on a map)
    static double position(const std::vector<double> &axis, double value);
};

// Engine control unit: replaces the script's fixed ignition curve, mixture
// and boost with maps over engine speed and load (manifold pressure), and
// adds rev limiters, launch control, idle speed control, anti-lag, overboost
// protection and an automatic gearbox. Runs every simulation step.
class Ecu {
    public:
        enum class Limiter {
            SparkCut,       // hard cut (the script's behaviour)
            Soft            // timing retard over a band below the limit, then cut
        };

        struct Settings {
            bool enabled = true;

            // Maps (degrees BTDC, lambda, gauge boost in kPa)
            bool useIgnitionMap = true;
            bool useFuelMap = true;
            bool useBoostMap = true;
            EcuTable ignition;
            EcuTable lambda;
            EcuTable boost;

            // Limiters
            Limiter limiter = Limiter::SparkCut;
            double revLimit = 7000;         // [rpm]
            double softBand = 300;          // [rpm]
            double cutTime = 0.1;           // [s]
            bool launchControl = false;
            double launchRpm = 4000;
            bool overboostCut = true;
            double overboostMargin = 35;    // [kPa] above the target

            // Idle speed control (air bypass)
            bool idleControl = true;
            double idleRpm = 850;

            // Anti-lag: retarded ignition and extra air off throttle keep a
            // turbocharger spinning
            bool antiLag = false;
            double antiLagRetard = 20;      // [degrees after TDC]
            double antiLagAir = 0.12;       // throttle opening
            double antiLagMinRpm = 2500;

            // Automatic gearbox
            bool automatic = false;
            EcuTable upshift;               // rpm over throttle [%]
            EcuTable downshift;
            std::vector<double> wotUpshift; // per gear, optimized; 0 = table
            double shiftTime = 0.25;        // [s]
            double launchClutchRpm = 1500;  // clutch fully in above idle + this
        };

        struct State {
            double rpm = 0;
            double load = 0;                // manifold pressure [kPa]
            double throttle = 0;
            double advance = 0;             // [degrees BTDC]
            double lambda = 1;
            double boostTarget = 0;         // [kPa gauge]
            double idleAir = 0;
            bool limiting = false;
            bool softLimiting = false;
            bool launchActive = false;
            bool antiLagActive = false;
            bool overboost = false;
            bool shifting = false;
            int gear = -1;
            std::string lastShift;
        };

    public:
        Ecu();

        // Builds the stock maps from the engine (its timing curve, mixture,
        // boost and rev limit)
        void initialize(Engine *engine, Transmission *transmission, Vehicle *vehicle);
        void reset() { m_settings = m_stock; m_shiftTimer = 0; m_idleAir = 0; m_boostFiltered = 0; }

        void update(double dt);

        Settings &settings() { return m_settings; }
        const Settings &settings() const { return m_settings; }
        const Settings &stock() const { return m_stock; }
        const State &state() const { return m_state; }
        bool isInitialized() const { return m_engine != nullptr; }

        // Shift points that keep the wheel torque highest at full throttle,
        // from a torque curve sampled at `rpm` (one value per entry)
        void optimizeShiftPoints(const std::vector<double> &rpm, const std::vector<double> &torque);

        // Manual shift request (sequential): up (+1) or down (-1)
        void requestShift(int direction);

        // Tune files: key=value text with the tables as comma separated rows
        std::string serialize() const;
        bool deserialize(const std::string &text, std::string *error);

        // Ready-made adjustments of the stock tune
        void applyPreset(const std::string &name);

    protected:
        void updateGearbox(double dt, double rpm, double throttle, double vehicleSpeed);
        double launchClutch(double dt, double rpm, double throttle);

        Engine *m_engine;
        Transmission *m_transmission;
        Vehicle *m_vehicle;

        Settings m_settings;
        Settings m_stock;
        State m_state;

        double m_scriptAdvanceOffset;
        double m_idleAir;
        double m_shiftTimer;
        int m_shiftTarget;
        double m_manualHold;            // automatic shifting paused after a manual shift [s]
        double m_timeInGear;
        bool m_idleArmed = false;
        double m_overboostTimer;
        double m_boostFiltered = 0;     // [kPa]
        double m_launchClutch = 0;
        double m_dashpot = 0;           // extra idle air off throttle
        double m_lastThrottle = 0;
};

#endif /* ATG_ENGINE_SIM_ECU_H */
