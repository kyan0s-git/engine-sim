# Gas Exchange Dynamics for Real-Time IC Engine Simulation (intake, valves/ports, exhaust, boosting)

> **How sources were accessed.** In this session WebSearch worked, but WebFetch could not resolve most hosts (Wikipedia, HAL, OpenWAM docs, Linköping, Polimi, Imperial and CVUT all failed with DNS or proxy errors). Findings tagged with a web link come from search-result summaries of those pages. Findings tagged **(textbook; not fetched)** are standard results from the named reference, written down from domain knowledge. They are the usual textbook forms, but no one checked them against the page in this session, so check the equation numbers before quoting them in a report. Findings tagged **(local code)** were read directly from this repository. Numbers tagged **(computed)** were calculated in this session with Python, and their inputs are stated.

---

## 1. Compressible flow through restrictions and valves: nozzle equation, choking, reverse flow, effective-area conventions, Cd vs L/D, flow-bench CFM conversion

### Takeaway
Every 0D/1D code models a restriction (valve, port, throttle, orifice) as a quasi-steady isentropic nozzle. Its effective area is A_eff = C_D·A_ref. The flow chokes when p_down/p_up ≤ (2/(γ+1))^{γ/(γ−1)}, which is 0.528 at γ=1.4. Reverse flow is handled by swapping upstream and downstream and using a reverse C_D. The C_D value means nothing without its reference area: curtain area, inner-seat area, or pipe area. A flow-bench rating converts to effective area at about **4.5–4.6 mm² per CFM @ 28 inH2O**.

### Cited Findings
**Nozzle (orifice) mass-flow equation, SI.** Symbols: ṁ is mass flow [kg/s]; C_D is the discharge coefficient [-]; A_R is the reference area [m²]; p_0 and T_0 are upstream stagnation pressure [Pa] and temperature [K]; p_T is downstream static pressure (at the throat) [Pa]; R is the gas constant [J/(kg·K)], 287 for air; γ = c_p/c_v.
- Subsonic case, p_T/p_0 > p_cr:
  ṁ = C_D A_R · p_0/√(R T_0) · (p_T/p_0)^{1/γ} · √{ 2γ/(γ−1) · [1 − (p_T/p_0)^{(γ−1)/γ}] }
- Choked case, p_T/p_0 ≤ p_cr:
  ṁ = C_D A_R · p_0/√(R T_0) · √γ · (2/(γ+1))^{(γ+1)/(2(γ−1))}
- Critical ratio: p_cr = (2/(γ+1))^{γ/(γ−1)}.

  Source: Heywood, *Internal Combustion Engine Fundamentals*, App. C and Ch. 6 **(textbook; not fetched)** — [Heywood 1988](https://openlibrary.org/isbn/007028637X); [Heywood 2nd ed. 2018](https://openlibrary.org/isbn/9781260116106).
- **(computed)** p_cr = 0.5283 at γ=1.40, 0.5369 at γ=1.35 and 0.5457 at γ=1.30. The choked-flow function √γ(2/(γ+1))^{(γ+1)/(2(γ−1))} is 0.6847, 0.6761 and 0.6673 for the same three values of γ.
- **(local code)** engine-sim already implements exactly this equation. `GasSystem::flowRate()` picks the upstream side by comparing pressures, which is how it handles reverse flow. It caches the choked limit and choked factor, and multiplies the result by `k_flow`, which is effectively C_D·A_R. It works in molar units through `constants::R`. The subsonic branch uses the algebraically equivalent form s = (p_T/p_0)^{1/γ}, giving flow ∝ √(2γ/(γ−1)·s·(s − p_T/p_0)) — [engine-sim src/gas_system.cpp](https://github.com/ange-yaghi/engine-sim/blob/master/src/gas_system.cpp) (local copy: /home/user/engine-sim/src/gas_system.cpp, lines 131–245).
- **(local code)** engine-sim turns flow-bench CFM figures into `k_flow` with `k_28inH2O(scfm)`. That function inverts the same nozzle equation at p_0 = 1 atm, Δp = 28 inH2O, T = 25 °C and γ=1.4. Carburetor ratings go through `k_carb()`, which uses 1.5 inHg — same file, lines 163–189.
- **Reference area and Cd are linked.** The product C_D·A_R is the effective flow area of the valve and port assembly. C_D is defined as actual mass flow divided by the isentropic mass flow — Heywood & Sher text as summarized at [Taylor & Francis: discharge coefficient](https://taylorandfrancis.com/knowledge/Engineering_and_technology/Chemical_engineering/Discharge_coefficient).
- **Constant Cd is an approximation.** Brahma (2019) reports that the usual constant-C_D assumption is inaccurate for highly compressible pulsating flow, especially at large amplitudes and low flow rates — [Frontiers in Mech. Eng. 2019](https://www.frontiersin.org/journals/mechanical-engineering/articles/10.3389/fmech.2019.00025/epub).
- **Cd depends on both lift and pressure ratio.** Accurate engine simulation needs C_D known as a function of both — [SAE 962527, "Relationship Between Discharge Coefficients and Accuracy of Engine Simulation"](https://saemobilus.sae.org/content/962527).
- **Cd depends on how it is measured.** C_D computed from throat static pressure at the seat was up to 12% lower than C_D computed from downstream static pressure, mainly at low mass flow — [SAE 2004-01-1676](https://saemobilus.sae.org/content/2004-01-1676/).
- **Dynamic Cd differs from steady Cd.** In a running engine, the dynamic C_D during valve opening was larger than during closing at low engine speed — [Da-Yeh Univ. study](https://people.dyu.edu.tw/paper/8701422_e.pdf).
- **Simple poppet-valve area model.** A_port = πD²/4, A_curtain = πDL·sin(α), and A_eff = C_D·min(A_port, A_curtain). This comes from a generic calculator page, so treat it as a sketch only — [Firgelli poppet valve](https://www.firgelliauto.com/blogs/mechanisms/poppet-valve).
- **Heywood's three-stage minimum valve flow area.** Symbols: L is lift, D_v the valve head diameter, w the seat width, β the seat angle, D_m the mean seat diameter, D_p the port diameter, D_s the stem diameter **(textbook; not fetched)** — [Heywood Ch. 6](https://openlibrary.org/isbn/007028637X):
  1. When 0 < L < w/(sinβ cosβ): A_m = π L cosβ (D_v − 2w + (L/2) sin2β).
  2. At intermediate lift: A_m = π D_m √[(L − w tanβ)² + w²].
  3. At high lift, when the port controls: A_m = (π/4)(D_p² − D_s²).

  Most codes avoid this by using the curtain area π D_v L, or the inner-seat area, as A_ref, and folding the geometry into C_D(L/D).
- **Flow-bench conventions.** Most testing is done at 10 and 28 inH2O. Lift is stepped as L/D from about 0.05 to 0.3, and normal engines run up to about L/D 0.35 — [Wikipedia, Air flow bench](https://en.wikipedia.org/wiki/Air_flow_bench).
- **28 inH2O is a convention, not a match to engine conditions.** It "stuck" historically. In a running engine the pressure drop across the intake valve is often about twice 28" and varies a lot — [Performance Trends, "Why do we flow heads at 28″ water?"](https://performancetrends.com/blog/?p=293); [Race Engine Technology](https://www.raceenginetechnology.com/News/why-do-we-flow-heads-at-28-water).
- **A 28 inH2O bench runs at a low pressure ratio.** It reaches only about 1.07, so C_D measured there may not transfer to high pressure ratios such as exhaust blowdown — [fiatdinoforum flow-bench note](https://fiatdinoforum.org/download/file.php?id=4007).
- **Converting between test depressions.** Flow scales with √Δp, which is the incompressible approximation. Converting 10" data to 28" is questioned on forums. One poster reports that 10" tracked 28" only up to about 150 CFM — [firstgenfirebird forum](https://firstgenfirebird.org/forums/ubbthreads.php/posts/110022.html); [slantsix forum](https://slantsix.org/forum/viewtopic.php?p=514442).
- **(computed) CFM to effective area.** Inputs: Δp = 28 inH2O = 6974.5 Pa, p_0 = 101325 Pa, T_0 = 298.15 K, so p_T/p_0 = 0.9312. The compressible nozzle equation gives A_eff = 4.59 mm² per CFM if CFM is referenced to ρ = 1.204 kg/m³, or 4.52 mm² per CFM at the upstream density of 1.184 kg/m³. The incompressible shortcut, A = Q/√(2Δp/ρ) with v = 108.5 m/s, gives 4.35 mm² per CFM, about 4% low. Worked example: a port flowing 300 CFM @ 28" has A_eff ≈ 1.37×10⁻³ m², the same as an ideal orifice of ≈ 41.8 mm diameter.
- **Commercial-code conventions (textbook; not fetched; from the author's knowledge of the manuals, which no one checked in this session):**
  - GT-POWER valve templates take a reference diameter plus forward and reverse C_D arrays against lift or L/D, and A_eff = C_D·πD_ref²/4.
  - Ricardo WAVE uses flow coefficients referenced to the valve inner-seat area.
  - AVL BOOST gives flow coefficients relative to the attached pipe cross-section, with a scaling factor of about n_v·d_vi²/d_pipe².

  Treat these as conventions to confirm, not facts.

### Inferences
- **Implementation:** keep engine-sim's `flowRate()` structure. Add separate forward and reverse C_D(L/D) tables per valve, and reference them to curtain area π D_v L, or seat area π D_v²/4 with an explicit flag. Then a user can paste bench data as CFM@28"(lift) and get A_eff(L) = 4.55 mm²·CFM(L). That is already what `k_28inH2O` does implicitly.
- **Typical values (textbook recollection, Heywood Ch. 6 / Blair; not fetched):**
  - Inlet C_D based on curtain area is about 0.6–0.7 at low lift (L/D < 0.1, flow attached), dips at intermediate L/D when flow separates from the seat, and falls at L/D > 0.25 where the port throat limits flow.
  - Well-developed 4-valve ports reach a flow coefficient on inner-seat area of about 0.5–0.6 at L/D ≈ 0.25–0.30.
  - Exhaust valves show similar or slightly higher C_D on curtain area at low lift.
  - Use these only as defaults when no bench data exists.
- **Exhaust blowdown runs with p_cyl/p_port of 3–6.** Valve flow is therefore choked early in the exhaust event. The choked branch must be cheap, and C_D should ideally be pressure-ratio aware.
- **Use a smooth transition near Δp → 0.** The subsonic formula has an infinite derivative there, which makes explicit integration stiff: tiny pressure differences produce large flow swings. Linearize below a small Δp (laminar-like ṁ ∝ Δp), or limit flow to the pressure-equalizing amount per step. engine-sim does the latter with `pressureEquilibriumMaxFlow`.

### Gaps
- Published C_D(L/D) curves (Heywood Fig. 6-x, Blair's measured coefficients) could not be fetched, so no numeric table is cited here.
- The exact definitions in the GT-SUITE Flow Theory Manual, WAVE and BOOST manuals were not accessible. They are paywalled or licensed, and search returned no excerpts.
- How strongly C_D depends on pressure ratio at engine-like pressure ratios (1.5–5) was not quantified.

---

## 2. Throttle body (butterfly) effective area vs angle; idle air control

### Takeaway
The open area of a butterfly throttle follows a closed-form geometric expression from Heywood. It depends on plate angle ψ, closed angle ψ₀ and the shaft-to-bore ratio a = d/D. Multiply it by an angle-dependent C_D, or calibrate it as A_eff(θ) from measurement. Idle air bypass (IAC) is a separate parallel orifice whose area is set by the controller duty cycle.

### Cited Findings
- **Heywood throttle open-area formula (textbook; not fetched — Heywood Ch. 7, flow through the throttle).** Symbols: D is the bore; d the shaft diameter; a = d/D; ψ the plate angle from the plane normal to the bore axis; ψ₀ the angle at which the plate seals, usually 5–10°.

  A_th / (πD²/4) = [1 − cosψ/cosψ₀] + (2/π)·[ (a/cosψ)·√(cos²ψ − a²cos²ψ₀) + (cosψ/cosψ₀)·asin(a cosψ₀/cosψ) − a√(1−a²) − asin(a) ]

  The formula holds until cosψ = a·cosψ₀. Beyond that point, the area is the bore minus the projected plate and shaft blockage — [Heywood 1988](https://openlibrary.org/isbn/007028637X).
- A paper from TU Cluj-Napoca gives this shaft-aware expression with attribution to its reference [6]. It notes that above about 70–75° the shaft becomes the restriction, depending on its thickness. The authors computed without shaft effects — [Acta Technica Napocensis](https://atna-mam.utcluj.ro/index.php/Acta/article/download/1540/1262) (search summary).
- **Simplest approximation:** A_eff = (πD²/4)·sin(θ), with θ = 0 closed. It is described as a simplification, not a derivation — [Firgelli throttle valve](https://www.firgelliauto.com/blogs/mechanisms/throttle-valve).
- **Geometric study of bore opening area.** A Malaysian study evaluated the opening area at 15°, 30°, 45°, 60°, 75° and 90°. It modeled the shaft blockage at 90° as a trapezium — [JETIA](https://jetia.mbot.org.my/index.php/jetia/article/download/45/38/205); [CIAST 2024](https://www.ciast.gov.my/journal/files/Vol10_1/4.%20Comparison%20on%20Area%20of%20Throttle%20Bore%20Opening%202024%20p22-28.pdf).
- **(computed)** Area ratio A_th/A_bore from the Heywood formula, for D = 70 mm, d = 8 mm, ψ₀ = 8°:

  | ψ | 10° | 15° | 20° | 30° | 45° | 60° | 75° |
  |---|---|---|---|---|---|---|---|
  | A_th/A_bore | 0.0055 | 0.025 | 0.051 | 0.125 | 0.286 | 0.494 | 0.734 |

  At full open, shaft blockage alone leaves about 1 − 4d/(πD) ≈ 0.85.
- **Mean-value control models treat the throttle as a restriction.** The restriction sits between control volumes in a chain: air filter, compressor, intercooler, throttle, engine, turbine, exhaust — [Linköping IFAC WC 2011 MVEM](https://www.fs.isy.liu.se/Publications/Articles/IFACWC_11_OL_LE.pdf) (search summary).

### Inferences
- **Throttle model:** ṁ_th = nozzle(C_D(ψ)·A_th(ψ), p_amb or p_boost, p_plenum), plus a leakage area A_leak for a closed plate (typically 1–5 mm² equivalent). Without A_leak, idle cannot be reproduced when the IAC is closed.
- Since the throttle runs choked whenever p_man/p_amb < 0.528, part-throttle airflow is nearly independent of manifold pressure. That property is what makes idle stable. Getting it requires the choked branch of the nozzle equation, not an incompressible ΔP law.
- **IAC / drive-by-wire idle:** add a parallel orifice with A_IAC = duty·A_IAC,max, or treat the electronic-throttle "idle position" as a minimum ψ. Model the actuator as a first-order lag, τ ≈ 20–100 ms. That value is a typical engineering assumption, not sourced.
- engine-sim already has an `m_idleFlowK` parallel path in `Intake::process()` **(local code**, /home/user/engine-sim/src/intake.cpp line ~104), which matches this structure.

### Gaps
- The Heywood equation number and figure (C_D vs throttle angle) were not verified against the text.
- No sourced throttle C_D(ψ) curve was found. Measured values are usually 0.6–0.9 and vary with angle, but that range is not sourced here.

---

## 3. Filling-and-emptying (0D) vs 1D wave-action models; when wave action matters; Helmholtz/Engelman intake tuning; exhaust pulse tuning

### Takeaway
A filling-and-emptying (F&E) model treats manifolds as uniform-pressure volumes joined by nozzles. It captures mean pumping and boost dynamics and can run on an ECU. It cannot show ram/tuning effects, runner-length sensitivity or pulse scavenging, which require either a 1D wave solver or added acoustic elements (inertial runners with Helmholtz resonance). Wave action matters whenever a pipe's acoustic transit time 2L/a is a significant fraction of the valve-event duration, which is true for nearly all runners and exhaust primaries on automotive engines.

### Cited Findings
- **F&E model structure.** A typical turbo mean-value model uses restrictions (air filter, compressor, intercooler, throttle, engine, turbine, exhaust) interconnected with control volumes. It has 14 states: six pairs of control-volume p and T, plus turbo shaft speed and compressor mass flow — [Linköping MVEM, IFAC WC 2011](https://www.fs.isy.liu.se/Publications/Articles/IFACWC_11_OL_LE.pdf).
- **Crank-angle-resolved real-time models are usually 0D/F&E.** One such model was derived from a detailed 1D model with shared parameters and validated against cylinder traces and transients — [Wurzenberger et al., SAE 2009-01-0589](https://saemobilus.sae.org/content/2009-01-0589). Also [SAE 2018-01-1245](https://saemobilus.sae.org/content/2018-01-1245).
- **1D codes exist to capture wave dynamics.** They predict wave motion in the ducts and its influence on cylinder gas exchange. Exhaust-pulse timing is vital to unobstructed expulsion of exhaust — [Lamborghini V12 1D study, SAE 2005-01-0692](https://saemobilus.sae.org/content/2005-01-0692).
- **Helmholtz resonator.** The resonance frequency depends on sound speed, neck area, neck length and cavity volume. Intake noise frequency scales with engine speed, and changing the neck area tunes the frequency (the basis of variable intakes) — [US Patent 7089901](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/7089901).
- **Helmholtz end correction:** L_eff = L + 0.85d for an unflanged end, or 0.61d for a flanged end — [Firgelli Helmholtz calculator](https://www.firgelliauto.com/blogs/engineering-calculators/helmholtz-resonator-calculator).
- **Engelman-type tuned-intake speed.** N_t [rpm] = (955/K)·a·√(A/(l·V_eff)). Symbols: K ≈ 2 for most engines; a is the speed of sound; A the effective runner cross-section; l the effective runner length; V_eff an effective volume derived from displacement and compression ratio. This comes from a forum post and is not attributed to Engelman, so verify it — [Pro-Touring forum](https://www.pro-touring.com/archive/index.php/t-38786.html).
- **Helmholtz theory has been used to set intake length.** An SAE study used Helmholtz resonator theory and acoustic (Kinsler) theory to pick optimum intake length, and simulation agreed with the predictions — [SAE 2015-01-1738](https://saemobilus.sae.org/content/2015-01-1738).
- **Pipe theory references (textbook; not fetched):** Winterbone & Pearson, *Theory of Engine Manifold Design* ([ISBN 1860582095](https://openlibrary.org/isbn/1860582095)) and *Design Techniques for Engine Manifolds* ([ISBN 186058179X](https://openlibrary.org/isbn/186058179X)), both covering wave action in manifolds; Blair, *Design and Simulation of Four-Stroke Engines* ([ISBN 0768004403](https://openlibrary.org/isbn/0768004403)), covering unsteady gas flow and tuning.

### Inferences
- **SI form of the Helmholtz tuned-intake model.** The cylinder plus runner act as a Helmholtz resonator: f_H = (a/2π)·√(A_r/(L_eff·V_eff)). Here V_eff ≈ mean cylinder volume = V_d(r_c+1)/(2(r_c−1)), where V_d is swept volume and r_c the compression ratio. The tuned speed N_t = 60 f_H/K [rpm] with K ≈ 2. The 955 constant in the forum formula is 60/(2π)·100, consistent with mixed units.
  - The V_eff = mean cylinder volume form is the commonly cited Engelman/Heywood choice **(textbook; not fetched)**.
- **Quarter-wave (organ-pipe) view of a runner open to a plenum:** f_n = (2n−1)·a/(4L_eff). Example: L = 0.35 m and a ≈ 345 m/s give f_1 ≈ 246 Hz. One cylinder's intake events at 6000 rpm occur at 50 Hz, so this runner is roughly 5th-order tuned at that speed. Tuning peaks occur where the reflected compression wave arrives near IVC.
- **Exhaust primary tuning rule of thumb:** the rarefaction reflected from the collector/open end should reach the exhaust valve during overlap. The round-trip time 2L/a_exh must equal the crank interval Δθ/(6N), with Δθ in degrees and N in rpm. That gives L ≈ a_exh·Δθ/(12N). Example: a_exh ≈ 550 m/s, Δθ ≈ 140° (EVO to mid-overlap), N = 6500 rpm gives L ≈ 0.99 m, the right order for a race header primary.
- **F&E fails for ram/tuning.** Lumping a 0.3–1 m runner into one volume with one flow element gives a single Helmholtz-like mode at best. The higher-order organ-pipe modes and the pulse interaction between cylinders in a collector are lost. For sound synthesis these modes are audible.
- engine-sim's current approach is partly this. It is a 0D `GasSystem` per runner/primary with a lumped momentum state and velocity decay **(local code**, /home/user/engine-sim/src/gas_system.cpp `flow()` momentum transfer around lines 419–456; `updateVelocity()`). Runner "length" enters only through volume = area × length (/home/user/engine-sim/src/combustion_chamber.cpp lines 84–111). So it captures an inertial/Helmholtz-like mode but not standing-wave tuning.
- **Cheap intermediate model:** split each runner into N = 3–8 volumes joined by inertial (momentum-carrying) flow elements. That is a staggered-grid 1D scheme at coarse resolution (see Section 4), and it recovers the first few acoustic modes cheaply.

### Gaps
- Engelman's original paper (ASME 73-WA/DGP-2, "Design of a tuned intake manifold", 1973) could not be located online. Its exact constant and V_eff definition are unverified.
- No sourced quantitative comparison of F&E vs 1D VE error, as % VE vs rpm, was retrieved.

---

## 4. 1D unsteady compressible Euler equations with area change, friction and heat transfer; numerical schemes; CFL, cell size and real-time cost

### Takeaway
The quasi-1D Euler equations with source terms are the standard model. Each scheme family has a known trade-off:
- **MOC (Benson):** historical; non-conservative.
- **Two-step Lax-Wendroff and MacCormack:** cheap and 2nd order, but oscillate at contact discontinuities and shocks unless FCT or TVD limiting is added.
- **Godunov-type FV (HLL/HLLC/Roe with MUSCL):** robust and conservative, with crisp contact surfaces (the exhaust/fresh-charge interface).
- **GT-POWER's staggered grid:** scalars at subvolume centroids, fluxes at boundaries.

For real time, a 1st/2nd-order FV scheme (HLLC, or a staggered volume–momentum-link scheme) with Δx ≈ 25–50 mm is feasible. A V8 then needs about 300–600 cells and an explicit step of ~15–40 μs. That is about 10–40 million cell updates per second, within one or two CPU cores with careful code.

### Cited Findings
- **Governing equations (textbook; not fetched — Winterbone & Pearson Ch. 2; Benson Vol. 1; Toro Ch. 1, 3).** Symbols: A(x) is pipe area; ρ density; u velocity; p pressure; E = ρ(e + u²/2) total energy per volume with e = p/((γ−1)ρ); D hydraulic diameter; f_F the Fanning friction factor (f_Darcy = 4f_F); h the wall heat-transfer coefficient; T_w the wall temperature.

  ∂U/∂t + ∂F/∂x = S, with

  - U = [ρA, ρuA, EA]ᵀ
  - F = [ρuA, (ρu² + p)A, u(E + p)A]ᵀ
  - S = [0, p·dA/dx − (ρu|u|/2)·f_F·πD, h·πD·(T_w − T)]ᵀ

  Species or burned fraction Y is advected with ∂(ρYA)/∂t + ∂(ρuYA)/∂x = 0 — [Winterbone & Pearson](https://openlibrary.org/isbn/1860582095); [Toro, *Riemann Solvers and Numerical Methods for Fluid Dynamics*](https://doi.org/10.1007/b79761).
- **OpenWAM's evolution.** The code began in 1984 using MOC based on Benson's work. MOC was later replaced by finite-difference schemes. A 1993 comparison (Desantes, Chust, Llorens) found two-step Lax-Wendroff and MacCormack predictor-corrector the most suitable; both are 2nd order. Payri, Galindo, Serrano and Arnau later compared high-resolution TVD and FCT schemes. Users choose a scheme for speed or accuracy. OpenWAM is GPLv3 C++ from CMT-Motores Térmicos (UPV) — [OpenWAM docs](https://openwam.webs.upv.es/docs/); [OpenWAM on SourceForge](https://www.sourceforge.net/projects/openwam).
- **Engine-pipe scheme comparison.** A HAL paper on 1D wave propagation in ICE pipes compares Lax-Wendroff, FCT and HLL (Riemann solver). Another cited work uses 1D finite elements with two-step Lax-Wendroff time integration and source terms for friction, heat transfer and variable area — [HAL hal-01158821](https://hal.archives-ouvertes.fr/hal-01158821) (search summary).
- **Variable-area Riemann solvers.** MOOSE implements HLLC for the 1-D, 1-phase, variable-area Euler equations, with Einfeldt or Davis wave-speed estimates — [MOOSE ADNumericalFlux3EqnHLLC](https://mooseframework.inl.gov/source/userobjects/ADNumericalFlux3EqnHLLC.html).
- **Well-balanced source terms.** Gascón, Corberán and García-Manrique (2021) recast the quasi-1D source term in divergence form so the HLL solver and flux limiters are well-balanced in ducts of varying cross-section — [Appl. Math. Comput. 400 (2021)](https://ideas.repec.org/a/eee/apmaco/v400y2021ics009630032100120x.html).
- **Area jumps.** For discontinuous cross-sections, building the area term into the Riemann solver generally beat discretizing it separately — [NTNU HLLC area paper](https://www.pvv.ntnu.no/~stm/dring/hllcs-hem.pdf) (search summary).
- **GT-SUITE's staggered grid.** Scalars (p, T, ρ, enthalpy, species) sit at the centroid of each subvolume and flux variables at the boundaries. The time step must be shorter than the time for flow to cross a subvolume, and too many subvolumes makes runs very slow — [Politecnico di Torino MSc thesis](https://webthesis.biblio.polito.it/14730/1/tesi.pdf) (search summary).
- **GT discretization guidance.** Roughly 0.4×bore on the intake side and 0.55×bore on the exhaust side, with the difference attributed to sound speed. With the implicit solver the time step is fixed and runtime scales with subvolume count. In one student study, 0.1 mm vs 40 mm gave almost the same torque, BSFC and peak pressure but ran about 12,500× longer. Student pages, not GT official — [Skill-Lync assignment](https://skill-lync.com/student-projects/assignment-11-9); [Skill-Lync GEM3D](https://skill-lync.com/student-projects/week-11-discretization-of-3d-intake-manifold-using-gem-3d-19).
- **Explicit staggered-grid gas-network methods** are bound by a Courant–Levy stability condition — [arXiv 1803.00418](https://arxiv.org/pdf/1803.00418); [arXiv 2404.04451](https://arxiv.org/pdf/2404.04451).
- **CFL in practice.** In one explicit transient-flow study, CFL was limited to 0.5, and raising it toward 1 often made the solver fail — [arXiv 2306.08334](https://arxiv.org/pdf/2306.08334).
- **HLLC solver formulas (textbook; not fetched — Toro Ch. 10; Toro, Spruce & Speares 1994).** Notation: K = L or R.
  - Wave speeds: S_L = min(u_L − a_L, u_R − a_R) and S_R = max(u_L + a_L, u_R + a_R), the Davis estimate.
  - Contact speed: S* = [p_R − p_L + ρ_L u_L(S_L − u_L) − ρ_R u_R(S_R − u_R)] / [ρ_L(S_L − u_L) − ρ_R(S_R − u_R)].
  - Star states: U*_K = ρ_K·(S_K − u_K)/(S_K − S*)·[1, S*, E_K/ρ_K + (S* − u_K)(S* + p_K/(ρ_K(S_K − u_K)))]ᵀ.
  - Star fluxes: F*_K = F_K + S_K(U*_K − U_K).
  - Interface flux: F_L if 0 ≤ S_L; F*_L if S_L ≤ 0 ≤ S*; F*_R if S* ≤ 0 ≤ S_R; F_R if S_R ≤ 0.

  Sources: [Toro 2009](https://doi.org/10.1007/b79761); [Toro, Spruce & Speares, Shock Waves 4 (1994)](https://doi.org/10.1007/BF01414629); HLL: [Harten, Lax & van Leer, SIAM Rev. 1983](https://doi.org/10.1137/1025002); Roe: [Roe, J. Comput. Phys. 1981](https://doi.org/10.1016/0021-9991(81)90128-5).
- **Two-step (Richtmyer) Lax-Wendroff (textbook; not fetched).**
  1. U_{i+½}^{n+½} = ½(U_i + U_{i+1}) − (Δt/2Δx)(F_{i+1} − F_i) + (Δt/4)(S_i + S_{i+1})
  2. U_i^{n+1} = U_i^n − (Δt/Δx)(F_{i+½}^{n+½} − F_{i−½}^{n+½}) + (Δt/2)(S_{i+½}^{n+½} + S_{i−½}^{n+½})

  Spurious oscillations near discontinuities are removed with Boris–Book flux-corrected transport — [Boris & Book, J. Comput. Phys. 11 (1973)](https://doi.org/10.1016/0021-9991(73)90147-2).
- **CFL condition (textbook; not fetched).** Δt ≤ C·min_i Δx_i/(|u_i| + a_i). For Godunov/HLLC and two-step LW, C ≤ 1. Practical values are 0.8–0.9 for 1st-order Godunov and MUSCL-Hancock, and lower (~0.5) for some RK2 variants — [Toro 2009](https://doi.org/10.1007/b79761).

### Inferences
- **Recommended scheme for a real-time simulator.** Use a cell-centred FV scheme on (ρA, ρuA, EA, ρYA), with HLLC fluxes and optional MUSCL-Hancock 2nd order using a minmod or van Leer limiter.
  - HLLC is preferred over LW/MacCormack because it is positivity-preserving with sensible wave speeds and needs no tuned artificial viscosity. It also resolves the hot-exhaust/cold-backflow contact surface without overshoot. Overshoot there drives spurious temperature and composition spikes into the cylinder during overlap, which is audible and destabilizing.
  - Cost: HLLC needs one sqrt per side (sound speed) and a handful of divisions, roughly 100–200 flops per interface. MUSCL adds about 50%.
- **Alternative: staggered "volume + inertial link" scheme (GT-like).** Volumes carry mass, energy and species. Links carry momentum: d(ṁ)/dt = [A(p_i − p_{i+1}) + (ṁu)_i − (ṁu)_{i+1} − friction]/Δx, with upwinded enthalpy transport. It is cheaper still and maps directly onto engine-sim's existing `GasSystem` volumes. Its weaknesses are more numerical dispersion and less accurate shock capture, which is acceptable at engine pressure ratios when Δx is modest.
- **Well-balancing:** compute the pressure-area source as p_i(A_{i+½} − A_{i−½})/Δx, using the same face areas as the fluxes, so a quiescent pipe with tapers stays at rest. For sudden area steps, place the step at a face and treat it as a junction (Section 5) rather than a source term.
- **Cost estimate for a V8 (computed; all inputs assumed):**
  - Pipe lengths: intake runners 8 × 0.35 m; exhaust primaries 8 × 0.8 m; secondaries 2 × 0.4 m; tailpipes/muffler pipes 2 × 2.0 m. Total ≈ 14 m.
  - Cell counts: Δx = 20 mm gives ≈ 700 cells; Δx = 40 mm gives ≈ 350.
  - Exhaust wave speed: a ≈ √(1.35·287·900 K) ≈ 590 m/s; with |u| up to ~300–400 m/s near the port during blowdown, |u|+a ≈ 1000 m/s. Δt ≈ 0.8·Δx/1000 gives 16 μs (Δx = 20 mm) or 32 μs (40 mm), i.e. 62.5k or 31k steps per second.
  - Intake side, with |u|+a ≈ 450 m/s, could use about 2.2× larger steps. Per-pipe (multirate) time stepping recovers this; OpenWAM's "independent time discretization" work by Galindo/Serrano/Arnau/Piqueras targets exactly this **(textbook/paper recollection; not fetched)**.
  - Global stepping: 700 × 62.5k ≈ 44 M cell-updates/s at 20 mm, or 350 × 31k ≈ 11 M/s at 40 mm.
  - At roughly 20–60 ns per cell update for scalar 1st-order HLLC on a modern core, 40 mm is comfortably real time on one core and 20 mm needs SIMD and/or 2–4 threads.
  - The per-update cost is an estimate, not a benchmark. Benchmark early.
- **Coupling with engine-sim's 10 kHz step.** engine-sim's default simulation frequency is 10 kHz **(local code**, /home/user/engine-sim/src/engine.cpp line 40: `m_initialSimulationFrequency = 10000.0`). A 1D pipe at Δx ≤ 50 mm therefore needs about 3–6 sub-steps per engine step on the exhaust side, or the pipe solver needs its own clock. One CFL sub-step of 16–32 μs is close to 44.1/48 kHz audio, which is convenient: pressure at a tailpipe-end cell can feed the audio synthesizer directly.
- **Typical cell sizes:**
  - 10 mm is design-grade for tuned runners.
  - 20–40 mm is the usual GT-style range: 0.4–0.55 × bore, i.e. 35–55 mm for a 90 mm bore.
  - 50–100 mm or more is enough for long tailpipes and mufflers, where frequencies of interest are below about 500 Hz. The rule of thumb is ≥ 6–10 cells per wavelength: λ = a/f = 590/500 ≈ 1.2 m, so Δx ≈ 0.12 m suffices at 500 Hz.
  - Higher-frequency sound content from 1D cells is unreliable above a/(6–10Δx).

### Gaps
- No public, sourced per-cell timing benchmark for engine 1D codes was found.
- GT-SUITE's actual explicit integrator (predictor-corrector vs RK) and its default Courant factor could not be confirmed. The manual is not public.
- The Payri/Galindo/Serrano/Arnau scheme-comparison paper (Int. J. Mech. Sci., c. 2004) and the OpenWAM independent-time-discretization paper were not retrieved, so their accuracy and cost numbers are not cited.

---

## 5. Boundary conditions: open ends, valve/port to cylinder, closed ends, multi-pipe junctions

### Takeaway
Classic codes impose pipe-end conditions by Benson's characteristic method. The incoming Riemann variable comes from the interior, and the outgoing one comes from solving the boundary physics: a nozzle equation for a valve, isentropic inflow or p = p_amb for an open end, u = 0 for a closed end. Multi-pipe merges use either Benson's constant-pressure junction (no loss data needed) or a pressure-loss junction, or a small 0D junction volume. For a real-time FV code, the most robust and simplest approach treats every end as a flux into or out of a 0D reservoir (cylinder, plenum, junction, atmosphere). That flux is computed with the nozzle equation, or with a partial Riemann problem against a ghost state.

### Cited Findings
- **Constant-pressure junction.** It rests on Benson's hypothesis of equal static pressure in every duct at the junction. It needs no loss data and ignores branch angles. Entropy levels at duct ends are assigned according to whether flow goes toward or away from the junction — [Baig, Politecnico di Milano thesis 2022](https://www.politesi.polimi.it/retrieve/8811c217-7723-4b04-bbfd-2bd5babf55cc/2022_07_Baig_MirzaFaizanAli_02.pdf) (search summary).
- **Valve boundaries.** Cylinder inlet and outlet valve boundaries are solved by the method of characteristics, modified for non-isentropic valve flow — [SAE 2010-01-1052](https://saemobilus.sae.org/content/2010-01-1052/); [SAE 950276](https://saemobilus.sae.org/content/950276/).
- **Primary reference:** Benson, *The Thermodynamics and Gas Dynamics of Internal-Combustion Engines*, Clarendon Press, 1982 — [ISBN 0198562101](https://openlibrary.org/isbn/0198562101); cited as such in [SAE 2003-01-0367](https://saemobilus.sae.org/content/2003-01-0367).
- **Experimental check.** Queen's University Belfast compared MOC and other methods on a single-pulse cylinder-to-duct rig with engine-like pressure waves — [SAE 941685](https://saemobilus.sae.org/content/941685) (search summary; attribution approximate).
- **OpenWAM pipe-end conditions.** Its boundary conditions, from Payri and co-authors, refine the characteristic-line handling at pipe ends — [OpenWAM docs](https://openwam.webs.upv.es/docs/).
- **Riemann-variable relations (textbook; not fetched — Benson; Winterbone & Pearson).**
  - Invariants: λ = a + ((γ−1)/2)u (rightward) and β = a − ((γ−1)/2)u (leftward), with a = √(γRT).
  - **Closed end:** u = 0, so λ_out = λ_in, a perfect reflection of the same sign.
  - **Open end, subsonic outflow:** p = p_amb, plus the extrapolated λ_in and entropy.
  - **Open end, inflow:** isentropic from stagnation (p_amb, T_amb), i.e. a_0² = a² + ((γ−1)/2)u², optionally with an entry loss.
  - **Choked outflow:** u = a.
  - **Valve:** the quasi-steady nozzle equation between cylinder (reservoir) state and pipe-end state, solved iteratively together with λ_in.

### Inferences
- **FV implementation recipe (recommended):**
  1. **Pipe ↔ 0D volume (cylinder, plenum, collector, atmosphere) through a restriction.** Compute ṁ with the Section 1 nozzle equation, using A_eff(t) from valve lift. Upstream stagnation comes from the reservoir state, or for a pipe-side upstream, from the end cell's stagnation p_0 = p(1 + (γ−1)/2·M²)^{γ/(γ−1)}. Convert to boundary fluxes F = [ṁ, ṁu_end + p_end·A, ṁh_0] and apply them to the end cell and the volume. This is the GT-style "valve between volume and first subvolume" coupling. It handles reverse flow by sign, and with a sub-cycled cylinder it is robust at large pressure ratios.
  2. **Pipe ↔ pipe without a restriction, e.g. an area step.** Place the step at a face and use a loss-free or loss-coefficient junction. The simplest robust option is a tiny 0D junction volume connecting both sides by HLLC or nozzle fluxes.
  3. **Multi-pipe merge (collector, Y-pipe).** Make the junction a 0D volume of physical size (e.g. collector volume), with each branch connected by flux. This is the constant-pressure junction model plus finite storage. It conserves mass and energy exactly and generalizes to N branches. Add directional loss coefficients K·½ρu² only if needed.
  4. **Atmosphere:** an infinite reservoir at (p_amb, T_amb) with an open-end flux. Use the nozzle equation with C_D ≈ 1 for outflow, adding an end-correction length of about 0.6D to the pipe. For audio, record p and u at the open end; radiated monopole pressure ∝ dṁ/dt.
  5. **Closed end:** a reflective ghost cell with mirrored velocity, or simply zero mass flux plus pressure force p·A.
- The cylinder must be sub-cycled or solved implicitly if its volume is small relative to the per-step flux. Near TDC during overlap, the explicit nozzle flux can overshoot the equalization pressure. Limit it as engine-sim's `pressureEquilibriumMaxFlow` does.

### Gaps
- Benson's full valve-boundary iteration procedure and Winterbone–Pearson's pressure-loss junction coefficients were not retrieved.
- No sourced comparison of constant-pressure vs pressure-loss junction accuracy was found.

---

## 6. Pipe friction factor and heat transfer correlations in 1D codes

### Takeaway
1D codes use steady-flow correlations applied quasi-steadily per cell. Friction uses Blasius or Colebrook/Haaland with a roughness term. Wall heat transfer uses the Reynolds–Colburn analogy (h from f) or Dittus–Boelter, with engine-specific multipliers for ports and bends. Exhaust wall temperature is a key calibration knob because it controls exhaust gas temperature and sound speed.

### Cited Findings
- **Chilton–Colburn analogy:** j_H = St·Pr^{2/3} = f_F/2, the most accurate of the classical friction/heat-transfer analogies. It is valid for fully developed turbulent flow with Re > 10⁴, 0.7 < Pr < 160 and L/d > 60 — [Wikipedia, Chilton and Colburn J-factor analogy](https://en.wikipedia.org/wiki/Chilton_and_Colburn_J-factor_analogy); [Reynolds analogy](https://en.wikipedia.org/wiki/Reynolds_analogy).
- **Accuracy.** A CFD validation found the Colburn correlation within ±10% for straight-pipe heat transfer at Pr = 0.7 and 10⁴ < Re < 1.2×10⁵ — [KNS paper (search summary)](https://www.kns.org/files/pre_paper/49/23S-255-%EB%B0%95%EB%8F%99%ED%98%81.pdf).
- **Rough pipes.** For rough pipes the Colburn modification fails at high Re: j_H keeps falling while f has gone constant. Kármán's analogy fits better — [UBC thesis](https://open.library.ubc.ca/collections/831/items/1.0059122) (search summary).
- **Engine heat-transfer coefficients** are often empirical correlations fitted to large datasets — [Asmarya J. (search summary)](https://journals.asmarya.edu.ly/jbs/index.php/jbs/article/download/156/116/185).
- **Friction correlations (textbook; not fetched).**
  - Blasius: f_F = 0.0791·Re^{−0.25} for 4×10³ < Re < 10⁵, with Re = ρ|u|D/μ.
  - Haaland's explicit Colebrook approximation: 1/√f_D = −1.8·log₁₀[(ε/D/3.7)^{1.11} + 6.9/Re], with f_D = 4f_F and ε the wall roughness (≈ 0.0015 mm for drawn tube, ~0.05 mm for commercial steel; 0.1–0.3 mm for cast iron or cast ports).
  - Laminar: f_F = 16/Re.

  Source: [Haaland, J. Fluids Eng. 105 (1983)](https://doi.org/10.1115/1.3240948).
- **Dittus–Boelter (textbook; not fetched):** Nu = 0.023·Re^{0.8}·Pr^{n}, with n = 0.4 for heating and 0.3 for cooling of the gas — [Heywood Ch. 12](https://openlibrary.org/isbn/007028637X).

### Inferences
- **Recommended per-cell source terms (cheap):**
  - Re = ρ|u|D/μ(T), with μ from Sutherland.
  - f_F = max(16/Re, Haaland/4).
  - h = (f_F/2)·ρ|u|c_p·Pr^{−2/3} by the Colburn analogy, times a multiplier: 1.0 for straight pipe, 1.5–3 for exhaust ports and bends, the usual calibration range in engine practice (unsourced).
  - Wall temperature: a lumped thermal node per pipe, (m c)_w dT_w/dt = Σ h A_s (T − T_w) − h_ext A_ext (T_w − T_amb). This gives realistic header glow-up and exhaust-temperature lag over tens of seconds, and costs nearly nothing.
- Pulsating flow raises instantaneous heat transfer above steady correlations near the exhaust port. Many codes apply a multiplier rather than a separate model.

### Gaps
- The exact friction and heat-transfer models in WAVE, GT-POWER and OpenWAM were not retrieved. Search found nothing beyond generic analogies.
- No sourced exhaust-port-specific correlation (e.g. Caton & Heywood) was retrieved in this session.

---

## 7. Turbochargers and superchargers: maps, corrected quantities, extrapolation, turbine as nozzle/ellipse law, shaft dynamics, wastegate, surge, intercooler

### Takeaway
Turbo components are 0D map-based elements placed between 1D pipes or 0D volumes.
- Compressor: pressure ratio and efficiency vs corrected flow and corrected speed.
- Turbine: reduced flow and efficiency vs pressure ratio and speed, or vs blade-speed ratio.
- Shaft: a single inertia ODE.
- Surge: a Moore–Greitzer/Greitzer mass-flow state.

Map extrapolation is the main accuracy risk, so prefer physics-based fits such as the turbine nozzle/ellipse law, the BSR-parabola for efficiency, and the compressor "Ellipse" model over table lookup.

### Cited Findings
- **Maps use corrected variables.** Mean-value models are usually built from manufacturer maps that give compressor pressure ratio and isentropic efficiency against corrected speed and corrected air mass flow. A Linköping licentiate thesis proposes also including inlet pressure in the speed correction — [Linköping LIC 1463](https://www.fs.isy.liu.se/Publications/Lic/10_LIC_1463_OL.pdf) (search summary).
- **Surge modeling.** Surge is captured with the Moore–Greitzer model (Greitzer 1981), which adds a compressor mass-flow state. A parameterized "Ellipse" compressor model (Bergström & Leufvén 2007) handles both forward and backward (surging) flow. Surge arises when a closing throttle cuts flow quickly while the slow turbo shaft keeps pressure ratio high, which can reverse flow — [Linköping IFAC WC 2011](https://www.fs.isy.liu.se/Publications/Articles/IFACWC_11_OL_LE.pdf); [IFAC WC 2008](https://www.fs.isy.liu.se/Publications/Articles/IFACWC_08_OL_LE.pdf).
- **Ellipse compressor flow model** extrapolates compressor behaviour outside the normal map range "in a physically sane way" — [Leufvén & Eriksson, Linköping](https://www.fs.isy.liu.se/Publications/Books/17_LEXLKEVL.html).
- **Moraal & Kolmanovsky (SAE 1999-01-0908).** Reviews curve-fitting for compressor and turbine maps and compares four compressor-flow fits, including neural networks, on three compressors. Covers wastegated and VGT turbines. Notes that table interpolation is not continuously differentiable, extrapolates unreliably and is not compact — [SAE 1999-01-0908](https://saemobilus.sae.org/content/1999-01-0908).
- **Physics-based extrapolation.** Purely mathematical fits extrapolate inaccurately; turbomachinery-physics extrapolation applies to both compressor and turbine — [SAE 2009-01-0310](https://saemobilus.sae.org/content/2009-01-0310). Also [HAL hal-00767727, physics-based map extrapolation validated on an SI engine](https://hal.archives-ouvertes.fr/hal-00767727).
- **Extrapolation matters for transients.** Turbine maps from hot-gas benches cover a narrow region, while engine transients push far outside it. Extrapolation rules are often experience-based, and transient prediction is sensitive to them — [SAE 2017-24-0019 (Imperial College)](https://saemobilus.sae.org/content/2017-24-0019).
- **Compressor map interpolation effect.** Using non-dimensional coefficients changed predicted compressor efficiency by up to 0.89 percentage points in a simple engine model — [SAE 2017 compressor map interpolation](https://saemobilus.sae.org/content/2009-01-0308) (search summary; attribution approximate).
- **Polynomial regression fails outside data** for VNT compressors; dimensional analysis can extend the map to low speeds — [Forsch. Ingenieurwes. 2014](https://link.springer.com/article/10.1007/s10010-014-0171-0).
- **Real-time HIL turbo engine.** A GT-POWER crank-angle model, co-simulated with dSPACE HiL, reproduced closed-loop boost and AFR control close to the dyno engine — [SAE 2019-01-2301](https://saemobilus.sae.org/content/2019-01-2301/).
- **Standard equations (textbook; not fetched — Guzzella & Onder, Eriksson & Nielsen, Watson & Janota, Heywood Ch. 6).** Symbols: subscripts c (compressor), t (turbine), 01 (inlet stagnation); Π the pressure ratio; η the isentropic efficiency; ω the shaft speed [rad/s]; J the rotor inertia [kg·m²]; U = ω·r_tip the blade tip speed.
  - Corrected flow and speed: ṁ_corr = ṁ·√(T_01/T_ref)/(p_01/p_ref); N_corr = N/√(T_01/T_ref).
  - Compressor outlet temperature: T_02 = T_01·[1 + (Π_c^{(γ−1)/γ} − 1)/η_c].
  - Compressor power: P_c = ṁ_c c_p T_01 (Π_c^{(γ−1)/γ} − 1)/η_c.
  - Turbine power: P_t = ṁ_t c_p,e T_03 η_t [1 − Π_t^{−(γ_e−1)/γ_e}], with Π_t = p_03/p_4.
  - Shaft dynamics: J·dω/dt = (η_m·P_t − P_c)/ω.
  - Turbine efficiency vs blade-speed ratio: BSR = U/c_s, with c_s = √(2 c_p,e T_03 [1 − Π_t^{−(γ_e−1)/γ_e}]); η_t ≈ η_t,max·[2(BSR/BSR_opt) − (BSR/BSR_opt)²], with BSR_opt ≈ 0.6–0.7 for radial turbines.
  - Turbine flow as a nozzle: ṁ_t = A_t,eff·p_03/√(R T_03)·Ψ(1/Π_t), with Ψ the nozzle flow function from Section 1. Alternatively the Stodola ellipse (cone) law: ṁ_t√T_03/p_03 = K_t·√(1 − Π_t^{−2}).
  - Wastegate: a nozzle of area A_wg(actuator) in parallel with the turbine.
  - Intercooler: T_out = T_in − ε(T_in − T_coolant), with effectiveness ε ≈ 0.6–0.9, and Δp = k·ṁ²/ρ.
  - Greitzer/Moore–Greitzer surge: dṁ_c/dt = (A_c/L_c)·(Π_c,map(ω, ṁ_c)·p_01 − p_02).

  References: [Guzzella & Onder, *Introduction to Modeling and Control of IC Engine Systems*](https://doi.org/10.1007/978-3-642-10775-7); [Eriksson & Nielsen, *Modeling and Control of Engines and Drivelines*](https://openlibrary.org/isbn/9781118479995); [Watson & Janota, *Turbocharging the Internal Combustion Engine*](https://openlibrary.org/isbn/0333242904); [Greitzer 1976, J. Eng. Power](https://doi.org/10.1115/1.3446138).
- **Superchargers (textbook; not fetched — Heywood Ch. 6, Watson & Janota).**
  - Roots (no internal compression): ṁ = ρ_1·V_disp·N_s·η_vol(Π, N_s), where η_vol falls with Π because of leakage. Indicated power for isochoric "backflow" compression: P = V̇_1·(p_2 − p_1). This is why Roots blowers are inefficient at high Π.
  - Twin-screw: has a built-in volume ratio. Use the isentropic relation to the internal pressure plus an over- or under-compression loss.
  - Centrifugal supercharger: the turbo-compressor map with N_comp = gear ratio × N_engine, and no turbine.

### Inferences
- **Real-time implementation:**
  - Represent compressor and turbine maps as smooth parametric fits: the Ellipse model for the compressor, nozzle/ellipse law plus BSR parabola for the turbine. Precompute them to 2D tables on a regular grid with bilinear lookup. Clamp, never extrapolate, from raw tables.
  - The shaft ODE is slow (time constants of 0.1–1 s) and can use the engine time step.
  - Turbine inflow is strongly pulsating: a twin-scroll or pulse turbine sees each exhaust pulse. Evaluate the quasi-steady turbine map at every step, using instantaneous inlet p and T from the collector volume or pipe-end cell. Do not cycle-average.
  - Make the blow-off/bypass valve a nozzle from compressor outlet to inlet, opened by manifold vacuum. The surge state then keeps compressor flow bounded during lift-throttle.
- **Audible surge/flutter** ("compressor stall chatter") comes naturally from the Greitzer state if the compressor map's positive-slope region at low flow is represented (Ellipse-type models). It is a distinctive sound worth supporting.

### Gaps
- The papers on the turbine ellipse law/Stodola formulation for automotive radial turbines and on Jensen–Kristensen-type extrapolation were not retrieved.
- Typical rotor inertias (J ≈ 1×10⁻⁵ – 1×10⁻⁴ kg·m² for automotive turbos) are an engineering recollection, unsourced.
- No sourced intercooler ε or Δp values were retrieved.

---

## 8. Volumetric efficiency drivers and typical VE curves

### Takeaway
VE emerges from the gas-exchange model; it is not an input. Its drivers:
- Quasi-steady losses: throttle/valve/port flow resistance and choking at high speed.
- Charge heating.
- Fuel vapour displacement.
- Residual gas.
- Backflow at IVO/IVC, set by valve timing.
- Unsteady effects: inertial ram and acoustic tuning of intake runners, exhaust scavenging during overlap.

A real-time simulator reproduces realistic VE-vs-rpm shapes (peaks from tuning, roll-off from choking and timing) only if it carries runner wave/inertia dynamics.

### Cited Findings
- VE vs speed in Heywood Ch. 6 is built up from these effects, starting at 100% and modified by fuel vapour/heat transfer, friction, ram and tuning, choking, backflow and charge heating **(textbook; not fetched)** — [Heywood Ch. 6](https://openlibrary.org/isbn/007028637X).
- Intake runner length optimized by Helmholtz/acoustic theory raised VE in simulation — [SAE 2015-01-1738](https://saemobilus.sae.org/content/2015-01-1738).
- Exhaust-pulse timing is vital to unobstructed exhaust expulsion — [SAE 2005-01-0692](https://saemobilus.sae.org/content/2005-01-0692).

### Inferences
- **Typical VE shapes (textbook recollection, unsourced):**
  - Production NA 2-valve engines: ~75–90% peak.
  - 4-valve tuned NA engines: ~90–105%, with peak VE near peak-torque rpm.
  - Race engines with tuned runners and headers: 105–120%+ at the tuned speed.
  - VE falls off at low rpm when backflow from late IVC dominates, and at high rpm from flow choking (inlet Mach index Z ≳ 0.6 per Heywood/Taylor) and insufficient time.
- **Validation target for the simulator:** run WOT sweeps and check that intake-runner length changes shift the VE/torque peak in the expected direction. Longer runners should give a lower-rpm peak, roughly ∝ 1/L in the quarter-wave view or ∝ 1/√L in the Helmholtz view.

### Gaps
- No sourced VE curve data set was retrieved in this session.
- The Mach-index Z threshold value is from memory (Heywood/Taylor) and was not verified.

---

## 9. Open-source implementations, commercial-code approaches and real-time 1D engine models

### Takeaway
- **OpenWAM (CMT-UPV, GPLv3 C++)** is the main open-source 1D engine gas-dynamics code. It started with MOC and moved to LW/MacCormack, then TVD/FCT.
- **Commercial codes:** GT-POWER (staggered FV, explicit), Ricardo WAVE and AVL BOOST are design-grade, not real-time at full resolution.
- **Real-time HIL products** use either lumped "fast running models" (GT-SUITE FRM: coarse volumes) or crank-angle-resolved 0D models. One source states explicitly that transient 1D pipe flow is the most time-consuming part and is dropped in the fast version.
- **engine-sim** uses 0D gas volumes with a lumped momentum state, isentropic nozzle flows and flow-bench-derived k values.

### Cited Findings
- **OpenWAM:** a free, open-source 1D gas-dynamics code from CMT-Motores Térmicos, mainly for ICE. It began in 1984 with MOC based on Benson, and offers LW/MacCormack and later TVD/FCT schemes. GPLv3, C++, Linux and Windows. The latest packages found are v2.1, December 2014 — [OpenWAM docs](https://openwam.webs.upv.es/docs/); [SourceForge](https://www.sourceforge.net/projects/openwam); [OSDN mirror](https://osdn.net/projects/sfnet_openwam/releases).
  - Caution: the GitHub name "OpenWAM/OpenWAM" is an unrelated 2026 Stanford robotics "World-Action Model" project. **(checked in session** via raw.githubusercontent.com/openwam/OpenWAM README.)
- **GT-SUITE fast running model.** A 1D real-time-capable heavy-duty diesel model was built as a GT-SUITE Fast Running Model from top-level configuration, test-cell data and maps, and used in Volvo Penta's VIRTEC virtual test cell — [Andric et al., SAE 2018-01-0874](https://saemobilus.sae.org/content/2018-01-0874); [Chalmers record](https://research.chalmers.se/publication/502822).
- **AVL crank-angle-resolved real-time model,** derived from a detailed 1D model with shared parameters — [SAE 2009-01-0589](https://saemobilus.sae.org/content/2009-01-0589).
- **Concept-to-HiL workflow** — [SAE 2018-01-1245](https://saemobilus.sae.org/content/2018-01-1245).
- **1D/0D model on an ECU.** A crank-angle-resolved 1D and 0D engine simulation ran in real time on a serial ECU at 240 MHz. Transient 1D pipe flow was the most time-consuming part, so the fast version neglects pipes — [SAE 2013-01-0344](https://saemobilus.sae.org/content/2013-01-0344) / [CVUT MECCA paper](https://ojs.cvut.cz/ojs/index.php/MECCA/article/view/6759) (search summary; exactly which of these two the statement belongs to is uncertain).
- **Multi-core real-time model.** Claytex's Modelica work split a crank-angle-resolved engine model (CAREM) across a multi-core processor; timings were not retrieved — [Modelica 2018 US presentation](https://2018.american.conference.modelica.org/FullPaper/Modelica2018US_Presentations_14.pdf).
- **FPGA.** A real-time 1D CFD engine model on FPGA (or ASIC) for HiL is patented — [US 7991488](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/7991488). A 0D crank-angle NOx model on FPGA ran an iteration in about 3 μs — [TRID 1432841](https://trid.trb.org/View/1432841).
- **engine-sim (AngeTheGreat):**
  - **(local code)** `GasSystem` volumes carry n (mol), kinetic energy, mixture fractions and a 2-D momentum vector. Flow between systems uses the isentropic nozzle equation with `k_flow`. Momentum and bulk kinetic energy are transferred with the flowing gas, and a velocity-decay term damps it ([gas_system.cpp](https://github.com/ange-yaghi/engine-sim/blob/master/src/gas_system.cpp); local /home/user/engine-sim/src/gas_system.cpp lines 291–456).
  - **(local code)** Intake: atmosphere → throttle k (flow attenuation) plus idle k → plenum. Exhaust: collector → atmosphere through `m_outletFlowRate` (/home/user/engine-sim/src/intake.cpp ~lines 90–108; /home/user/engine-sim/src/exhaust_system.cpp lines 62–87).
  - **(local code)** Cylinder flows chain manifold → runner → valve → cylinder → valve → primary → collector (/home/user/engine-sim/src/combustion_chamber.cpp lines 280–330).
  - Community docs describe heads defined by chamber volume, runner volumes and cross-sections, and a flow curve — [engine_sim_tutorial](https://github.com/SnowyHype/engine_sim_tutorial_angethegreat).
  - Ange's later "Engine Simulator 3D" was reported to target exhaust-shape and intake-length effects on sound — [The Drive](https://www.thedrive.com/news/new-3d-engine-sim-can-synthesize-super-realistic-exhaust-sounds-in-real-time).
- **Textbook simulation methods (textbook; not fetched):** Blair's book describes his GPB (Gordon Blair) finite-system unsteady-gas-flow method used in his 4T simulations — [Blair 1999](https://openlibrary.org/isbn/0768004403).

### Inferences
- engine-sim already sits at the "0D + lumped inertia" level. The largest realism gain for both sound and VE comes from replacing each runner/primary `GasSystem` with a short 1D pipe of 4–30 cells. Plenum, cylinders, collector and atmosphere stay as 0D volumes. This is exactly the commercial architecture (pipes + volumes + restrictions) and keeps the code's flow-bench-based valve model.
- OpenWAM is the best open reference to read for boundary-condition and junction code. It is GPLv3, so copying code into a differently licensed project has licensing implications; reimplement from the papers instead.

### Gaps
- No public source quantified GT-POWER or WAVE run-time ratios (e.g. "×N slower than real time") for a typical V8 detailed model.
- The OpenWAM source tree was not inspected: GitHub codeload access was blocked in this session and the UPV docs host did not resolve.

---

## 10. Recommended choices for a real-time gas-exchange simulator (synthesis)

### Takeaway
Use a hybrid network. 0D volumes hold cylinders, plenum, collector, mufflers and turbo housings. Restrictions use the compressible nozzle equation with forward and reverse C_D·A_ref tables, plus a smooth low-Δp branch. Selected pipes (intake runners, exhaust primaries/secondaries, tailpipe) use a 1D FV HLLC solver with Δx ≈ 25–50 mm, CFL ≈ 0.8, per-pipe sub-cycling, Haaland/Colburn sources and a lumped wall-temperature node. Turbos use map-fit components with a Greitzer surge state. This fits in about 1–2 CPU cores for a V8 at audio-rate time steps.

### Cited Findings
- 1D pipe flow is the dominant cost in real-time crank-angle engine models — [SAE 2013-01-0344](https://saemobilus.sae.org/content/2013-01-0344) (search summary; attribution uncertain, see Section 9).
- Commercial practice is volumes plus restrictions plus pipes on a staggered grid, with subvolume length about 0.4–0.55 × bore — [PoliTo thesis](https://webthesis.biblio.polito.it/14730/1/tesi.pdf); [Skill-Lync](https://skill-lync.com/student-projects/assignment-11-9).
- Map-based turbo models need physics-based extrapolation for transients — [SAE 2017-24-0019](https://saemobilus.sae.org/content/2017-24-0019); [SAE 2009-01-0310](https://saemobilus.sae.org/content/2009-01-0310).
- engine-sim already has the nozzle equation, flow-bench conversion and 0D volumes with momentum **(local code**, /home/user/engine-sim/src/gas_system.cpp).

### Inferences
**Proposed implementation order, each step independently testable:**
1. **Restriction upgrade.** Add forward/reverse C_D(L/D) tables, a Heywood throttle-area function, an IAC parallel orifice, a leakage area, and a low-Δp linearization. Validate by reproducing 28" bench CFM at fixed lift on a virtual bench (steady reservoir to reservoir).
2. **1D pipe element.** An FV HLLC solver on (ρA, ρuA, EA, ρYA), 1st order first, then MUSCL-Hancock. Well-balanced area source. Haaland friction, Colburn heat transfer, lumped wall node.
   - Unit tests: Sod shock tube (Toro Ch. 4 test 1) against the exact Riemann solution; quiescent tapered pipe (no spurious flow); closed-end reflection; open-end quarter-wave frequency f = a/(4(L + 0.6D)) for a pipe closed at one end and open at the other.
3. **Pipe–volume coupling** through nozzle-flux boundaries (Section 5). Collector or merge as a small 0D volume.
4. **Time integration.** Global CFL step Δt_pipe ≤ 0.8·Δx/(|u|+a) with sub-cycling inside engine-sim's 10 kHz engine step. Option: per-pipe local Δt in the spirit of OpenWAM's independent time discretization.
   - Per-step cell budget for a V8 at 40 mm: about 350 cells × ~31k steps/s ≈ 11 M updates/s.
   - At 20 mm: about 44 M updates/s. Use SIMD (structure-of-arrays) and one thread per bank.
5. **Turbo/supercharger module.** Compressor Ellipse fit or a map with clamped lookup; turbine as a nozzle plus BSR-parabola efficiency; shaft ODE; wastegate and BOV nozzles; Greitzer surge state; ε-NTU intercooler with quadratic Δp.
6. **Audio tap.** Tailpipe open-end ṁ(t) and p(t) at the pipe-solver rate, which is about the audio sample rate, so the synthesizer gets physically propagated pulses with correct delays and resonances instead of synthetic delay lines.

**Validation targets (from Sections 3 and 8):**
- Moving runner length shifts the VE peak (longer runner → lower rpm).
- Primary length shifts the scavenging benefit.
- Throttle idle airflow is choke-limited and roughly independent of MAP below 0.53 pressure ratio.
- Wastegated boost settles with a time constant set by shaft inertia.

### Gaps
- The per-cell cost figures (20–60 ns per update) and wall-node and IAC time constants are engineering estimates, not sourced benchmarks. Prototype and profile before committing to a cell size.
- Whether 1st-order HLLC at 40 mm preserves enough high-frequency content for convincing sound is untested. Compare against 2nd-order MUSCL at 20 mm on a single-cylinder test case.
