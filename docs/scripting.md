# Engine scripting guide

Engines, vehicles, transmissions and themes are described with *Piranha*, a small node based language. This guide covers what you need to build your own engines. The bundled engines in `assets/engines` are complete, working examples; copying one and changing it is the quickest way to get started.

- [Basics](#basics)
- [Anatomy of an engine script](#anatomy-of-an-engine-script)
- [Units](#units)
- [Piston engine parts](#piston-engine-parts)
- [Ignition](#ignition)
- [Intake and exhaust](#intake-and-exhaust)
- [Simulation quality and audio settings](#simulation-quality-and-audio-settings)
- [Vehicle and transmission](#vehicle-and-transmission)
- [Rotary (Wankel) engines](#rotary-wankel-engines)
- [Themes](#themes)
- [Testing scripts](#testing-scripts)

## Basics

A script is a set of *nodes*. A node has inputs (with optional default values) and outputs; using a node creates an instance of it:

```
units units()                          // instance of the node "units"
label stroke(86 * units.mm)            // a named constant

crankshaft c0(                         // instance with named inputs
    throw: stroke / 2,
    flywheel_mass: 10 * units.lb
)

c0.add_rod_journal(rj0)                // "methods" are nodes taking `this`
```

- `public node name { ... }` defines a node; `input x: default;` declares an input and `output y: expression;` an output. `alias output __out: ...;` makes the node usable as a value.
- `import "engine_sim.mr"` imports the whole library (parts, units, constants, sound library). Imported files only provide definitions; top level statements of imported files are not executed.
- Comments use `//` and `/* */`.
- Only top level statements of the file you load run, so an engine script has to *call* something at the end (see below).

## Anatomy of an engine script

```
import "engine_sim.mr"

units units()
constants constants()
impulse_response_library ir_lib()

public node my_engine {
    alias output __out: engine;

    engine engine(
        name: "My Engine",
        starter_torque: 150 * units.lb_ft,
        redline: 7000 * units.rpm
    )

    // crankshaft, cylinder banks, heads, intake, exhaust, ignition ...
}

public node main {
    run(
        engine: my_engine(),
        transmission: my_transmission(),   // optional: a 5-speed is used otherwise
        vehicle: my_car()                  // optional: a generic car is used otherwise
    )
}

main()
```

`run()` selects what to simulate. The lower level actions `set_engine()`, `set_vehicle()` and `set_transmission()` are also available.

Load a script with `engine-sim-app --engine path/to/script.mr`, by editing `assets/main.mr`, or put it in `assets/engines` and use Page Up / Page Down in the application. Errors are shown in the console (F5) and written to `error_log.log` in the user data folder.

## Units

Internally everything is SI. Multiply literal values by a unit from the `units` node:

| Quantity | Units |
| :-- | :-- |
| Length | `mm`, `cm`, `m`, `inch`, `foot`, `thou`, `mile` |
| Area / volume | `cm2`, `m2`, `cc`, `mL`, `L`, `m3`, `cubic_inches` |
| Mass | `g`, `kg`, `lb` |
| Force / torque | `N`, `lbf`, `Nm`, `lb_ft` |
| Pressure | `Pa`, `kPa`, `MPa`, `atm`, `psi`, `inHg`, `inH2O`, `bar`, `mbar` |
| Angle | `deg`, `rad` |
| Speed | `rpm`, `mph`, `kph` |
| Time | `sec` |

Temperatures are written as `celsius(90)` (`celcius` is also accepted). `constants.pi` is available from `constants constants()`.

Flow coefficients (how freely gas passes through an opening) are usually given as `k_carb(cfm)` (the flow of a carburetor rated at that many CFM at 1.5 inHg), `k_28inH2O(cfm)` or `k_32inH2O(cfm)` (cylinder head flow bench ratings at 28 or 32 inches of water).

## Piston engine parts

A piston engine is assembled from:

| Node | Purpose |
| :-- | :-- |
| `engine` | The engine itself: name, starter, redline, fuel, throttle and simulation settings |
| `crankshaft` | `throw` (half the stroke), masses, `moment_of_inertia`, `friction_torque`, `tdc` (angle of top dead center, usually `constants.pi / 2`) |
| `rod_journal` | A crank pin at an `angle` around the crankshaft, added with `crankshaft.add_rod_journal()` |
| `cylinder_bank` | A row of cylinders: `angle` (from vertical), `bore`, `deck_height` |
| `piston` | `mass`, `compression_height`, `wrist_pin_position`, `blowby` |
| `connecting_rod` | `mass`, `moment_of_inertia`, `center_of_mass`, `length` (and `slave_throw` for radial master rods) |
| `cylinder_head` | Combustion chamber volume, port flow curves, valvetrain and runner sizes |
| `camshaft` | Lobe profile, lobe angles and base radius |

Cylinders are added to a bank with `bank.add_cylinder(piston:, connecting_rod:, rod_journal:, intake:, exhaust_system:, ignition_wire:)`, the bank gets its head with `bank.set_cylinder_head(...)` and everything is attached to the engine with `engine.add_cylinder_bank(bank)` and `engine.add_crankshaft(c0)`.

The *part library* (`es/part-library`) has ready made heads, camshaft builders, intakes and distributors that save a lot of typing; the bundled engines show how they're used.

The deck height has to fit the rotating assembly: `deck_height = stroke / 2 + rod_length + compression_height` puts the piston crown flush with the deck at TDC.

## Ignition

```
function timing_curve(1000 * units.rpm)
timing_curve
    .add_sample(0000 * units.rpm, 12 * units.deg)
    .add_sample(3000 * units.rpm, 30 * units.deg)

label cycle(2 * 360 * units.deg)

engine.add_ignition_module(
    ignition_module(timing_curve: timing_curve, rev_limit: 7500 * units.rpm)
        .connect_wire(wires.wire1, (0.0 / 4.0) * cycle)
        .connect_wire(wires.wire3, (1.0 / 4.0) * cycle)
        .connect_wire(wires.wire4, (2.0 / 4.0) * cycle)
        .connect_wire(wires.wire2, (3.0 / 4.0) * cycle))
```

Each cylinder is connected to an `ignition_wire`; the module fires each wire at the given crankshaft angle (measured from the crankshaft's `tdc` position) minus the advance from the timing curve. A four-stroke cycle is 720 degrees. A wire may be connected more than once to fire it several times per cycle.

## Intake and exhaust

`intake` models the plenum and throttle: `plenum_volume`, `plenum_cross_section_area`, `intake_flow_rate` (throttle body flow at wide open throttle), `runner_flow_rate`, `runner_length`, `idle_flow_rate`, `idle_throttle_plate_position` (how far the throttle is open at idle; raise it if the engine stalls, lower it if it idles too fast) and `velocity_decay`.

`exhaust_system` models the collector and pipe: `volume` or `length`, `collector_cross_section_area`, `primary_tube_length`, `primary_flow_rate`, `outlet_flow_rate`, `audio_volume`, `impulse_response` (the acoustic character, e.g. `ir_lib.mild_exhaust_0`) and `backflow_atmospheric_mixing` (0 treats gas drawn back in through the tailpipe as exhaust, 1 as fresh air).

Several cylinders can share an intake or exhaust; each distinct exhaust system is an audio channel.

## Simulation quality and audio settings

These `engine` inputs trade accuracy for speed and set the initial audio mix (which can also be changed live with the Z, X, C, V, B and N keys):

| Input | Default | Description |
| :-- | :-- | :-- |
| `simulation_frequency` | 10000 | Physics steps per second. Fast-revving or many-cylinder engines need more |
| `fluid_simulation_steps` | 8 | Gas flow substeps per physics step |
| `max_sle_solver_steps` | 128 | Iterations of the constraint solver |
| `heat_transfer_coefficient` | 100 | Heat exchange between the charge and the cylinder walls (W/m²K), legacy model |
| `block_temperature` | `celsius(90)` | Cylinder wall temperature, legacy model |
| `hf_gain`, `noise`, `jitter`, `convolution` | | Initial audio settings |

## Engineering physics model

`physics_model: "engineering"` on `engine` (or `wankel_engine`) switches to
real air and fuel chemistry, variable gas properties, Woschni heat transfer
and a component friction model, and makes the engine's figures meaningful
for engineering work. It adds inputs to `engine` (`wall_temperature`,
`friction_model`, `valvetrain_type`, ...), `fuel` (`type`,
`combustion_efficiency`) and `intake` (`lambda`, `evaporative_cooling`).
See [engineering.md](engineering.md) for the inputs, the models and the
`engine-sim-dyno` tool.

## Vehicle and transmission

```
vehicle(
    mass: 1400 * units.kg,
    drag_coefficient: 0.3,
    cross_sectional_area: (70 * units.inch) * (55 * units.inch),
    diff_ratio: 3.9,
    tire_radius: 12 * units.inch,
    rolling_resistance: 200 * units.N,
    max_brake_force: 8000 * units.N,     // braking with [.]

    // Optional drivetrain compliance (axles and tires wind up and spring back)
    simulate_flex: true,
    stiffness: 50 * units.lb_ft / units.deg,
    damping: 15.0,
    max_flex: 5 * units.deg,
    limit_flex: true
)

transmission(
    max_clutch_torque: 400 * units.lb_ft,

    // Optional clutch damper springs; the clutch slips above max_clutch_torque
    simulate_flex: true,
    clutch_stiffness: 50 * units.lb_ft / units.deg,
    clutch_damping: 2.0,
    max_clutch_flex: 5 * units.deg,
    limit_clutch_flex: true
)
    .add_gear(3.5)
    .add_gear(2.1)
    .add_gear(1.4)
    .add_gear(1.0)
    .add_gear(0.8)
```

`vehicle` also accepts the tire, wheelbase and weight transfer inputs used by newer versions of Engine Simulator so that their scripts load, but they don't affect this simulation.

## Rotary (Wankel) engines

Rotary engines are built with `wankel_engine` and `add_rotor`. `assets/engines/rotary/two_rotor_13b.mr` is a complete example.

```
wankel_engine engine(
    name: "Twin Rotor",
    redline: 8500 * units.rpm,
    rotor_calculation_quality: 1024,   // resolution of the rotor shape calculation
    rotor_housing_resolution: 256
)

// The crankshaft is the eccentric shaft: its throw is the eccentricity
crankshaft eccentric_shaft(throw: 15 * units.mm, tdc: constants.pi / 2, ...)
rod_journal front_lobe(angle: 0 * units.deg)
rod_journal rear_lobe(angle: 180 * units.deg)

wankel_rotor rotor(
    mass: 4.5 * units.kg,
    moment_of_inertia: ...,
    radius: 105 * units.mm,            // generating radius: rotor center to apex
    depth: 80 * units.mm,              // housing width
    depression_volume: 45 * units.cc,  // recess in each rotor face
    crown_gear_radius: 45 * units.mm   // only used for drawing
)

engine.add_rotor(
    intake: intake,
    exhaust_system: exhaust,
    rotor: rotor,
    rod_journal: front_lobe,
    ignition_wire: wires.front,
    intake_port: default_wankel_peripheral_port(121 * units.deg, 123 * units.deg, k_carb(220.0)),
    exhaust_port: default_wankel_peripheral_port(238 * units.deg, 240 * units.deg, k_carb(300.0)),
    intake_runner_cross_section_area: 8 * units.cm2,
    exhaust_cross_section_area: 8 * units.cm2,
    spark_plug_angle: 0 * units.deg
)
```

How rotary engines are modeled:

- The housing is the epitrochoid traced by the rotor's apexes. Each rotor has three working chambers whose volumes are computed exactly from the geometry; the rotor flank is the largest shape that clears the housing. The face displacement is `3 * sqrt(3) * e * R * depth`, and like real rotaries the engine's displacement is reported as one face per rotor (a 13B is 2 x 654 cc).
- Housing angles (ports and spark plug) are measured counter-clockwise from the housing's minor axis on the spark plug side, which points in the crankshaft's `tdc` direction. The rotor turns clockwise, so a face passes the intake port, then the spark plug (at 0 degrees) and then the exhaust port.
- A full cycle takes three crankshaft revolutions (`3 * 360 * units.deg`). Each rotor has one spark plug that fires once per revolution and ignites whichever face is passing it, so connect each rotor's wire three times, 360 degrees apart. With rotors 180 degrees apart on the shaft, the second rotor fires 180 degrees later.
- A peripheral port is uncovered by the apex seals, so it stays open for 360 degrees of shaft rotation plus three times its angular width. Narrow ports and little overlap give the smoothest idle. The port's flow is a function of the fraction of the port that is uncovered; `default_wankel_peripheral_port(start, end, max_flow)` ramps linearly to `max_flow`.
- The `angle` input of `wankel_rotor` is accepted for compatibility but has no effect: the rotor's position always follows the eccentric shaft.

## Themes

Theme scripts change the colors and the units shown on the gauges. They import the light-weight `engine_sim_theme.mr` and call `set_application_settings`:

```
import "engine_sim_theme.mr"

unit_names units()

set_application_settings(
    speed_units: units.kph,        // mph or kph
    pressure_units: units.kPa,     // inHg, kPa, mbar, bar or psi
    torque_units: units.Nm,        // lb_ft or Nm
    power_units: units.kW,         // hp or kW
    color_background: 0x0E1012,
    color_foreground: 0xFFFFFF
    // color_shadow, color_highlight1, color_highlight2, color_pink, color_red,
    // color_orange, color_yellow, color_blue, color_green
)
```

Put theme scripts in `assets/themes` (in the installation or the user data folder) and cycle through them with `[` and `]`, or load one with `--theme`. An engine script may also call `set_application_settings`; a theme selected by the user takes priority.

## Testing scripts

`engine-sim-script-check` (built alongside the application) compiles scripts and runs each engine without a window:

```
engine-sim-script-check --data <install or source dir> --require-running my_engine.mr
engine-sim-script-check --trace --seconds 4 my_engine.mr     # per-frame state of cylinder 1
```

It cranks the engine for 1.5 seconds (`--crank` changes this), then opens the throttle, and reports the peak and final speed. `--trace` prints the first cylinder's volume, pressure, temperature, port openings and mixture every frame, which is useful when an engine doesn't start.
