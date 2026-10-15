#include "../include/engine_design.h"

#include "../include/constants.h"

// Well-known production and racing engines as designs. The dimensions,
// compression ratios, redlines and boost levels follow published figures;
// the cam timing, port flow and masses are typical values for each type of
// engine rather than exact specifications.

namespace {

    constexpr double Deg = constants::pi / 180.0;
    constexpr double mm = 0.001;
    constexpr double psi = 6894.757;

    EngineDesign base(const char *name, const char *description) {
        EngineDesign d;
        d.name = name;
        d.description = description;
        d.engineering = true;
        return d;
    }

    void cams(EngineDesign &d,
        double intakeDuration, double exhaustDuration,
        double intakeLift, double exhaustLift,
        double intakeCenter, double exhaustCenter)
    {
        d.intakeDuration = intakeDuration * Deg;
        d.exhaustDuration = exhaustDuration * Deg;
        d.intakeLift = intakeLift * mm;
        d.exhaustLift = exhaustLift * mm;
        d.intakeLobeCenter = intakeCenter * Deg;
        d.exhaustLobeCenter = exhaustCenter * Deg;
    }

    void timing(EngineDesign &d, double idle, double max, double rpm) {
        d.idleAdvance = idle * Deg;
        d.maxAdvance = max * Deg;
        d.maxAdvanceRpm = rpm;
    }

    std::vector<EngineDesign> makePresets() {
        std::vector<EngineDesign> presets;

        {
            EngineDesign d = base("Honda K24",
                "2.4 L DOHC i-VTEC inline four (K24A2 class), about 200 hp at 7000 rpm.");
            d.layout = EngineDesign::Layout::Inline;
            d.cylinders = 4;
            d.bore = 87 * mm; d.stroke = 99 * mm; d.rodLength = 152 * mm;
            d.compressionRatio = 10.5;
            d.valvesPerCylinder = 4;
            cams(d, 228, 222, 11.5, 10.5, 110, 115);
            timing(d, 12, 34, 4500);
            d.redline = 7200;
            d.runnerLength = 0.33; d.primaryLength = 0.45;
            d.exhaustLength = 2.6; d.exhaustDiameter = 60 * mm;
            d.flywheelMass = 7.5;
            d.lambda = 0.95;
            d.exhaustSound = "mild_exhaust_0";
            d.vehicleMass = 1450;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("GM LS3 V8",
                "6.2 L pushrod V8 with a cross-plane crank, about 430 hp at 5900 rpm.");
            d.layout = EngineDesign::Layout::V;
            d.cylinders = 8;
            d.bankAngle = 90 * Deg;
            // GM 1-8-7-2-6-5-4-3 (odd cylinders on the left bank)
            d.firingOrderText = "1-8-4-5-7-3-6-2";
            d.bore = 103.25 * mm; d.stroke = 92 * mm; d.rodLength = 154 * mm;
            d.compressionRatio = 10.7;
            d.valvetrain = "pushrod_roller";
            d.valvesPerCylinder = 2;
            cams(d, 204, 211, 14.0, 13.4, 113, 121);
            timing(d, 15, 28, 4000);
            d.redline = 6600;
            d.runnerLength = 0.24; d.primaryLength = 0.75;
            d.exhaustLength = 3.0; d.exhaustDiameter = 70 * mm;
            d.flywheelMass = 12;
            d.lambda = 0.92;
            d.exhaustSound = "mild_exhaust_0";
            d.vehicleMass = 1650;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("Toyota 2JZ-GTE",
                "3.0 L inline six with sequential twin turbochargers, about 320 hp at 5600 rpm.");
            d.layout = EngineDesign::Layout::Inline;
            d.cylinders = 6;
            d.bore = 86 * mm; d.stroke = 86 * mm; d.rodLength = 142 * mm;
            d.compressionRatio = 8.5;
            d.valvesPerCylinder = 4;
            cams(d, 205, 205, 8.3, 8.4, 110, 112);
            timing(d, 10, 24, 4000);
            d.redline = 6800;
            d.induction = EngineDesign::Induction::SequentialTurbo;
            d.boost = 11 * psi;
            d.turbineAr = 1.0;
            d.intercoolerEffectiveness = 0.7;
            d.runnerLength = 0.3; d.primaryLength = 0.35;
            d.exhaustLength = 2.8; d.exhaustDiameter = 76 * mm;
            d.flywheelMass = 8.5;
            d.lambda = 0.85;
            d.exhaustSound = "minimal_muffling_01";
            d.vehicleMass = 1510;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("BMW B58",
                "3.0 L inline six with a single twin-scroll turbocharger, about 380 hp at 6500 rpm.");
            d.layout = EngineDesign::Layout::Inline;
            d.cylinders = 6;
            d.bore = 82 * mm; d.stroke = 94.6 * mm; d.rodLength = 144.35 * mm;
            d.compressionRatio = 11.0;
            d.valvesPerCylinder = 4;
            cams(d, 212, 216, 9.9, 9.7, 112, 110);
            timing(d, 10, 26, 4000);
            d.redline = 7000;
            d.induction = EngineDesign::Induction::Turbo;
            d.boost = 16 * psi;
            d.turbineAr = 0.55;
            d.intercoolerEffectiveness = 0.8;
            d.runnerLength = 0.25; d.primaryLength = 0.3;
            d.exhaustLength = 3.0; d.exhaustDiameter = 70 * mm;
            d.flywheelMass = 9;
            d.lambda = 0.88;
            d.exhaustSound = "mild_exhaust_0_reverb";
            d.vehicleMass = 1600;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("F1 2026 V6 turbo",
                "1.6 L 90 degree V6 to the 2026 Formula 1 rules: 80 mm bore, 16:1 compression, "
                "15,000 rpm limit, single turbocharger without the MGU-H (combustion engine only).");
            d.layout = EngineDesign::Layout::V;
            d.cylinders = 6;
            d.bankAngle = 90 * Deg;
            d.bore = 80 * mm; d.stroke = 53.05 * mm; d.rodLength = 110 * mm;
            d.compressionRatio = 16.0;
            d.valvesPerCylinder = 4;
            cams(d, 290, 285, 15.0, 13.5, 104, 108);
            timing(d, 20, 38, 8000);
            d.idleThrottle = 0.97;
            d.redline = 15000;
            d.induction = EngineDesign::Induction::Turbo;
            // The 2026 fuel energy flow limit holds the engine to about
            // 400 kW; the boost is set for that power
            d.boost = 2.0e5;
            d.turbineAr = 0.85;
            d.ballBearings = true;
            d.intercoolerEffectiveness = 0.85;
            d.runnerLength = 0.12; d.primaryLength = 0.35;
            d.exhaustLength = 1.0; d.exhaustDiameter = 65 * mm;
            d.flywheelMass = 1.0;
            d.lambda = 1.0;
            d.exhaustSound = "minimal_muffling_03";
            d.vehicleMass = 800;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("Chevrolet small-block 350",
                "5.7 L pushrod V8 (L48 class) with a mild cam and a four-barrel carburetor.");
            d.layout = EngineDesign::Layout::V;
            d.cylinders = 8;
            d.bankAngle = 90 * Deg;
            // GM 1-8-4-3-6-5-7-2 (odd cylinders on the left bank)
            d.firingOrderText = "1-8-6-2-7-3-4-5";
            d.bore = 101.6 * mm; d.stroke = 88.39 * mm; d.rodLength = 144.8 * mm;
            d.compressionRatio = 8.5;
            d.valvetrain = "pushrod";
            d.valvesPerCylinder = 2;
            cams(d, 195, 202, 9.9, 10.4, 108, 116);
            timing(d, 8, 32, 3500);
            d.redline = 5200;
            d.runnerLength = 0.2; d.primaryLength = 0.5;
            d.exhaustLength = 3.2; d.exhaustDiameter = 64 * mm;
            d.flywheelMass = 14;
            d.lambda = 0.95;
            d.exhaustSound = "real_engine_0";
            d.vehicleMass = 1600;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("Chrysler 426 Hemi",
                "7.0 L street Hemi V8 with hemispherical chambers and dual four-barrels, about 425 hp.");
            d.layout = EngineDesign::Layout::V;
            d.cylinders = 8;
            d.bankAngle = 90 * Deg;
            // Chrysler 1-8-4-3-6-5-7-2 (odd cylinders on the left bank)
            d.firingOrderText = "1-8-6-2-7-3-4-5";
            d.bore = 107.95 * mm; d.stroke = 95.25 * mm; d.rodLength = 174.2 * mm;
            d.compressionRatio = 10.25;
            d.valvetrain = "pushrod";
            d.valvesPerCylinder = 2;
            cams(d, 230, 232, 12.2, 12.2, 110, 114);
            timing(d, 10, 34, 3500);
            d.redline = 6000;
            d.runnerLength = 0.22; d.primaryLength = 0.8;
            d.exhaustLength = 3.0; d.exhaustDiameter = 64 * mm;
            d.flywheelMass = 15;
            d.lambda = 0.9;
            d.exhaustSound = "minimal_muffling_01";
            d.vehicleMass = 1650;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("Mazda 13B-REW",
                "1.3 L twin-rotor Wankel with sequential twin turbochargers (RX-7 FD), about 255 hp at 6500 rpm.");
            d.layout = EngineDesign::Layout::Rotary;
            d.rotors = 2;
            d.eccentricity = 15 * mm; d.generatingRadius = 105 * mm; d.rotorWidth = 80 * mm;
            d.compressionRatio = 9.0;
            d.intakeOpen = 9 * Deg; d.intakeClose = 87 * Deg;
            d.exhaustOpen = 90 * Deg; d.exhaustClose = 6 * Deg;
            timing(d, 5, 25, 4000);
            d.redline = 8000;
            d.induction = EngineDesign::Induction::SequentialTurbo;
            d.boost = 10 * psi;
            d.turbineAr = 0.9;
            d.intercoolerEffectiveness = 0.7;
            d.runnerLength = 0.3; d.primaryLength = 0.3;
            d.exhaustLength = 2.5; d.exhaustDiameter = 76 * mm;
            d.flywheelMass = 8;
            d.lambda = 0.85;
            d.engineering = false;
            d.exhaustSound = "minimal_muffling_02";
            d.vehicleMass = 1280;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("Porsche M97.74",
                "4.0 L flat six of the 997 GT3 RS 4.0, about 500 hp at 8250 rpm.");
            d.layout = EngineDesign::Layout::Flat;
            d.cylinders = 6;
            d.firingOrderText = "1-6-2-4-3-5";
            d.bore = 102.7 * mm; d.stroke = 80.4 * mm; d.rodLength = 130 * mm;
            d.compressionRatio = 12.6;
            d.valvesPerCylinder = 4;
            cams(d, 250, 245, 12.3, 10.8, 105, 110);
            timing(d, 12, 36, 6500);
            d.redline = 8500;
            d.runnerLength = 0.3; d.primaryLength = 0.6;
            d.exhaustLength = 2.2; d.exhaustDiameter = 70 * mm;
            d.flywheelMass = 5;
            d.lambda = 0.9;
            d.exhaustSound = "sharp_0";
            d.vehicleMass = 1360;
            presets.push_back(d);
        }

        {
            EngineDesign d = base("Volkswagen W12",
                "6.0 L W12: two narrow-angle VR6 banks 72 degrees apart (Phaeton / Bentley class).");
            d.layout = EngineDesign::Layout::W;
            d.wBanks = 4;
            d.cylinders = 12;
            d.bankAngle = 72 * Deg;
            d.vrAngle = 15 * Deg;
            d.bore = 84 * mm; d.stroke = 90.2 * mm; d.rodLength = 164 * mm;
            d.compressionRatio = 10.75;
            d.valvesPerCylinder = 4;
            cams(d, 215, 210, 10.5, 9.5, 112, 112);
            timing(d, 12, 32, 4000);
            d.redline = 6200;
            d.runnerLength = 0.35; d.primaryLength = 0.4;
            d.exhaustLength = 3.2; d.exhaustDiameter = 65 * mm;
            d.flywheelMass = 10;
            d.lambda = 0.95;
            d.vehicleMass = 2300;
            presets.push_back(d);
        }

        for (EngineDesign &d : presets) d.validate();
        return presets;
    }

} /* namespace */

const std::vector<EngineDesign> &builtinEnginePresets() {
    static const std::vector<EngineDesign> presets = makePresets();
    return presets;
}
