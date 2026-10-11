# Rebuilding engine-sim's physics for engineering-grade answers

engine-sim's mechanical skeleton is sound, but its thermofluid core is not predictive. The parts that already match engineering practice are the exact crank-slider multibody model, the speed-servo dynamometer measurement, the flow-bench-calibrated compressible orifice law and the Wankel geometry. The core itself does not hold up. Every gas has constant γ = 1.4 and a molar mass of 28.97 g/mol. Flowing gas carries internal energy c_v·T instead of enthalpy c_p·T. A random "burning efficiency" capped at 0.8 decides how much fuel burns, and the flame speed behind it is evaluated with λ where the correlation expects φ, then frozen at the spark. "Air" is 25 % O₂. Wall heat transfer uses a constant h = 100 W/m²K, crank friction is a constant Coulomb torque, and there are no one-dimensional pressure waves. These errors partly cancel. That is why shipped engines produce plausible torque curves, and it is also why fixing any one of them alone will shift every asset's output. The cure is the architecture GT-POWER, WAVE and BOOST share, scaled to real time:

- **Cylinder and volumes:** an open single-zone cylinder (two-zone for knock and NOx) with enthalpy-carrying flows and NASA-7 lumped-species properties.
- **Combustion:** a Wiebe, then quasi-dimensional, burn model driven by a φ-correct Rhodes–Keck flame speed updated every step.
- **Heat transfer and friction:** Woschni heat transfer, and Patton–Nitschke–Heywood (PNH) or Chen–Flynn friction mean effective pressure (FMEP).
- **Gas exchange:** finite-volume 1-D pipes (HLLC flux) for runners and headers.
- **Sound:** radiating the derivative of tailpipe mass flow instead of runner pressure.

Almost all of this is cheap. Thermodynamics and combustion together cost well under 10 % of one core for 12 cylinders at 50 kHz. The 1-D pipes are the only item that needs a budget: about 11 million cell updates per second for a V8 at 40 mm cells. Calibrated commercial 1-D models reach 2–5 % error on torque, VE and BSFC, with worst points around 7 %, and that is the bar to aim for. Reaching it needs a strict order of work: instrument and verify first, then fix outright bugs, then thermodynamics, heat transfer and friction, combustion, 1-D pipes, mechanics, acoustics and boosting. Validation against EPA's public Toyota A25A-FKS benchmark data runs throughout. A legacy-physics switch and golden-file regression tests protect the 36 shipped engine scripts.

## Compensating errors make today's power curves look plausible

engine-sim represents every gas space as a 0-D lumped control volume (`GasSystem`). It holds moles, internal energy, volume, a 2-D bulk momentum vector and a three-species mole-fraction mix of fuel, inert and O₂ (`include/gas_system.h:12-30`). The volumes are chained as atmosphere → throttle and idle orifices → plenum → runner → valve → cylinder → valve → exhaust runner/primary → collector → atmosphere. Every link is the isentropic compressible-orifice equation, with C_d·A/√M lumped into a constant k (`src/gas_system.cpp:191-237`). The crank, rods and pistons are 2-D rigid bodies solved by a velocity-level projected Gauss–Seidel constraint solver with Baumgarte stabilisation and symplectic Euler integration (`src/simulator.cpp:31-46`). This is the standard 0-D filling-and-emptying architecture, minus the 1-D pipes and the property and combustion sub-models that commercial cycle codes add ([GT-SUITE cylinder pressure analysis](https://www.gtisoft.com/gt-suite-applications/propulsion-systems/cylinder-pressure-analysis/?mode=grid); [software comparison, academia.edu](https://www.academia.edu/57739777/Comparison_of_Engine_Simulation_Software_for_Development_of_Control_System)).

The audit ran isolated numerical tests by compiling `src/gas_system.cpp` unmodified into a scratch harness. Total energy and moles were conserved to machine precision (relative drift −8×10⁻¹⁶ and 1.4×10⁻¹⁵), but the energy went to the wrong places:

- **Blowdown.** A 1 L vessel at 5 bar and 1500 K vented to atmosphere stayed at **1500.0 K** when its pressure reached 2.5 bar. The isentropic answer is **1230.5 K**.
- **Filling.** A 1 L vessel filled from 0.1 bar to 1 atm from a 300 K reservoir ended at **300.0 K**. The control-volume energy balance gives **403.9 K**.
- **Combustion.** Complete constant-volume combustion of the default intake charge from 700 K and 10 atm rose by **4182 K**, to about 4900 K. With the default 0.8 efficiency it reached 4089 K.

For scale, a computed equilibrium constant-volume flame temperature for stoichiometric iso-octane–air from 700 K and 20 bar is **2907 K** ([gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml); [NASA CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp)).

Three compensations hold the shipped engines together. Model "air" is 25 % O₂ against the real 20.95 % ([CEA air composition](https://github.com/nasa/cea/blob/main/data/thermo.inp)), which gives each mole of charge about 19 % more oxygen. The fixed charge is λ = 0.8 in O₂ terms (`src/intake.cpp:63-73`). The default fuel LHV is 48.1 MJ/kg against a reference 43.55 MJ/kg for gasoline ([IFP Energies Nouvelles table](https://ogst.ifpenergiesnouvelles.fr/articles/ogst/full_html/2021/01/ogst210229/T1.html)). On the other side, `max_burning_efficiency` throws away 20 % of the fuel by default. Of the 36 engine scripts under `assets/`, 20 mention `max_burning_efficiency`, with active values from 0.75 to 1.0 (a few more are commented out). Crank `friction_torque` ranges from 0 to 50 lb·ft. Rolling resistance ranges from 100 N to 10 000 N, with some scripts using 0.015·m·g (grep of this tree). These are calibration knobs absorbing model-form error, not physical parameters.

The table ranks the audit findings for engineering use. Each line reference is to this repository.

| # | Area | Current implementation (file:line) | Standard practice | Severity |
|---|---|---|---|---|
| 1 | Gas dynamics | 0-D volumes only; runner and header length set volume and audio delay only (`src/combustion_chamber.cpp:82-114`) | 1-D pipe network with wave action | Critical |
| 2 | Properties | f = 5, γ = 1.4, c_v = 2.5R, M = 28.97 g/mol for all gases (`include/gas_system.h:45,134-136,242-244`) | NASA polynomials per species and lump | Critical |
| 3 | Flow energy | Moved gas carries U/n = c_v·T (`src/gas_system.cpp:424-426`) | Enthalpy flux ṁ·h₀ | Critical |
| 4 | Combustion | Random efficiency from C `rand()` × max 0.8; volume fraction used as mass fraction (`src/combustion_chamber.cpp:196-220,345-384`) | Wiebe, or two-zone entrainment | Critical |
| 5 | Flame speed | Metghalchi–Keck evaluated with λ (`src/fuel.cpp:56`); frozen at ignition (`src/combustion_chamber.cpp:221-227`); pressure terms unused (`src/fuel.cpp:46`) | S_L(φ, T_u, p, x_r) every step | Major |
| 6 | Mixture | 25 % O₂ air; fixed λ = 0.8 "perfect carb" (`src/intake.cpp:63-73`); LHV 48.1 MJ/kg (`es/objects/objects.mr:53-67`) | 20.95 % O₂; metered λ; LHV ≈ 43–44 MJ/kg | Major |
| 7 | Heat transfer | Newton cooling, constant h = 100 W/m²K, 90 °C wall, cylinder only (`src/combustion_chamber.cpp:261-278`) | Woschni or Hohenberg; per-surface walls; pipe heat transfer | Major |
| 8 | Friction | Constant Coulomb crank torque (`src/piston_engine_simulator.cpp:83-85`); Stribeck v_St = √2·v_brk instead of v_brk/√2; script friction node unwired (`src/combustion_chamber.cpp:404-420`, `include/combustion_chamber.h:42-47`) | FMEP correlations (Chen–Flynn, PNH/Sandoval) | Major |
| 9 | AFR display | Divides n_O₂ by 0.21 although model air is 25 % O₂; uses M_O₂ and iso-octane M (`src/engine.cpp:337-354`) | Mass AFR on a dry-air basis | Major |
| 10 | Vehicle | 2000 N constant rolling-resistance default (`es/objects/objects.mr:830`); kinetic-energy-preserving gear change (`src/transmission.cpp:79-107`) | C_rr·m·g or A + Bv + Cv²; vehicle speed continuous across a shift | Major |
| 11 | Rotary | Equivalent-bore flame and heat transfer; no apex leakage (`scripting/include/wankel_engine_node.h:251`) | Chamber-specific geometry and leakage paths | Major (rotary) |
| 12 | `react()` | Independent minima consume O₂ with zero fuel; C8H16 label on C8H18 stoichiometry; mass not conserved (`src/gas_system.cpp:82-129`) | Element-conserving stoichiometry | Major/Minor |
| 13 | Solver | Baumgarte β = 1; Gauss–Seidel stops at a 10 % relative change; θ-overwrite "drift hack" (`src/simulator.cpp:133-138`; `dependencies/simple-2d-constraint-solver/src/optimized_nsv_rigid_body_system.cpp:8`) | Reduced coordinates, or β ≈ 0.1–0.2 | Minor (torque) / Major (NVH) |
| 14 | Dyno sweep | Reads T − I·α at 500 rpm/s (`src/engine_sim_application.cpp:1409-1431`) | Inertia-corrected sweep | Minor |
| 15 | VE gauge | Molar flow including fuel; idle circuit excluded; 25 °C/1 atm reference (`src/right_gauge_cluster.cpp:259-271`) | Dry-air mass VE, referenced to ambient and to manifold | Minor |
| 16 | Audio | Runner gauge pressure; delay at 343 m/s with a hard-coded 10 kHz rate; 1.9 kHz low-pass; automatic gain control (AGC) (`src/piston_engine_simulator.cpp:252-259,419-458`; `src/synthesizer.cpp`) | Tailpipe dṁ/dt monopole, geometry-derived acoustics | Minor (performance) / Critical (acoustics) |

Several components are worth keeping as they are:

- **Flow-bench calibration is consistent.** `k_28inH2O(260 cfm)` ≈ 7.15×10⁻³ corresponds to C_d·A ≈ 12.2 cm². An incompressible check gives 11.7 cm², and the compressible nozzle conversion of about 4.52–4.59 mm² per CFM at 28 inH₂O gives about 11.8 cm².
- **Chamber volume** comes from exact kinematics (`src/combustion_chamber.cpp:125-137`).
- **The dyno measurement** is how a speed-controlled absorption dyno works. It averages constraint torque over 512 crank-angle bins of the 720° cycle (`src/simulator.cpp:140-216`).
- **Wankel kinematics** use the epitrochoid, Green's-theorem chamber volume and virtual-work torque (`src/wankel_rotor.cpp`), and are energy-consistent.

**Labels used in this report.** Each coefficient carries one of four tags:
- **[V]**: confirmed against a retrieved source during research.
- **[C]**: computed from primary databases (NASA CEA, GRI-Mech 3.0 through Cantera 3.2) using the scripts in `research/scripts/`.
- **[R]**: the standard value recalled from the cited primary paper or from Heywood, but not text-checked. Check these before release.
- **[E]**: an engineering estimate with no source.

## An enthalpy-correct, variable-property gas core removes ~1000 K of error

### Every volume must obey the open-system first law

The governing equations for a cylinder, plenum or runner are the open single-zone control-volume balances. These are the filling-and-emptying model defined by mass and energy conservation ([Lisowski, KONES 2010](https://yadda.icm.edu.pl/baztech/element/bwmeta1.element.baztech-article-BUJ7-0017-0080/c/httpwww_bg_utp_edu_plartjok42010jokones2010no4vol17lisowski.pdf); [US patent 9482164](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/9482164)). In the form below, u and h include formation enthalpy, and h_j is the upstream stagnation enthalpy: the source volume's for outflow, the neighbour's for inflow.

```
dm/dt        = Σ_j ṁ_j  − ṁ_blowby (+ ṁ_fuel,DI)
d(m·u)/dt    = Q̇_wall − p·dV/dt + Σ_j ṁ_j·h_j
m·c_v·dT/dt  = Q̇ − p·dV/dt + Σ_j ṁ_j·(h_j − u) − m·Σ_k (∂u/∂Y_k)·dY_k/dt
p            = m·R_mix·T / V
d(m·Y_k)/dt  = Σ_j ṁ_j·Y_k,j + (burn-rate source)
Constant-property limit: dp/dt = [(γ−1)(Q̇ + Σ ṁ_j h_j) − γ·p·dV/dt] / V
```

The single most important line change is `src/gas_system.cpp:424-426`. Each transferred mole must carry the source's molar enthalpy h̄ = ū + R·T (plus the bulk kinetic energy, if the momentum model is kept), not `kineticEnergyPerMol()` = ū. With that change the blowdown and filling tests reproduce 1230.5 K and 403.9 K. Those tests are the acceptance gate for the change. Two other gas-core items belong in the same pass. The upstream temperature in `flowRate()` should be the stagnation value, T₀ = T + v²/(2c_p), rather than the static value (`src/gas_system.cpp:401`). And `pressureEquilibriumMaxFlow()`, computed at line 407 but unused, should cap the per-step transfer, as it already does for environment flows (lines 541-544). Without the cap, explicit sub-steps can overshoot pressure equilibrium in small volumes near TDC during overlap.

Real-gas corrections are not needed for gasoline engines. The Peng–Robinson compressibility of N₂, which makes up 70–75 mol % of the charge, is **Z = 1.020** at 800 K/60 bar (SI top dead centre), 1.009 at 2500 K/80 bar and 1.046 at 900 K/150 bar (boosted diesel) [C]. That is a 1–2 % pressure error, inside every other uncertainty ([SAE 2010-01-0627](https://saemobilus.sae.org/articles/real-gas-effects-high-pressure-engine-environment-2010-01-0627) found real-gas modelling more accurate mainly at extreme pressures). For heavy-duty diesel work, an Abel–Noble covolume form, p = ρRT/(1 − bρ) with b ≈ 8.6×10⁻⁴ m³/kg, costs two flops and leaves the energy equation unchanged [C/E].

### NASA-7 lumped species are exact and cost five multiply-adds

The recommended property scheme tracks three mass-fraction "lumps" per volume: fresh air including humidity, fuel vapour, and burned products at the current φ. Each lump's NASA-7 coefficients are mole-weighted sums of its species' coefficients. This is exact for an ideal-gas mixture of frozen composition, because c_p/R is linear in the coefficients ([Cantera NasaPoly1](https://cantera.org/3.2/cxx/d2/d2a/NasaPoly1_8h_source.html)):

```
c_p/R    = a1 + a2·T + a3·T² + a4·T³ + a5·T⁴
h/(R·T)  = a1 + a2·T/2 + a3·T²/3 + a4·T³/4 + a5·T⁴/5 + a6/T      (includes h_f at 298.15 K)
s°/R     = a1·ln T + a2·T + a3·T²/2 + a4·T³/3 + a5·T⁴/4 + a7
u = h − R·T;  c_v = c_p − R;  R = 8.314462618 J/(mol·K)
```

The species coefficients below are exact copies from GRI-Mech 3.0 [V]. The iso-octane and dry-air rows are least-squares refits of NASA CEA NASA-9 data, with h matched to about 10 J/mol at 1000 K [C] ([gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml); [CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp)). The low range runs up to 1000 K and the high range from 1000 K.

| Species | Low range a1…a7 | High range a1…a7 |
|---|---|---|
| N₂ (300–5000 K) | 3.298677, 1.4082404e-3, −3.963222e-6, 5.641515e-9, −2.444854e-12, −1020.8999, 3.950372 | 2.92664, 1.4879768e-3, −5.68476e-7, 1.0097038e-10, −6.753351e-15, −922.7977, 5.980528 |
| O₂ (200–3500 K) | 3.78245636, −2.99673416e-3, 9.84730201e-6, −9.68129509e-9, 3.24372837e-12, −1063.94356, 3.65767573 | 3.28253784, 1.48308754e-3, −7.57966669e-7, 2.09470555e-10, −2.16717794e-14, −1088.45772, 5.45323129 |
| CO₂ (200–3500 K) | 2.35677352, 8.98459677e-3, −7.12356269e-6, 2.45919022e-9, −1.43699548e-13, −48371.9697, 9.90105222 | 3.85746029, 4.41437026e-3, −2.21481404e-6, 5.23490188e-10, −4.72084164e-14, −48759.166, 2.27163806 |
| H₂O (200–3500 K) | 4.19864056, −2.0364341e-3, 6.52040211e-6, −5.48797062e-9, 1.77197817e-12, −30293.7267, −0.849032208 | 3.03399249, 2.17691804e-3, −1.64072518e-7, −9.7041987e-11, 1.68200992e-14, −30004.2971, 4.9667701 |
| CO (200–3500 K) | 3.57953347, −6.1035368e-4, 1.01681433e-6, 9.07005884e-10, −9.04424499e-13, −14344.086, 3.50840928 | 2.71518561, 2.06252743e-3, −9.98825771e-7, 2.30053008e-10, −2.03647716e-14, −14151.8724, 7.81868772 |
| H₂ (200–3500 K) | 2.34433112, 7.98052075e-3, −1.9478151e-5, 2.01572094e-8, −7.37611761e-12, −917.935173, 0.683010238 | 3.3372792, −4.94024731e-5, 4.99456778e-7, −1.79566394e-10, 2.00255376e-14, −950.158922, −3.20502331 |
| Ar | 2.5, 0, 0, 0, 0, −745.375, 4.366 | same |
| iso-octane C₈H₁₈ gas [C] | −1.43984399, 8.96796087e-2, −2.38210565e-5, −2.52511610e-8, 1.55447845e-11, −30245.6829, 33.6027464 | 12.8875565, 6.12991780e-2, −2.37067433e-5, 4.55959217e-9, −3.59231952e-13, −34692.8575, −43.0050693 |
| Dry air lump [C] | 3.73575283, −1.90527849e-3, 4.68261602e-6, −3.32947493e-9, 7.90328412e-13, −1079.39549, 3.01566618 | 2.75662059, 1.99255272e-3, −9.97962072e-7, 2.49807656e-10, −2.45442833e-14, −887.498933, 7.73235391 |

Refits for n-heptane, ethanol and methanol are in the thermodynamics notes and in `research/scripts/b.py`. For an even cheaper evaluation, 1-D tables of u_sens(T), h_sens(T) and c_v(T) per lump, at 10 K spacing from 250 to 3500 K (about 330 entries each), cost one multiply-add per property. The resulting frozen γ runs from **1.356** for unburned φ = 1 mixture at 300 K, to **1.293** at 700 K, to **1.25** for burned gas at 2000–2500 K, and back to about 1.31–1.33 in the exhaust [C]. With shifting equilibrium at peak conditions, the effective isentropic exponent falls to **1.18–1.20**: 1.199 at 2500 K/50 bar and 1.175 at 3000 K/100 bar [C].

The usual linear γ(T) shortcut of Gatowski et al. is fine for unburned gas: γ = 1.3473 − 1.257×10⁻⁴·(T − 300), with a maximum error of 0.0088 over 300–1100 K [C]. For burned gas over 300–3000 K its error reaches 0.037, which is the weakness Klein and Eriksson address with a model they report as having 15× lower error ([SAE 2004-01-1464](https://saemobilus.sae.org/content/2004-01-1464); [Gatowski, SAE 841359](https://saemobilus.sae.org/papers/heat-release-analysis-engine-pressure-data-841359)). If a single-zone γ is needed, blend by composition: c_v,mix = (1 − x_b)·c_v,u(T) + x_b·c_v,b(T), R_mix likewise, then γ = 1 + R_mix/c_v,mix. A quadratic for burned φ = 1 gas, γ = 0.024954x² − 0.07548x + 1.3004 with x = (T − 1000)/1000, has a maximum error of 0.008 [C].

### Real air, real fuels and chemistry-limited combustion efficiency

Fresh air should use the CEA composition: N₂ 78.084, O₂ 20.9476, Ar 0.9365 and CO₂ 0.0319 mol %, giving **M = 28.965 g/mol** ([CEA](https://github.com/nasa/cea/blob/main/data/thermo.inp)) [V]. Humidity should enter as a separate H₂O lump through ω = 0.622·p_v/(p − p_v). The Magnus-type saturation fit, p_sat ≈ 610.94·exp(17.625t/(t + 243.04)) Pa, is [R]. The fuel definition has to become mass-based, with real molar mass and real heating value. The `molecular_afr = 12.5` parameter is actually stoichiometric moles of O₂ per mole of fuel, not a mass AFR, and is easy to misread. The properties below were computed from CEA formation enthalpies [C]. They agree with a published simulation table (gasoline AFR_st 14.5, LHV 44.3 MJ/kg; ethanol 8.94 and 27 MJ/kg) ([IntechOpen](https://www.intechopen.com/chapters/64099)) and with a tuning-industry sheet (gasoline 14.7, E85 9.8, methane 17.2) ([HP Academy](https://www.hpacademy.com/assets/Uploads/045852edfc/Stoichiometric-air-to-fuel-ratios-of-common-fuels-Sheet1-3.pdf)).

| Fuel | M (g/mol) | LHV, liquid fuel (MJ/kg) | AFR_st (mass) | h_fg at 298 K (kJ/kg) | Stoichiometric evaporative cooling |
|---|---|---|---|---|---|
| iso-octane C₈H₁₈ | 114.23 | 44.34 | 15.13 | 308 | ≈18 K |
| n-heptane C₇H₁₆ | 100.20 | 44.56 | 15.18 | 365 | – |
| ethanol | 46.07 | 26.81 | 9.00 | 924 | ≈88 K |
| methanol | 32.04 | 19.92 | 6.47 | 1185 | ≈150 K |
| methane (gas) | 16.04 | 50.03 | 17.24 | – | – |
| propane (gas) | 44.10 | 46.33 | 15.68 | – | – |
| C₁₂H₂₃ diesel/Jet-A surrogate | 167.31 | 43.03 | 14.67 | 321 | – |
| hydrogen (gas) | 2.016 | 119.96 | 34.29 | – | – |

Iso-octane overstates market gasoline's AFR_st (14.5–14.7), so expose AFR_st as a parameter while keeping the iso-octane thermodynamics. The evaporative-cooling column is ΔT ≈ (F/A)·h_fg/(c_p,air + (F/A)·c_p,fuel). It is large enough for alcohols and direct injection that a "fraction of evaporation drawing heat from the charge" parameter (≈0.3–1.0 [E]) is worth adding.

Burned-gas composition should come from element-conserving stoichiometry. That replaces the `react()` bug, where independent minima burn O₂ without fuel (`src/gas_system.cpp:82-129`). For C_nH_mO_r + (a/φ)(O₂ + 3.773 N₂ + 0.0447 Ar), with a = n + m/4 − r/2, lean products are n CO₂ + (m/2) H₂O + a(1/φ − 1) O₂ + inerts. Rich products use the water–gas-shift (WGS) equilibrium with closed-form CO: CO₂ = n − c, H₂O = 2a/φ − 2n + c − r, H₂ = m/2 − H₂O, and solve K·(n − c)·H₂ = c·H₂O. The computed K is 3.41 at 1700 K and 3.82 at 1800 K, so the customary K ≈ 3.5 corresponds to freezing at about 1720–1740 K [C]. This reproduces equilibrium major species within about 0.001 mole fraction at or below 1700 K [C].

The chemistry also sets the combustion-efficiency ceiling that `max_burning_efficiency` now fakes. At φ = 1.2 only **34.90 MJ/kg (78 % of LHV)** is released, because CO and H₂ leave unburned [C]. The model should therefore be η_c = η_chem(φ)·η_HC, where η_chem is the exact WGS released-energy ratio rather than 1/φ, and η_HC ≈ 0.95–0.98 covers crevice and quench losses [R]. A 5.7 L V8 simulation reports a combustion efficiency of 94.1–94.4 % ([FME Transactions 2018](https://doaj.org/article/be5e16669c35471db01b629fedc79e8d)) [V].

Dissociation lowers the constant-volume φ = 1 flame temperature from 3206 K to 2907 K and peak pressure by about 8 % (97 → 89 bar), but most of that energy comes back on expansion [C]. The cheapest physically consistent treatment is a 3-D table of equilibrium burned-gas u, R and ∂u/∂T over T (300–3500 K in 50 K steps), log p (0.5–300 bar) and φ (0.5–1.6). That is about 12.5k nodes per property and about 30 flops per trilinear lookup [C]. An online Olikara–Borman solve, a 4×4 Newton iteration, is borderline at 50 kHz × 8 cylinders ([SAE 750468](https://saemobilus.sae.org/content/750468)).

## A φ-correct, continuously updated burn model replaces the dice roll

### The current flame model gets the physics premise right and the details wrong

`CombustionChamber::ignite()` sets the turbulence intensity to u' = 0.5·S̄p. That matches Heywood's open-chamber estimate [R] and measured intensities, which are ≤ 0.5·S̄p without squish or tumble and about double with tumble ([Vafidis 1986 thesis](https://spiral.imperial.ac.uk/bitstream/10044/1/38177/2/Vafidis-C-1986-PhD-Thesis.pdf)). Four details are wrong:

- **The equivalence ratio is inverted.** `equivalenceRatio = (p_O₂/p_fuel)/molecularAfr` is λ (`src/combustion_chamber.cpp:176-181`). The same λ goes into the Rhodes–Keck correlation, which expects φ (`src/fuel.cpp:56`). The flame-speed peak therefore sits at a lean λ = 1.21 instead of a rich φ = 1.21. At λ = 1.2 the code returns S_L0 = 30.5 cm/s where the correct value is about 22.6 cm/s, and at λ = 0.85 it returns 23.4 cm/s instead of about 30.4. Rich-versus-lean burn-rate trends are inverted.
- **The flame speed is frozen.** S_T is computed once at the spark (`src/combustion_chamber.cpp:221-227`), although T_u rises 100–200 K during the burn and S_L ∝ T_u^~2.1.
- **There is no residual-dilution term in S_L.**
- **The flame geometry is not a propagating flame.** A cylinder of burned gas grows from the bore axis, and the volume fraction is used as the mass fraction. An analytic estimate for an 86 × 86 mm engine at 3000 rpm gives a 10–90 % burn of about **72° CA**. The literature range is 20–40° CA. This estimate depends on the assumed spark-time state and must be confirmed by instrumenting the simulator.

The fix order follows cost.

The first change is a one-line correction: `phi = m_fuel->getMolecularAfr() / afr`, plus the Rhodes–Keck residual factor. Next, recompute S_L(T_u, p) and u'(ρ_u) every step. After that, move to Wiebe, then quasi-dimensional. Throughout, cycle-to-cycle variation (CCV) should become a phasing perturbation instead of a fuel-loss lottery.

### Laminar and turbulent flame speeds

All the common correlations use the Metghalchi–Keck power-law form, S_L = S_L0(φ)·(T_u/298)^α·(p/1 atm)^β·f(x_dil) ([Verhelst et al., IJHE](https://www.sciencedirect.com/science/article/abs/pii/S0360319910020938)) [V].

| Correlation | S_L0 (cm/s) | α | β | Dilution factor | Status |
|---|---|---|---|---|---|
| Rhodes–Keck, gasoline (in engine-sim) | 30.5 − 54.9·(φ − 1.21)² | 2.4 − 0.271·φ^3.51 | −0.357 + 0.14·φ^2.77 | 1 − 2.06·x_r^0.77 | S_L0, α, β [V] in code and in the [ANSYS Forte table](https://ansyshelp.ansys.com/public/Views/Secured/corp/v252/en/forte_th/pgfId-1082878.html); dilution factor [R] ([SAE 850047](https://www.sae.org/publications/technical-papers/content/850047/)) |
| Metghalchi–Keck 1982 | B_m + B_φ(φ − φ_m)²; iso-octane 26.3, −84.7, φ_m 1.13; indolene 27.6, −78.3, 1.13; methanol 36.9, −140.5, 1.11; propane 34.2, −138.7, 1.08 | 2.18 − 0.8(φ − 1) | −0.16 + 0.22(φ − 1) | 1 − 2.1·x_b | [R] ([Combust. Flame 48:191](https://doi.org/10.1016/0010-2180(82)90127-4)) |
| Gülder 1984 | Z·W·φ^η·exp[−ξ(φ − 1.075)²]; iso-octane W 46.58, η −0.326, ξ 4.48; ethanol 46.50, 0.25, 6.34; methane 42.2, 0.15, 5.18 | 1.56 (iso-octane) | −0.22 (iso-octane) | 1 − 2.1·x_r | form [V], coefficients [R] ([SAE 841000](https://www.sae.org/publications/technical-papers/content/841000/)) |

The measured validity range is φ ≈ 0.8–1.5, 298–750 K and 0.4–40 atm ([Metghalchi 1977 thesis](https://james-keck-memorial-collection.unibs.it/ThesisAdvisor/Thesis-Metghalchi-1977.pdf)) [V]. The quadratic goes negative outside φ ≈ 0.5–1.9, so clamp S_L ≥ 0. Worked values from Rhodes–Keck: S_L0 = 28.1 cm/s at stoichiometric, 298 K and 1 atm; **0.79 m/s at 600 K and 8 atm**; 0.96 m/s at 700 K and 15 atm. A 10 % residual multiplies these by about 0.65. Hydrogen needs a dedicated correlation such as Verhelst's. The choice of S_L correlation measurably changes quasi-dimensional output ([Energy Procedia 2018](https://www.sciencedirect.com/science/article/pii/S1876610218304466)).

For the turbulent speed, Peters, Zimont, Kobayashi and Ronney-type correlations converge on similar values for engine regimes ([comparison study](https://www.researchgate.net/publication/309359406_A_Comparison_of_Turbulent_Flame_Speed_Correlations_for_Hydrocarbon_Fuels_at_Elevated_Pressures)) [V]. A Zimont-type closure gives S_T ∝ u'^0.55 ([Energies 2021](https://www.mdpi.com/1996-1073/14/14/4210)) [V]. The recommended real-time form is S_T = S_L·[1 + C_T·(u'/S_L)^n], with n ≈ 0.7 and C_T ≈ 1.5–2.5 calibrated so the 10–90 % burn lands at 20–40° CA [E]. Alternatives are Zimont, S_T = A·u'·Da^(1/4) with A ≈ 0.52, and Gülder 1990, S_T/S_L = 1 + 0.62·(u'/S_L)^½·Re_η^¼ [R] ([Peters 1999](https://doi.org/10.1017/S0022112098004212)). The existing script default for the S_T/S_L ratio, f(x) = 1.5x (`es/objects/objects.mr:38-51`), is a Damköhler-type linear law with no additive S_L term. Five assets override it.

### Wiebe first, then quasi-dimensional entrainment

The Wiebe function is the cheapest credible burn rate and also the "imposed burn rate" mode every commercial code offers:

```
x_b(θ)   = 1 − exp{−a·[(θ − θ0)/Δθ]^(m+1)}
dx_b/dθ  = (a(m+1)/Δθ)·[(θ − θ0)/Δθ]^m·(1 − x_b)
dQ/dθ    = η_c·m_f·LHV·dx_b/dθ
a = 5 or 6.908, m = 2   [V: Unipa MFB paper; R: Heywood eq. 9.23]
```

For a = 5 and m = 2, x_b = 10/50/90 % occur at (θ − θ0)/Δθ = 0.276, 0.518 and 0.772, so Δθ ≈ 2.0 × Δθ₁₀₋₉₀. For a = 6.908 they occur at 0.248, 0.465 and 0.693 ([Unipa MFB paper](https://iris.unipa.it/retrieve/e3ad8928-cc78-da0e-e053-3705fe0a2b96/innovative_MFB_function-revised.pdf); [ANSYS Chemkin SI zonal model](https://ansyshelp.ansys.com/public/views/secured/corp/v251/en/chemkin_th/pgfId-1352744.html)). Typical values are a 0–10 % flame-development angle of 10–30° CA and a 10–90 % rapid-burn angle of 20–40° CA [R]. Dilution has the strongest influence on burn angles ([VVT Wiebe fits](https://www.researchgate.net/publication/245395016_Burn_angles_and_form_factors_for_Wiebe_function_fits_to_mass_fraction_burned_curves_of_a_spark_ignition_engine_with_variable_valve_timing)) [V]. The crank-angle duration grows only mildly with speed while the peak burn rate stays roughly constant ([Alam et al. 2021](https://depcik.ku.edu/sites/depcik/files/images/Alam2021.pdf)) [V]. A Wiebe-only simulator can set Δθ₁₀₋₉₀ ∝ 6N·B/S_T(u', S_L) at the spark and calibrate one constant per engine [E].

The predictive upgrade is Blizard–Keck/Tabaczynski entrainment [R] ([SAE 740191](https://www.sae.org/publications/technical-papers/content/740191/)). It adds one ODE per cylinder:

```
dm_e/dt = ρ_u·A_f·(u' + S_L)
dm_b/dt = (m_e − m_b)/τ_b + ρ_u·A_f·S_L
τ_b = λ_T/S_L,   λ_T/L ≈ (15/Re_L)^½,   Re_L = u'·L/ν
After IVC (rapid distortion):  u' ∝ ρ_u^(1/3),  L ∝ ρ_u^(−1/3)
Optional fractal closure: D3 = 2.35·u'/(u'+S_L) + 2.05·S_L/(u'+S_L)   [R, Matthews–Chin SAE 910079]
```

The flame area A_f(r_f, piston position) for a spherical front from the plug, clipped by the chamber, should be precomputed as a 32 × 32 table per chamber geometry [E]. Measured fractal dimensions of premixed flames lie between 2.1 and 2.7 ([fractal QD model](https://www.researchgate.net/publication/257174262_A_fractal-based_quasi-dimension_combustion_model_for_SI_engines_fuelled_by_hydrogen_enriched_compressed_natural_gas)) [V]. Near the wall, models switch to an exponential-decay wall-combustion law ([Fuel 2018](https://www.sciencedirect.com/science/article/abs/pii/S0016236118315254)) [V].

For port and tumble effects, the Poulos–Heywood K–k model adds two states per cylinder [R] ([SAE 830334](https://www.sae.org/publications/technical-papers/content/830334/)):
- dK/dt = ½ṁ_in·v_in² − P − K·ṁ_out/m
- dk/dt = P − m·ε − k·ṁ_out/m
- P = 0.3307·c_β·(K/L)·√(k/m), ε = u'³/L, u' = √(2k/3m)

Both upgrades cost under 1 µs per active step, about 7.5 % of a core for 12 cylinders at 50 kHz, because combustion is active for only about 1/8 of the cycle [E].

### Ignition, knock, variability and misfire

At MBT, CA50 sits at about **8–10° ATDC** and peak pressure at about **15° ATDC** ([US7290442](https://patents.google.com/patent/US7290442); [US4867123](https://patents.google.com/patent/US4867123)) [V]. That supports a cheap "MBT finder" which iterates spark advance until CA50 ≈ 9° ATDC. Model the spark-to-1 % kernel as θ_kernel = C_k·6N·δ_k/S_L with δ_k ≈ 1–2 mm [E]. That makes the delay balloon naturally at lean, dilute or cold idle.

For knock, track an isentropic unburned-zone temperature and integrate the Livengood–Wu criterion with the Douaud–Eyzat ignition delay ([Princeton CEFRC](https://cefrc.princeton.edu/document/420); [Energy Convers. Manag. 2015](https://www.sciencedirect.com/science/article/abs/pii/S0196890414007262)) [V]:

```
∫ dt/τ = 1 at knock onset
τ [ms] = 17.68·(ON/100)^3.402·p[atm]^(−1.7)·exp(3800/T_u)
```

Sources quote the prefactor as 17.68, 17.69 or 18.69 depending on unit and reference-pressure conversion ([Energy & Fuels 2025](https://pubs.acs.org/doi/10.1021/acs.energyfuels.4c04775)). The correlation was fitted to primary reference fuels and fails with cooled EGR. The knock ping is a set of chamber acoustic modes, f_mn = ρ_mn·c/(πB), with ρ = 1.841, 3.054, 3.832 and 4.201 ([US10364760](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10364760)) [V]. With an 86 mm bore and c = 950 m/s these fall at 6.5, 10.7, 13.5 and 14.8 kHz, above the Nyquist frequency of a 10 kHz physics step, so they must be injected in the audio domain.

CCV originates in the flame kernel and appears as variation in burn phasing. The drivability limit is COV(IMEP) ≈ 10 % ([IOP Conf. Ser. 1303](https://iopscience.iop.org/article/10.1088/1757-899X/1303/1/012017/pdf)) [V]. Replace `rand()` with a seeded Gaussian draw per cycle that scales kernel delay and u' by (1 + σξ), with σ ≈ 0.05–0.15 growing as u'/S_L falls [E]. Target COV(IMEP) of 1–3 % at mid load and 5–10 % at idle [R]. Misfire and partial burn then follow from thresholds: no kernel if S_L < 0.1–0.15 m/s, and the burn freezes when S_L(T_u) collapses during expansion [E]. Unburned fuel leaving as HC allows physically caused afterfire.

The compression-ignition path combines the Hardenberg–Hase ignition delay [V for structure] ([OSTI 1462880](https://www.osti.gov/servlets/purl/1462880)) with the Watson double Wiebe [R] ([SAE 800029](https://www.sae.org/publications/technical-papers/content/800029/)):

- **Ignition delay:** τ_id[°CA] = (0.36 + 0.22·S̄p)·exp[E_A·(1/(R̃T) − 1/17190)·(21.2/(p − 12.4))^0.63], with E_A = 618840/(CN + 25), p in bar and T in K.
- **Premixed fraction:** β = 1 − 0.926·φ^0.37/τ_id^0.26.

Thermal NO needs a two-zone burned gas and the extended Zeldovich rates. Heywood's values are k₁ = 7.6×10¹³·exp(−38000/T) and an initial rate of (6×10¹⁶/√T)·exp(−69090/T)·[O₂]^½·[N₂] [R]; published fits disagree ([Wikipedia: Zeldovich mechanism](https://en.wikipedia.org/wiki/Zeldovich_mechanism)).

## One-dimensional pipes are the largest remaining fidelity gap

### Keep the orifice law, but give it forward/reverse C_D and a real throttle

The restriction equation engine-sim uses is exactly the textbook form, ṁ = C_D·A_R·p₀/√(RT₀)·Ψ(p_T/p₀), with choking at (2/(γ+1))^(γ/(γ−1)): 0.5283 at γ = 1.40, 0.5369 at 1.35 and 0.5457 at 1.30 [C] ([FAMU valve-flow notes](https://web1.eng.famu.fsu.edu/~shih/propulsion/lecture%20notes/Valve%20Flows.pdf)). Its limitations are in the coefficients:

- **One coefficient for both directions.** The same k serves forward and reverse flow, and it is constant over pressure ratio. Measured exhaust-valve C_D varies between about 0.6 and 0.9 with pressure ratio and pulsation ([Hanriot et al. 2015](https://repository.up.ac.za/bitstream/2263/44566/1/Hanriot_Influence_2015.pdf)). Accurate simulation needs C_D as a function of both lift and pressure ratio ([SAE 962527](https://saemobilus.sae.org/content/962527)).
- **Bench conditions differ from engine conditions.** The 28 inH₂O bench convention reaches a pressure ratio of only about 1.07, while blowdown runs at 3–6 ([Performance Trends](https://performancetrends.com/blog/?p=293); [air flow bench](https://en.wikipedia.org/wiki/Air_flow_bench)).

The implementation should keep `flowRate()`. It should add separate forward and reverse C_D(L/D) tables, referenced explicitly to curtain area πD_vL or seat area πD_v²/4. It should convert bench CFM at 28 inH₂O to effective area at **≈4.55 mm² per CFM** [C]. And it should linearise the flow below a small Δp, because the subsonic branch has an infinite derivative at Δp → 0, which makes explicit integration chatter.

The throttle's cos(θ·π/2) law (`src/intake.cpp:86`) is linear near closed, where the true geometric area is quadratic, and it cannot fully close (0.975 cap). Replace it with Heywood's butterfly area. Here a = d/D (shaft to bore), ψ is the plate angle and ψ₀ the sealing angle [R] ([Acta Technica Napocensis](https://atna-mam.utcluj.ro/index.php/Acta/article/download/1540/1262)):

```
A_th/(πD²/4) = [1 − cosψ/cosψ0] + (2/π)·[ (a/cosψ)·√(cos²ψ − a²cos²ψ0)
               + (cosψ/cosψ0)·asin(a·cosψ0/cosψ) − a·√(1 − a²) − asin(a) ]
```

For D = 70 mm, d = 8 mm and ψ₀ = 8°, the area ratio is 0.0055, 0.051, 0.125, 0.286, 0.494 and 0.734 at 10°, 20°, 30°, 45°, 60° and 75° [C]. Add a 1–5 mm² closed-plate leakage area and an IAC orifice with a first-order lag of 20–100 ms [E]. Also wire up the ignored `throttle_gamma` (`scripting/include/intake_node.h:38`).

### Filling-and-emptying cannot reproduce tuning

A filling-and-emptying model captures mean pumping and boost dynamics, but not ram, runner-length sensitivity or pulse scavenging. Wave action matters whenever 2L/a is a significant fraction of the valve event, which is true for almost every automotive runner and primary ([Lamborghini V12 1-D study, SAE 2005-01-0692](https://saemobilus.sae.org/content/2005-01-0692); [Linköping MVEM](https://www.fs.isy.liu.se/Publications/Articles/IFACWC_11_OL_LE.pdf)). engine-sim's lumped momentum state, with velocity decay of 0.25 at the intake, 1.0 at the exhaust and 0.5 in the cylinder, gives at best one Helmholtz-like mode. Its momentum is also created by the mass flux rather than by Δp acting on a gas column, so momentum itself is not conserved (`src/gas_system.cpp:415-525`). The design rules a 1-D model must reproduce are:

```
Helmholtz runner:   f_H = (a/2π)·√(A_r/(L_eff·V_eff)),  V_eff ≈ V_d(r_c+1)/(2(r_c−1)),  N_t = 60·f_H/K, K ≈ 2   [R]
Quarter-wave:       f_n = (2n−1)·a/(4·L_eff),  L_eff = L + 0.6133·r (unflanged)   [Levine–Schwinger, V]
Exhaust primary:    L ≈ a_exh·Δθ/(12·N)   (Δθ from EVO to mid-overlap; a_exh ≈ 550 m/s)   [E]
                    e.g. Δθ = 140°, N = 6500 rpm → L ≈ 0.99 m
```

Helmholtz and acoustic theory have been used to set optimum intake length, with simulation agreeing ([SAE 2015-01-1738](https://saemobilus.sae.org/content/2015-01-1738)) [V].

### Quasi-1-D Euler with HLLC fluxes fits in one to two cores

The model is the quasi-1-D Euler system with area change, friction and heat transfer [R] ([Winterbone & Pearson](https://openlibrary.org/isbn/1860582095); [Toro](https://doi.org/10.1007/b79761)):

```
∂U/∂t + ∂F/∂x = S
U = [ρA, ρuA, EA, ρYA]ᵀ,   F = [ρuA, (ρu² + p)A, u(E + p)A, ρuYA]ᵀ
S = [0, p·dA/dx − ½ρu|u|·f_F·πD, h·πD·(T_w − T), 0]ᵀ,   E = ρ(e + u²/2)
```

The recommended scheme is cell-centred finite volume with HLLC fluxes, first order to start, then MUSCL–Hancock with a minmod or van Leer limiter. HLLC is positivity-preserving and resolves the hot-exhaust/cold-backflow contact surface without overshoot. Overshoot there would send spurious temperature and composition spikes into the cylinder during overlap ([Toro, Spruce & Speares 1994](https://doi.org/10.1007/BF01414629); [MOOSE variable-area HLLC](https://mooseframework.inl.gov/source/userobjects/ADNumericalFlux3EqnHLLC.html)). The HLLC flux [R] is:

```
S_L = min(u_L − a_L, u_R − a_R),  S_R = max(u_L + a_L, u_R + a_R)
S*  = [p_R − p_L + ρ_L u_L(S_L − u_L) − ρ_R u_R(S_R − u_R)] / [ρ_L(S_L − u_L) − ρ_R(S_R − u_R)]
U*_K = ρ_K (S_K − u_K)/(S_K − S*)·[1, S*, E_K/ρ_K + (S* − u_K)(S* + p_K/(ρ_K(S_K − u_K)))]ᵀ
F = F_L (0 ≤ S_L) | F_L + S_L(U*_L − U_L) (S_L ≤ 0 ≤ S*) | F_R + S_R(U*_R − U_R) (S* ≤ 0 ≤ S_R) | F_R (S_R ≤ 0)
```

To keep a quiescent tapered pipe at rest, compute the area source as p_i·(A_{i+½} − A_{i−½})/Δx using the same face areas as the fluxes ([Gascón et al. 2021](https://ideas.repec.org/a/eee/apmaco/v400y2021ics009630032100120x.html)). Treat abrupt area steps as junctions. Per-cell source terms come from the Reynolds number Re = ρ|u|D/μ, with μ from Sutherland's law [R]:

- **Friction:** f_F = max(16/Re, f_Haaland/4), with 1/√f_D = −1.8·log₁₀[(ε/D/3.7)^1.11 + 6.9/Re] [R] ([Haaland 1983](https://doi.org/10.1115/1.3240948)).
- **Wall heat transfer:** h = (f_F/2)·ρ|u|c_p·Pr^(−2/3) from the Chilton–Colburn analogy, valid for Re > 10⁴ and accurate to about ±10 % in straight pipes ([Chilton–Colburn](https://en.wikipedia.org/wiki/Chilton_and_Colburn_J-factor_analogy)) [V]. Apply a 1.5–3× multiplier near exhaust ports [E].
- **Pipe wall:** a lumped thermal node, (mc)_w·dT_w/dt = Σ hA(T − T_w) − h_ext·A(T_w − T_amb) − εσA(T_w⁴ − T_amb⁴). This produces glowing headers and realistic warm-up.

Boundary conditions follow the GT-style "volume–restriction–pipe" coupling. Every pipe end exchanges a nozzle-equation flux, F = [ṁ, ṁ·u_end + p_end·A, ṁ·h₀], with a 0-D reservoir: cylinder, plenum, collector or atmosphere. Collectors and Y-merges are small 0-D volumes, which is Benson's constant-pressure junction with storage, and that conserves mass and energy exactly ([Baig, Politecnico di Milano 2022](https://www.politesi.polimi.it/retrieve/8811c217-7723-4b04-bbfd-2bd5babf55cc/2022_07_Baig_MirzaFaizanAli_02.pdf); [Benson](https://openlibrary.org/isbn/0198562101)). OpenWAM, the GPLv3 open-source 1-D engine code from CMT-UPV, is the best reference for boundary code. Its licence means the code should be reimplemented from the papers, not copied ([OpenWAM docs](https://openwam.webs.upv.es/docs/)).

The real-time budget is the deciding constraint. A V8 has about 14 m of pipe. At Δx = 40 mm that is about 350 cells; exhaust |u| + a ≈ 1000 m/s gives Δt ≈ 32 µs at CFL 0.8, so about **11 million cell updates per second**. At 20 mm it is about 700 cells, a 16 µs step and about 44 million updates per second [E]. At 20–60 ns per first-order HLLC update, the coarse grid fits one core and the fine grid needs SIMD and 2–4 threads [E]. Benchmark this early: transient 1-D pipe flow is the dominant cost in real-time crank-angle engine models ([SAE 2013-01-0344](https://saemobilus.sae.org/content/2013-01-0344)). Commercial discretisation guidance of about 0.4 × bore on the intake side and 0.55 × bore on the exhaust side ([PoliTo thesis](https://webthesis.biblio.polito.it/14730/1/tesi.pdf); [Skill-Lync](https://skill-lync.com/student-projects/assignment-11-9)) lands at 35–55 mm for a 90 mm bore, which is the affordable range. One 16–32 µs CFL step is close to the 44.1/48 kHz audio rate, so the tailpipe cell can feed the synthesiser directly. Per-pipe sub-cycling recovers the roughly 2.2× larger step the cooler intake side allows.

### Boosting as map-based components with a surge state

Turbochargers sit between pipes or volumes as 0-D map components [R] ([Guzzella & Onder](https://doi.org/10.1007/978-3-642-10775-7)):

```
ṁ_corr = ṁ·√(T01/T_ref)/(p01/p_ref);   N_corr = N/√(T01/T_ref)
T02 = T01·[1 + (Π_c^((γ−1)/γ) − 1)/η_c];   P_c = ṁ_c·c_p·T01·(Π_c^((γ−1)/γ) − 1)/η_c
P_t = ṁ_t·c_p,e·T03·η_t·[1 − Π_t^(−(γ_e−1)/γ_e)];   J·dω/dt = (η_m·P_t − P_c)/ω
η_t ≈ η_t,max·[2(BSR/BSR_opt) − (BSR/BSR_opt)²],  BSR_opt ≈ 0.6–0.7
Turbine flow: nozzle Ψ with A_t,eff, or Stodola ellipse ṁ_t√T03/p03 = K_t·√(1 − Π_t^(−2))
Greitzer surge: dṁ_c/dt = (A_c/L_c)·(Π_c,map(ω, ṁ_c)·p01 − p02)
Intercooler: T_out = T_in − ε(T_in − T_cool), ε ≈ 0.6–0.9;  Δp = k·ṁ²/ρ
```

Table interpolation extrapolates unreliably. Use physics-based fits such as the Ellipse compressor model, which extrapolates "in a physically sane way" including backflow ([Leufvén & Eriksson](https://www.fs.isy.liu.se/Publications/Books/17_LEXLKEVL.html); [SAE 1999-01-0908](https://saemobilus.sae.org/content/1999-01-0908); [SAE 2017-24-0019](https://saemobilus.sae.org/content/2017-24-0019)) [V]. Evaluate the turbine at each step from instantaneous inlet p and T, never cycle-averaged, because pulse turbines see each blowdown. The Greitzer state gives audible compressor surge on lift-off for free ([Greitzer 1976](https://doi.org/10.1115/1.3446138)).

## Woschni heat loss and component friction close the energy balance

### In-cylinder heat transfer

Woschni (1967) is the default correlation [R; C₁ and C₂ values V] ([Woschni, SAE 670931](https://doi.org/10.4271/670931); [FSU notes](https://web1.eng.famu.fsu.edu/~shih/propulsion/lecture%20notes/Convective%20Heat%20Transfer.pdf); [libICEpost](https://libice-post.readthedocs.io/en/latest/autoapi/libICEpost/src/engineModel/HeatTransferModel/Woschni/index.html)):

```
h [W/m²K] = 3.26·B^−0.2·p^0.8·T^−0.55·w^0.8      (B m, p kPa, T K; ≡ ~130 with p in bar)
w [m/s]   = C1·S̄p + C2·(V_d·T_r/(p_r·V_r))·(p − p_m)
  gas exchange:            C1 = 6.18, C2 = 0
  compression:             C1 = 2.28, C2 = 0
  combustion/expansion:    C1 = 2.28, C2 = 3.24e−3 m/(s·K)
  p_m = p_r·(V_r/V)^n (motored, n ≈ 1.32–1.37), reference state frozen at IVC
Huber–Woschni low-load:  w ≥ C1·S̄p·[1 + 2(V_c/V)²·imep^−0.2]   [R, SAE 905018]
Hohenberg (DI diesel):   h = 130·V^−0.06·p^0.8·T^−0.4·(S̄p + 1.4)^0.8   (p bar)   [R, SAE 790825]
Annand + radiation:      q/A = a(k/B)Re^0.7(T − T_w) + c(T⁴ − T_w⁴), a 0.35–0.8, c = 0.576σ (CI) / 0.075σ (SI)   [R]
Chang (HCCI):            3.4·L(t)^−0.2·p^0.8·T^−0.73·v^0.8, combustion term C2/6   [R]
```

Woschni's original paper uses T^−0.53 where Heywood writes −0.55. The prefactor depends on the pressure unit: 3.26·100^0.8 ≈ 130. Motored engines needed a multiplier of about 1.7, and correlations are generally tuned by 0.5–2× ([Rzeszów paper](https://czasopisma.prz.edu.pl/amme/article/download/1436/1090); [Páv 2024](https://publi.cz/download/publication/5462?online=1)). Hohenberg agreed well with turbocharged-engine experiments ([Archives of Thermodynamics 2021](https://www.czasopisma.pan.pl//Content/121382/PDF/art08_final.pdf)). Radiation is 20–40 % of time-averaged heat flux in diesels ([Univ. of Bath](https://researchportal.bath.ac.uk/en/publications/thermal-modelling-of-modern-diesel-engines-proposal-of-a-new-heat/)) [V].

Apply the flux per surface, Q̇_i = h·A_i·(T − T_w,i). The areas are A_liner = πB·(x(θ) + h_c), A_piston = (πB²/4)·k_p and A_head = (πB²/4)·k_h, with k ≈ 1.1–1.5 for domes, bowls and pent roofs [E]. Warm-engine wall temperatures are about 180–300 °C for the head, 200–300 °C for the piston crown and 150–200 °C for the liner top, with coolant at 85–100 °C [R]. In-cycle surface swings are about 10 K, so wall temperatures can be held constant over a cycle and updated once per cycle from a lumped RC network. That network gives warm-up over minutes and costs nothing [R/E]. The exhaust port needs its own much higher correlation: Caton–Heywood Nu = 0.258·Re^0.8, about 2–3× Dittus–Boelter (Nu = 0.023·Re^0.8·Pr^0.3 when cooling the gas) [R]. Coolant heat is 16–35 % of fuel energy and exhaust-port temperatures are 800–950 °C at SI full load [R], which makes both good energy-balance sanity checks. Rotary engines need their actual chamber surface area rather than the equivalent bore (`scripting/include/wankel_engine_node.h:251`).

### Friction mean effective pressure

Chen–Flynn is the simplest model to calibrate: FMEP = A + B·p_max + C·S̄p + D·S̄p² ([Pipitone, SAE 2009-01-1984](https://saemobilus.sae.org/content/2009-01-1984/)) [V]. Handbook ranges are A ≈ 0.3–0.5 bar, B ≈ 0.004–0.006 and D ≈ 0.0006–0.0012 bar/(m/s)² ([US patent 8511265](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/8511265)) [V]. The frequently quoted set 0.137 + 0.005·p_max + 0.162·S̄p could not be verified and lies outside those ranges. The geometry-aware alternative is Patton–Nitschke–Heywood, in kPa, with B, S, D_b, L_b and L_v in mm, N in rpm and S̄p in m/s [R] ([SAE 890836](https://dx.doi.org/10.4271/890836)):

```
cfmep = 1.22e5·D_b/(B²·S·n_c) + 3.03e−4·N·D_b³·L_b·n_b/(B²·S·n_c) + 1.35e−10·D_b²·N²·n_b/n_c
rfmep = 2.94e2·S̄p/B + 4.06e4·(1 + 500/N)/B² + 3.03e−4·N·D_b³·L_b·n_b/(B²·S·n_c)
        + 6.89·(p_i/p_a)·[0.088·r_c + 0.182·r_c^(1.33 − 2.38e−2·S̄p)]
vfmep = 244·N·n_b/(B²·S·n_c) + C_ff·(1 + 1000/N)·n_v/(S·n_c)  [or C_rf·N·n_v/(S·n_c) for rollers]
        + C_oh·L_v^1.5·N^0.5·n_v/(B·S·n_c) + C_om·(1 + 500/N)·L_v·n_v/(S·n_c)
        (DOHC bucket: C_ff 200, C_rf 0.0076, C_oh 0.5, C_om 10; pushrod: 400, 0.0151, 0.5, 32.1)
afmep = 8.32 + 1.86e−3·N + 7.45e−7·N²       Sandoval update: 6.23 + 5.22e−3·N − 1.79e−7·N²
Sandoval–Heywood: hydrodynamic terms × (μ/μ0)^0.5, μ = k·exp(θ1/(T_oil + θ2))   [R, SAE 2003-01-0725]
Whole-engine check (4-cyl SI, motored WOT): tfmep[bar] = 0.97 + 0.15(N/1000) + 0.05(N/1000)²   [R]
Torque: T_f = FMEP·V_d/(4π)  (four-stroke)
```

Evaluate these once per revolution from N, MAP and T_oil ([Sandoval thesis](https://dspace.mit.edu/bitstream/1721.1/80657/2/52940875-MIT.pdf)). Pumping must not be double-counted: the simulator computes PMEP exactly from the p–V loop. FMEP should rise from about 0.8–1 bar at 1000 rpm to 2–3 bar at 6000–7000 rpm [R]. engine-sim's constant Coulomb `friction_torque` instead gives friction power ∝ rpm. Its piston friction uses a 20 N·s/m viscous term that alone produces about 28 kW at 6000 rpm in a 4-cylinder. That is the right order of magnitude, but the parameters are arbitrary and identical for every engine. The script's `cylinder_friction_parameters` node is never read (`es/objects/objects.mr:542-555`), and the Stribeck velocity is v_brk·√2 instead of Simscape's v_brk/√2 ([MathWorks Translational Friction](https://www.mathworks.com/help/simscape/ref/translationalfriction.html)) [V]. The recommended architecture keeps a crank-angle-resolved ring term only to shape torque ripple, normalised so its cycle mean equals the PNH reciprocating FMEP (rfmep): F_ring = f_Stribeck(H)·[F_tension + p_gas·πB·h_ring,eff], with H = μ·|S_p|/(F/πB) [E]. A full Reynolds or elastohydrodynamic ring-pack solver is not worth its cost in real time.

## Reduced-coordinate mechanics make torque auditable

The slider-crank has exactly one degree of freedom, yet engine-sim models it with 3 DOF per body and an iterative constraint solve. Joint compliance therefore comes from Baumgarte β = 1 and a 10 % relative Gauss–Seidel tolerance (`gauss_seidel_sle_solver.cpp:9-10`), not from any physical parameter. The `ks = 5000` and `kd = 10` values set in `src/piston_engine_simulator.cpp:61-62,143-144` are ignored by the default `OptimizedNsvRigidBodySystem`. Game-physics practice uses β ≈ 0.1–0.2 because full-error correction injects energy ([Allen Chou](https://allenchou.net/2013/12/game-physics-resolution-contact-constraints); [Catto, Soft Constraints](https://box2d.org/files/ErinCatto_SoftConstraints_GDC2011.pdf)). The recommended option is a reduced-coordinate crank behind a flag. The exact kinematics are [V for form, derivative C] ([Piston motion equations](https://en.wikipedia.org/wiki/Piston_motion_equations)):

```
x(θ) = r·cosθ + √(l² − r²·sin²θ);  s = l + r − x;  sinβ = λ·sinθ,  λ = r/l
ds/dθ = r·sinθ·[1 + r·cosθ/√(l² − r²·sin²θ)] = r·sin(θ+β)/cosβ
Desaxe (offset e): x = r·cosθ + √(l² − (r·sinθ − e)²),  TDC at asin(e/(l+r))
F_p = (p_cyl − p_cc)·A_p − m_B·a_piston;  T = F_p·ds/dθ;  F_rod = F_p/cosβ;  F_side = F_p·tanβ
J(θ)·θ̈ + ½·J'(θ)·θ̇² = Σ_i F_gas,i·ds_i/dθ − T_fric − T_load + T_starter
J(θ) = J_crank+flywheel + Σ_i [m_A,i·r² + m_B,i·(ds_i/dθ)² + I_rod,i·(dβ_i/dθ)²]
Two-mass rod: m_A = m_rod·(l − l_A)/l (rotating), m_B = m_rod·l_A/l (reciprocating)
```

The two-harmonic approximation of piston acceleration errs by about 1.1 % at λ ≈ 0.3. The inertia-torque harmonics are 0.076, 0.500, 0.230 and 0.023 m·r²·ω² for orders 1–4, and the mean reciprocating inertia is 0.51·m·r² [C]. Integrated with Störmer–Verlet, this model costs O(N_cyl) per step with no iteration and no drift. It yields indicated, inertia and friction torque, rod and side loads, and an exact energy audit (∫T dθ against ΔKE plus friction work). The existing torque-clamped constraints for dyno, starter and clutch can stay, acting on the single crank body. Two smaller fixes belong here too. Multi-crank engines need a position-level gear constraint instead of the θ-overwrite hack. The 500 rpm/s dyno sweep must add I_total·α back to its reading: α = 52.4 rad/s², so the reading is 5.2 N·m low at I = 0.1 kg·m² and 21 N·m low at the script default of 0.4. Engine scripts use 0.05–0.12 kg·m².

Balance outputs come almost free. Sum −m_B·a_piston along each bore axis, add rotating and counterweight terms, and take moments. The computed layout table reproduces known results [C]. I6, 60° V12 and flat-6 are fully balanced. An inline-4 has a vertical 2nd-order force peaking at 4λ·m·r·ω², and a flat-plane V8 has a horizontal one peaking at 4√2·λ·m·r·ω². A cross-plane V8 has a 1st-order couple of √10 × m·r·ω²·pitch that rotates with the crank, so counterweights cancel it ([Engine balance](https://en.wikipedia.org/wiki/Engine_balance)). For torsion, a 3–4 DOF chain (damper ring, crank, flywheel, optional dual-mass flywheel (DMF) secondary) is stable under semi-implicit Euler when h·ω_n < 2. A 400 Hz crank mode at 10 kHz has h·ω = 0.25 [E]. A conventional clutch-disc mode at 40–80 Hz and a DMF mode at 7.5–15 Hz are sourced targets ([Schaeffler DMF](https://www.schaeffler.com/remotemedien/media/_shared_media/08_media_library/01_publications/schaeffler_2/symposia_1/downloads_11/1_Dual_Mass_Flywheel_1.pdf)) [V]. Crank-segment stiffness of 0.5–2 MN·m/rad and modes of 250–600 Hz are estimates [E].

Several other mechanical items have simple upgrades:

- **Valvetrain.** Keep it kinematic, but add a float flag. Float occurs when m_eq·|a_min| > F_preload + k_s·L, with m_eq = m_valve + m_retainer + ⅓m_spring + (rocker inertia)/arm². Valve acceleration follows from a = (ω/2)²·L''(θ_cam). An optional 1-DOF valve with a contact spring captures bounce [E] ([Norton ch. 10](https://designofmachinery.com/wp-content/uploads/2018/11/Chap10p307.pdf)).
- **Starter.** Replace the speed-servo starter with a DC-machine curve, T = T_stall·(1 − ω/ω_nl) behind a pinion ratio, with battery sag [E]. Cranking speed then oscillates with each compression event, as real cranking (150–250 rpm) does ([US 10107219](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10107219)).
- **Drivetrain.** Rescaling ω to preserve kinetic energy on a gear change (`src/transmission.cpp:79-107`) changes vehicle speed, because reflected inertia ∝ 1/i_g². Hold v constant and let the clutch absorb the mismatch.
- **Vehicle and road load.** Add wheel and vehicle DOFs and a tyre model. The Magic Formula for dry tarmac uses B ≈ 10, C ≈ 1.9, D ≈ 1.0, E ≈ 0.97 ([MathWorks](https://www.mathworks.com/help/sdl/ref/tireroadinteractionmagicformula.html)) [V]; sources conflict on C (1.65 versus 1.9). Use a slip relaxation length, road load A + Bv + Cv² from coastdown ([40 CFR 1066.301](https://www.law.cornell.edu/cfr/text/40/1066.301)), and C_rr·m·g instead of the 2000 N default, which corresponds to C_rr ≈ 0.20.

## Physical sound means radiating the tailpipe mass-flow derivative

The pressure that radiates from an exhaust is a monopole driven by the fluctuating mass flow leaving the pipe ([NASA NTRS 19690002231](https://ntrs.nasa.gov/api/citations/19690002231/downloads/19690002231.pdf); [IJAV, Kunz summary](https://iiav.org/ijav/content/volumes/9_2004_1722161272264886/vol_3/396_firstpage_415331286453860.pdf)). In free field, p(r, t) = (1/4πr)·dṁ_out/dt evaluated at the retarded time (t − r/c), with up to +6 dB near the ground [R]. A steady jet adds only weak quadrupole noise ([MIT 16.50](https://ocw.mit.edu/courses/16-50-introduction-to-propulsion-systems-spring-2012/3ef25ccfca81dc2240acd75d665c33e3_MIT16_50S12_lec36.pdf)). engine-sim instead uses a different source signal and processing chain:

- **Source signal.** It takes each cylinder's lumped runner gauge pressure plus 0.1 × dynamic pressure, scaled by a³·1600 (`src/piston_engine_simulator.cpp:419-458`).
- **Delay.** It delays the signal by L/343 m/s. The integer sample count is computed for a hard-coded 10 kHz rate (`m_delayFilters[i].initialize(delay, 10000.0)`, line 258), while the filter actually runs at f_sim. The true delay is therefore (L/343)·(10000/f_sim): halved at 20 kHz and 54 % too long at 6.5 kHz. On top of that, hot exhaust has c ≈ 443–586 m/s.
- **Gain.** It scales each cylinder by 1/L², which has no acoustic basis.
- **Processing chain.** It interpolates linearly, low-passes at 1.9 kHz, mixes in only 1 % of the derivative (`dF_F_mix = 0.01`), multiplies by low-passed noise, convolves with a recorded WAV impulse response unrelated to the modelled geometry, and auto-levels the result (`src/synthesizer.cpp`).

Per-cylinder identity survives the chain, which is why firing-order "burble" in half orders (0.5, 1.5, 2.5…) is audible. Absolute level, geometry dependence and bandwidth do not survive.

A physical pipeline is real-time affordable. Its source is valve or tailpipe ṁ from the gas model. Its propagation is a nonlinear 1-D solver near the valves, where Δp/p₀ is order 0.3–1 and pulses steepen, so linear acoustics fails ([NASA NTRS 19820005954](https://ntrs.nasa.gov/api/citations/19820005954/downloads/19820005954.pdf); [Okda & Okumura](https://www.ioa.org.uk/system/files/publications/J%20OKDA%20Y%20OKUMURA%20PROPAGATION%20OF%20A%20PULSIVE%20FLOW%20WITH%20FINITE%20AMPLITUDE%20IN%20A%20STRAIGHT%20PIPE.pdf)). Downstream of the collector it switches to linear waveguides or transfer matrices. The main formulas are [R unless noted]:

```
Exhaust c = √(γR_sT): 443 m/s at 500 K, 557 at 800 K, 586 at 900 K
Cut-on of first non-planar mode: f_c = 1.841·c/(π·d)  (50 mm header, c 550 → 6.4 kHz; 300 mm muffler, c 450 → 0.88 kHz)
Pipe four-pole: [p1; U1] = [[cos kL, jZ sin kL], [j sin kL/Z, cos kL]]·[p2; U2],  Z = ρc/S;  mean flow k± = k/(1 ± M)
Expansion chamber TL = 10·log10[1 + ¼(m − 1/m)²·sin²(kL)],  zero-TL troughs at f = n·c/(2L)   [troughs V]
Open end: δ = 0.6133a unflanged, 0.8216a flanged [V];  δ(ka) = a(0.6133 − 0.1168(ka)²) [V];  |R| ≈ 1 − ½(ka)²
Helmholtz branch: f_H = (c/2π)·√(A/(L·V))   [V]
```

Sources for the bracketed claims: [Munjal et al. 2020](https://www.iiav.org/ijav/content/volumes/25_2020_36521584896523/vol_2/1666_fullpaper_1041381593513084.pdf), [Levine & Schwinger 1948](https://link.aps.org/doi/10.1103/PhysRev.73.383), [Silva et al., arXiv:0811.3625](https://pdf.arxiv.org/pdf/0811.3625) and [Acta Acustica 2025](https://acta-acustica.edpsciences.org/articles/aacus/full_html/2025/01/aacus250133/aacus250133.html). Below about 1 kHz a tailpipe reflects more than 90 % of incident energy, and that is what produces the pipe resonances in an exhaust note. A digital-waveguide pipe is two fractional delay lines of length L·f_a/(c(T) ± u), interpolated with Lagrange-3 and smoothed at 10–50 Hz, each followed by a loss filter. They meet at scattering junctions p_J = 2Σ Y_i·p_i⁺/Σ Y_i with Y_i = S_i/(ρ_i·c_i), and end in a low-order IIR fit of the Levine–Schwinger reflection ([JOS PASP](https://www.dsprelated.com/freebooks/pasp/)). Procedural engine synthesisers built on this idea exist ([Baldan et al. 2015](https://www.researchgate.net/publication/280086598_Physically_informed_car_engine_sound_synthesis_for_virtual_and_augmented_environments); [Doerfler & Wyse 2026](https://arxiv.org/html/2603.09391)). The cheapest geometry-correct upgrade keeps engine-sim's partitioned FFT convolution but computes the impulse response offline from the muffler/tailpipe four-pole chain plus radiation. It is regenerated when geometry or mean exhaust temperature changes.

Combustion and structure noise comes from feeding dp/dt of cylinder pressure into 10–30 modal resonators at 0.5–4 kHz with Q ≈ 10–50. Combustion noise above about 800 Hz is driven by cylinder pressure ([Anderton, IoA](https://www.ioa.org.uk/system/files/publications/D%20ANDERTON%20I.C.%20ENGINE%20COMBUSTION%20NOISE%20AND%20METHODS%20FOR%20ITS%20REDUCTION.pdf)); the resonator values are [E]. Knock modes are injected the same way.

Linear interpolation from f_sim = 10 kHz suppresses an image of a 4 kHz tone by only about 12 dB. A 32–64-tap polyphase windowed-sinc asynchronous sample-rate converter (ASRC) costs 100–200 MACs per sample [E]. All downstream stages for a dual-exhaust V8 total about 0.1–0.25 GFLOP/s at 48 kHz [E]. Absolute level needs an engineering mode that bypasses the automatic gain control.

## Verify against closed forms, then validate against EPA dyno data

### Code verification comes first

Every physics change should land with automated tests that compare against closed-form answers. The Sod shock tube is the canonical 1-D check: γ = 1.4, 100–200 cells, output at t = 0.2 ([Cholla](https://cholla.readthedocs.io/en/latest/ChollaExamples/1D-Sod/overview.html)). An HLLC solver at 200 cells matches the exact Riemann solution, and limiter choice shows up as contact-surface overshoot ([Sod FV benchmark](https://www.preprints.org/frontend/manuscript/1e397aff66898b0dec0c4533b9074d6d/download_pub)) [V]. The tolerances below are suggested engineering practice, not cited standards.

| Test | Analytic target | Pass criterion |
|---|---|---|
| V1 sealed adiabatic motored cylinder | p·V^γ = const; motored IMEP = 0 | peak p within 0.1 %; loop work ≈ 0 |
| V2/V3 ideal Otto, Diesel, dual | η = 1 − r^(1−γ) (r = 10, γ = 1.4 → 0.602); Diesel and dual forms | within 0.5 % |
| V4/V5 choked and subsonic orifice | nozzle equation; choke at 0.528 | 0.5 % / 1 % |
| V6 Sod shock tube | exact Riemann solution (Toro) | shock within 1–2 cells; design-order convergence |
| V7–V9 Helmholtz, quarter-wave, half-wave | f formulas with end correction | 2–3 % |
| V10 crank-slider | exact x(θ), V(θ), dV/dθ | round-off |
| V11 conservation | Σm, Σ(mu) + ΣW constant | drift < 1e−6 per cycle |
| V12 two-plenum equilibration | mass and energy balance | exact |
| V13/V14 Fanno flow; grid/time-step convergence | Fanno relations; Richardson order | 1–2 %; design order |
| E1 blowdown / filling (gas core) | 1230.5 K / 403.9 K | within 0.5 % |
| E2 fuel–air cycle (no heat loss, instant burn at TDC, 1 bar/350 K) | φ = 1: η = 0.425 (r_c 8), **0.460** (r_c 10), 0.487 (12), 0.527 (16); φ = 0.8, r_c 10: 0.485; φ = 1.2, r_c 10: 0.383 [C] | within 1–2 points (frozen products read 0.5–1.5 points high) |
| E3 adiabatic flame temperature | const-V from 700 K/20 bar, φ = 1: 3206 K complete, 2907 K equilibrium [C] | 1 % |
| E4 Wiebe phasing | x_b = 0.1/0.5/0.9 at 0.276/0.518/0.772·Δθ (a = 5, m = 2) | exact |
| E5 balance and torsion | computed layout table; f = (1/2π)·√(k(J1+J2)/(J1·J2)) | round-off / 1 % |
| E6 acoustics DSP | expansion-chamber TL troughs at n·c/2L; open-pipe f_n = n·c/(2(L + 2δ)); Doppler | 1–2 % |

The fuel–air-cycle targets matter because they expose the current error directly. Constant-γ Otto at γ = 1.4 overstates the r_c = 10, φ = 1 efficiency by 14 points (0.602 against 0.460) [C].

### Engineering outputs the tool must report

All of these follow from quantities the simulator already has [R] ([Heywood](https://www.accessengineeringlibrary.com/content/book/9781260116106)):

```
imep_g = ∫_{compression+expansion} p dV / V_d,cyl;  imep_n = ∮ p dV / V_d,cyl;  pmep = imep_g − imep_n
bmep = 4π·T_b/V_d (four-stroke);  fmep = imep_n − bmep (state convention: Heywood includes pumping)
η_m = bmep/imep_g;  η_f = P/(ṁ_f·LHV);  bsfc = ṁ_f/P_b;  η_f = 3600/(bsfc[g/kWh]·LHV[MJ/kg])
η_v = 2·ṁ_air/(ρ_air,ref·V_d·N)  — report against ambient AND against intake-manifold state
λ = AFR/AFR_st (dry-air mass basis);  CA10/50/90 from true x_b and from Rassweiler–Withrow on simulated p
Apparent heat release: dQ_n/dθ = γ/(γ−1)·p·dV/dθ + 1/(γ−1)·V·dp/dθ
```

The integrals should use per-cylinder trapezoidal accumulators split at BDC and TDC. Showing the apparent heat release next to the true burn rate exposes wall heat loss and γ error, which is a useful teaching feature.

Corrected power is only a reporting layer. SAE J1349, ISO 1585, JIS D1001 and EEC 80/1269 share a reference of 25 °C and 99 kPa dry air; DIN 70020 uses 20 °C and 101.3 kPa ([DDI Software](https://ddisoftware.com/tech-archive/tt-dyno/the-ambient-correction-factors/index.html); [Sandu & Ungureanu](https://webbut.unitbv.ro/index.php/Series_I/article/download/1085/969)) [V]. The correction factors are [R]:
- **SAE J1349:** CF = 1.18·(99/p_d)·(T/298)^0.5 − 0.18, valid for about 15–35 °C and 90–105 kPa ([wahiduddin](https://www.wahiduddin.net/calc/calc_hp.htm)).
- **ISO 1585 (SI):** CF = (99/p_d)^1.2·(T/298)^0.6.
- **DIN 70020:** CF = (1013/p)·(T/293)^0.5.

J1349 matched measured road acceleration best of the four ([Aston repository](https://research.aston.ac.uk/en/publications/comparison-of-engine-power-correction-factors-for-varying-atmosph/)). The existing AFR gauge (`src/engine.cpp:337-354`) and VE gauge (`src/right_gauge_cluster.cpp:259-271`) must be recomputed on this basis. For the default charge, the AFR gauge reads about 13.3 while the model's own mass ratio is about 11.6.

### Validation data and acceptance targets

The best open validation source is EPA's engine benchmarking programme. It publishes complete ALPHA fuel maps (idle to WOT, minimum motoring torque, maximum speed) and raw test packages for the 2018 Toyota A25A-FKS 2.5 L Atkinson engine with cooled EGR, the Mazda 2.5 L Skyactiv-G, the Honda 1.5 L L15B7 turbo and the Ford 1.6 L EcoBoost ([EPA ALPHA maps](https://19january2025snapshot.epa.gov/vehicle-and-fuel-emissions-testing/combining-data-complete-engine-alpha-maps); [SAE 2019-01-0249 on epa.gov](https://www.epa.gov/sites/default/files/2020-08/documents/sae-2019-01-0249.pdf); [Data.gov](https://catalog.data.gov/dataset/data-package-from-benchmarking-a-toyota-a25a-fks-engine-at-epa-nvfel)). Sandia's Engine Combustion Network hosts pressure-trace files, including motored traces ([ECN](https://ecn.sandia.gov/?p=370)). Whether any EPA package contains crank-angle-resolved cylinder pressure was not confirmed, and licence statements should be checked before data enter the repository.

The validation ladder runs in four steps:
1. Motored traces, to check compression, heat transfer and blow-by.
2. A25A-FKS WOT torque, airflow-derived VE and the BSFC map.
3. A turbo engine, once boosting exists.
4. Qualitative textbook trends, such as the VE peak moving with runner length.

Calibrated GT-POWER models of SI engines reach errors of no more than 7 % on power, torque, VE and fuel consumption, and authors describe about 5 % as typical ([SAE 2019-36-0170](https://saemobilus-test.cld.sae.org/papers/numerical-model-si-engine-using-gt-power-code-2019-36-0170)). Another model reached 4 % on VE, MAP and BSFC ([UKM](https://ptsldigital.ukm.my/handle/123456789/394137?mode=full)). Hendricks' mean-value model reached ±2 % ([SAE 900616](https://saemobilus.sae.org/content/900616)). "Validated" claims vary widely, though: one turbo-diesel study reported 39 % torque RMS error ([Jurnal Teknologi](https://journals.utm.my/jurnalteknologi/article/view/9197)) [V].

engine-sim's own acceptance targets should be WOT torque and VE within ±5 % (aiming for ±3 %), BSFC ±5 %, peak pressure ±5 %, peak-pressure location ±2° CA and exhaust temperature ±30–50 K, always reporting RMS and maximum error [E]. Calibration follows the GT workflow: fit on a subset of points and verify on held-out points, as KTH did with 22 and 12 points ([KTH CCGEx](https://www.ccgex.kth.se/polopoly_fs/1.752537.1600688541!/SenthilKMahendar_2017_CCGEXday.pdf)). Tune parameters in a fixed order:
1. Geometry, which is measured, never tuned.
2. Valve C_D and lift.
3. Pipe friction and heat transfer.
4. The Woschni multiplier.
5. Wiebe or quasi-dimensional constants.
6. FMEP constants.
7. Blow-by.

Combustion and heat transfer dominate both sensitivity and the differences between commercial codes ([Aston calibration study](https://research.aston.ac.uk/en/publications/cylinder-pressure-based-calibration-model-for-engines-using-ethan)).

Numerical hygiene completes the picture:
- **Resolution.** Report effective CFL and degrees of crank angle per step. At 10 kHz and 6000 rpm the mechanical step is 3.6° CA. The 8 gas sub-steps bring the gas step to 0.45° CA, against a typical 1-D-code target of ≤ 0.5–1° CA.
- **Cycle convergence.** Stop when |ΔIMEP|/IMEP < 0.1–0.5 % and |ΔVE| < 0.2 % over three cycles, then average N cycles.
- **Conservation.** Flag mass or energy imbalance above 0.1 %.
- **Determinism.** Seed every random draw so regression tests are bit-reproducible.

The industry pattern of a high-fidelity offline mode plus a real-time mode, cross-checked against each other, matches the GT fast-running-model practice. One such model reached 0.7–0.9 real-time factor with consistent results ([SAE 2016-01-0549](https://saemobilus.sae.org/content/2016-01-0549); [GT FRM training](https://www.gtisoft.com/training_catalog/fast-running-engine-models-frms-for-integrated-simulations-including-real-timesilhil/)).

## Roadmap: measure first, then rebuild from the gas core outward

The ordering follows two rules. First, nothing changes physics until the simulator can measure its own IMEP, energy balance and torque decomposition. Without that, no later change can be quantified. Second, the compensating-error structure means thermodynamics must be fixed before combustion and friction are calibrated. Re-tuning `max_burning_efficiency` against a 4900 K gas would only re-bury the error. Every physics phase sits behind a `physics_model` switch with `legacy` as the default for existing scripts. Every phase also adds golden-file regression CSVs of WOT sweeps for a representative engine subset, so shifts are deliberate and documented.

| Phase | Core deliverables | Verification gate | Impact on existing engine scripts | Added real-time cost |
|---|---|---|---|---|
| 0. Instrumentation | Headless sweep runner with CSV output; per-cycle IMEP_g/n, PMEP, BMEP, FMEP, BSFC, η, CA10/50/90; mass and energy audit; seeded RNG replacing `rand()`; J1349/ISO/DIN reporting; corrected AFR and VE gauges | V10, V11, V12; baseline golden files for all 36 scripts | None on physics; AFR and VE readouts change | Negligible |
| 1. Correctness fixes | λ→φ in `fuel.cpp:56` and the ignition window; element-conserving `react()`; Stribeck v_brk/√2; wire `cylinder_friction_parameters` and `throttle_gamma`; use `pressureEquilibriumMaxFlow`; fix `dynamicPressure` for f ∉ {3, 5}; audio delay from the real f_sim, c(T) and fractional samples; filtered-rpm weighting; dyno sweep +I·α; position-level multi-crank constraint; constant-v gear change; expose Baumgarte β | Unit tests per fix; golden-file diffs explained line by line | Rich mixtures burn faster and lean ones slower; exhaust delays change for scripts not at 10 kHz; vehicle speed stops jumping on shifts | None |
| 2. Thermodynamic core | Enthalpy flow (`gas_system.cpp:424-426`); stagnation T₀; NASA-7 lumps (air 20.95 % O₂ + Ar, humidity, fuel, burned); mass-based fuel definition (M, LHV, AFR_st, h_fg) with a fuel library; WGS products; η_c = η_chem·η_HC; metered λ instead of the fixed 0.8 charge | E1, E2, E3, V1–V5; property tables against Cantera within 0.5 % | Large for every engine. Peak T falls from ~4000+ K to ~2600–2900 K; `max_burning_efficiency` becomes a deprecated η_HC (~0.97); `energy_density`, `molecular_afr` and `molecular_mass` are replaced by real fuel properties; carb mixtures need a λ setting. Ship an auto-converter that maps old parameters and flags scripts | ~3 polynomial evaluations per property per volume; <1 % of a core |
| 3. Heat transfer and friction | Woschni (with Hohenberg/Annand options), per-surface walls, lumped thermal network and warm-up; pipe heat transfer and exhaust temperature; PNH/Sandoval and Chen–Flynn FMEP with oil temperature; accessory loads; normalised crank-angle ring friction | Motored ECN trace; motoring torque against the Bishop correlation; coolant 16–35 % and exhaust-port 800–950 °C at SI WOT | `heat_transfer_coefficient` and `friction_torque` are superseded by a Woschni multiplier and FMEP geometry or constants. Every torque curve shifts; low-rpm torque rises and high-rpm torque falls relative to Coulomb friction | ~3 `pow` per cylinder-step; negligible |
| 4. Combustion | S_L(φ, T_u, p, x_r) and u'(ρ_u) every step; Wiebe imposed mode; Blizard–Keck QD mode with A_f tables and optional K–k turbulence; kernel delay; Gaussian per-cycle CCV; misfire and partial burn; unburned-zone Livengood–Wu knock index; MBT finder; Rhodes–Keck, Metghalchi–Keck and Gülder fuel selection | E4; 10–90 % burn 20–40° CA at mid speed; MBT CA50 8–10° ATDC; COV(IMEP) 1–3 % mid-load and 5–10 % idle; A25A-FKS WOT torque ±10 % as first contact | Spark tables (e.g. the LFA's 12°→40°) need re-tuning or the MBT finder; five scripts' `turbulence_to_flame_speed_ratio` overrides become C_T calibrations; randomness parameters are retired | ≤1 µs per active step; ~7.5 % of a core for 12 cylinders at 50 kHz |
| 5. Gas exchange and 1-D pipes | Forward/reverse C_D(L/D) tables; Heywood throttle plus leakage and IAC; low-Δp linearisation; FV-HLLC pipe element (first, then second order) with well-balanced area source, Haaland/Colburn and a wall node; volume–restriction–pipe boundary fluxes; collector junction volumes; per-pipe sub-cycling | V6–V9, V13, V14; virtual 28 inH₂O flow bench reproduces input CFM; quiescent tapered pipe stays at rest; longer runner moves the VE peak lower in rpm | `runner_length`, `primary_tube_length` and collector geometry now tune the engine; `velocity_decay` is retired; VE curves reshape and some scripts gain or lose torque peaks. Default cell size 0.4–0.55 × bore | 11 M cell updates/s for a V8 at 40 mm; ~44 M at 20 mm (SIMD and threads) |
| 6. Mechanics and drivetrain | Reduced-coordinate crank option with J(θ) and torque decomposition; bearing and side loads; shaking forces and moments; optional torsional chain and DMF; valve-float check and optional 1-DOF valve; DC starter; wheel and vehicle DOFs, Magic Formula tyre, road load A + Bv + Cv², gearbox efficiency | E5; energy audit closes within 0.1 %; maximal- and reduced-coordinate torque agree; coastdown against A, B, C | Flywheel mass must be expressed as inertia; rolling-resistance values (100 N–10 kN today) become C_rr; starter scripts gain motor parameters | Lower than today (removes 2N bodies and the Gauss–Seidel solve) |
| 7. Acoustics | Source dṁ/dt at tailpipe and intake from the 1-D solver; windowed-sinc ASRC; Levine–Schwinger termination; geometry-derived impulse responses from four-pole chains; waveguide muffler network; structure modal bank driven by dp/dt; knock modes; engineering mode without AGC; Doppler, 1/r, ground image | E6; order-tracked run-up maps against recordings; expansion-chamber TL troughs | Recorded IRs become optional "room/listener" colouring; per-script `audio_volume` and IR choices need re-levelling | 0.1–0.25 GFLOP/s at 48 kHz |
| 8. Validation and extensions | `toyota_a25a.mr` built from SAE 2019-01-0249 geometry; validation dashboard (RMS and max error); OAT sensitivity sweeps; turbo/supercharger module with Ellipse maps and Greitzer surge; two-zone burned gas with Zeldovich NO; CI path (Hardenberg–Hase + Watson); 3-D equilibrium property tables | A25A-FKS WOT torque and VE ±5 %, BSFC ±5 %; held-out-point verification; offline and real-time modes agree | New assets only; existing scripts unaffected | Turbo negligible; two-zone ~2× cylinder cost |

Phase 0 matters because it is what makes later claims checkable. With the per-cycle accumulators in place, the 72° CA burn-duration estimate and the 28 kW viscous-friction estimate stop being analytic guesses and become measured baselines. Phase 1 is cheap, but its λ/φ fix changes flame speed in every script with a non-stoichiometric charge, so its golden-file diffs need explanation. Phase 2 is the critical one. It removes the oxygen and heating-value bonuses and the 20 % fuel discard together, so its net effect on shipped torque is not knowable in advance. It must be measured against the Phase 0 baseline, with heat transfer and friction recalibrated immediately afterwards in Phase 3. The A25A-FKS asset should be built during Phase 2 so that Phases 3–5 can each be scored against the same measured curve.

Phase 5 is the largest engineering effort and the largest single gain in realism, for both VE and sound. It comes after the 0-D physics is right because 1-D pipes amplify 0-D errors: a wrong exhaust temperature gives the wrong wave speed and therefore the wrong tuning. Phase 7 depends on Phase 5 for its source signal. Only its simple fixes (the delay bug and the resampler) move forward into Phase 1.

## Conclusion

The central finding is structural rather than about any single model. engine-sim's realism today comes from mutually compensating errors: too much oxygen, too much heating value and too hot a gas, offset by discarded fuel and arbitrary friction. So the "engineering tool" upgrade is a re-foundation that must be measured into place, not a set of independent plug-ins. The upside is that every recommended model is the same 0-D/1-D practice used by GT-POWER, WAVE and BOOST. Except for the 1-D pipes, each one costs a few transcendental evaluations per cylinder-step. The real-time constraint therefore does not force any compromise on thermodynamics, combustion, heat transfer or friction. It bites only on pipe resolution, and there the commercial guidance of 0.4–0.55 × bore happens to sit right at the affordable 35–55 mm.

Several important numbers remain recalled rather than verified and must be checked against primary texts before release: the PNH coefficients, the Woschni exponent variant, the Metghalchi–Keck and Gülder tables, the J1349 and ISO formulas, and the Watson constants. The cheapest de-risking step is to buy or borrow Heywood (2nd edition) and the SAE J1349 text before Phase 3. The most valuable step overall is Phase 0's measurement harness combined with an EPA-derived reference engine. Once engine-sim can report its own error against a measured torque curve, every later choice in this roadmap can be judged on evidence rather than on how the engine sounds.
