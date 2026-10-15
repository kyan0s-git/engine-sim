#include "../include/engine_design.h"

#include "../include/constants.h"
#include "../include/crankshaft.h"
#include "../include/wankel_rotor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>

namespace {

    constexpr double Pi = constants::pi;
    constexpr double Deg = Pi / 180.0;
    constexpr double Atmosphere = 101325.0;
    constexpr double AirDensity = 1.18;         // [kg/m^3] at 25 C

    // The script language has no exponent notation
    std::string num(double v, int precision = 6) {
        std::ostringstream ss;
        ss.precision(precision);
        ss << v;
        std::string text = ss.str();
        if (text.find_first_of("eE") == std::string::npos) return text;

        std::ostringstream fixed;
        fixed.setf(std::ios::fixed);
        fixed.precision(std::min(precision + 12, 24));
        fixed << v;
        text = fixed.str();
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
        return text;
    }

    // Values stored in the design header round-trip exactly
    std::string exact(double v) {
        return num(v, 17);
    }

    std::string layoutKey(EngineDesign::Layout layout) {
        switch (layout) {
        case EngineDesign::Layout::V: return "v";
        case EngineDesign::Layout::Flat: return "flat";
        case EngineDesign::Layout::W: return "w";
        case EngineDesign::Layout::Rotary: return "rotary";
        case EngineDesign::Layout::Inline:
        default: return "inline";
        }
    }

    std::string inductionKey(EngineDesign::Induction induction) {
        switch (induction) {
        case EngineDesign::Induction::Turbo: return "turbo";
        case EngineDesign::Induction::TwinTurbo: return "twin_turbo";
        case EngineDesign::Induction::SequentialTurbo: return "sequential_turbo";
        case EngineDesign::Induction::Roots: return "roots";
        case EngineDesign::Induction::TwinScrew: return "twin_screw";
        case EngineDesign::Induction::Centrifugal: return "centrifugal";
        case EngineDesign::Induction::Natural:
        default: return "natural";
        }
    }

    // Firing orders of common inline engines (positions along the crank)
    std::vector<int> inlineFiringOrder(int n) {
        switch (n) {
        case 2: return { 1, 2 };
        case 3: return { 1, 2, 3 };
        case 4: return { 1, 3, 4, 2 };
        case 5: return { 1, 2, 4, 5, 3 };
        case 6: return { 1, 5, 3, 6, 2, 4 };
        case 8: return { 1, 6, 2, 5, 8, 3, 7, 4 };
        default: {
            std::vector<int> order;
            for (int i = 1; i <= n; ++i) order.push_back(i);
            return order;
        }
        }
    }

    std::string sanitize(const std::string &s, const std::string &fallback) {
        std::string result;
        for (const char c : s) {
            if (c == '"' || c == '\\' || c == '\n' || c == '\r') continue;
            result += c;
        }

        return result.empty() ? fallback : result;
    }

    // Key-value pairs stored in the generated script's header
    std::map<std::string, std::string> serialize(const EngineDesign &d) {
        return {
            { "name", d.name },
            { "description", d.description },
            { "layout", layoutKey(d.layout) },
            { "cylinders", std::to_string(d.cylinders) },
            { "bank_angle", exact(d.bankAngle) },
            { "w_banks", std::to_string(d.wBanks) },
            { "vr_angle", exact(d.vrAngle) },
            { "firing_order", d.firingOrderText },
            { "bore", exact(d.bore) },
            { "stroke", exact(d.stroke) },
            { "rod_length", exact(d.rodLength) },
            { "compression_ratio", exact(d.compressionRatio) },
            { "rotors", std::to_string(d.rotors) },
            { "eccentricity", exact(d.eccentricity) },
            { "generating_radius", exact(d.generatingRadius) },
            { "rotor_width", exact(d.rotorWidth) },
            { "intake_open", exact(d.intakeOpen) },
            { "intake_close", exact(d.intakeClose) },
            { "exhaust_open", exact(d.exhaustOpen) },
            { "exhaust_close", exact(d.exhaustClose) },
            { "valvetrain", d.valvetrain },
            { "valves_per_cylinder", std::to_string(d.valvesPerCylinder) },
            { "intake_duration", exact(d.intakeDuration) },
            { "exhaust_duration", exact(d.exhaustDuration) },
            { "intake_lift", exact(d.intakeLift) },
            { "exhaust_lift", exact(d.exhaustLift) },
            { "intake_lobe_center", exact(d.intakeLobeCenter) },
            { "exhaust_lobe_center", exact(d.exhaustLobeCenter) },
            { "throttle_flow", exact(d.throttleFlow) },
            { "plenum_volume", exact(d.plenumVolume) },
            { "runner_length", exact(d.runnerLength) },
            { "primary_length", exact(d.primaryLength) },
            { "exhaust_length", exact(d.exhaustLength) },
            { "exhaust_diameter", exact(d.exhaustDiameter) },
            { "exhaust_sound", d.exhaustSound },
            { "induction", inductionKey(d.induction) },
            { "boost", exact(d.boost) },
            { "turbine_ar", exact(d.turbineAr) },
            { "compressor_scale", exact(d.compressorScale) },
            { "ball_bearings", d.ballBearings ? "1" : "0" },
            { "blowoff_valve", d.blowoffValve ? "1" : "0" },
            { "intercooler", exact(d.intercoolerEffectiveness) },
            { "drive_ratio", exact(d.driveRatio) },
            { "idle_advance", exact(d.idleAdvance) },
            { "max_advance", exact(d.maxAdvance) },
            { "max_advance_rpm", exact(d.maxAdvanceRpm) },
            { "redline", exact(d.redline) },
            { "idle_throttle", exact(d.idleThrottle) },
            { "flywheel_mass", exact(d.flywheelMass) },
            { "engineering", d.engineering ? "1" : "0" },
            { "fuel", d.fuel },
            { "lambda", exact(d.lambda) },
            { "vehicle_mass", exact(d.vehicleMass) }
        };
    }

    // Swept volume of one face of a rotor (3 sqrt(3) e R B)
    double rotorFaceDisplacement(const EngineDesign &d) {
        return 3 * std::sqrt(3.0) * d.eccentricity * d.generatingRadius * d.rotorWidth;
    }

    // Smallest chamber volume of a rotor without a recess in its faces
    double rotorMinimumVolume(const EngineDesign &d) {
        Crankshaft::Parameters cp;
        cp.mass = 1;
        cp.flywheelMass = 0;
        cp.momentOfInertia = 1;
        cp.crankThrow = d.eccentricity;
        cp.tdc = Pi / 2;
        cp.rodJournals = 1;

        Crankshaft crankshaft;
        crankshaft.initialize(cp);

        WankelRotor::Parameters rp;
        rp.crankshaft = &crankshaft;
        rp.generatingRadius = d.generatingRadius;
        rp.depth = d.rotorWidth;
        rp.calculationQuality = 512;
        rp.housingResolution = 128;

        WankelRotor rotor;
        rotor.initialize(rp);
        const double volume = rotor.getMinimumVolume();
        crankshaft.destroy();

        return volume;
    }

    // Engine speed in revolutions per second at which the charge of the
    // whole displacement is drawn once
    double displacementFlow(const EngineDesign &d, double rpm) {
        // Rotary: every rotor takes one face's charge per shaft revolution
        return d.isRotary()
            ? d.displacement() * rpm / 60.0
            : d.displacement() * rpm / 120.0;
    }

    int simulationFrequency(const EngineDesign &d) {
        const double f = d.isRotary()
            ? std::max(15000.0, 1.8 * d.redline)
            : std::max(10000.0, 1.4 * d.redline);
        return static_cast<int>(std::min(std::round(f / 1000.0) * 1000.0, 30000.0));
    }

    // engine.add_forced_induction(...) for boosted designs
    std::string forcedInductionScript(const EngineDesign &d) {
        if (!d.isBoosted()) return std::string();

        const double pr = 1.0 + d.boost / Atmosphere;
        const double massFlow = 0.9 * AirDensity * pr * displacementFlow(d, d.redline);

        std::ostringstream s;
        s << "\n    // Forced induction\n";
        if (d.isTurbocharged()) {
            const char *arrangement = "single";
            double flow = 1.15 * massFlow;
            if (d.induction == EngineDesign::Induction::TwinTurbo) {
                arrangement = "twin";
                flow /= 2;
            }
            else if (d.induction == EngineDesign::Induction::SequentialTurbo) {
                arrangement = "sequential";
                flow *= 0.6;
            }

            flow *= d.compressorScale;
            const double diameter = std::clamp(0.060 * std::sqrt(flow / 0.30), 0.030, 0.120);
            const double inertia = 3.0e-5 * std::pow(diameter / 0.060, 5.0);

            s << "    engine.add_forced_induction(\n"
              << "        turbocharger(\n"
              << "            arrangement: \"" << arrangement << "\",\n"
              << "            compressor_diameter: " << num(diameter * 1000) << " * units.mm,\n"
              << "            compressor_flow: " << num(flow) << " * units.kg / units.sec,\n"
              << "            turbine_ar: " << num(d.turbineAr) << ",\n"
              << "            inertia: " << num(inertia, 4) << ",\n"
              << "            bearings: \"" << (d.ballBearings ? "ball" : "journal") << "\",\n"
              << "            boost: " << num(d.boost / 1000.0) << " * units.kPa,\n"
              << "            blowoff_valve: " << (d.blowoffValve ? "true" : "false") << ",\n"
              << "            intercooler_effectiveness: " << num(d.intercoolerEffectiveness) << ",\n"
              << "            charge_volume: " << num(std::max(3.0, 2.0 * d.displacement() * 1000)) << " * units.L,\n"
              << "            sequential_speed: " << num(0.55 * d.redline) << " * units.rpm\n"
              << "        )\n"
              << "    )\n";
            return s.str();
        }

        const double crankRevFlow = displacementFlow(d, 60.0);     // per crank revolution [m^3]
        if (d.induction == EngineDesign::Induction::Centrifugal) {
            // Impeller tip speed for a little more than the target at redline
            const double flow = 1.1 * massFlow * d.compressorScale;
            const double diameter = std::clamp(0.075 * std::sqrt(flow / 0.40), 0.040, 0.140);
            const double tipSpeed = std::sqrt(
                (std::pow(1.1 * pr, 1 / 3.5) - 1.0) * 1005.0 * 298.15 / (0.75 * 0.9));
            const double ratio = (d.driveRatio > 0)
                ? d.driveRatio
                : tipSpeed / (diameter / 2) / (d.redline * 2 * Pi / 60.0);

            s << "    engine.add_forced_induction(\n"
              << "        supercharger(\n"
              << "            type: \"centrifugal\",\n"
              << "            drive_ratio: " << num(ratio) << ",\n"
              << "            compressor_diameter: " << num(diameter * 1000) << " * units.mm,\n"
              << "            compressor_flow: " << num(flow) << " * units.kg / units.sec,\n"
              << "            blowoff_valve: " << (d.blowoffValve ? "true" : "false") << ",\n"
              << "            intercooler_effectiveness: " << num(d.intercoolerEffectiveness) << ",\n"
              << "            charge_volume: " << num(std::max(3.0, 2.0 * d.displacement() * 1000)) << " * units.L\n"
              << "        )\n"
              << "    )\n";
            return s.str();
        }

        // Positive displacement: sized for the target boost with the rotor
        // leakage and the hotter charge
        const bool roots = (d.induction == EngineDesign::Induction::Roots);
        const double ratio = (d.driveRatio > 0) ? d.driveRatio : 2.2;
        const double margin = roots ? 1.45 : 1.2;
        const double displacement = crankRevFlow * pr * margin / ratio * d.compressorScale;

        s << "    engine.add_forced_induction(\n"
          << "        supercharger(\n"
          << "            type: \"" << (roots ? "roots" : "twin_screw") << "\",\n"
          << "            drive_ratio: " << num(ratio) << ",\n"
          << "            displacement: " << num(displacement * 1000) << " * units.L,\n"
          << "            intercooler_effectiveness: " << num(d.intercoolerEffectiveness) << ",\n"
          << "            charge_volume: " << num(std::max(2.0, 1.5 * d.displacement() * 1000)) << " * units.L\n"
          << "        )\n"
          << "    )\n";
        return s.str();
    }

    void vehicleScript(std::ostringstream &s, const EngineDesign &d, const char *engineNode) {
        s << "private node designed_vehicle {\n"
          << "    alias output __out:\n"
          << "        vehicle(\n"
          << "            mass: " << num(d.vehicleMass) << " * units.kg,\n"
          << "            drag_coefficient: 0.3,\n"
          << "            cross_sectional_area: 2.2 * units.m2,\n"
          << "            diff_ratio: 3.7,\n"
          << "            tire_radius: 0.31 * units.m,\n"
          << "            rolling_resistance: " << num(0.015 * d.vehicleMass * 9.81) << "\n"
          << "        );\n"
          << "}\n\n"
          << "private node designed_transmission {\n"
          << "    alias output __out:\n"
          << "        transmission(max_clutch_torque: " << num(std::max(200.0, 300.0 * d.displacement() * 1000 * (1 + d.boost / Atmosphere * d.isBoosted()))) << " * units.lb_ft)\n"
          << "            .add_gear(3.6)\n"
          << "            .add_gear(2.2)\n"
          << "            .add_gear(1.5)\n"
          << "            .add_gear(1.15)\n"
          << "            .add_gear(0.92)\n"
          << "            .add_gear(0.75);\n"
          << "}\n\n"
          << "public node main {\n"
          << "    run(\n"
          << "        engine: " << engineNode << "(),\n"
          << "        transmission: designed_transmission(),\n"
          << "        vehicle: designed_vehicle()\n"
          << "    )\n"
          << "}\n\n"
          << "main()\n";
    }

    void timingCurveScript(std::ostringstream &s, const EngineDesign &d) {
        // Script functions interpolate between neighbouring samples within
        // the function's resolution, so the curve is sampled at that spacing
        s << "\n    function timing_curve(500 * units.rpm)\n"
          << "    timing_curve\n";
        const double lastRpm = std::max(d.redline, d.maxAdvanceRpm) + 1500;
        for (double rpm = 0; rpm <= lastRpm; rpm += 500) {
            const double t = std::clamp((rpm - 1000) / std::max(d.maxAdvanceRpm - 1000, 1.0), 0.0, 1.0);
            const double advance = d.idleAdvance + t * (d.maxAdvance - d.idleAdvance);
            s << "        .add_sample(" << num(rpm) << " * units.rpm, " << num(advance / Deg) << " * units.deg)\n";
        }
    }

    void headerScript(std::ostringstream &s, const EngineDesign &d, const char *cycle) {
        s << "// Generated by the Engine Simulator engine designer. Edit the design in\n"
          << "// the application (Tools > Engine Designer) to keep this header in sync.\n"
          << "//\n";
        for (const auto &kv : serialize(d)) {
            s << "// @design " << kv.first << "=" << kv.second << "\n";
        }

        s << "\nimport \"engine_sim.mr\"\n\n"
          << "units units()\n"
          << "constants constants()\n"
          << "impulse_response_library ir_lib()\n\n"
          << "label cycle(" << cycle << ")\n\n";
    }

    std::string impulseResponse(const EngineDesign &d) {
        for (const auto &sound : EngineDesign::exhaustSounds()) {
            if (sound.first == d.exhaustSound) return "ir_lib." + sound.first;
        }

        return "ir_lib.default_0";
    }

    std::string generateRotaryScript(const EngineDesign &d) {
        const int n = d.rotors;
        const double faceVolume = rotorFaceDisplacement(d);
        const double recess = std::max(
            faceVolume / (d.compressionRatio - 1) - rotorMinimumVolume(d), 0.0);
        const double sizeScale = faceVolume / 654e-6;
        const double rotorMass = 4.5 * std::pow(d.generatingRadius / 0.105, 2.0) * (d.rotorWidth / 0.080);

        // Peripheral ports (housing angles, see WankelRotor): a port opens
        // when the face uncovers its far edge and closes when the face's
        // trailing apex passes its near edge,
        //     open = 810 deg - 3 * end, close = 1170 deg - 3 * start
        // in eccentric shaft angles from the face's compression TDC, with the
        // following TDC (overlap) at 450 deg.
        const double io = d.intakeOpen / Deg, ic = d.intakeClose / Deg;
        const double eo = d.exhaustOpen / Deg, ec = d.exhaustClose / Deg;
        const double intakeEnd = (360.0 + io) / 3.0;
        const double intakeStart = std::min((450.0 - ic) / 3.0, intakeEnd - 0.5);
        const double exhaustEnd = (630.0 + eo) / 3.0;
        const double exhaustStart = std::min((720.0 - ec) / 3.0, exhaustEnd - 0.5);

        const double displacementCi = d.displacement() / 1.6387064e-5;
        const double throttleFlow = (d.throttleFlow > 0)
            ? d.throttleFlow
            : 2.6 * displacementCi * d.redline / 3456;

        std::ostringstream s;
        headerScript(s, d, "3 * 360 * units.deg");

        s << "private node designed_wires {\n";
        for (int i = 0; i < n; ++i) {
            s << "    output wire" << (i + 1) << ": ignition_wire();\n";
        }
        s << "}\n\n";

        s << "public node designed_engine {\n"
          << "    alias output __out: engine;\n\n"
          << "    wankel_engine engine(\n"
          << "        name: \"" << d.name << "\",\n"
          << "        starter_torque: " << num(std::max(60.0, 25.0 * d.displacement() * 1000 + 30)) << " * units.lb_ft,\n"
          << "        starter_speed: 300 * units.rpm,\n"
          << "        redline: " << num(d.redline) << " * units.rpm,\n"
          << "        dyno_max_speed: " << num(d.redline) << " * units.rpm,\n"
          << "        physics_model: \"" << (d.engineering ? "engineering" : "legacy") << "\",\n"
          << "        fuel: fuel(type: \"" << d.fuel << "\"),\n"
          << "        throttle_gamma: 2.0,\n"
          << "        hf_gain: 0.01,\n"
          << "        noise: 0.8,\n"
          << "        jitter: 0.4,\n"
          << "        simulation_frequency: " << simulationFrequency(d) << "\n"
          << "    )\n\n"
          << "    designed_wires wires()\n\n"
          << "    crankshaft eccentric_shaft(\n"
          << "        throw: " << num(d.eccentricity * 1000) << " * units.mm,\n"
          << "        flywheel_mass: " << num(d.flywheelMass) << " * units.kg,\n"
          << "        mass: " << num(4.5 * n * sizeScale) << " * units.kg,\n"
          << "        friction_torque: " << num(4.0 + 2.0 * n * sizeScale) << " * units.lb_ft,\n"
          << "        moment_of_inertia:\n"
          << "            disk_moment_of_inertia(mass: " << num(4.5 * n * sizeScale) << " * units.kg, radius: 30 * units.mm)\n"
          << "            + disk_moment_of_inertia(mass: " << num(d.flywheelMass) << " * units.kg, radius: 0.15 * units.m),\n"
          << "        position_x: 0.0,\n"
          << "        position_y: 0.0,\n"
          << "        tdc: constants.pi / 2\n"
          << "    )\n\n";

        // Even firing: the rotors are spread around the eccentric shaft
        for (int i = 0; i < n; ++i) {
            s << "    rod_journal lobe" << i << "(angle: " << num(360.0 * i / n) << " * units.deg)\n";
        }
        s << "    eccentric_shaft\n";
        for (int i = 0; i < n; ++i) s << "        .add_rod_journal(lobe" << i << ")\n";
        s << "    engine.add_crankshaft(eccentric_shaft)\n\n";

        s << "    wankel_rotor rotor(\n"
          << "        mass: " << num(rotorMass) << " * units.kg,\n"
          << "        moment_of_inertia: 0.5 * " << num(rotorMass) << " * (" << num(0.8 * d.generatingRadius) << ") * (" << num(0.8 * d.generatingRadius) << "),\n"
          << "        radius: " << num(d.generatingRadius * 1000) << " * units.mm,\n"
          << "        crown_gear_radius: " << num(3 * d.eccentricity * 1000) << " * units.mm,\n"
          << "        depth: " << num(d.rotorWidth * 1000) << " * units.mm,\n"
          << "        depression_volume: " << num(recess * 1e6) << " * units.cc\n"
          << "    )\n\n"
          << "    intake intake(\n"
          << "        plenum_volume: " << num(std::max(d.plenumVolume > 0 ? d.plenumVolume * 1000 : 1.5 * d.displacement() * 1000, 0.5)) << " * units.L,\n"
          << "        plenum_cross_section_area: " << num(10.0 * std::max(1.0, sizeScale * n / 2)) << " * units.cm2,\n"
          << "        intake_flow_rate: k_carb(" << num(throttleFlow) << "),\n"
          << "        runner_flow_rate: k_carb(" << num(250.0 * sizeScale) << "),\n"
          << "        runner_length: " << num(d.runnerLength * 1000) << " * units.mm,\n"
          << "        idle_flow_rate: k_carb(0.0),\n"
          << "        idle_throttle_plate_position: " << num(d.idleThrottle) << ",\n"
          << "        velocity_decay: 0.5,\n"
          << "        lambda: " << num(d.lambda) << "\n"
          << "    )\n\n"
          << "    exhaust_system_parameters es_params(\n"
          << "        outlet_flow_rate: k_carb(" << num(350.0 * n * sizeScale) << "),\n"
          << "        primary_tube_length: " << num(d.primaryLength * 1000) << " * units.mm,\n"
          << "        primary_flow_rate: k_carb(" << num(400.0 * sizeScale) << "),\n"
          << "        velocity_decay: 1.0,\n"
          << "        collector_cross_section_area: circle_area(" << num(d.exhaustDiameter * 1000) << " * units.mm),\n"
          << "        length: " << num(d.exhaustLength * 1000) << " * units.mm\n"
          << "    )\n"
          << "    exhaust_system exhaust(es_params, audio_volume: 2.0, impulse_response: " << impulseResponse(d) << ")\n\n"
          << "    engine\n";

        for (int i = 0; i < n; ++i) {
            s << "        .add_rotor(\n"
              << "            intake: intake,\n"
              << "            exhaust_system: exhaust,\n"
              << "            rotor: rotor,\n"
              << "            rod_journal: lobe" << i << ",\n"
              << "            ignition_wire: wires.wire" << (i + 1) << ",\n"
              << "            primary_length: " << num(0.1 * (i + 1)) << " * units.m,\n"
              << "            intake_port: default_wankel_peripheral_port(\n"
              << "                " << num(intakeStart) << " * units.deg, " << num(intakeEnd) << " * units.deg, k_carb(" << num(220.0 * sizeScale) << ")),\n"
              << "            exhaust_port: default_wankel_peripheral_port(\n"
              << "                " << num(exhaustStart) << " * units.deg, " << num(exhaustEnd) << " * units.deg, k_carb(" << num(300.0 * sizeScale) << ")),\n"
              << "            intake_runner_cross_section_area: " << num(8.0 * sizeScale) << " * units.cm2,\n"
              << "            exhaust_cross_section_area: " << num(8.0 * sizeScale) << " * units.cm2,\n"
              << "            spark_plug_angle: 0 * units.deg\n"
              << "        )\n";
        }

        s << forcedInductionScript(d);
        timingCurveScript(s, d);

        // Every face fires once per rotor revolution, i.e. each rotor's plug
        // fires every shaft revolution; rotor i trails rotor 0 by its
        // journal angle
        s << "\n    ignition_module ignition(\n"
          << "        timing_curve: timing_curve,\n"
          << "        rev_limit: " << num(d.redline + 300) << " * units.rpm,\n"
          << "        limiter_duration: 0.05\n"
          << "    )\n"
          << "    ignition\n";
        for (int j = 0; j < 3; ++j) {
            for (int i = 0; i < n; ++i) {
                s << "        .connect_wire(wires.wire" << (i + 1) << ", " << num(360.0 * j + 360.0 * i / n) << " * units.deg)\n";
            }
        }
        s << "    engine.add_ignition_module(ignition)\n"
          << "}\n\n";

        vehicleScript(s, d, "designed_engine");
        return s.str();
    }

} /* namespace */

const char *EngineDesign::layoutName(Layout layout) {
    switch (layout) {
    case Layout::V: return "V";
    case Layout::Flat: return "Flat (boxer)";
    case Layout::W: return "W";
    case Layout::Rotary: return "Rotary (Wankel)";
    case Layout::Inline:
    default: return "Inline";
    }
}

const char *EngineDesign::inductionName(Induction induction) {
    switch (induction) {
    case Induction::Turbo: return "Turbocharger";
    case Induction::TwinTurbo: return "Twin turbochargers";
    case Induction::SequentialTurbo: return "Sequential turbochargers";
    case Induction::Roots: return "Roots supercharger";
    case Induction::TwinScrew: return "Twin-screw supercharger";
    case Induction::Centrifugal: return "Centrifugal supercharger";
    case Induction::Natural:
    default: return "Naturally aspirated";
    }
}

const std::vector<std::pair<std::string, std::string>> &EngineDesign::exhaustSounds() {
    static const std::vector<std::pair<std::string, std::string>> sounds = {
        { "default_0", "Smooth (default)" },
        { "mild_exhaust_0", "Mild exhaust" },
        { "mild_exhaust_0_reverb", "Mild exhaust with reverb" },
        { "minimal_muffling_01", "Minimal muffling 1" },
        { "minimal_muffling_02", "Minimal muffling 2" },
        { "minimal_muffling_03", "Minimal muffling 3" },
        { "sharp_0", "Sharp" },
        { "real_engine_0", "Recorded engine 1" },
        { "real_engine_1", "Recorded engine 2" },
        { "real_engine_2", "Recorded engine 3" }
    };

    return sounds;
}

bool EngineDesign::isTurbocharged() const {
    return induction == Induction::Turbo
        || induction == Induction::TwinTurbo
        || induction == Induction::SequentialTurbo;
}

int EngineDesign::bankCount() const {
    switch (layout) {
    case Layout::V:
    case Layout::Flat: return 2;
    case Layout::W: return (wBanks == 3) ? 3 : 4;
    case Layout::Rotary:
    case Layout::Inline:
    default: return 1;
    }
}

std::vector<double> EngineDesign::bankAngles() const {
    switch (layout) {
    case Layout::V: return { -bankAngle / 2, bankAngle / 2 };
    case Layout::Flat: return { -Pi / 2, Pi / 2 };
    case Layout::W:
        if (wBanks == 3) return { -bankAngle / 2, 0.0, bankAngle / 2 };
        return {
            -bankAngle / 2 - vrAngle / 2, -bankAngle / 2 + vrAngle / 2,
            bankAngle / 2 - vrAngle / 2, bankAngle / 2 + vrAngle / 2 };
    case Layout::Rotary:
    case Layout::Inline:
    default: return { 0.0 };
    }
}

double EngineDesign::cylinderDisplacement() const {
    if (isRotary()) return rotorFaceDisplacement(*this);
    return Pi * bore * bore / 4 * stroke;
}

double EngineDesign::displacement() const {
    if (isRotary()) return rotorFaceDisplacement(*this) * rotors;
    return cylinderDisplacement() * cylinders;
}

bool EngineDesign::validate() {
    const EngineDesign before = *this;

    if (wBanks != 3) wBanks = 4;
    switch (layout) {
    case Layout::Inline:
        cylinders = std::clamp(cylinders, 1, 12);
        break;
    case Layout::V:
    case Layout::Flat:
        cylinders = std::clamp(cylinders + (cylinders % 2), 2, 16);
        break;
    case Layout::W: {
        const int banks = bankCount();
        const int perBank = std::clamp((cylinders + banks - 1) / banks, 1, 6);
        cylinders = perBank * banks;
        break;
    }
    case Layout::Rotary:
        rotors = std::clamp(rotors, 1, 4);
        cylinders = rotors;
        break;
    }

    if (layout == Layout::Flat) bankAngle = Pi;
    bankAngle = std::clamp(bankAngle, 10 * Deg, 180 * Deg);
    vrAngle = std::clamp(vrAngle, 5 * Deg, std::min(45 * Deg, bankAngle));
    bore = std::clamp(bore, 0.02, 0.25);
    stroke = std::clamp(stroke, 0.02, 0.3);
    rodLength = std::clamp(rodLength, 1.2 * stroke, 6.0 * stroke);
    compressionRatio = std::clamp(compressionRatio, 4.0, 18.0);

    eccentricity = std::clamp(eccentricity, 0.005, 0.04);
    generatingRadius = std::clamp(generatingRadius, 4.0 * eccentricity, 12.0 * eccentricity);
    rotorWidth = std::clamp(rotorWidth, 0.02, 0.2);
    intakeOpen = std::clamp(intakeOpen, -60 * Deg, 80 * Deg);
    intakeClose = std::clamp(intakeClose, std::min(std::max(0.0, 95 * Deg - intakeOpen), 120 * Deg), 120 * Deg);
    exhaustOpen = std::clamp(exhaustOpen, 30 * Deg, 120 * Deg);
    exhaustClose = std::clamp(exhaustClose, std::max(-30 * Deg, 95 * Deg - exhaustOpen), 80 * Deg);

    valvesPerCylinder = std::clamp(valvesPerCylinder, 2, 5);
    intakeDuration = std::clamp(intakeDuration, 150 * Deg, 330 * Deg);
    exhaustDuration = std::clamp(exhaustDuration, 150 * Deg, 330 * Deg);
    intakeLift = std::clamp(intakeLift, 0.002, 0.025);
    exhaustLift = std::clamp(exhaustLift, 0.002, 0.025);
    intakeLobeCenter = std::clamp(intakeLobeCenter, 90 * Deg, 140 * Deg);
    exhaustLobeCenter = std::clamp(exhaustLobeCenter, 90 * Deg, 140 * Deg);
    throttleFlow = std::clamp(throttleFlow, 0.0, 5000.0);
    plenumVolume = std::clamp(plenumVolume, 0.0, 0.05);
    runnerLength = std::clamp(runnerLength, 0.02, 1.0);
    primaryLength = std::clamp(primaryLength, 0.05, 2.0);
    exhaustLength = std::clamp(exhaustLength, 0.2, 10.0);
    exhaustDiameter = std::clamp(exhaustDiameter, 0.02, 0.15);

    boost = std::clamp(boost, 0.1e5, 4.0e5);
    turbineAr = std::clamp(turbineAr, 0.3, 1.6);
    compressorScale = std::clamp(compressorScale, 0.4, 2.5);
    intercoolerEffectiveness = std::clamp(intercoolerEffectiveness, 0.0, 1.0);
    driveRatio = std::clamp(driveRatio, 0.0, 25.0);

    idleAdvance = std::clamp(idleAdvance, 0.0, 40 * Deg);
    maxAdvance = std::clamp(maxAdvance, idleAdvance, 60 * Deg);
    maxAdvanceRpm = std::clamp(maxAdvanceRpm, 1000.0, 15000.0);
    redline = std::clamp(redline, 1500.0, 20000.0);
    idleThrottle = std::clamp(idleThrottle, 0.9, 1.0);
    flywheelMass = std::clamp(flywheelMass, 0.5, 100.0);
    lambda = std::clamp(lambda, 0.6, 1.6);
    vehicleMass = std::clamp(vehicleMass, 100.0, 40000.0);
    name = sanitize(name, "Custom engine");
    description = sanitize(description, "");
    if (fuel.empty()) fuel = "gasoline";
    if (valvetrain.empty()) valvetrain = "dohc_bucket";

    bool knownSound = false;
    for (const auto &sound : exhaustSounds()) knownSound |= (sound.first == exhaustSound);
    if (!knownSound) exhaustSound = "default_0";

    std::vector<int> order;
    if (!firingOrderText.empty() && (isRotary() || !parseFiringOrder(firingOrderText, cylinders, &order))) {
        firingOrderText.clear();
    }

    return serialize(before) == serialize(*this);
}

bool EngineDesign::parseFiringOrder(const std::string &text, int cylinders, std::vector<int> *order) {
    std::vector<int> result;
    std::string token;
    auto flush = [&]() {
        if (token.empty()) return true;
        for (const char c : token) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        result.push_back(std::atoi(token.c_str()));
        token.clear();
        return true;
    };

    for (const char c : text) {
        if (c == '-' || c == ',' || c == ' ') {
            if (!flush()) return false;
        }
        else {
            token += c;
        }
    }
    if (!flush()) return false;

    if (static_cast<int>(result.size()) != cylinders) return false;
    std::vector<int> sorted = result;
    std::sort(sorted.begin(), sorted.end());
    for (int i = 0; i < cylinders; ++i) {
        if (sorted[i] != i + 1) return false;
    }

    *order = result;
    return true;
}

std::vector<int> EngineDesign::firingOrder() const {
    if (isRotary()) {
        std::vector<int> order;
        for (int i = 1; i <= rotors; ++i) order.push_back(i);
        return order;
    }

    std::vector<int> custom;
    if (!firingOrderText.empty() && parseFiringOrder(firingOrderText, cylinders, &custom)) {
        return custom;
    }

    // Banks fire in turn, each following an inline order; cylinders are
    // numbered bank by bank
    const int banks = bankCount();
    const int perBank = cylinders / banks;
    const std::vector<int> bankOrder = inlineFiringOrder(perBank);
    std::vector<int> order;
    for (int k = 0; k < perBank; ++k) {
        for (int b = 0; b < banks; ++b) {
            order.push_back(bankOrder[k] + b * perBank);
        }
    }

    return order;
}

std::string designFileName(const std::string &name) {
    std::string result;
    for (const char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        else if (!result.empty() && result.back() != '_') {
            result += '_';
        }
    }

    while (!result.empty() && result.back() == '_') result.pop_back();
    return result.empty() ? "custom_engine" : result;
}

std::string generateEngineScript(const EngineDesign &design_) {
    EngineDesign d = design_;
    d.validate();

    if (d.isRotary()) return generateRotaryScript(d);

    const int n = d.cylinders;
    const int banks = d.bankCount();
    const int perBank = d.cylindersPerBank();
    const std::vector<double> bankAngles = d.bankAngles();

    // The banks left of the vertical share the first exhaust system, the
    // others the second
    auto exhaustOf = [&](int bank) {
        return (banks > 1 && bankAngles[bank] >= 0 && !(banks == 3 && bankAngles[bank] == 0)) ? 1 : 0;
    };
    const int exhaustCount = (banks > 1) ? 2 : 1;

    // Each cylinder fires at an even interval in firing order. Every
    // cylinder gets its own crankpin, at its firing angle plus its bank's
    // offset from the first bank, so any layout fires evenly.
    const std::vector<int> order = d.firingOrder();
    std::vector<double> firing(n);
    for (int slot = 0; slot < n; ++slot) {
        firing[order[slot] - 1] = slot * (4 * Pi / n);
    }

    const double vd = d.cylinderDisplacement();
    const double chamberVolume = vd / (d.compressionRatio - 1);
    const double compressionHeight = 0.35 * d.bore;
    const double pistonMass = 600.0 * d.bore * d.bore * d.bore;
    const double rodMass = 1.2 * pistonMass;
    const double crankMass = 3.0 * n * (d.stroke / 0.086);

    // Throttle sized for about 0.85 VE at redline when not specified:
    // cfm = displacement[in^3] * rpm / 3456
    const double displacementCi = d.displacement() / 1.6387064e-5;
    const double throttleFlow = (d.throttleFlow > 0)
        ? d.throttleFlow
        : 1.3 * displacementCi * d.redline / 3456;
    const double plenumVolume = (d.plenumVolume > 0) ? d.plenumVolume : std::max(d.displacement(), 0.5e-3);

    // Port flow scales with valve area (~bore^2); 2-valve heads flow less
    const double valveFactor = (d.valvesPerCylinder >= 4) ? 1.0 : (d.valvesPerCylinder == 3 ? 0.85 : 0.7);
    const double flowScale = valveFactor * std::pow(d.bore / 0.0889, 2.0);
    const double intakeLiftScale = d.intakeLift / 0.0102;
    const double exhaustLiftScale = d.exhaustLift / 0.0102;
    const double runnerArea = Pi * std::pow(0.42 * d.bore, 2) / 4 * (d.valvesPerCylinder >= 4 ? 1.5 : 1.0);

    std::ostringstream s;
    headerScript(s, d, "2 * 360 * units.deg");

    // Ignition wires
    s << "private node designed_wires {\n";
    for (int i = 0; i < n; ++i) {
        s << "    output wire" << (i + 1) << ": ignition_wire();\n";
    }
    s << "}\n\n";

    // Cylinder head
    s << "private node designed_head {\n"
      << "    input intake_camshaft;\n"
      << "    input exhaust_camshaft;\n"
      << "    input flip_display: false;\n"
      << "    alias output __out: head;\n\n"
      << "    function intake_flow(50 * units.thou)\n"
      << "    intake_flow\n";
    const double intakeFlow[] = { 0, 58, 103, 156, 214, 249, 268, 280, 280, 281 };
    const double exhaustFlow[] = { 0, 37, 72, 113, 160, 196, 222, 235, 245, 246 };
    for (int i = 0; i < 10; ++i) {
        s << "        .add_flow_sample(" << num(i * 50 * intakeLiftScale) << ", " << num(intakeFlow[i] * flowScale) << ")\n";
    }
    s << "\n    function exhaust_flow(50 * units.thou)\n"
      << "    exhaust_flow\n";
    for (int i = 0; i < 10; ++i) {
        s << "        .add_flow_sample(" << num(i * 50 * exhaustLiftScale) << ", " << num(exhaustFlow[i] * flowScale) << ")\n";
    }
    s << "\n    generic_cylinder_head head(\n"
      << "        chamber_volume: " << num(chamberVolume * 1e6) << " * units.cc,\n"
      << "        intake_runner_volume: " << num(runnerArea * 0.1 * 1e6) << " * units.cc,\n"
      << "        intake_runner_cross_section_area: " << num(runnerArea * 1e4) << " * units.cm2,\n"
      << "        exhaust_runner_volume: " << num(runnerArea * 0.05 * 1e6) << " * units.cc,\n"
      << "        exhaust_runner_cross_section_area: " << num(runnerArea * 1e4) << " * units.cm2,\n"
      << "        intake_port_flow: intake_flow,\n"
      << "        exhaust_port_flow: exhaust_flow,\n"
      << "        valvetrain: standard_valvetrain(\n"
      << "            intake_camshaft: intake_camshaft,\n"
      << "            exhaust_camshaft: exhaust_camshaft\n"
      << "        ),\n"
      << "        flip_display: flip_display\n"
      << "    )\n"
      << "}\n\n";

    // Engine
    s << "public node designed_engine {\n"
      << "    alias output __out: engine;\n\n"
      << "    engine engine(\n"
      << "        name: \"" << d.name << "\",\n"
      << "        starter_torque: " << num(std::max(60.0, 30.0 * d.displacement() * 1000 + 40)) << " * units.lb_ft,\n"
      << "        starter_speed: 300 * units.rpm,\n"
      << "        redline: " << num(d.redline) << " * units.rpm,\n"
      << "        dyno_max_speed: " << num(d.redline) << " * units.rpm,\n"
      << "        physics_model: \"" << (d.engineering ? "engineering" : "legacy") << "\",\n"
      << "        valvetrain_type: \"" << d.valvetrain << "\",\n"
      << "        valves_per_cylinder: " << d.valvesPerCylinder << ",\n"
      << "        max_valve_lift: " << num(d.intakeLift * 1000) << " * units.mm,\n"
      << "        simulation_frequency: " << simulationFrequency(d) << ",\n"
      << "        fuel: fuel(type: \"" << d.fuel << "\")\n"
      << "    )\n\n"
      << "    designed_wires wires()\n\n"
      << "    label flywheel_radius(" << num(std::max(0.12, 1.5 * d.stroke)) << " * units.m)\n"
      << "    crankshaft c0(\n"
      << "        throw: " << num(d.stroke * 500) << " * units.mm,\n"
      << "        flywheel_mass: " << num(d.flywheelMass) << " * units.kg,\n"
      << "        mass: " << num(crankMass) << " * units.kg,\n"
      << "        friction_torque: " << num(1.0 + 0.6 * n) << " * units.lb_ft,\n"
      << "        moment_of_inertia:\n"
      << "            disk_moment_of_inertia(mass: " << num(crankMass) << " * units.kg, radius: " << num(d.stroke * 500) << " * units.mm)\n"
      << "            + disk_moment_of_inertia(mass: " << num(d.flywheelMass) << " * units.kg, radius: flywheel_radius),\n"
      << "        position_x: 0.0,\n"
      << "        position_y: 0.0,\n"
      << "        tdc: " << num((bankAngles[0] + Pi / 2) / Deg) << " * units.deg\n"
      << "    )\n\n";

    for (int i = 0; i < n; ++i) {
        const int bank = i / perBank;
        double angle = std::fmod(firing[i] + (bankAngles[bank] - bankAngles[0]), 2 * Pi);
        if (angle < 0) angle += 2 * Pi;
        s << "    rod_journal rj" << i << "(angle: " << num(angle / Deg) << " * units.deg)\n";
    }
    s << "    c0\n";
    for (int i = 0; i < n; ++i) {
        s << "        .add_rod_journal(rj" << i << ")\n";
    }

    s << "\n    piston_parameters piston_params(\n"
      << "        mass: " << num(pistonMass * 1000) << " * units.g,\n"
      << "        compression_height: " << num(compressionHeight * 1000) << " * units.mm,\n"
      << "        wrist_pin_position: 0.0,\n"
      << "        displacement: 0.0\n"
      << "    )\n\n"
      << "    connecting_rod_parameters cr_params(\n"
      << "        mass: " << num(rodMass * 1000) << " * units.g,\n"
      << "        moment_of_inertia: rod_moment_of_inertia(mass: " << num(rodMass * 1000) << " * units.g, length: " << num(d.rodLength * 1000) << " * units.mm),\n"
      << "        center_of_mass: 0.0,\n"
      << "        length: " << num(d.rodLength * 1000) << " * units.mm\n"
      << "    )\n\n"
      << "    cylinder_bank_parameters bank_params(\n"
      << "        bore: " << num(d.bore * 1000) << " * units.mm,\n"
      << "        deck_height: " << num((d.stroke / 2 + d.rodLength + compressionHeight) * 1000) << " * units.mm\n"
      << "    )\n\n"
      << "    intake intake(\n"
      << "        plenum_volume: " << num(plenumVolume * 1000) << " * units.L,\n"
      << "        plenum_cross_section_area: " << num(std::max(20.0, 15.0 * d.displacement() * 1000)) << " * units.cm2,\n"
      << "        intake_flow_rate: k_carb(" << num(throttleFlow) << "),\n"
      << "        runner_flow_rate: k_carb(" << num(throttleFlow * 2.5 / n) << "),\n"
      << "        runner_length: " << num(d.runnerLength * 1000) << " * units.mm,\n"
      << "        idle_flow_rate: k_carb(0.0),\n"
      << "        idle_throttle_plate_position: " << num(d.idleThrottle) << ",\n"
      << "        velocity_decay: 0.5,\n"
      << "        lambda: " << num(d.lambda) << "\n"
      << "    )\n\n"
      << "    exhaust_system_parameters es_params(\n"
      << "        outlet_flow_rate: k_carb(" << num(throttleFlow * 2) << "),\n"
      << "        primary_tube_length: " << num(d.primaryLength * 1000) << " * units.mm,\n"
      << "        primary_flow_rate: k_carb(" << num(throttleFlow * 2.5 / n) << "),\n"
      << "        velocity_decay: 1.0,\n"
      << "        collector_cross_section_area: circle_area(" << num(d.exhaustDiameter * 1000) << " * units.mm),\n"
      << "        length: " << num(d.exhaustLength * 1000) << " * units.mm\n"
      << "    )\n\n";

    for (int e = 0; e < exhaustCount; ++e) {
        s << "    exhaust_system exhaust" << e << "(es_params, audio_volume: 1.0, impulse_response: " << impulseResponse(d) << ")\n";
    }
    s << "\n";

    for (int b = 0; b < banks; ++b) {
        s << "    cylinder_bank b" << b << "(bank_params, angle: " << num(bankAngles[b] / Deg) << " * units.deg)\n";
    }
    s << "\n";

    for (int b = 0; b < banks; ++b) {
        s << "    b" << b << "\n";
        for (int k = 0; k < perBank; ++k) {
            const int i = b * perBank + k;
            s << "        .add_cylinder(\n"
              << "            piston: piston(piston_params, blowby: k_28inH2O(0.1)),\n"
              << "            connecting_rod: connecting_rod(cr_params),\n"
              << "            rod_journal: rj" << i << ",\n"
              << "            intake: intake,\n"
              << "            exhaust_system: exhaust" << exhaustOf(b) << ",\n"
              << "            ignition_wire: wires.wire" << (i + 1) << ",\n"
              << "            sound_attenuation: 1.0,\n"
              << "            primary_length: " << num(0.06 * (perBank - 1 - k)) << " * units.m\n"
              << "        )\n";
        }
        const bool flip = bankAngles[b] > 0;
        s << "        .set_cylinder_head(\n"
          << "            designed_head(\n"
          << "                intake_camshaft: intake_cam_" << b << ",\n"
          << "                exhaust_camshaft: exhaust_cam_" << b << (flip ? ",\n                flip_display: true)\n" : ")\n")
          << "        )\n";
    }

    s << "\n    engine\n";
    for (int b = 0; b < banks; ++b) s << "        .add_cylinder_bank(b" << b << ")\n";
    s << "    engine.add_crankshaft(c0)\n\n";

    // Camshafts: one intake and one exhaust camshaft per bank with a lobe
    // per cylinder at its firing angle
    s << "    harmonic_cam_lobe intake_lobe(\n"
      << "        duration_at_50_thou: " << num(d.intakeDuration / Deg) << " * units.deg,\n"
      << "        gamma: 1.1,\n"
      << "        lift: " << num(d.intakeLift * 1000) << " * units.mm,\n"
      << "        steps: 100\n"
      << "    )\n"
      << "    harmonic_cam_lobe exhaust_lobe(\n"
      << "        duration_at_50_thou: " << num(d.exhaustDuration / Deg) << " * units.deg,\n"
      << "        gamma: 1.1,\n"
      << "        lift: " << num(d.exhaustLift * 1000) << " * units.mm,\n"
      << "        steps: 100\n"
      << "    )\n"
      << "    camshaft_parameters cam_params(advance: 0 * units.deg, base_radius: " << num(std::max(12.0, 0.2 * d.bore * 1000)) << " * units.mm)\n";

    for (int b = 0; b < banks; ++b) {
        s << "    camshaft intake_cam_" << b << "(cam_params, lobe_profile: intake_lobe)\n"
          << "    camshaft exhaust_cam_" << b << "(cam_params, lobe_profile: exhaust_lobe)\n";
        s << "    intake_cam_" << b << "\n";
        for (int k = 0; k < perBank; ++k) {
            const int i = b * perBank + k;
            s << "        .add_lobe(360 * units.deg + " << num(d.intakeLobeCenter / Deg) << " * units.deg + " << num(firing[i] / Deg) << " * units.deg)\n";
        }
        s << "    exhaust_cam_" << b << "\n";
        for (int k = 0; k < perBank; ++k) {
            const int i = b * perBank + k;
            s << "        .add_lobe(360 * units.deg - " << num(d.exhaustLobeCenter / Deg) << " * units.deg + " << num(firing[i] / Deg) << " * units.deg)\n";
        }
    }

    s << forcedInductionScript(d);
    timingCurveScript(s, d);

    s << "\n"
      << "    ignition_module ignition(\n"
      << "        timing_curve: timing_curve,\n"
      << "        rev_limit: " << num(d.redline + 200) << " * units.rpm,\n"
      << "        limiter_duration: 0.1\n"
      << "    )\n"
      << "    ignition\n";
    for (int i = 0; i < n; ++i) {
        s << "        .connect_wire(wires.wire" << (i + 1) << ", " << num(firing[i] / Deg) << " * units.deg)\n";
    }
    s << "    engine.add_ignition_module(ignition)\n"
      << "}\n\n";

    vehicleScript(s, d, "designed_engine");
    return s.str();
}

bool readEngineDesign(const std::string &scriptText, EngineDesign *design) {
    std::istringstream in(scriptText);
    std::string line;
    std::map<std::string, std::string> values;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string prefix = "// @design ";
        if (line.compare(0, prefix.size(), prefix) != 0) continue;

        const std::string kv = line.substr(prefix.size());
        const size_t eq = kv.find('=');
        if (eq == std::string::npos) continue;
        values[kv.substr(0, eq)] = kv.substr(eq + 1);
    }

    if (values.empty()) return false;

    EngineDesign d;
    auto number = [&](const char *key, double *target) {
        auto it = values.find(key);
        if (it != values.end()) *target = std::atof(it->second.c_str());
    };
    auto integer = [&](const char *key, int *target) {
        auto it = values.find(key);
        if (it != values.end()) *target = std::atoi(it->second.c_str());
    };
    auto text = [&](const char *key, std::string *target) {
        auto it = values.find(key);
        if (it != values.end()) *target = it->second;
    };
    auto flag = [&](const char *key, bool *target) {
        auto it = values.find(key);
        if (it != values.end()) *target = (it->second == "1");
    };

    text("name", &d.name);
    text("description", &d.description);
    if (values.count("layout")) {
        const std::string &l = values["layout"];
        d.layout = (l == "v") ? EngineDesign::Layout::V
            : (l == "flat") ? EngineDesign::Layout::Flat
            : (l == "w") ? EngineDesign::Layout::W
            : (l == "rotary") ? EngineDesign::Layout::Rotary
            : EngineDesign::Layout::Inline;
    }

    if (values.count("induction")) {
        const std::string &i = values["induction"];
        d.induction = (i == "turbo") ? EngineDesign::Induction::Turbo
            : (i == "twin_turbo") ? EngineDesign::Induction::TwinTurbo
            : (i == "sequential_turbo") ? EngineDesign::Induction::SequentialTurbo
            : (i == "roots") ? EngineDesign::Induction::Roots
            : (i == "twin_screw") ? EngineDesign::Induction::TwinScrew
            : (i == "centrifugal") ? EngineDesign::Induction::Centrifugal
            : EngineDesign::Induction::Natural;
    }

    integer("cylinders", &d.cylinders);
    integer("w_banks", &d.wBanks);
    integer("rotors", &d.rotors);
    integer("valves_per_cylinder", &d.valvesPerCylinder);
    flag("engineering", &d.engineering);
    flag("ball_bearings", &d.ballBearings);
    flag("blowoff_valve", &d.blowoffValve);
    text("fuel", &d.fuel);
    text("firing_order", &d.firingOrderText);
    text("valvetrain", &d.valvetrain);
    text("exhaust_sound", &d.exhaustSound);

    number("bank_angle", &d.bankAngle);
    number("vr_angle", &d.vrAngle);
    number("bore", &d.bore);
    number("stroke", &d.stroke);
    number("rod_length", &d.rodLength);
    number("compression_ratio", &d.compressionRatio);
    number("eccentricity", &d.eccentricity);
    number("generating_radius", &d.generatingRadius);
    number("rotor_width", &d.rotorWidth);
    number("intake_open", &d.intakeOpen);
    number("intake_close", &d.intakeClose);
    number("exhaust_open", &d.exhaustOpen);
    number("exhaust_close", &d.exhaustClose);
    number("intake_duration", &d.intakeDuration);
    number("exhaust_duration", &d.exhaustDuration);
    number("intake_lift", &d.intakeLift);
    number("exhaust_lift", &d.exhaustLift);
    number("intake_lobe_center", &d.intakeLobeCenter);
    number("exhaust_lobe_center", &d.exhaustLobeCenter);
    number("throttle_flow", &d.throttleFlow);
    number("plenum_volume", &d.plenumVolume);
    number("runner_length", &d.runnerLength);
    number("primary_length", &d.primaryLength);
    number("exhaust_length", &d.exhaustLength);
    number("exhaust_diameter", &d.exhaustDiameter);
    number("boost", &d.boost);
    number("turbine_ar", &d.turbineAr);
    number("compressor_scale", &d.compressorScale);
    number("intercooler", &d.intercoolerEffectiveness);
    number("drive_ratio", &d.driveRatio);
    number("idle_advance", &d.idleAdvance);
    number("max_advance", &d.maxAdvance);
    number("max_advance_rpm", &d.maxAdvanceRpm);
    number("redline", &d.redline);
    number("idle_throttle", &d.idleThrottle);
    number("flywheel_mass", &d.flywheelMass);
    number("lambda", &d.lambda);
    number("vehicle_mass", &d.vehicleMass);

    d.validate();
    *design = d;
    return true;
}
