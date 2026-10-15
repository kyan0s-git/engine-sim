#ifndef ATG_ENGINE_SIM_ENGINE_DESIGN_H
#define ATG_ENGINE_SIM_ENGINE_DESIGN_H

#include <string>
#include <vector>

// Parametric description of an engine. generateEngineScript() turns it into
// an engine script (with a vehicle and transmission) that the application can
// load like any other script. All values are in SI units (m, m^3, rad, kg,
// Pa) unless noted.
struct EngineDesign {
    enum class Layout {
        Inline,
        V,
        Flat,           // boxer
        W,              // three banks (broad arrow) or four (two narrow V's)
        Rotary          // Wankel
    };

    enum class Induction {
        Natural,
        Turbo,
        TwinTurbo,
        SequentialTurbo,
        Roots,
        TwinScrew,
        Centrifugal
    };

    std::string name = "Custom engine";

    // Shown in the engine library
    std::string description;

    // Configuration
    Layout layout = Layout::Inline;
    int cylinders = 4;
    double bankAngle = 1.5707963267948966;      // V: between the banks; W: between the outer banks [rad]
    int wBanks = 4;                             // W engines: 3 or 4 banks
    double vrAngle = 0.2617993877991494;        // four-bank W: between the banks of each half [rad]

    // Firing order as cylinder numbers separated by dashes ("1-8-7-2-6-5-4-3");
    // empty for an even firing order chosen automatically. Cylinders are
    // numbered bank by bank: the first bank holds 1..n/banks.
    std::string firingOrderText;

    // Geometry (piston engines)
    double bore = 0.086;
    double stroke = 0.086;
    double rodLength = 0.145;
    double compressionRatio = 10.5;

    // Rotary engines
    int rotors = 2;
    double eccentricity = 0.015;
    double generatingRadius = 0.105;
    double rotorWidth = 0.080;

    // Port timing of rotary engines (eccentric shaft angles)
    double intakeOpen = 0.1570796;              // before TDC [rad]
    double intakeClose = 1.5184364;             // after BDC
    double exhaustOpen = 1.5707963;             // before BDC
    double exhaustClose = 0.1047198;            // after TDC

    // Valvetrain (crank angles)
    std::string valvetrain = "dohc_bucket";     // friction model: dohc_bucket, sohc_finger, sohc_rocker, pushrod (+ "_roller")
    int valvesPerCylinder = 4;
    double intakeDuration = 4.10;               // at 0.050" lift [rad]
    double exhaustDuration = 4.10;
    double intakeLift = 0.0105;                 // [m]
    double exhaustLift = 0.0095;
    double intakeLobeCenter = 1.92;             // after TDC [rad]
    double exhaustLobeCenter = 1.92;            // before TDC [rad]

    // Intake and exhaust
    double throttleFlow = 0;                    // [cfm at 1.5 inHg], 0 = sized from displacement
    double plenumVolume = 0;                    // [m^3], 0 = displacement
    double runnerLength = 0.25;
    double primaryLength = 0.5;
    double exhaustLength = 2.5;
    double exhaustDiameter = 0.055;
    std::string exhaustSound = "default_0";     // see exhaustSounds()

    // Forced induction
    Induction induction = Induction::Natural;
    double boost = 0.8e5;                       // target (gauge) [Pa]
    double turbineAr = 0.64;
    double compressorScale = 1.0;               // compressor size relative to the automatic sizing
    bool ballBearings = false;
    bool blowoffValve = true;
    double intercoolerEffectiveness = 0.7;
    double driveRatio = 0;                      // superchargers, 0 = automatic

    // Ignition (crank angles before TDC)
    double idleAdvance = 0.21;                  // 12 degrees
    double maxAdvance = 0.61;                   // 35 degrees
    double maxAdvanceRpm = 4500;

    // Operating range [rpm]
    double redline = 7000;
    double idleThrottle = 0.993;                // idle throttle plate position

    // Inertia
    double flywheelMass = 9.0;                  // [kg]

    // Physics
    bool engineering = true;
    std::string fuel = "gasoline";
    double lambda = 1.0;

    // Vehicle
    double vehicleMass = 1400;                  // [kg]

    // Derived quantities
    bool isRotary() const { return layout == Layout::Rotary; }
    bool isBoosted() const { return induction != Induction::Natural; }
    bool isTurbocharged() const;
    int bankCount() const;
    int cylindersPerBank() const { return cylinders / bankCount(); }
    std::vector<double> bankAngles() const;     // [rad] from vertical
    double displacement() const;
    double cylinderDisplacement() const;        // rotary: per face

    // Clamps values to sensible ranges; returns false if something had to be
    // changed
    bool validate();

    // Firing order (1-based cylinder numbers) used by the generated script
    std::vector<int> firingOrder() const;

    // Parses a firing order; returns false unless it lists every cylinder
    // from 1 to cylinders exactly once
    static bool parseFiringOrder(const std::string &text, int cylinders, std::vector<int> *order);

    static const char *layoutName(Layout layout);
    static const char *inductionName(Induction induction);

    // Exhaust sounds: script names and descriptions
    static const std::vector<std::pair<std::string, std::string>> &exhaustSounds();
};

// Script text for the design
std::string generateEngineScript(const EngineDesign &design);

// Reads the design stored in a generated script's header comment; returns
// false if the script wasn't generated by generateEngineScript()
bool readEngineDesign(const std::string &scriptText, EngineDesign *design);

// Short identifier made of letters, digits and underscores
std::string designFileName(const std::string &name);

// Engines that come with the application, as designs
const std::vector<EngineDesign> &builtinEnginePresets();

#endif /* ATG_ENGINE_SIM_ENGINE_DESIGN_H */
