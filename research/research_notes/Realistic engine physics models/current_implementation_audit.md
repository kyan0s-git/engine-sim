# Current Implementation Audit: physics models in engine-sim (local tree at /home/user/engine-sim, HEAD f3e9a0a)

Scope: every physical model in the C++ engine and the `.mr` script defaults, written out as equations with file:line references, compared with standard engine-modelling practice (Heywood-style 0-D/1-D cycle simulation). The "source" for each code finding is the file and line in this repository; paths are relative to `/home/user/engine-sim`. Numerical checks were run with a scratch harness, `scratchpad/gt/t.cpp`, which compiles `src/gas_system.cpp` unmodified. No repository files were changed.

Severity scale (for using the program as an engineering tool):
- **Critical**: the results are not predictive or are qualitatively wrong for normal engineering questions such as torque or power curves, VE or AFR.
- **Major**: the results are noticeably biased or need calibration fudges, but trends may hold.
- **Minor**: a small or second-order error, or a cosmetic or labelling issue.

---

## 1. GasSystem: state, equation of state, specific heats, flow law, momentum, conservation

### Takeaway
Each gas volume is one 0-D lumped control volume. It holds moles, internal energy, volume, a 2-D bulk momentum vector and a three-species mole-fraction mix. It is ideal-gas with a constant γ = 1.4 (f = 5) and a fixed molar mass of 28.97 g/mol. Flow between volumes uses the standard isentropic orifice equation, choked flow included, with a lumped flow constant k. Two features are non-standard:
- The energy carried by flowing gas is the internal energy per mole (c_v·T), not the enthalpy (c_p·T).
- Momentum and "dynamic pressure" are bolted on ad hoc. They are not a 1-D momentum equation, so there is no wave action.

Total energy and moles are conserved to machine precision (verified), but the energy is split between the volumes wrongly.

### Cited Findings
**State.** `struct State { n_mol; E_k; V; momentum[2]; Mix mix; }`. Mix = `{p_fuel, p_inert, p_o2}` (mole fractions). Default mix is 100 % inert — [include/gas_system.h:12-30](include/gas_system.h).

**Equation of state and properties.** All of the following are in [include/gas_system.h](include/gas_system.h) unless another file is named:
- Internal energy: U ≡ `E_k` = (f/2)·n·R·T, set at `initialize` — [src/gas_system.cpp:16-27](src/gas_system.cpp).
- Pressure: P = U / ((f/2)·V) = nRT/V — lines 246-251.
- Temperature: T = U / ((f/2)·n·R) — lines 253-256.
- f (`degreesOfFreedom`) defaults to 5 everywhere — line 45 and line 119.
- Heat capacity ratio: γ = 1 + 2/f, so γ = 1.4 and c_v = 2.5R. These are constant, independent of temperature and composition — lines 134-136.
- Mass: m = 28.97 g/mol × n for every gas (air, fuel vapour, exhaust) — lines 242-244 and [include/units.h:178](include/units.h).
- Density: ρ = M_air·n/V — lines 151-153.
- Speed of sound: c = √(γP/ρ) — lines 171-180.
- Gas constant: R = 8.31446261815324 J/mol·K — [include/constants.h:7](include/constants.h).

**Compression and expansion work.** `changeVolume(dV)` computes L = (V+dV)^{1/3}, A = L², dL = −dV/A, W = dL·P·A, which reduces algebraically to W = −P·dV with P evaluated before the step (explicit first-order). Then U += W — [src/gas_system.cpp:45-54](src/gas_system.cpp).

**Orifice flow law.** `flowRate()` — [src/gas_system.cpp:191-237](src/gas_system.cpp):
- Let p0 = upstream pressure, p_T = downstream pressure, pr = p_T/p0.
- If pr ≤ (2/(γ+1))^{γ/(γ−1)} (= 0.528 for γ = 1.4), the flow is choked:
  ṅ = k · p0/√(R·T0) · √γ · (2/(γ+1))^{(γ+1)/(2(γ−1))}
- Otherwise, with s = pr^{1/γ}:
  ṅ = k · p0/√(R·T0) · √( 2γ/(γ−1) · s·(s − pr) ) = k · p0/√(RT0) · √( 2γ/(γ−1)·(pr^{2/γ} − pr^{(γ+1)/γ}) )
- This is the textbook compressible flow-through-a-restriction equation (Heywood App. C form) with C_d·A/√M lumped into k, because molar R is used. The same form, and the 0.528 critical ratio, are described in [FAMU valve-flow notes](https://web1.eng.famu.fsu.edu/~shih/propulsion/lecture%20notes/Valve%20Flows.pdf), [Hanriot et al. (UP repository)](https://repository.up.ac.za/items/beea213c-cf64-495e-9dfc-00ff4802487f) and [industrialmonitordirect critical-flow note](https://industrialmonitordirect.com/blogs/knowledgebase/critical-flow-calculation-through-restriction-orifices).
- The upstream temperature T0 is the source's static temperature, not its stagnation temperature (line 401).
- p0 and p_T include ad-hoc "dynamic pressure" terms. Source: P + q(+dir); sink: P + q(−dir) — lines 367-372.

**Flow constants from flow-bench data.** `flowConstant()` inverts the same equation at test conditions (1 atm upstream, 25 °C, γ = 1.4) to turn a CFM figure into k — [src/gas_system.cpp:131-189](src/gas_system.cpp):
- `k_28inH2O(cfm)`: pressure drop 28 inH₂O, i.e. pr = 0.931.
- `k_32inH2O`: pressure drop 32 inH₂O.
- `k_carb(cfm)`: pressure drop 1.5 inHg, i.e. pr ≈ 0.950. This matches the carburettor CFM rating convention.
- Unit conversions come from `units::scfm` = 0.002641 lbmol/min ([include/units.h:60](include/units.h)), `inH2O` = 0.0734824 inHg (line 78) and `inHg` = 3386.39 Pa (line 77).

**Moles moved per step.** Δn = clamp(ṅ·dt, 0, 0.9·n_source) — [src/gas_system.cpp:397-408](src/gas_system.cpp). `maxFlow = source->pressureEquilibriumMaxFlow(sink)` is computed at line 407 but **never used** in the volume-to-volume `flow()`. Only the environment version applies the equilibrium cap (lines 541-544).

**Energy carried by the flow.**
```
E_k_per_mol = source->kineticEnergyPerMol();   // = U_s / n_s = c_v T_s
sink->gainN(flow, E_k_per_mol, source->mix());
source->loseN(flow, E_k_per_mol);
```
[src/gas_system.cpp:424-426](src/gas_system.cpp), with `gainN`/`loseN` at lines 239-267. The standard filling-and-emptying energy balance instead carries enthalpy: dU/dt = Σṁ_in·h_in − Σṁ_out·h − P·dV/dt + Q̇, which gives Ṗ = (Rγ/V)(ṁ_in·T_in − ṁ_out·T_out) for a manifold ([engine control patent 9482164](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/9482164); the filling/emptying model is defined by mass and energy conservation in a control volume per [Lisowski, J. KONES 2010](https://yadda.icm.edu.pl/baztech/element/bwmeta1.element.baztech-article-BUJ7-0017-0080/c/httpwww_bg_utp_edu_plartjok42010jokones2010no4vol17lisowski.pdf); [SAE 960037](https://saemobilus.sae.org/content/960037)).

**Numerical check (my harness, momentum disabled by zero cross-sections).**
- Blowdown of 1 L from 5 bar, 1500 K to a large 1 atm reservoir: model T stays at **1500.0 K** when P reaches 2.5 bar. The isentropic value is **1230.5 K**.
- Adiabatic filling of a 1 L vessel from 0.1 bar to about 1 atm from a 300 K reservoir: model T = **300.0 K**. The control-volume energy balance with h_in = c_p·T0 gives **403.9 K**.
- Total energy drift: −8e-16 (relative). Mole drift: 1.4e-15 (relative).

**Momentum ("velocity") model** — [src/gas_system.cpp:415-525](src/gas_system.cpp):
- The moved fraction carries its share of the source momentum.
- Both source and sink then receive extra momentum Δm·v_f in the flow direction, where v_f = clamp(ΔV_frac / A_cross / dt, 0, c), using each side's cross-section (lines 460-484).
- The resulting bulk kinetic-energy change is subtracted from U, so total energy is conserved (lines 486-517).
- E_k is clamped to ≥ 0 (lines 519-525).

**Momentum damping.**
- `updateVelocity(dt, β)` decelerates the bulk velocity with forces F = Σ q_face·A_face on four faces of a box of `width × height × depth`, scaled by β. The KE removed is returned to U (lines 291-336).
- β is the "velocity_decay" parameter: intake default 0.25 ([es/objects/objects.mr:711](es/objects/objects.mr)), exhaust default 1.0 (line 770), cylinder 0.5 ([src/combustion_chamber.cpp:334](src/combustion_chamber.cpp)).
- `dissipateExcessVelocity()` caps |v| ≤ c and puts the excess KE into U (lines 269-289).

**Dynamic pressure.** q = P·[(1 + (γ−1)/2·M²)^{γ/(γ−1)} − 1], used only for a positive velocity component along the chosen direction. It is implemented as `sqrt(x^7)` for f = 5 and `sqrt(x^5)` for f = 3. Any other f falls through to `sqrt(x)`, which is wrong — [include/gas_system.h:203-240](include/gas_system.h).

### Inferences
- **Thermodynamic properties (Critical).** Constant c_v = 2.5R and constant M are fine for cold air but under-predict hot burned-gas heat capacity. Combined with the combustion energy below, they give unrealistically high peak temperatures and pressures (see Section 2). This is the root reason the `max_burning_efficiency` fudge exists.
- **u instead of h in flow (Critical for VE, residuals and exhaust temperature).** Real cylinder blowdown cools the remaining gas isentropically, and filling heats the received charge. In the model, a source volume empties isothermally and the receiving volume is under-heated (≈300 K versus ≈404 K in the filling test).
  - In-cylinder residual temperature and mass, charge temperature at IVC, exhaust-gas temperature and the pumping loop will all be biased.
  - The KE bookkeeping in the momentum stage recovers part of the cooling while gas is moving, but that energy is later returned to the same volume by `updateVelocity`/`dissipateExcessVelocity`.
- **No 1-D wave dynamics (Critical for intake/exhaust tuning).** Runners and pipes are single lumped volumes whose length only sets volume and a "box" geometry. Pressure-gradient-driven gas-column inertia (Helmholtz resonance) and acoustic wave reflections are absent.
  - The "dynamic pressure" added to the upstream pressure gives a crude ram effect.
  - The momentum is created by the mass flux itself rather than by Δp acting on a gas column. Both source and sink gain momentum in the same direction, so momentum is not conserved, only energy is.
  - Ram/tuned-length effects on VE versus rpm are therefore not physically predicted.
- **Explicit integration of P·dV and of flows.** These are first-order explicit steps. The 0.9·n clamp and the unused equilibrium-limit computation mean that very small volumes or large k values can oscillate or overshoot pressure equilibrium within one sub-step (Minor to Major numerically).
- **Momentum direction (Minor).** Flow uses the source's static temperature rather than stagnation temperature, and the 2-D momentum direction is a fixed ±x/±y per volume. This is consistent with the ad-hoc design.
- **Flow-constant calibration is sound (positive finding).** My worked example: k_28inH2O(260 cfm) ≈ 7.15e-3 mol·s⁻¹/(Pa·(J/mol)^{-1/2}). This corresponds to C_d·A ≈ k·√M_air ≈ 12.2 cm². An incompressible check (ρ = 1.184 kg/m³, Δp = 6968 Pa, v ≈ 108 m/s) gives ≈ 11.7 cm². The flow-bench-to-k conversion is therefore physically consistent. It treats k as a constant effective area, i.e. constant C_d over the pressure ratio.

### Gaps
- I did not run a full engine cycle to quantify the u-versus-h effect on VE or torque. Only the isolated vessel tests were run.
- The influence of the momentum terms on the blowdown temperature when cross-sections are non-zero was not quantified.

---

## 2. Combustion, fuel chemistry and mixture definition

### Takeaway
Combustion works as follows:
- A spark event lights a geometric "flame cylinder" that grows radially and axially at one turbulent flame speed, fixed at the moment of ignition.
- Each step, the newly swept volume fraction × n × a burning efficiency is reacted with octane stoichiometry (mislabelled C8H16). Fuel energy (LHV × mass) is added to internal energy.
- The burning efficiency is a heuristic blend of a random number, a turbulence factor and a dilution factor (default maximum 0.8). It decides how much fuel never burns.
- The laminar flame speed correlation is Metghalchi–Keck for gasoline, but it is evaluated with λ instead of φ.
- The fresh charge is a fixed, slightly rich (λ = 0.8 in O₂ terms) mixture of "air" that is 25 % O₂.

### Cited Findings
**Ignition gate.**
- `afr = p_o2/p_fuel` (molar O₂/fuel). `equivalenceRatio = afr / molecularAfr`. Ignition is refused if this is < 0.5 or > 1.9 — [src/combustion_chamber.cpp:176-181](src/combustion_chamber.cpp). Because afr is the oxidiser/fuel ratio divided by its stoichiometric value, this "equivalenceRatio" is λ, not φ.
- Dilution: `idealInert = p_o2/0.7`, `dilution = p_inert/idealInert − 1` (lines 183-184). For fresh charge (inert/O₂ = 3), dilution = 3×0.7 − 1 = 1.1. This is a magic constant.

**Burning efficiency (per flame event)** — lines 196-220:
- turbulence u' = `MeanPistonSpeedToTurbulence(mean |piston speed|)`.
- mixingFactor = 1 − clamp(u'/maxTurbulenceEffect) · clamp(1 − dilution/maxDilutionEffect)
- rand_s = lowEfficiencyAttenuation · ((1 − r) + r·U[0,1]), drawn with C `rand()`
- efficiency = maxBurningEfficiency · (mixingFactor·rand_s + (1 − mixingFactor))

**Fuel and burning defaults.**
- Script defaults — [es/objects/objects.mr:53-67](es/objects/objects.mr): `molecular_mass` 100 g, `energy_density` 48.1 kJ/g, `density` 0.755 kg/L, `molecular_afr` 12.5, `max_burning_efficiency` 0.8, `burning_efficiency_randomness` 0.5, `low_efficiency_attenuation` 0.6, `max_turbulence_effect` 2.0, `max_dilution_effect` 10.0.
- The C++ defaults are the same except maxDilutionEffect = 50 — [include/fuel.h:12-27](include/fuel.h).
- Many shipped engines override `max_burning_efficiency` (nine scripts set 1.0, others 0.75 to 0.9; grep of `assets/`).

**Turbulence.** u' = 0.5 × mean piston speed: a hard-coded 30-sample table of s → 0.5s — [scripting/include/engine_node.h:113-118](scripting/include/engine_node.h). Measured TDC turbulence intensity is often ≤ 0.5 × mean piston speed without squish or tumble, about double with tumble, and as low as 1/6 in a motored diesel ([Vafidis 1986 thesis](https://spiral.imperial.ac.uk/bitstream/10044/1/38177/2/Vafidis-C-1986-PhD-Thesis.pdf); [SAE 890618](https://saemobilus.sae.org/content/890618)).

**Turbulent flame speed.** S_T = f(u'/S_L)·S_L, where f is a script function. Default: f(0) = 3, f(x) = 1.5x for x = 5…45 — [es/objects/objects.mr:38-51](es/objects/objects.mr) and [src/fuel.cpp:37-49](src/fuel.cpp). The `firingPressure` and `motoringPressure` (160 psi) arguments are unused (`p_adjustment = 1.0`, fuel.cpp:46).

**Laminar burning velocity** — [src/fuel.cpp:51-65](src/fuel.cpp):
- S_L0 = B_m + B_er·(er − 1.21)², with B_m = 30.5 cm/s and B_er = −54.9 cm/s.
- α = 2.4 − 0.271·er^3.51; β = −0.357 + 0.14·er^2.77.
- S_L = S_L0·(T/298)^α·(P/1 atm)^β.
- `er = molecularAfr / m_molecularAfr`, i.e. **λ**.
- These match the Metghalchi–Keck form, S_L0 = B_M + B_φ(φ − φ_M)² with fuel-specific constants (the gasoline row 30.5 / −54.9 / 1.21 appears in the ANSYS Forte theory table; [ANSYS Forte 7.4.2](https://ansyshelp.ansys.com/public/Views/Secured/corp/v252/en/forte_th/pgfId-1082878.html); [BU flame-speed lecture](https://feng.stafpu.bu.edu.eg/Mechanical%20Engineering/2511/crs-14278/Files/Lec.%203%20Flame%20Speed%20Correlations.pdf); [Metghalchi 1977 thesis](https://james-keck-memorial-collection.unibs.it/ThesisAdvisor/Thesis-Metghalchi-1977.pdf)).
- The correlation is written in **equivalence ratio φ**, and the quadratic can go negative for very lean or rich mixtures (ANSYS Forte notes Gülder's fix).
- The code uses no residual-gas dilution term, and the quadratic is unguarded.

**Flame frozen at ignition.** The flame speed is computed once in `ignite()` from the spark-time T and P (combustion_chamber.cpp:221-227) and held constant for the whole burn.

**Flame geometry** — [src/combustion_chamber.cpp:345-384](src/combustion_chamber.cpp):
- travel_x (radius) ≤ bore/2 and travel_y (height) ≤ V/A_bore, both growing at S_T·dt. travel_y is stretched with volume expansion.
- Burned volume V_b = π·r²·h.
- Moles lit per step = (ΔV_b/V)·n_cyl. That is, the *volume* fraction is used as the *mass* fraction, with no burned/unburned density ratio.
- `react(n·efficiency, globalMix)` is called, then U += n_fuel_burned·M_fuel·LHV (lines 366-372).
- The flame is extinguished when travel stops, or immediately if any intake flow > 1e-9 mol occurs (lines 337-339).

**Reaction (`GasSystem::react`)** — [src/gas_system.cpp:82-129](src/gas_system.cpp):
- The comment says "25[O2] + 2[C8H16] → 16[CO2] + 18[H2O]". That is actually the iso-octane (C8H18) equation; the C8H16 label is wrong.
- Code: a_fuel = min(sys_fuel, p_fuel·n, (2/25)·p_o2·n); a_O2 = min(sys_O2, p_O2·n, 12.5·p_fuel·n).
- products = (34/27)·(a_fuel + a_O2) moles, all added to "inert". Δn = products − reactants.
- The fuel and O₂ minima are independent, so O₂ can be consumed with no fuel. My harness, with zero fuel in the system and a global mix with fuel, consumed **100 % of the O₂ (1.002e-2 of 1.002e-2 mol) while burning zero fuel**.

**Energy release check (my harness).**
- Constant-volume combustion of the default intake charge from 700 K, 10 atm with complete reaction: ΔT = **4182 K**, giving T = 4882 K. With efficiency 0.8: T = 4089 K.
- Only 80 % of the fuel can burn because the charge is O₂-limited at λ = 0.8.

**Heating value.** The default fuel LHV is 48.1 MJ/kg. A reference gasoline LHV is 43.55 MJ/kg with stoichiometric AFR 14.7 ([IFP Energies Nouvelles OGST table](https://ogst.ifpenergiesnouvelles.fr/articles/ogst/full_html/2021/01/ogst210229/T1.html)). Gasoline stoichiometric AFR ≈ 14.7 also appears in [FAA AFR note](https://www.faasafety.gov/files/events/WP/WP25/2026/WP25143284/Air_fuel_ratio_definition_(1).pdf).

**Intake mixture** — [src/intake.cpp:63-73](src/intake.cpp):
- `ideal_afr = 0.8·molecularAfr·4` = 40 mol "air" per mol fuel.
- p_air = 40/41; p_fuel = 1/41; p_o2 = 0.25·p_air; p_inert = 0.75·p_air. So "air" is **25 % O₂**.
- O₂:fuel = 10 versus the stoichiometric 12.5, i.e. λ = 0.8.
- The exhaust backflow comment confirms "Air in the model is 25% oxygen" — [src/exhaust_system.cpp:63-65](src/exhaust_system.cpp).
- `molecular_afr` is therefore the **stoichiometric moles of O₂ per mole of fuel** (12.5 for C8H18). It is not a mass AFR.

**Idle circuit.** A parallel orifice of constant `idle_flow_rate` k supplies a fixed mixture with `idle_afr = 2.0` mol air per mol fuel: p_fuel = 1/3, O₂/fuel = 0.5 — [src/intake.cpp:78-105](src/intake.cpp).

**No combustion-chemistry extras.** There is no knock or autoignition model, no dissociation, no CO/H₂ partial oxidation in rich mixtures, no emissions, and no wall film or evaporation. Fuel is gaseous from the start (code inspection; no such code exists in `src/`).

### Inferences
- **Combustion heat release (Critical).** Heat release is a geometric flame with a constant speed plus a random efficiency fudge, not a physically closed burn-rate model.
  - Standard 0-D practice is either a fitted Wiebe function, x_b = 1 − exp(−a·((θ−θ0)/Δθ)^{m+1}) (defaults like a = 5, m = 2 are used in [ANSYS Chemkin SI zonal simulator](https://ansyshelp.ansys.com/public/views/secured/corp/v251/en/chemkin_th/pgfId-1352744.html); [bibliotekanauki Wiebe paper](https://bibliotekanauki.pl/articles/949481.pdf)), or a two-zone, entrainment-type turbulent flame model.
  - Energy that is not burned is left as unburned "fuel", so efficiency directly scales IMEP.
  - `rand()` makes the results non-reproducible, and cycle-to-cycle variation is modelled as a uniformly random completeness of combustion rather than as variation in burn rate or phasing.
- **Peak temperature (Critical).** With constant c_v = 2.5R and LHV 48.1 MJ/kg, a complete burn of the default charge reaches ~4900 K. From general combustion knowledge (Heywood ch. 3–4, not re-verified in this session), real constant-volume adiabatic flame temperatures for gasoline-air are ~2800–3000 K, because of rising c_p and dissociation. The default `max_burning_efficiency` 0.8 partly compensates. Peak pressure and temperature, heat-transfer driving ΔT, exhaust temperature and indicated efficiency are therefore all biased.
- **Laminar flame speed with λ (Major).** Using λ where the correlation expects φ puts the flame-speed peak at a *lean* λ = 1.21 (φ ≈ 0.83) instead of a rich φ = 1.21. The temperature and pressure exponents use the wrong argument too. Rich-versus-lean burn-rate trends are inverted.
- **Flame speed frozen at ignition (Major).** Real S_L rises strongly with the unburned-gas temperature during compression, and S_T follows u'. A burn duration that responds correctly to spark timing therefore cannot emerge.
- **Fixed charge composition (Major for AFR and power).** A fixed λ = 0.8 charge with 25 % O₂ air (real dry air is about 20.95 % O₂, general knowledge) makes "air" about 19 % richer in O₂ per mole than reality. That raises energy per unit charge volume and is compensated elsewhere by fudges. The carburettor is "perfect": AFR does not depend on airflow, venturi signal, rpm, temperature or throttle (apart from the idle circuit).
- **Mass not conserved in reaction (Minor).** The C8H16/C8H18 mislabel and the fixed molar mass mean that mass is not conserved: mass grows by the 34/27 mole-increase factor on reacted species. The `react()` O₂-without-fuel bug (Major-ish) will trigger when the cylinder's fuel fraction falls below the ignition-time global mix. That happens late in the burn, or when backflowed exhaust dilutes the charge, and it silently converts O₂ to inert and inflates the mole count with no heat release.

### Gaps
- The exact Metghalchi–Keck table could not be fetched (the ANSYS host was unreachable from this sandbox). Constants were confirmed only from search snippets and the BU lecture description.
- Adiabatic flame temperature values come from general knowledge and were not fetched from a source.

---

## 3. Heat transfer, blowby and crankcase, friction

### Takeaway
- **Heat transfer** is Newton cooling with a constant h (default 100 W/m²K) to a uniform 90 °C wall, applied only in the cylinder. It uses an exact exponential update.
- **Blowby** is an orifice flow to a constant-pressure 1 atm crankcase.
- **Piston friction** is the MATLAB Simscape "Translational Friction" Stribeck/Coulomb/viscous formula, with the Coulomb part proportional to the computed side thrust.
- **Crank friction** is a constant Coulomb torque. There is no speed- or load-dependent FMEP model.

### Cited Findings
**Heat transfer** — [src/combustion_chamber.cpp:261-278](src/combustion_chamber.cpp):
- Area A = (V/A_bore)·π·B + 2·A_bore, i.e. liner + head + crown with flat faces.
- C = (f/2)·n·R (heat capacity).
- T_next = T_wall + (T − T_wall)·exp(−h·A·dt/C), then U += C·(T_next − T).
- Defaults: h = 100.0 and T_wall = 90 °C ([include/combustion_chamber.h:25-26](include/combustion_chamber.h); script default `heat_transfer_coefficient: 100.0`, `block_temperature: celcius(90)`, [es/objects/objects.mr:135-136](es/objects/objects.mr)). No shipped engine changes h (grep of assets).
- The intake runner, plenum, exhaust runner and collector have no heat transfer (no such calls in intake.cpp or exhaust_system.cpp).

**Standard practice for heat transfer.** In-cylinder heat transfer is usually computed with the time-varying Woschni (1967, SAE 670931) correlation: a Nusselt–Reynolds form with gas velocity tied to mean piston speed plus a combustion-induced term based on pressure rise. It is known to need adjustment at low and high speeds and at low loads ([Páv 2024, publi.cz](https://publi.cz/download/publication/5462?online=1); [CVUT MECCA](https://ojs.cvut.cz/ojs/index.php/MECCA/article/view/7571/6039); [Colorado State notes](https://www.engr.colostate.edu/~allan/heat_trans/page7a/page7a.html)).

**Blowby** — [src/combustion_chamber.cpp:279](src/combustion_chamber.cpp):
- `m_system.flow(piston->getBlowbyK(), dt, P_crankcase, 25 °C)`, an orifice to an infinite environment. Reverse flow brings in an inert mix.
- Crankcase pressure is fixed at 1 atm ([scripting/include/engine_node.h:125](scripting/include/engine_node.h)) and is also the pressure acting on the underside of the piston: F = −A_bore·(P_cyl − P_crankcase) ([src/combustion_chamber.cpp:444-445](src/combustion_chamber.cpp)).
- Blowby k is given per piston as `k_28inH2O(cfm)`, typically 0–0.6 cfm in assets.

**Piston friction** — [src/combustion_chamber.cpp:404-420](src/combustion_chamber.cpp):
- F = √2·e·(F_brk − F_C)·(v/v_St)·exp(−(v/v_St)²) + F_C·tanh(v/v_Coul) + f_v·v
- F_C = μ·|F_wall|, where F_wall is the magnitude of the line-constraint (side-thrust) reaction force ([src/piston.cpp:48-52](src/piston.cpp)).
- v_St = v_brk·√2 and v_Coul = v_brk/10.
- The force is attenuated linearly for |v| < 1e-3 m/s (lines 451-453).
- Defaults: μ = 0.06, F_brk = 50 N, v_brk = 0.1 m/s, f_v = 20 N·s/m. The last is labelled `units::force(20, N)`, which is a unit mislabel — [include/combustion_chamber.h:42-47](include/combustion_chamber.h).

**Comparison with Simscape.** The Simscape Translational Friction block defines F as the sum of Stribeck, Coulomb and viscous terms: √(2e)(F_brk − F_C)·exp(−(v/v_St)²)·v/v_St + F_C·tanh(v/v_Coul) + f·v, with **v_St = v_brk/√2** and v_Coul = v_brk/10 ([MathWorks Translational Friction](https://www.mathworks.com/help/simscape/ref/translationalfriction.html)). The code uses v_St = v_brk·**√2**, so the Stribeck hump sits at twice the intended velocity (a minor transcription deviation).

**Script friction parameters are not wired.** The script node `cylinder_friction_parameters` (friction_k / breakaway_friction / … with defaults 0.06/0/0/0, [es/objects/objects.mr:542-555](es/objects/objects.mr)) is not consumed by any C++ code: grep finds no reference in `scripting/` or `src/`. The hard-coded C++ defaults always apply.

**Crankshaft friction** — [src/piston_engine_simulator.cpp:83-85](src/piston_engine_simulator.cpp):
- A `RotationFrictionConstraint`, i.e. a velocity constraint ω = 0 whose impulse is limited to ±T_f — [dependencies/simple-2d-constraint-solver/src/rotation_friction_constraint.cpp:17-40](dependencies/simple-2d-constraint-solver/src/rotation_friction_constraint.cpp). This is ideal Coulomb friction: constant torque, independent of speed and load.
- Script default `friction_torque` is 5 lb·ft ([es/objects/objects.mr:377](es/objects/objects.mr)). Assets use 1–20 lb·ft.

**Standard practice for friction.** Typical engine-level friction models use FMEP correlations such as Chen–Flynn, FMEP = a + b·P_max + c·S̄p², with handbook ranges of roughly a ≈ 0.3–0.5 bar, b ≈ 0.004–0.006 and c ≈ 0.0006–0.0012 (from [US patent 8511265](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/8511265), quoting Basshuysen & Schäfer; not verified against the handbook). Newer SI models report better accuracy than Chen–Flynn ([SAE 2009-01-1984](https://saemobilus.sae.org/content/2009-01-1984/)).

### Inferences
- **Constant h = 100 W/m²K (Major).** From general textbook knowledge (not fetched), Woschni-type coefficients during combustion are typically in the thousands of W/m²K. Heat loss is therefore probably under-predicted during combustion and over-predicted relative to a correlation during intake. It never responds to speed, load or swirl.
  - Together with the over-hot gas (Section 2), the net heat loss could be either sign of wrong. It is not predictive for thermal efficiency, knock margin or exhaust temperature.
  - Adiabatic runners and exhaust mean exhaust gas temperature at the collector is not physically modelled (Major for turbo or exhaust work).
- **Constant 1 atm crankcase (Minor).** It ignores crankcase pumping and PCV. Fine for basic torque.
- **Piston friction magnitude (Major).** The viscous coefficient (20 N·s/m per piston) dominates at speed. My estimate for a 4-cylinder at 6000 rpm (mean piston speed ≈ 17 m/s, sinusoidal ⟨v²⟩ ≈ 356 m²/s²) is ≈ 28 kW of viscous friction. That is the same order as a plausible total FMEP ≈ 2 bar for a 2 L engine (≈ 22 kW at 6000 rpm). The magnitude is plausible, but the parameters are arbitrary, not calibrated, and identical for every engine. There is no ring-tension or gas-pressure ring loading, bearing friction, valvetrain friction, pumping split or accessory load.
- **Crank friction (Major).** A constant Coulomb torque gives friction power ∝ rpm instead of the FMEP rise with S̄p² that Chen–Flynn-type correlations predict.

### Gaps
- No quantitative comparison of the simulated friction against measured FMEP for any shipped engine.
- Woschni coefficient values were not retrieved: the search returned only secondary descriptions.

---

## 4. Mechanics: rigid-body solver, integration, timestep, inertia, starter, dyno, torque/power measurement

### Takeaway
The crank-slider mechanism is simulated as a true multibody system (crank, rods, pistons as 2-D rigid bodies joined by link and line constraints):
- Constraints are solved at velocity level with projected Gauss–Seidel and Baumgarte stabilisation β = 1, followed by semi-implicit (symplectic) Euler.
- Gas forces are applied to the piston bodies; for the Wankel, torque is applied to the shaft directly.
- The dyno is an ideal speed-servo constraint with a torque limit. Displayed torque is the constraint reaction torque averaged over 512 crank-angle bins of one 720° cycle, and power = torque × instantaneous ω.

This is physically sound for steady-state brake torque, with a small inertia error in sweep mode.

### Cited Findings
**Solver.** `OptimizedNsvRigidBodySystem` with `GaussSeidelSleSolver` — [src/simulator.cpp:31-46](src/simulator.cpp). One ODE step per timestep (`m_system->process(timestep, 1)`, line 122).
- Constraint solve — [dependencies/simple-2d-constraint-solver/src/optimized_nsv_rigid_body_system.cpp:179-230](dependencies/simple-2d-constraint-solver/src/optimized_nsv_rigid_body_system.cpp):
  - q̇' = q̇ + M⁻¹·F_ext·dt
  - (J·M⁻¹·Jᵀ)·λ = −(J·q̇' + v_bias + (β/dt)·C), with limits·dt as impulse bounds
  - R = Jᵀ·λ/dt; then a = M⁻¹(F_ext + R)
- β = 1.0 (line 8).
- The constraint outputs `ks`/`kd` (e.g. ks = 5000, kd = 10 set in [src/piston_engine_simulator.cpp:61-62,143-144](src/piston_engine_simulator.cpp)) are **not used** by the optimized system. Only `GenericRigidBodySystem` reads them (generic_rigid_body_system.cpp:149-177).

**Integrator.** v += a·dt; x += v·dt, i.e. symplectic Euler — [nsv_ode_solver.cpp:20-32](dependencies/simple-2d-constraint-solver/src/nsv_ode_solver.cpp).

**Gauss–Seidel.** `m_maxIterations = 128` and `m_minDelta = 1E-1`, i.e. stop when the max *relative* change is < 10 % — [gauss_seidel_sle_solver.cpp:9-10,45,81](dependencies/simple-2d-constraint-solver/src/gauss_seidel_sle_solver.cpp). The iteration count is settable per engine via `max_sle_solver_steps` ([src/engine.cpp:45,81](src/engine.cpp)).

**Timestep.**
- Default `simulation_frequency` is 10 kHz, and `fluid_simulation_steps` is 8 gas sub-steps per mechanical step ([es/objects/objects.mr:124-126](es/objects/objects.mr)). Assets range from 5 kHz to 40 kHz.
- Gas sub-stepping is in [src/piston_engine_simulator.cpp:333-349](src/piston_engine_simulator.cpp).
- The number of steps per frame adapts to keep the audio buffer latency on target ([src/simulator.cpp:89-101](src/simulator.cpp)). Simulated time per step is fixed, but wall-clock speed varies.

**Operator splitting.** Each step first integrates the rigid bodies using the chamber forces computed from the *previous* step's pressures (`CombustionChamber::apply`, [src/combustion_chamber.cpp:435-466](src/combustion_chamber.cpp)). It then updates throttle, ignition, volumes and gas flows ([src/simulator.cpp:121-162](src/simulator.cpp); [src/piston_engine_simulator.cpp:316-352](src/piston_engine_simulator.cpp)). This is explicit coupling.

**Bodies and inertia** — [src/piston_engine_simulator.cpp:76-81,199-203](src/piston_engine_simulator.cpp):
- Crank body: mass = crank mass + flywheel mass, I = `moment_of_inertia` from the script. The flywheel mass does not add to I.
- Piston: mass = piston mass, I = 1.0 (a dummy; piston rotation is unconstrained and irrelevant).
- Rod: mass and I from the script. The big-end and little-end offsets use the centre of mass ([src/connecting_rod.cpp:46-52](src/connecting_rod.cpp)).

**Joints and drift.**
- Piston-to-bore: `LineConstraint` at the wrist-pin point.
- Rod joints: `LinkConstraint`.
- Crank: fixed in position by `FixedPositionConstraint`.
- Multiple crankshafts are tied by `ClutchConstraint`s with infinite torque.
- A "Correct drift (temporary hack)" overwrites every crank's θ with the output shaft's θ each step ([src/simulator.cpp:133-138](src/simulator.cpp)).

**Chamber volume (exact geometry).** V = A_bore·(deck − s − compression_height) + V_chamber − V_piston-dome ([src/combustion_chamber.cpp:125-137](src/combustion_chamber.cpp)). Displacement is computed numerically from the min/max piston position over 1000 crank angles ([src/engine.cpp:262-313](src/engine.cpp)).

**Starter.** A one-sided speed-servo constraint: target −200 rpm by default, torque limit `starter_torque` (script default 200 lb·ft for piston engines, [es/objects/objects.mr:73-74](es/objects/objects.mr)) — [src/starter_motor.cpp:21-45](src/starter_motor.cpp). There is no motor torque-speed curve and no battery model.

**Dyno** — [src/dynamometer.cpp:25-55](src/dynamometer.cpp):
- J = [0, 0, 1] on the crank, v_bias = ±target speed, torque limits ±10000 lb·ft.
- Without "hold" it can only brake. With hold it can both brake and motor.
- `getTorque()` returns the constraint reaction torque F_t with a sign for the rotation direction.

**Displayed torque and power.**
- `Simulator::simulateStep` stores `m_dyno.getTorque()` in bin ⌊512·θ_cycle/720°⌋ and fills skipped bins ([src/simulator.cpp:140-160](src/simulator.cpp)).
- `getFilteredDynoTorque()` is the mean of the 512 bins (lines 201-210).
- `getDynoPower()` = mean torque × |ω| (lines 212-216).
- The UI applies a further RC = 0.1 s low-pass and peak tracking ([src/load_simulation_cluster.cpp:272-299](src/load_simulation_cluster.cpp)).

**Sweep mode.** In non-hold mode the target speed ramps at +500 rpm/s while torque > 1 ft·lb, and the dyno is disabled at redline ([src/engine_sim_application.cpp:1409-1431](src/engine_sim_application.cpp)).

**Filtered engine speed.** `updateFilteredEngineSpeed`: α = dt/(100 + dt); filtered = α·filtered + (1 − α)·rpm ([src/simulator.cpp:235-238](src/simulator.cpp)). With dt = 1e-4 this is effectively unfiltered. The weighting is inverted relative to the intended low-pass. It is used for audio attenuation.

### Inferences
- **Dyno physics (Minor).** At steady hold speed the averaged constraint torque equals net brake torque at the flywheel, after modelled piston, crank and blowby losses. This is physically correct and is how an absorption dyno with speed control works.
- **Sweep inertia error (Minor to Major).** During the 500 rpm/s sweep the dyno reads T_engine − I_total·α. With α = 52.4 rad/s² and the script-default I = 0.4 kg·m², the reading is about 21 N·m (~15 lb·ft) low. Rod and piston reciprocating inertia also contribute. Real dyno software corrects for this; the code does not.
- **Bin averaging (Minor).** Averaging over crank-angle bins rather than time is equivalent at constant speed. Under acceleration it slightly mis-weights. Bins are refreshed only when visited.
- **Solver accuracy (Minor for torque, Major for vibration).**
  - Velocity-level constraints with Baumgarte β = 1 and a 10 % relative GS tolerance give constraint compliance and drift. Energy is not exactly conserved; symplectic Euler is first order.
  - At 10 kHz and 6000 rpm a step is 3.6° of crank, coarse relative to the sub-degree resolution typical of cycle simulation. Explicit gas–mechanics coupling adds a one-step lag in gas force.
  - The mechanism is planar and rigid: no torsional crank flexibility, no bearing loads or outputs, no out-of-plane balance (rocking couples).
  - NVH, bearing-load or balance studies are not supported.
- **Starter and flywheel (Minor).** The starter is an idealised servo. The flywheel mass not adding to I is a modelling choice: users must fold flywheel inertia into `moment_of_inertia`.

### Gaps
- I did not measure solver constraint error or energy drift in a running engine.
- The exact effective I_total, including reflected rod and piston mass, for any asset was not computed.

---

## 5. Intake, throttle, carburetion, valvetrain, port flow and exhaust

### Takeaway
Gas-path topology per cylinder:

atmosphere → [throttle orifice + idle orifice] → plenum volume → [runner orifice, k_carb] → intake runner+port volume → [valve orifice, k(lift) table] → cylinder → [valve orifice] → exhaust runner+primary volume → [primary orifice] → collector volume → [outlet orifice] → atmosphere

Every element is an orifice between lumped volumes. Valve flow uses flow-bench CFM-versus-lift tables converted to effective areas, which is good practice. The throttle is a cos-law area scaling. Valve lift is kinematic from cam-lobe tables. There are no 1-D pipes, so runner and header lengths only set volumes, and the header length also sets the audio delay.

### Cited Findings
**Throttle and idle orifices** — [src/intake.cpp:75-117](src/intake.cpp):
- Main orifice: k = cos(θ_plate·π/2)·k_input, with θ_plate = `idle_throttle_plate_position` (default 0.975) × m_throttle.
- m_throttle = 1 − s^γ for pedal s ∈ [0, 1] ([src/direct_throttle_linkage.cpp:20-28](src/direct_throttle_linkage.cpp)).
- Plate angle displayed = (1 − θ_plate)·π/2 ([src/engine.cpp:187-189](src/engine.cpp)).
- The idle orifice (k_idle, fixed rich mixture) is always open.
- The atmosphere is reset to 1 atm, 25 °C, with the charge mixture, every sub-step (line 95).
- The intake script input `throttle_gamma` is accepted but unused (`m_throttleGammaUnused`, [scripting/include/intake_node.h:38](scripting/include/intake_node.h)).

**Governor.** v̇ = −k_s·(ω_t² − ω²) − k_d·v; throttle += v·dt; throttle output = 1 − (1 − x)^γ ([src/governor.cpp:36-52](src/governor.cpp)).

**Plenum and runners.**
- The plenum is one `GasSystem` (default 2 L, cross-section 100 cm²) ([src/intake.cpp:24-57](src/intake.cpp); [es/objects/objects.mr:702-712](es/objects/objects.mr)).
- Per cylinder, the runner volume is the head runner volume plus cross-section × `runner_length` (default 4 in), initialised at 1 atm and 25 °C ([src/combustion_chamber.cpp:82-97](src/combustion_chamber.cpp)).
- Plenum → runner uses k = `runner_flow_rate` (default `k_carb(200)`).

**Valve flow.** k = PortFlow(lift) via `Function::sampleTriangle`, a triangle-kernel weighted average of table points ([src/cylinder_head.cpp:51-59](src/cylinder_head.cpp); [src/function.cpp:104-130](src/function.cpp)). Head tables are given as lift (thou) → `k_28inH2O(cfm)` ([es/part-library/parts/heads.mr:5-34](es/part-library/parts/heads.mr)). The same k is used for forward and reverse flow.

**Valve lift.**
- lift = LobeProfile(θ_cam + lobe offset), with θ_cam = (θ_crank + advance)·0.5 mod 2π ([src/camshaft.cpp:41-58](src/camshaft.cpp); [src/standard_valvetrain.cpp](src/standard_valvetrain.cpp)).
- Lift is purely kinematic: no valve spring, float, bounce or lash dynamics.
- VTEC is a cam switch (`vtec_valvetrain.cpp`).

**Exhaust.**
- Per-cylinder exhaust runner+primary volume = head exhaust runner volume + cross-section × (primary tube length + header primary length) ([src/combustion_chamber.cpp:99-114](src/combustion_chamber.cpp)).
- Primary → collector uses k = `primary_flow_rate` (default `k_carb(100)`).
- The collector is one volume = collector cross-section × `length` (default 100 L; [es/objects/objects.mr:763-773](es/objects/objects.mr)).
- Collector → atmosphere uses k = `outlet_flow_rate` (default `k_carb(1000)`).
- Backflow from the atmosphere is inert unless `backflow_atmospheric_mixing` is set ([src/exhaust_system.cpp:61-87](src/exhaust_system.cpp)).

**Standard practice for valves.** Engine valve flow is normally modelled with the compressible restriction equation and a discharge coefficient. Measured C_d for exhaust valves varies (≈0.6–0.9) with pressure ratio and pulsating conditions, so a constant C_d is a simplification ([Hanriot et al. 2015](https://repository.up.ac.za/bitstream/2263/44566/1/Hanriot_Influence_2015.pdf); [FAMU valve-flow notes](https://web1.eng.famu.fsu.edu/~shih/propulsion/lecture%20notes/Valve%20Flows.pdf)).

### Inferences
- **No 1-D wave dynamics (Critical for tuning).** With lumped volumes only, intake and exhaust tuning, header length/diameter effects, collector scavenging and resonance (Helmholtz) are not physically represented. `runner_length` and `primary_tube_length` only change volumes. 1-D gas-dynamics codes such as GT-Power or WAVE solve the pipe equations; this is the largest gap for an engineering tool.
- **Port flow tables (Minor, a good approach).** Using flow-bench data is standard. Limitations: C_d is fixed at the 28 inH₂O test pr, the same coefficient applies to reverse flow, and there is no separate valve-curtain area / C_d formulation.
- **Throttle law (Minor to Major).**
  - A geometric butterfly open area goes as ≈ 1 − cos ψ / cos ψ₀ (Heywood eq. 7.x, from memory, not re-verified). The cos(θ·π/2) law is linear near closed rather than quadratic, which affects part-load and idle airflow resolution.
  - The plate position cannot reach fully closed (0.975 cap).
  - The ignored `throttle_gamma` can mislead users who set it.
- **"Perfect carburettor" (Major for mixture studies).** Fixed composition means no venturi metering, no enrichment, no transient wall-wetting and no injection timing.
- **Exhaust (Major for exhaust T, back-pressure and turbo studies).** One collector volume per exhaust system, adiabatic walls and a fixed outlet k. There is no catalyst or muffler flow resistance beyond the outlet orifice, and no turbocharger or supercharger model (none exists in `src/`).

### Gaps
- The exact Heywood throttle-area equation and the valve-curtain C_d formulation were not fetched in this session.

---

## 6. Vehicle, transmission, clutch and drivetrain

### Takeaway
The vehicle is a single rotational body whose inertia equals the vehicle mass reflected through gear, final drive and tyre radius. It is coupled to the crank by an ideal Coulomb clutch, i.e. a velocity constraint with a torque limit. Drag, rolling resistance and brake force are applied as a one-sided torque-limited velocity constraint. Optional torsional springs model clutch and axle compliance. There is no tyre slip, driveline efficiency, wheel/gearbox inertia or grade.

### Cited Findings
**Reflected inertia** — [src/transmission.cpp:78-106](src/transmission.cpp):
- I_veh = m_car·(r_tyre/(i_diff·i_gear))². On a gear change the vehicle-body kinetic energy is kept, so vehicle speed is preserved.
- v = √(2·E_rot/m) = |ω|·r/(i_diff·i_gear) ([src/vehicle.cpp:61-71](src/vehicle.cpp)).
- Force reflects to torque by T = √(I/m)·F = F·r/(i_diff·i_gear) (lines 73-79).

**Clutch.** `ClutchConstraint` relates crank ω to vehicle-body ω with impulse limits ±T_clutch,max·pressure ([src/transmission.cpp:44-54](src/transmission.cpp); [clutch_constraint.cpp:58-87](dependencies/simple-2d-constraint-solver/src/clutch_constraint.cpp)). Static and kinetic friction are equal.

**Road load.** T_res = reflected(F_rr + ½·ρ·v²·C_d·A + F_brake) as a one-sided limit ([src/vehicle_drag_constraint.cpp:23-72](src/vehicle_drag_constraint.cpp)):
- ρ is fixed: M_air·1 atm/(R·298.15 K) ≈ 1.18 kg/m³.
- Default `rolling_resistance` is 2000 N, constant and independent of mass ([es/objects/objects.mr:830](es/objects/objects.mr)).
- Default mass is 1000 kg, C_d 0.25, area 72 in × 72 in, diff 3.42, tyre radius 10 in (lines 825-831).

**Compliance.** `DrivetrainSpring`: T = k·windup + c·Δω, with an end-stop at 50·k beyond max flex and slip clamping at max torque ([src/drivetrain_spring.cpp:40-87](src/drivetrain_spring.cpp)). The driveshaft inertia is 0.05 kg·m² ([include/piston_engine_simulator.h:73](include/piston_engine_simulator.h)). Compliance is used only if `simulate_flex` is set.

**Unused vehicle inputs.** `coeff_static_friction`, `wheel_base`, `center_of_mass` and similar are accepted but explicitly unused ([es/objects/objects.mr:841-849](es/objects/objects.mr)).

### Inferences
- **Reflected-inertia model (Minor for engine work).** It is a reasonable first-order longitudinal model.
- **Rolling resistance default (Major for vehicle-performance outputs).** 2000 N is about 20 % of the weight of a 1000 kg car, about an order of magnitude above typical C_rr·m·g ≈ 0.01–0.015·m·g (general knowledge). Acceleration, top speed and mpg are strongly biased unless scripts override it.
- **Missing elements (Major for vehicle outputs).** No transmission or final-drive efficiency, no rotating wheel inertia, no tyre slip or traction limit, and fixed air density.

### Gaps
- None of the shipped vehicle parameters were checked against real vehicles.

---

## 7. Rotary (Wankel) engine

### Takeaway
The rotary geometry is handled carefully:
- Epitrochoid housing, numerically computed inner-envelope rotor flank, chamber volume by Green's theorem.
- Torque by virtual work, T = (p − p_cc)·dV/dθ.
- Rotor inertia folded into the eccentric shaft as m·e² + I/9.

The gas, combustion and heat-transfer models reuse the piston-engine chamber with an "equivalent bore". There is no apex or side-seal leakage between chambers.

### Cited Findings
**Geometry** — [src/wankel_rotor.cpp](src/wankel_rotor.cpp):
- Housing: x = e·cos 3α + R·cos α, y = e·sin 3α + R·sin α (lines 167-175).
- Face chord area by Green's theorem (lines 90-106) and its derivative (108-115).
- V_face = (A_chord − A_flank)·depth + V_depression (117-124).
- dV/dθ = dA/dφ·depth/3 (126-129).
- Port opening = fraction of the port's angular width covered by the face (131-148).
- Flank shape is the inner envelope by sweeping (216-273).
- Effective inertia = m·e² + I_rotor/9 (157-161).

**Torque.** `system->t[shaft] += (P − P_crankcase)·dV/dθ` ([src/wankel_combustion_chamber.cpp:26-37](src/wankel_combustion_chamber.cpp)). Seal friction is lumped into shaft friction; the per-chamber friction force is 0 (lines 39-43).

**Turbulence velocity.** The "piston speed" used for turbulence is R·|ω|/3 (lines 49-56).

**Equivalent bore.** bore = √(4·depth·R·√3/π) ([scripting/include/wankel_engine_node.h:251](scripting/include/wankel_engine_node.h)). The flame-geometry and heat-transfer area formulas in `CombustionChamber::flow` use this bore ([src/combustion_chamber.cpp:261-265,346-348](src/combustion_chamber.cpp)).

**Ignition.** One spark per rotor lights whichever face is at the plug angle ([src/wankel_engine_simulator.cpp:37-52](src/wankel_engine_simulator.cpp)).

### Inferences
- **Kinematics and torque (Minor; good).** Correct and energy-consistent.
- **Thermal and combustion (Major for rotary-specific engineering).** The Wankel's long, thin chamber (high surface-to-volume ratio, flame quench at the trailing apex) and its apex-seal leakage drive its efficiency and HC characteristics, and the cylinder-equivalent flame and heat-transfer geometry cannot capture them. Port flow vs. opening is linear in covered angle times a flow function, a reasonable approximation.

### Gaps
- No validation of Wankel displacement or compression ratio against a reference engine (e.g. 13B) was done in this audit.

---

## 8. Audio synthesis: how exhaust pressure becomes sound

### Takeaway
Sound is not a physical acoustic prediction. Each cylinder's exhaust-runner gauge pressure plus a fraction of its dynamic pressure is scaled, delayed by L/343 m/s, attenuated by 1/L², summed per exhaust system, then processed:
1. resampled to 44.1 kHz
2. low-passed at 1.9 kHz
3. jittered
4. DC-filtered
5. mixed with its derivative
6. modulated by filtered noise
7. convolved with an impulse-response WAV
8. auto-levelled

### Cited Findings
**Per-cylinder signal** — [src/piston_engine_simulator.cpp:419-458](src/piston_engine_simulator.cpp):
- s = a³·1600·[(P_runner − 1 atm) + 0.1·q(+x) + 0.1·q(−x)], where a = min(|filteredEngineSpeed|, 40)/40.
- Delayed by `DelayFilter` with delay = (header length + exhaust length)/343 m/s ([src/piston_engine_simulator.cpp:253-259](src/piston_engine_simulator.cpp)).
- Scaled by sound attenuation × audio volume/n_cyl/L².

**Synthesizer** — [src/synthesizer.cpp:178-205,272-339](src/synthesizer.cpp):
- Linear-interpolation resampling with a 1900 Hz input anti-alias low-pass (line 77).
- Then a jitter filter, DC removal at 10 Hz, and v = dF_F_mix·dF/dt + (1 − dF_F_mix)·F·(airNoise·r + 1 − airNoise), with r = noise low-passed at 2 kHz.
- Then `convolution.process` with an impulse response clipped to ≤ 10000 samples (lines 96-110), a leveling AGC (target 30000, gain 1e-5…1.9), and int16 clipping.
- Defaults: dF_F_mix = 0.01, airNoise = 1.0, inputSampleNoise = 0.5 ([include/synthesizer.h:23-32](include/synthesizer.h)).

### Inferences
- **Sound realism (Critical if acoustics or noise predictions are wanted; Minor for engine performance).** Absolute SPL is lost (AGC), bandwidth is capped near 1.9 kHz, and the muffler and room response is an arbitrary recorded IR, not computed from the exhaust geometry. The delay uses ambient c = 343 m/s instead of hot exhaust c (≈ 500–700 m/s, general knowledge).
- **Physical basis (Minor).** The derivative mix loosely corresponds to monopole radiation (∝ dQ/dt), but only at 1 %.

### Gaps
- None for code behaviour. No acoustic validation data was consulted.

---

## 9. Units system and constants

### Takeaway
Internally everything is SI (m, kg, s, mol, Pa, K, J). `units.h` supplies conversion factors, which are numerically correct, with a few naming and labelling bugs.

### Cited Findings
- **Base units** are 1.0 for N, kg, m, s, Pa, K, J, mol ([include/units.h:7-95](include/units.h)).
- **Conversion factors:**
  - lbf = 4.44822 N; inch = 2.54 cm; hp = 745.699872 W; atm = 101.325 kPa; inHg = 3386.39 Pa; inH₂O = 0.0734824·inHg ≈ 248.84 Pa (≈ 60 °F water).
  - rpm factor = 0.104719755 (= 2π/60).
  - scfm = 0.002641 lbmol/min ≈ 1.198 mol/min per ft³. By my calculation this corresponds to ideal-gas standard conditions of ≈ 15 °C and 1 atm.
- **Constants:** R = 8.31446261815324, π, √2, e ([include/constants.h:6-9](include/constants.h)). `AirMolecularMass` = 28.97 g/mol ([include/units.h:178](include/units.h)).
- **`units::psia(p)`** returns p·psi − 1 atm, a gauge-style conversion under a "psia" name, and `psig = psi` ([include/units.h:76,121-127](include/units.h)). Neither is used in `src/`, `include/` or `scripting/` C++ besides the definitions (grep).
- **Viscous friction coefficient** is declared with `units::force(20, units::N)` although it is N·s/m ([include/combustion_chamber.h:46](include/combustion_chamber.h)).

### Inferences
- **Unit naming (Minor).** The units system is sound. The psia/psig naming and the friction-unit label are cosmetic but could mislead future development.
- **Constants (Minor).** The real physical-constant risk is not units but the fixed γ, M_air and 25 % O₂ assumptions covered above.

### Gaps
- None.

---

## 10. Engineering outputs currently computed, and how

### Takeaway
The program displays brake torque and power (physically sound at steady state), MAP, a molar-flow "VE", an intake "AFR" and an exhaust O₂ % (both with inconsistent constants), fuel consumption and mpg, and per-cylinder pressure and temperature. It does not compute IMEP, BMEP, PMEP, FMEP, BSFC, indicated or brake efficiency, burn angles (CA10/50/90), heat-release rate, knock or emissions.

### Cited Findings
**Brake torque and power.** See Section 4: the dyno constraint torque averaged over a cycle, P = T·ω ([src/simulator.cpp:201-216](src/simulator.cpp)). Displayed only when the dyno is enabled ([src/load_simulation_cluster.cpp:188-195](src/load_simulation_cluster.cpp)).

**Volumetric efficiency** — [src/right_gauge_cluster.cpp:259-271](src/right_gauge_cluster.cpp):
- VE = ṅ_intake / ṅ_theoretical, where ṅ_theoretical = 0.5·P_amb·V_d/(R·T_amb)·rpm/60 with P_amb = 1 atm and T_amb = 25 °C.
- ṅ_intake = the sum of `Intake::m_flow` (main throttle orifice only) per frame divided by frame time ([src/piston_engine_simulator.cpp:341-344,380-384](src/piston_engine_simulator.cpp); [src/engine.cpp:315-322](src/engine.cpp)).
- The "CFM" gauge is the same ṅ converted to scfm (line 272-275).

**Intake AFR.** (M_O2·n_O2/0.21)/(n_fuel·114.23 g/mol), measured over the plenum contents ([src/engine.cpp:337-354](src/engine.cpp)).

**Exhaust O₂.** Mass fraction M_O2·n_O2/(n_fuel·M_octane + n_inert·M_N2 + n_O2·M_O2) over the collector contents, shown ×100 ([src/engine.cpp:356-379](src/engine.cpp); [src/afr_cluster.cpp:76-83](src/afr_cluster.cpp)).

**Per-event AFR.** (M_O2·n_O2 + M_N2·n_inert)/(M_octane·n_fuel) at ignition ([src/combustion_chamber.cpp:387-402](src/combustion_chamber.cpp)).

**Fuel consumption.**
- Total moles of fuel through both the throttle and the idle orifice × M_fuel, divided by density for volume ([src/intake.cpp:110-116](src/intake.cpp); [src/engine.cpp:387-398](src/engine.cpp)).
- mpg = distance/gallons; cost = $4.761/gal ([src/fuel_cluster.cpp:56-69](src/fuel_cluster.cpp)).

**Other displays.**
- MAP: mean plenum pressure ([src/engine.cpp:328-335](src/engine.cpp)).
- Cylinder pressure and temperature gauges: `m_system.pressure()` / `temperature()` ([src/cylinder_pressure_gauge.cpp:62,78](src/cylinder_pressure_gauge.cpp); [src/cylinder_temperature_gauge.cpp:43,89](src/cylinder_temperature_gauge.cpp)).
- Oscilloscopes: P–V, exhaust flow, and torque/power versus rpm ([src/oscilloscope_cluster.cpp:234-355](src/oscilloscope_cluster.cpp)).
- `m_peakTemperature` and `m_nBurntFuel` are accumulated in the chamber ([src/combustion_chamber.cpp:257-259,377](src/combustion_chamber.cpp)).

### Inferences
- **Torque and power (Minor issues).** Correct as a measurement method. Accuracy is limited by the upstream models (Sections 1–3).
- **VE (Minor).** It is computed on total moles including fuel vapour (~2.4 % of the charge), ignores idle-circuit airflow (significant at idle), and is measured at the throttle rather than trapped at IVC. It is not the standard air-mass VE on a dry-air basis.
- **Intake AFR (Major).** The formula divides n_O2 by 0.21 although model air is 25 % O₂, uses M_O2 (32) instead of M_air (28.97), and uses iso-octane's 114.23 g/mol although the default fuel is 100 g/mol.
  - For the default charge (n_O2/n_fuel = 10) the gauge shows ≈ 13.3. The model's own mass ratio is 40·28.97/100 ≈ 11.6, and a real gasoline equivalent at λ = 0.8 would be ≈ 0.8 × 14.7 ≈ 11.8 (stoichiometric AFR 14.7 per the IFP table above).
  - Displayed AFR values are not trustworthy.
- **Exhaust O₂ and per-event AFR (Minor).** Both count all inert (CO₂ and H₂O included) as N₂ mass. They are indicative only.
- **Missing standard engineering outputs (Major as a feature gap).** IMEP/PMEP/BMEP/FMEP, BSFC, efficiencies, MFB angles, heat-release rate, exhaust temperature, trapped mass, residual fraction and knock index. Most could be derived cheaply from existing state, e.g. ∮P·dV per cycle from the chamber volume and pressure.

### Gaps
- I did not run the application to read actual gauge values; the numbers above are hand calculations from the code formulas.

---

## 11. Consolidated severity ranking and bug list

### Takeaway
For engineering use, the decisive limitations are:
1. No 1-D wave dynamics.
2. Constant-property thermodynamics (γ = 1.4, c_v = 2.5R, fixed M, 25 % O₂ air).
3. Flow energy carried as internal energy rather than enthalpy.
4. A heuristic, random combustion-efficiency model with a frozen flame speed evaluated at λ instead of φ.
5. Constant-h heat transfer.
6. Uncalibrated friction.

The dyno measurement, port-flow calibration, crank-slider kinematics and Wankel geometry are physically sound foundations.

### Cited Findings
**Ranked issues** (evidence in the sections above; file:line in brackets):

| # | Area | Issue | Severity |
|---|---|---|---|
| 1 | Gas dynamics | 0-D lumped volumes, no 1-D pipe/wave model; runner and header length only set volume [combustion_chamber.cpp:82-114] | Critical |
| 2 | Thermo properties | Constant f = 5 (γ = 1.4, c_v = 2.5R), M = 28.97 g/mol for all species [gas_system.h:45,134-136,242-244] → ~4900 K complete-burn temperature in test | Critical |
| 3 | Flow energy | Moved gas carries c_v·T, not c_p·T [gas_system.cpp:424-426] → isothermal blowdown (1500 K vs 1230 K) and no filling heating (300 K vs 404 K) | Critical |
| 4 | Combustion | Random burning efficiency (C `rand()`) × max 0.8 decides fuel burned; geometric flame using volume fraction as mass fraction [combustion_chamber.cpp:196-220,345-384] | Critical |
| 5 | Flame speed | Metghalchi–Keck evaluated with λ, not φ [fuel.cpp:56]; frozen at ignition [combustion_chamber.cpp:221-227]; unused pressure terms [fuel.cpp:46] | Major |
| 6 | Mixture | Air = 25 % O₂; fixed λ = 0.8 charge, "perfect carb" [intake.cpp:63-73]; LHV 48.1 MJ/kg vs ~43.5 | Major |
| 7 | Heat transfer | Newton cooling, constant h = 100 W/m²K, uniform 90 °C wall, cylinder only [combustion_chamber.cpp:261-278] | Major |
| 8 | Friction | Constant Coulomb crank torque; Simscape-form piston friction with hard-coded, unwired parameters and v_St = √2·v_brk instead of v_brk/√2 [combustion_chamber.cpp:404-420; combustion_chamber.h:42-47] | Major |
| 9 | AFR display | 0.21 vs 0.25 O₂, M_O2 vs M_air, octane molar mass [engine.cpp:337-354] | Major |
| 10 | Vehicle | 2000 N constant rolling resistance default; no driveline losses [objects.mr:830] | Major (vehicle outputs) |
| 11 | Rotary thermal | Equivalent-bore flame and heat transfer; no apex leakage [wankel_engine_node.h:251] | Major (rotary) |
| 12 | `react()` | O₂ consumed without fuel because of independent minima; C8H16 label with C8H18 stoichiometry; mass not conserved [gas_system.cpp:82-129] | Major/Minor |
| 13 | Solver | Velocity-level PGS, 10 % relative tolerance, Baumgarte β = 1, symplectic Euler, 3.6° steps at 6000 rpm, one-step-lagged gas force; θ drift hack [simulator.cpp:133-138] | Minor (torque) / Major (NVH) |
| 14 | Dyno sweep | Reads T − I·α at 500 rpm/s (≈ 21 N·m low for I = 0.4) [engine_sim_application.cpp:1409-1431] | Minor |
| 15 | VE gauge | Molar incl. fuel, excludes idle circuit, 25 °C/1 atm reference [right_gauge_cluster.cpp:259-271] | Minor |
| 16 | Throttle | cos law; `throttle_gamma` ignored [intake.cpp:86; intake_node.h:38] | Minor |
| 17 | Gas numerics | `pressureEquilibriumMaxFlow` computed but unused in volume-to-volume flow [gas_system.cpp:407-408]; `dynamicPressure` wrong for f ∉ {3, 5} [gas_system.h:235-236] | Minor |
| 18 | Misc | Filtered engine speed weighting inverted (effectively unfiltered) [simulator.cpp:235-238]; audio delay at 343 m/s [piston_engine_simulator.cpp:257]; `psia()` naming [units.h:121-127]; friction-coefficient unit label [combustion_chamber.h:46]; crankcase fixed 1 atm [engine_node.h:125]; flywheel mass not added to I [piston_engine_simulator.cpp:79-81] | Minor |
| 19 | Audio | Non-physical pipeline (AGC, 1.9 kHz LPF, arbitrary IR) [synthesizer.cpp] | Minor (performance) / Critical (acoustics) |

### Inferences
- **What is sound and reusable:** the orifice-flow law with flow-bench k, the exact chamber-volume kinematics, the multibody crank-slider (piston side force feeds friction), the speed-controlled dyno measurement, and the Wankel volume and torque formulation.
- **Highest-leverage upgrades:**
  - Carry enthalpy in flows.
  - Introduce temperature- and composition-dependent c_p and γ (e.g. NASA polynomials for N₂/O₂/CO₂/H₂O/fuel) together with real air composition (≈21 % O₂) and a real LHV.
  - Replace the random efficiency with a Wiebe or two-zone entrainment model: φ-correct S_L with residual dilution, updated every step.
  - Use Woschni heat transfer.
  - Use a Chen–Flynn-type FMEP or calibrated component friction.
  - Add 1-D pipe gas dynamics for runners and headers.
  - Add IMEP/BMEP/BSFC/MFB outputs.
- **Tuning knobs act as compensating calibration factors.** `max_burning_efficiency`, the 25 % O₂ air and the 0.8·…·4 mixture factor make the default engines produce plausible-looking power. Fixing one model in isolation (e.g. the enthalpy flow or the variable c_p) will likely shift the power output of every shipped engine, so asset re-calibration should be expected.

### Gaps
- No full-engine quantitative validation (torque curve versus a dyno sheet) was performed in this audit. Severity ratings are based on model-form analysis and isolated numerical tests.
- Some textbook comparison values (adiabatic flame temperature, Woschni magnitudes, dry-air O₂ fraction, throttle-area formula, typical C_rr) are from general knowledge and were not fetched from sources in this session.
