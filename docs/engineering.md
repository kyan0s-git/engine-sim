# Engineering physics model

Engine Simulator was built to sound and feel right in real time. Its
original ("legacy") physics compensates for simplified gas properties with
tuned numbers: air with 25 % oxygen, a rich mixture, a high heating value and
a capped burning efficiency. These errors cancel out well enough for sound, but
efficiency, power and fuel consumption can't be trusted for engineering work.

The **engineering** physics model replaces these approximations with standard
engine-modelling methods, and the **dynamometer tool** measures what an engine
does. The legacy model stays the default, so the bundled engines keep
sounding the way they were tuned.

The research behind every model and coefficient is in
[`research/`](../research/); the plan for further work is in
[`research/ROADMAP.md`](../research/ROADMAP.md).

## Selecting the model

```
engine engine(
    name: "My engine",
    physics_model: "engineering",
    ...
)
```

Rotary engines accept the same input on `wankel_engine`.

## What changes

| | Legacy | Engineering |
| --- | --- | --- |
| Air | 25 % O₂, 75 % "inert" | Dry air: N₂ 78.08 %, O₂ 20.95 %, Ar 0.94 %, CO₂ 0.03 % |
| Gas properties | Diatomic ideal gas, γ = 1.4, molar mass of air | Eight species (fuel, O₂, N₂, Ar, CO₂, H₂O, CO, H₂) with NASA 7-coefficient polynomials: c_p, γ and molar mass vary with temperature and composition |
| Flow between volumes | Carries internal energy | Carries enthalpy, so blowdown cools the remaining gas and filling heats the charge; flow rates scale with 1/√M |
| Mixture | Fixed by `molecular_afr` and a very rich idle circuit | Metered by the intake's `lambda`; the idle circuit supplies the same mixture |
| Fuel | Molar mass 100 g/mol, 48.1 MJ/kg, efficiency capped at 0.8 with random variation | Built-in fuel library (formula, heating value, latent heat); heat release follows from formation enthalpies |
| Combustion products | Generic inert gas | CO₂ and H₂O; rich mixtures also form CO and H₂ (water-gas shift equilibrium); unburned fuel (crevices, quenching) set by `combustion_efficiency` |
| Charge cooling | – | Fuel evaporation cools the fresh charge (`evaporative_cooling`) |
| Flame | Geometric front moving at the turbulent flame speed, speed fixed at ignition | Mass entrainment, dm/dt = ρᵤ A_f S_T, with the flame area from the burned gas volume; laminar flame speed recomputed every step from the equivalence ratio, unburned gas temperature, pressure and residual gas |
| Wall heat transfer | Constant coefficient | Woschni correlation |
| Friction | Constant crankshaft friction torque plus piston friction | Patton–Nitschke–Heywood component model (or Chen–Flynn) |

Everything else (valve and port flow, runners, exhaust, mechanics, sound) is
shared by both models.

### Thermodynamics

Each gas volume tracks the amount of every species. Properties come from
NASA 7-coefficient polynomials (GRI-Mech 3.0 for the combustion species and
refits of the NASA CEA database for liquid fuels). Temperature is found from
the internal energy by Newton iteration, and pressure follows from the ideal
gas law. Combustion conserves the elements and the total internal energy, so
the heat released follows from the formation enthalpies: complete
combustion releases the fuel's lower heating value, and CO and H₂ in rich
exhaust carry away the energy they didn't release.

Dissociation (equilibrium chemistry at peak temperatures) is not modelled
yet, so peak temperatures are 100–300 K higher than in a real engine at
stoichiometric conditions.

### Combustion

At the spark the mixture's equivalence ratio must lie between the fuel's
flammability limits. The flame then entrains unburned gas at

```
dn/dt = ρᵤ · A_f · S_T
```

where ρᵤ is the density of the unburned gas (compressed isentropically
since the spark) and A_f the area of the flame front. The burned gas is
taken at the adiabatic flame temperature, which gives its volume, and the
front is shaped like a cylinder centred below the spark plug that grows
outwards and downwards until it reaches the cylinder wall and the piston.
The turbulent flame speed S_T is the fuel's `turbulence_to_flame_speed_ratio`
applied to the turbulence intensity (half the mean piston speed) and the
laminar flame speed

```
S_L = (B_m + B_φ (φ - φ_m)²) · (Tᵤ / 298 K)^α · (p / 1 atm)^β · (1 - c x_r^d)
```

(Rhodes–Keck coefficients for gasoline, Metghalchi–Keck for the other
fuels).

### Heat transfer

Woschni (1967):

```
h = 3.26 B^-0.2 p^0.8 T^-0.55 w^0.8          [W/(m² K)], B [m], p [kPa]
w = C1 · S̄p + C2 · (V_d T_r / (p_r V_r)) · (p - p_motored)
```

C1 = 6.18 while a valve is open and 2.28 otherwise. C2 = 3.24·10⁻³ m/(s K)
from ignition until the exhaust opens. The reference state is taken when the
intake closes. Heat flows to walls at `wall_temperature`, and the coefficient
can be scaled with `heat_transfer_multiplier`.

### Friction

The Patton–Nitschke–Heywood model (SAE 890836) adds up crankshaft
(seals, main bearings), reciprocating (piston skirt, rings, connecting rod
bearings, gas pressure loading of the rings), valvetrain and accessory
friction from the engine's geometry, speed and manifold pressure. The
resulting friction mean effective pressure (FMEP) is applied as a crankshaft
friction torque `FMEP · V_d / 4π`. Pumping losses are not part of it because
the simulation computes them directly.

Bore, stroke, cylinder count, compression ratio and the number of main
bearings (crankpins + 1) come from the engine. Bearing sizes default to
typical proportions of the bore. The valvetrain is described by the inputs
below. Below 500 rpm the friction is held at its 500 rpm value.

`friction_model: "chen_flynn"` uses `FMEP = A + B·p_max + C·S̄p + D·S̄p²`
instead, with the last cycle's peak cylinder pressure; it is the usual choice
for calibrating against measured friction.

Rotary engines keep the eccentric shaft's `friction_torque`.

## Script inputs

### `engine` / `wankel_engine`

| Input | Default | Description |
| --- | --- | --- |
| `physics_model` | `"legacy"` | `"legacy"` or `"engineering"` |
| `wall_temperature` | `450 * units.K` | Mean combustion chamber wall temperature (engineering) |
| `heat_transfer_multiplier` | `1.0` | Scales the Woschni coefficient |

### `engine` only

| Input | Default | Description |
| --- | --- | --- |
| `friction_model` | `"auto"` | `"auto"` (Patton–Nitschke–Heywood with the engineering model, legacy otherwise), `"legacy"`, `"patton_nitschke_heywood"` or `"chen_flynn"` |
| `friction_multiplier` | `1.0` | Scales the friction model |
| `valvetrain_type` | `"dohc_bucket"` | `"dohc_bucket"`, `"sohc_finger"`, `"sohc_rocker"` or `"pushrod"`; add `_roller` for roller followers (e.g. `"pushrod_roller"`) |
| `valves_per_cylinder` | `4` | |
| `max_valve_lift` | `10 * units.mm` | |
| `main_bearing_diameter` | `0` | 0 = 0.65 × bore |
| `main_bearing_length` | `0` | 0 = 0.4 × main bearing diameter |
| `main_bearing_count` | `0` | 0 = crankpins + 1 |
| `chen_flynn_constant` | `0.4 * units.bar` | A |
| `chen_flynn_peak_pressure` | `0.005` | B |
| `chen_flynn_piston_speed` | `0.08 * units.bar / (units.m / units.sec)` | C |
| `chen_flynn_piston_speed_squared` | `0.0009 * units.bar / ((units.m / units.sec) * (units.m / units.sec))` | D |

### `fuel`

| Input | Default | Description |
| --- | --- | --- |
| `type` | `"gasoline"` | Fuel from the built-in library (below) |
| `combustion_efficiency` | `0.97` | Fraction of the fuel reached by the flame that burns |

`turbulence_to_flame_speed_ratio` is used by both models. The other legacy
inputs (`molecular_mass`, `energy_density`, `molecular_afr`,
`max_burning_efficiency`, ...) are ignored by the engineering model.

| Fuel | Formula | AFR (stoich.) | LHV (MJ/kg) | Latent heat (kJ/kg) |
| --- | --- | --- | --- | --- |
| `gasoline` | C₇H₁₃ surrogate | 14.6 | 43.4 | 350 |
| `iso-octane` | C₈H₁₈ | 15.1 | 44.3 | 308 |
| `n-heptane` | C₇H₁₆ | 15.2 | 44.6 | 365 |
| `ethanol` | C₂H₅OH | 9.0 | 26.8 | 924 |
| `methanol` | CH₃OH | 6.5 | 19.9 | 1185 |
| `e10` | 10 % ethanol by volume | 14.0 | 41.7 | 410 |
| `e85` | 85 % ethanol by volume | 9.8 | 29.2 | 842 |
| `methane` (`natural_gas`) | CH₄ | 17.2 | 50.0 | – |
| `propane` (`lpg`) | C₃H₈ | 15.7 | 46.3 | – |
| `hydrogen` | H₂ | 34.3 | 120.0 | – |

Heating values are for the liquid fuel. The vapour, which is what the engine
burns, releases the heating value plus the latent heat.

### `intake`

| Input | Default | Description |
| --- | --- | --- |
| `lambda` | `1.0` | Air-fuel ratio relative to stoichiometric (0.85–0.9 for best power) |
| `evaporative_cooling` | `0.5` | Fraction of the fuel's latent heat taken from the air (the rest comes from the port walls) |

## Engine metrics

Every cylinder records its last complete four-stroke cycle: the p·dV work of
each stroke, peak pressure and temperature, the spark timing, the 10/50/90 %
burn angles, the fuel and air it inducted and the heat it lost to the walls.
`calculateEngineMetrics()` (`include/engine_metrics.h`) combines them with
the dynamometer torque into:

* gross and net IMEP, PMEP, BMEP, total FMEP (IMEP − BMEP) and the friction
  model's FMEP
* indicated and brake torque and power, mechanical, indicated and brake
  efficiency (lower heating value)
* ISFC and BSFC, air and fuel flow
* volumetric efficiency (air only, relative to 1 atm and 25 °C), air-fuel
  ratio and λ
* spark advance, CA10, CA50, CA90, peak pressure and its angle, peak
  temperature, wall heat loss as a fraction of the fuel energy

## Dynamometer tool

`engine-sim-dyno` loads an engine, starts it and holds it at a series of
speeds with the dynamometer. At each speed it lets the engine settle and then
averages the measurements, writing one CSV row per speed:

```
engine-sim-dyno --data . --from 1000 --to 7000 --step 500 \
    --frequency 20000 assets/engines/engineering/audi_i5_engineering.mr > audi.csv
```

| Option | Default | |
| --- | --- | --- |
| `--from`, `--to`, `--step` | engine's dyno range, 500 rpm | Speeds [rpm] |
| `--throttle` | `1.0` | Throttle position |
| `--settle` | `1.5` | Simulated seconds before measuring at each speed |
| `--measure` | `1.0` | Simulated seconds of averaging |
| `--frequency` | engine's `simulation_frequency` | Simulation steps per second |
| `--data` | current directory | Folder that contains `assets/` and `es/` |

The `_j1349` columns are corrected to SAE J1349 conditions (99 kPa dry air,
25 °C) with `CF = 1.18 (99 / p_d) √(T / 298) − 0.18`. The simulation runs
at 1 atm and 25 °C, so the factor is 0.973.

In the packaged macOS version the tool is next to the app; use
`--data "Engine Simulator.app/Contents/Resources"`.

### Time step

The simulation exchanges work between the gas and the pistons once per time
step. At the default 10 kHz this overstates the losses by up to about 1 bar
of FMEP at high speed (seen as a gap between the total FMEP and the friction
model's FMEP); at 20 kHz the gap is 0.1–0.6 bar. Use `--frequency 20000` or
more for engineering results.

## Reference engine

`assets/engines/engineering/audi_i5_engineering.mr` is the bundled Audi 2.2
inline five set up for the engineering model. At wide open throttle and
20 kHz:

| rpm | Torque (N·m) | Power (kW) | BMEP (bar) | BSFC (g/kWh) | VE | CA50 (° ATDC) | FMEP model (bar) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2000 | 162 | 34 | 9.2 | 259 | 0.81 | 11.7 | 0.91 |
| 4000 | 176 | 74 | 9.9 | 247 | 0.84 | 6.5 | 1.30 |
| 6000 | 165 | 103 | 9.3 | 266 | 0.85 | −0.6 | 1.81 |
| 7000 | 155 | 113 | 8.7 | 279 | 0.83 | −1.1 | 2.11 |

These are in the range of production naturally aspirated engines of this
size (BMEP 9–11 bar, BSFC 240–280 g/kWh at full load). They are not yet
validated against measurements of this engine; see Milestone F of the
roadmap.

## Converting an engine

Engines tuned for the legacy model usually need some changes:

1. Set `physics_model: "engineering"` and pick the fuel with
   `fuel(type: ...)`.
2. Set the intake's `lambda` (1.0 for part load, about 0.88 for best power).
3. Describe the valvetrain (`valvetrain_type`, `valves_per_cylinder`,
   `max_valve_lift`) so that the friction is right; the crankshaft's
   `friction_torque` no longer applies.
4. Check idle. The legacy idle circuit supplied a very rich mixture that hid
   two problems. With a closed throttle, engines with large valve overlap
   push exhaust gas back into the intake, and a slow-burning charge with
   that much residual gas can't sustain idle. Realistic friction also needs
   more idle air. Raise the idle air (lower `idle_throttle_plate_position`
   or increase `idle_flow_rate`), and reduce the overlap if needed.
5. Retune the ignition timing for the best torque: CA50 around 8–10° after
   TDC (`engine-sim-dyno` reports it).

## Verification

`test/thermodynamics_tests.cpp` and `test/engine_model_tests.cpp` check the
models against closed-form solutions and published values:

* air properties (c_p, γ, speed of sound) against NASA CEA
* stoichiometric air-fuel ratios and heating values of the fuel library
* rich combustion products against a Cantera calculation
* γ of unburned and burned mixtures
* constant-volume adiabatic flame temperatures (φ = 0.8, 1.0, 1.2) against
  Cantera
* isentropic compression and blowdown against the variable-c_p isentrope
* tank filling against the first law and choked flow against the
  closed-form flow rate
* conservation of mass and energy in flows between volumes
* Patton–Nitschke–Heywood friction magnitudes against the Bishop
  correlation, the Chen–Flynn formula and the SAE J1349 factor

## Limitations

* The friction coefficients (Patton–Nitschke–Heywood, Chen–Flynn), the
  Woschni constants, the flame speed correlations and the SAE J1349 formula
  were taken from secondary sources and still have to be checked against the
  original papers; they are marked [R] in the research notes.
* No dissociation; no knock; no cycle-to-cycle variation in the engineering
  model yet.
* The flame model is quasi-dimensional (entrainment with a simple flame
  shape) without burn-up of entrained eddies; Wiebe and Blizard–Keck burn
  models are planned (Milestone B).
* Pipes are single volumes rather than one-dimensional, so wave tuning is
  approximate (Milestone C).
* The engineering model runs about 1.5 times slower than the legacy model;
  engines with many cylinders may not run in real time on slower computers.
