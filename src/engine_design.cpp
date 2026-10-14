#include "../include/engine_design.h"

#include "../include/constants.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>

namespace {

    constexpr double Pi = constants::pi;
    constexpr double Deg = Pi / 180.0;

    std::string num(double v, int precision = 6) {
        std::ostringstream ss;
        ss.precision(precision);
        ss << v;
        return ss.str();
    }

    std::string layoutName(EngineDesign::Layout layout) {
        switch (layout) {
        case EngineDesign::Layout::V: return "v";
        case EngineDesign::Layout::Flat: return "flat";
        case EngineDesign::Layout::Inline:
        default: return "inline";
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

    // Key-value pairs stored in the generated script's header
    std::map<std::string, std::string> serialize(const EngineDesign &d) {
        return {
            { "name", d.name },
            { "layout", layoutName(d.layout) },
            { "cylinders", std::to_string(d.cylinders) },
            { "bank_angle", num(d.bankAngle) },
            { "bore", num(d.bore) },
            { "stroke", num(d.stroke) },
            { "rod_length", num(d.rodLength) },
            { "compression_ratio", num(d.compressionRatio) },
            { "valves_per_cylinder", std::to_string(d.valvesPerCylinder) },
            { "intake_duration", num(d.intakeDuration) },
            { "exhaust_duration", num(d.exhaustDuration) },
            { "intake_lift", num(d.intakeLift) },
            { "exhaust_lift", num(d.exhaustLift) },
            { "intake_lobe_center", num(d.intakeLobeCenter) },
            { "exhaust_lobe_center", num(d.exhaustLobeCenter) },
            { "throttle_flow", num(d.throttleFlow) },
            { "plenum_volume", num(d.plenumVolume) },
            { "runner_length", num(d.runnerLength) },
            { "primary_length", num(d.primaryLength) },
            { "exhaust_length", num(d.exhaustLength) },
            { "exhaust_diameter", num(d.exhaustDiameter) },
            { "idle_advance", num(d.idleAdvance) },
            { "max_advance", num(d.maxAdvance) },
            { "max_advance_rpm", num(d.maxAdvanceRpm) },
            { "redline", num(d.redline) },
            { "idle_throttle", num(d.idleThrottle) },
            { "flywheel_mass", num(d.flywheelMass) },
            { "engineering", d.engineering ? "1" : "0" },
            { "fuel", d.fuel },
            { "lambda", num(d.lambda) },
            { "vehicle_mass", num(d.vehicleMass) }
        };
    }

    std::string sanitizeLabel(const std::string &s) {
        std::string result;
        for (const char c : s) {
            if (c == '"' || c == '\\' || c == '\n' || c == '\r') continue;
            result += c;
        }

        return result.empty() ? "Custom engine" : result;
    }

} /* namespace */

double EngineDesign::cylinderDisplacement() const {
    return Pi * bore * bore / 4 * stroke;
}

double EngineDesign::displacement() const {
    return cylinderDisplacement() * cylinders;
}

bool EngineDesign::validate() {
    const EngineDesign before = *this;

    if (layout == Layout::Inline) cylinders = std::clamp(cylinders, 1, 12);
    else cylinders = std::clamp(cylinders + (cylinders % 2), 2, 16);

    bankAngle = std::clamp(bankAngle, 15 * Deg, 180 * Deg);
    bore = std::clamp(bore, 0.02, 0.25);
    stroke = std::clamp(stroke, 0.02, 0.3);
    rodLength = std::clamp(rodLength, 1.2 * stroke, 6.0 * stroke);
    compressionRatio = std::clamp(compressionRatio, 4.0, 18.0);
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
    idleAdvance = std::clamp(idleAdvance, 0.0, 40 * Deg);
    maxAdvance = std::clamp(maxAdvance, idleAdvance, 60 * Deg);
    maxAdvanceRpm = std::clamp(maxAdvanceRpm, 1000.0, 15000.0);
    redline = std::clamp(redline, 1500.0, 20000.0);
    idleThrottle = std::clamp(idleThrottle, 0.9, 1.0);
    flywheelMass = std::clamp(flywheelMass, 0.5, 100.0);
    lambda = std::clamp(lambda, 0.6, 1.6);
    vehicleMass = std::clamp(vehicleMass, 100.0, 40000.0);
    name = sanitizeLabel(name);
    if (fuel.empty()) fuel = "gasoline";

    return serialize(before) == serialize(*this);
}

std::vector<int> EngineDesign::firingOrder() const {
    if (layout == Layout::Inline) {
        return inlineFiringOrder(cylinders);
    }

    // Two banks; cylinders 1..n/2 on the left bank and n/2+1..n on the
    // right. Alternate banks, each following an inline order.
    const int perBank = cylinders / 2;
    const std::vector<int> bankOrder = inlineFiringOrder(perBank);
    std::vector<int> order;
    for (int k = 0; k < perBank; ++k) {
        order.push_back(bankOrder[k]);
        order.push_back(bankOrder[k] + perBank);
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

    const int n = d.cylinders;
    const bool twoBanks = (d.layout != EngineDesign::Layout::Inline);
    const int perBank = twoBanks ? n / 2 : n;
    const int banks = twoBanks ? 2 : 1;

    // Bank angles: the crank's TDC is 90 degrees from the first bank
    double bankAngles[2] = { 0, 0 };
    if (d.layout == EngineDesign::Layout::V) {
        bankAngles[0] = -d.bankAngle / 2;
        bankAngles[1] = d.bankAngle / 2;
    }
    else if (d.layout == EngineDesign::Layout::Flat) {
        bankAngles[0] = -Pi / 2;
        bankAngles[1] = Pi / 2;
    }

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
      << "label cycle(2 * 360 * units.deg)\n\n";

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
      << "        valves_per_cylinder: " << d.valvesPerCylinder << ",\n"
      << "        max_valve_lift: " << num(d.intakeLift * 1000) << " * units.mm,\n"
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
        const int bank = twoBanks ? (i / perBank) : 0;
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

    for (int b = 0; b < banks; ++b) {
        s << "    exhaust_system exhaust" << b << "(es_params, audio_volume: 1.0, impulse_response: ir_lib.default_0)\n";
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
              << "            exhaust_system: exhaust" << b << ",\n"
              << "            ignition_wire: wires.wire" << (i + 1) << ",\n"
              << "            sound_attenuation: 1.0,\n"
              << "            primary_length: " << num(0.06 * (perBank - 1 - k)) << " * units.m\n"
              << "        )\n";
        }
        s << "        .set_cylinder_head(\n"
          << "            designed_head(\n"
          << "                intake_camshaft: intake_cam_" << b << ",\n"
          << "                exhaust_camshaft: exhaust_cam_" << b << (b == 1 ? ",\n                flip_display: true)\n" : ")\n")
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

    // Ignition
    // Script functions interpolate between neighbouring samples within the
    // function's resolution, so the curve is sampled at that spacing
    s << "\n    function timing_curve(500 * units.rpm)\n"
      << "    timing_curve\n";
    const double lastRpm = std::max(d.redline, d.maxAdvanceRpm) + 1500;
    for (double rpm = 0; rpm <= lastRpm; rpm += 500) {
        const double t = std::clamp((rpm - 1000) / std::max(d.maxAdvanceRpm - 1000, 1.0), 0.0, 1.0);
        const double advance = d.idleAdvance + t * (d.maxAdvance - d.idleAdvance);
        s << "        .add_sample(" << num(rpm) << " * units.rpm, " << num(advance / Deg) << " * units.deg)\n";
    }

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

    // Vehicle and transmission
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
      << "        transmission(max_clutch_torque: " << num(std::max(200.0, 300.0 * d.displacement() * 1000)) << " * units.lb_ft)\n"
      << "            .add_gear(3.6)\n"
      << "            .add_gear(2.2)\n"
      << "            .add_gear(1.5)\n"
      << "            .add_gear(1.15)\n"
      << "            .add_gear(0.92)\n"
      << "            .add_gear(0.75);\n"
      << "}\n\n"
      << "public node main {\n"
      << "    run(\n"
      << "        engine: designed_engine(),\n"
      << "        transmission: designed_transmission(),\n"
      << "        vehicle: designed_vehicle()\n"
      << "    )\n"
      << "}\n\n"
      << "main()\n";

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

    if (values.count("name")) d.name = values["name"];
    if (values.count("layout")) {
        const std::string &l = values["layout"];
        d.layout = (l == "v") ? EngineDesign::Layout::V
            : (l == "flat") ? EngineDesign::Layout::Flat
            : EngineDesign::Layout::Inline;
    }

    if (values.count("cylinders")) d.cylinders = std::atoi(values["cylinders"].c_str());
    if (values.count("valves_per_cylinder")) d.valvesPerCylinder = std::atoi(values["valves_per_cylinder"].c_str());
    if (values.count("engineering")) d.engineering = (values["engineering"] == "1");
    if (values.count("fuel")) d.fuel = values["fuel"];

    number("bank_angle", &d.bankAngle);
    number("bore", &d.bore);
    number("stroke", &d.stroke);
    number("rod_length", &d.rodLength);
    number("compression_ratio", &d.compressionRatio);
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
