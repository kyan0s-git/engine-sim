# Mechanical Dynamics, Balance, Valvetrain and Drivetrain Models for a Real-Time Engine Simulator

Research notes for the "Realistic engine physics models" effort. Scope covers slider-crank dynamics, balance, torsional vibration, flywheel inertia, valvetrain, starter, drivetrain/vehicle, real-time multibody numerics, and an assessment of how engine-sim models mechanics today.

**Method and source caveats (read first)**
- Web search worked, but direct page fetches (WebFetch/curl) were blocked by the sandbox proxy (DNS failure / HTTP 403). Cited web findings therefore come from search-result summaries of the linked pages, not full-text reads. Where a formula is only "standard textbook" and I could not retrieve a page for it, I say so and put the claim under Inferences or Gaps.
- Several results were **derived and checked numerically in this session** with a Python script (numpy). These are marked "[verified numerically in-session]". The script: (a) checks the exact slider-crank derivative against finite differences, (b) takes FFTs of piston acceleration and inertia torque, and (c) sums the 1st- and 2nd-order reciprocating forces and moments for each layout, splitting each into the part that rotates with the crank and the part that rotates against it. The method is reproducible from the equations given below.
- Statements about engine-sim come from reading the local source at `/home/user/engine-sim` (this fork; upstream is https://github.com/ange-yaghi/engine-sim). File and line references are given, and no repository files were modified.

---

## 1. Slider-crank kinematics and dynamics (exact position/velocity/acceleration, rod angle, gas + inertia forces, crank torque, desaxe, side thrust, two-mass rod)

### Takeaway
Use the exact closed-form kinematics: they are cheap (one sqrt per cylinder) and the 2-harmonic approximation has about 1% acceleration error at λ = r/l ≈ 0.3. Compute crank torque by virtual work, T = F_piston · ds/dθ. This one expression covers gas and inertia forces, offset pins and side thrust, and it is also the basis of a reduced 1-DOF crank model.

### Cited Findings
- Exact piston position, velocity and acceleration are derived from the crank–rod triangle, as functions of crank angle and of time. Piston motion is not simple harmonic, and the velocity extrema do not fall at 90° crank angle; they depend on rod length and half-stroke. — [Wikipedia: Piston motion equations](https://en.wikipedia.org/wiki/Piston_motion_equations)
- The standard approximate forms, with n = l/r and θ measured from TDC: v = ωr(sin θ + sin 2θ/(2n)) and a = ω²r(cos θ + cos 2θ/n). — [FOSSEE Theory of Machines ch.15 worked examples](https://tbc-python.fossee.in/convert-notebook/Theory_Of_Machines/ch15.ipynb); [marinediesels.info piston velocity](https://marinediesels.info/Theory/piston_velocity.htm)
- Most IC engines use rod/crank ratios l/r of about 3:1 to 4:1, i.e. λ = r/l ≈ 0.25–0.33. — [FIRGELLI crank-slider calculator](https://www.firgelliauto.com/blogs/engineering-calculators/crank-slider-mechanism-calculator) (vendor calculator page; moderate reliability)
- The connecting rod is commonly replaced by an "equivalent dynamical system" of two point masses, at the big end and the small end, plus a correction couple. This is treated in Dynamics of Machines course notes. — [ME8594 Dynamics of Machines notes (St. Anne's)](https://www.stannescet.ac.in/cms/staff/qbank/MECH/Notes/ME8594-DYNAMICS%20OF%20MACHINES-8315393-ME8594%20DOM%20NOTES.pdf)
- Exact equations, with crank radius r, rod length l, λ = r/l, crank angle θ from TDC, crank speed ω, and pin offset e = 0 [the derivative was verified numerically in-session; max abs error vs finite difference was 1.1e-11 m/rad]:
  - Pin height above the crank axis: x(θ) = r cos θ + √(l² − r² sin² θ). Distance from TDC: s = l + r − x.
  - Rod angle: sin β = λ sin θ.
  - ds/dθ = r sin θ · [1 + r cos θ / √(l² − r² sin² θ)] = r sin(θ+β)/cos β.
  - Velocity: v = ω · ds/dθ.
  - Acceleration: a = ω² · d²s/dθ² + α · ds/dθ, where α is the crank angular acceleration.
  - The FFT of exact d²s/dθ² at λ = 0.2966 gave harmonics 1.000·r, 0.303·r (≈ λ) and a 4th-order term of 0.0070·r (≈ λ³/4 = 0.0065). The 2-term approximation therefore has a max error of about 1.1% of peak acceleration.
- Inertia torque of a reciprocating mass m at constant ω is T_i = −m · (d²s/dθ²) · (ds/dθ) · ω². Its harmonic amplitudes, in units of m r² ω², were 0.076, 0.500, 0.230 and 0.023 for orders 1–4. These match the classic series (λ/4, 1/2, 3λ/4, ~λ²/4) [verified numerically in-session]. The 2nd order dominates, and the mean is zero, so inertia torque does no net work per revolution.
- Variable effective inertia of one cylinder's reciprocating mass, J_rec(θ) = m (ds/dθ)². It averages 0.51·m r² (≈ m r²(1/2 + λ²/8)), peaks at 1.09·m r², and is zero at TDC/BDC [verified numerically in-session].

### Inferences
- **Gas force and torque.** Gas force on the piston is F_g = (p_cyl − p_crankcase) · A_piston. The total axial piston force is F_p = F_g − m_B · a_piston, where m_B is the reciprocating mass (piston + pin + rings + small-end share of the rod). By virtual work the crank torque is T = F_p · ds/dθ = F_p · r sin(θ+β)/cos β. This is a derivation from the verified kinematics; it is the same as the textbook "F_p r sin(θ+β)/cos β" form found in Heywood ch.2 and Taylor vol.2, but neither book was retrieved here.
- **Force decomposition along the rod and wall.** Rod force F_rod = F_p / cos β. Side thrust on the liner F_n = F_p · tan β. Its sign flips at TDC/BDC; on the power stroke it acts on the "major thrust" side.
- **Desaxe (offset pin, offset e toward the thrust side).**
  - x(θ) = r cos θ + √(l² − (r sin θ − e)²), with sin β = (r sin θ − e)/l.
  - ds/dθ = r sin θ + r cos θ (r sin θ − e)/√(l² − (r sin θ − e)²).
  - TDC/BDC no longer fall at 0°/180°. TDC is at θ_TDC = asin(e/(l+r)) and BDC at π + asin(e/(l−r)). Stroke = √((l+r)² − e²) − √((l−r)² − e²), slightly more than 2r.
  - Torque is still T = F_p · ds/dθ. All of this is derived; I found no retrieved source with typical e values (inference: production offsets are about 0.5–1.5 mm, but this is unverified).
- **Two-mass rod.** With rod mass m_rod, centre-to-centre length l, and CG at distance l_A from the big end:
  - Big-end (rotating) share m_A = m_rod (l − l_A)/l; small-end (reciprocating) share m_B = m_rod · l_A/l.
  - The pure two-mass model has inertia m_A l_A² + m_B (l−l_A)² = m_rod l_A (l − l_A), which generally exceeds the true I_cg. The correction is ΔJ = I_cg − m_rod l_A (l − l_A), usually negative and small.
  - In a real-time model the ΔJ couple (ΔJ · β̈) is often neglected. Keeping it costs one more term: a θ-dependent inertia contribution ΔJ (dβ/dθ)².
- **Implementation recommendation.** Compute the exact kinematics per cylinder each step (one sqrt, one sin/cos). Drop the λ-series except for documentation and balance analysis. For bearing loads, compute rod force components from F_p and β, plus the big-end centripetal load m_A r ω².

### Gaps
- Heywood, Taylor, Hafner & Maass and Rao could not be opened, so book-page citations for the gas torque and desaxe formulas are missing. The formulas above are derivations checked numerically, not quotations.
- No retrieved source gave typical reciprocating/rotating mass splits or desaxe offsets for production engines.

---

## 2. Engine balance: primary/secondary forces and moments by layout, balance shafts, firing order and torque uniformity

### Takeaway
For reciprocating masses only, with ideal geometry, the computed table below reproduces the known results:
- I6, V12 (60°) and flat-6 are fully balanced in 1st and 2nd order.
- I4 and flat-plane V8 have a 2nd-order shaking force.
- I3, I5 and the V6s have rocking couples.
- Cross-plane V8 has a 1st-order couple that rotates with the crank, so crank counterweights can cancel it.

The key implementation rule is to split each imbalance into a component rotating *with* the crank (cancellable with counterweights) and one rotating *against* it (needs a counter-rotating balance shaft).

### Cited Findings
- Primary imbalance occurs at crankshaft frequency and secondary at twice crank frequency. Secondary forces are typically about 25% of primary in most engines. — [Wikipedia: Engine balance](https://en.wikipedia.org/wiki/Engine_balance) (via search summary)
- A 120° inline-three, a 60° V6 and a 90° cross-plane V8 all have a primary rocking couple. In the 90° cross-plane V8 it is countered by crank counterweights. A straight-4 retains a net secondary imbalance. — [Wikipedia: Engine balance](https://en.wikipedia.org/wiki/Engine_balance)
- A flat-plane V8 without counterweights is balanced for primary forces but not for secondary forces. A cross-plane V8's four phases per bank cancel the 2nd-order free forces, and its primary couple is cancelled by heavy counterweights. — [Jalopnik cross- vs flat-plane](https://jalopnik.com/2063399/cross-vs-flat-plane-cranks-differences/); [EngineLabs flat-plane article](https://www.enginelabs.com/?p=6180); [EDT Engineers "Be Flat or Be Crossed"](https://www.edtengineers.com/articles/gearhead-quandary-be-flat-or-be-crossed)
- Per-cylinder reciprocating force (2-term series): F = m_B r ω² (cos θ_i + λ cos 2θ_i), along the bore axis, where θ_i is the crank angle measured from that cylinder's TDC. — [FOSSEE Theory of Machines](https://tbc-python.fossee.in/convert-notebook/Theory_Of_Machines/ch15.ipynb)
- **Computed balance table** [verified numerically in-session].
  - Units: F1 in m_B r ω²; F2 in λ m_B r ω²; M1 and M2 additionally × cylinder pitch.
  - Each entry is "co-rotating / counter-rotating amplitude". Peak magnitude = co + ctr; a fixed-direction shake has co = ctr. Co-rotating parts can be cancelled by crank counterweights (1st order). Counter-rotating parts need a counter-rotating shaft at 1× (1st order). Any 2nd-order term needs 2× shafts.
  - Rotating masses are assumed to be counterweighted. Banks of V engines share the cylinder plane: bank stagger and articulated rods are ignored.

| Layout (crank) | F1 | M1 | F2 | M2 |
|---|---|---|---|---|
| Single cylinder | 0.50/0.50 | 0 | 0.50/0.50 | 0 |
| I2 360° | 1.00/1.00 | 0 | 1.00/1.00 | 0 |
| I2 180° | 0 | 0.50/0.50 | 1.00/1.00 | 0 |
| I3 (120°) | 0 | 0.87/0.87 | 0 | 0.87/0.87 |
| I4 (flat 180°) | 0 | 0 | 2.00/2.00 (peak 4λ m r ω², vertical) | 0 |
| I5 (72°) | 0 | 1.31/1.31 | 0 | 2.13/2.13 |
| I6 | 0 | 0 | 0 | 0 |
| 90° V-twin, common pin | 1.00/0 (pure rotating; counterweight cancels) | 0 | 0.71/0.71 | 0 |
| V6 60°, split pins (even fire) | 0 | 1.50/0 (counterweight cancels) | 0 | 0/1.50 (needs shaft) |
| V6 90°, 30° split pins (even fire) | 0 | 1.67/0.45 (residual needs 1× balance shaft) | 0 | 0.45/1.67 |
| V6 90°, common pins (odd fire) | 0 | 1.73/0 | 0 | 1.22/1.22 |
| V6 120°, common pins | 0 | 1.73/0.87 | 0 | 0.87/1.73 |
| V8 90° cross-plane | 0 | 3.16/0 (= √10, counterweights) | 0 | 0 |
| V8 90° flat-plane | 0 | 0 | 2.83/2.83 (peak 4√2 λ m r ω², horizontal) | 0 |
| V10 72°, common pins | 0 | 2.63/0.81 | 0 | 3.44/1.31 |
| V10 90°, 18° split pins | 0 | 2.60/0.41 | 0 | 1.93/3.79 |
| V12 60° | 0 | 0 | 0 | 0 |
| Flat-4 boxer | 0 | 0 | 0 | 1.00/1.00 (secondary rocking couple) |
| Flat-6 boxer | 0 | 0 | 0 | 0 |
| Radial 3 (ideal) | 1.50/0 | 0 | 0/1.50 | 0 |
| Radial 5/7/9 (ideal) | n/2 / 0 (counterweight cancels) | 0 | 0 | 0 |

- Balance shafts: other V angles generally need a balancer shaft for smoothness. — [Wikipedia: Engine balance](https://en.wikipedia.org/wiki/Engine_balance)

### Inferences
- **Real-time implementation.** The shaking force/moment vector is cheap to compute exactly every step. Sum over cylinders: F = Σ (−m_B,i a_piston,i) ê_i, plus rotating-mass terms −m_rot r ω² ê_throw,i, plus counterweight terms. Take M = Σ z_i × F_i. This gives exact all-order results, so no series is needed, and feeds an "engine mount / cabin vibration" output channel or a camera-shake/sound layer.
- **Balance-shaft option.** Model a balance shaft as a pair of eccentric masses on 2ω (Lanchester, I4) or 1ω counter-rotating (V6 90°, I3). Their force is m_e e (kω)² ê(±kθ + φ_0). Kinematically locked shafts add rotational inertia J_bs·k² to the crank and need no extra DOF.
- **Firing order and torque uniformity.**
  - Four-stroke even-firing interval is 720°/N. I4 = 180°, so its gas torque has a dominant order of N/2 = 2, which coincides with the 2nd-order inertia torque.
  - Cross-plane V8 fires evenly at 90° but each bank fires unevenly. That gives the characteristic exhaust burble, and in engine-sim the sound comes from exhaust pulses, not mechanics.
  - The inertia torque order-2 amplitude (0.5 m r² ω² per cylinder) cancels in total for N ≥ 3 even-fire engines with symmetric cranks only where the phases cancel. Since 2nd-order inertia torques add up for I4 (all cylinders in phase at 2θ), I4s have a strong 2nd-order speed ripple at high rpm.
  - These are inferences from the formulas; I did not retrieve a table of engine torque-order amplitudes.

### Gaps
- No retrieved primary source had a full layout table with numbers for comparison. The table above is computed in-session (method stated) and agrees qualitatively with every layout the cited sources cover (I3, I4, 60° V6, cross/flat V8).
- Articulated-rod radials (master/slave rods) are not exactly balanced in 2nd order; I found no source quantifying this.
- The 90° V10 split-pin results depend on the exact pin split and firing order chosen (18° split assumed); verify against a real crank.

---

## 3. Crankshaft torsional vibration: lumped mass–spring models, natural frequencies, dampers, dual-mass flywheel, Holzer; real-time inclusion?

### Takeaway
Represent the crank as a chain of inertias (damper, cylinder throws, flywheel) joined by torsional springs. The first mode of a passenger-car crank is high, a few hundred Hz (inference), and lies well inside a 10–35 kHz simulation's stable range. A 1–2-mode model (pulley/damper + crank + flywheel, optionally a DMF secondary) is real-time feasible and adds audible and measurable effects.

### Cited Findings
- **Holzer method (torque-summation).** Model the crank as a chain of inertias and springs, assume a trial natural frequency, and propagate amplitude and torque along the chain. Iterate until the residual torque at the free end is zero. It is the simplest and most commonly used approach. — [MSU thesis (crank torsional vibration)](https://d.lib.msu.edu/etd/13060/FULL_TEXT/download); [SCEEE journal article](https://sceee.journals.ekb.eg/article_390474_be68a4ac612f9366cebe88d51c1cca2d.pdf)
- For a four-cylinder diesel, Holzer natural frequencies agreed well with finite-element results. — [SCEEE journal article](https://sceee.journals.ekb.eg/article_390474_be68a4ac612f9366cebe88d51c1cca2d.pdf) (via search summary)
- **Viscous damper.** An inertia ring sits in silicone fluid, with no stiffness between ring and hub. It adds damping but no new natural frequency. A tuned rubber damper, by contrast, adds a natural frequency below the crank's first torsional mode. — [Damping of torsional vibrations of ship engine crankshafts – viscous damper selection (Polish Maritime Research)](https://journal.mostwiedzy.pl/pmr/article/view/1039); [MSU thesis](https://d.lib.msu.edu/etd/13060/FULL_TEXT/download)
- **Excitation.** Gas pressure torque and inertia torque vary strongly over the 720° cycle and drive the torsional resonances. — [e3s-conferences TransSiberia 2024 paper](https://www.e3s-conferences.org/articles/e3sconf/pdf/2024/79/e3sconf_transsiberia2024_07012.pdf) (via search summary)
- **DMF frequencies.**
  - A conventional clutch-disc torsional mode lies at 40–80 Hz and is a common cause of gear rattle. The DMF shifts part of the flywheel inertia to the transmission input and greatly lowers the torsion spring rate, which moves the driveline resonance to f ≈ 7.5–15 Hz. — [Schaeffler, "Advanced Development of Dual Mass Flywheel"](https://www.schaeffler.com/remotemedien/media/_shared_media/08_media_library/01_publications/schaeffler_2/symposia_1/downloads_11/1_Dual_Mass_Flywheel_1.pdf)
  - One DMF patent places the natural frequency near 750 rpm, well below idle. — [US5156066A](https://patents.google.com/patent/US5156066)
  - A 2026 SAE paper on a 3-cylinder DMF reports resonance below idle rpm and notes that crossing it during start-up needs special attention. — [SAE paper 2026-26-0349](https://saemobilus-test.cld.sae.org/papers/design-analysis-dual-mass-flywheel-engine-startup-condition-3-cylinder-gasoline-application-2026-26-0349)

### Inferences
- **Equations (n-mass chain, free–free).** J_k θ̈_k = T_ext,k(θ_k, θ̇_k) − k_{k−1}(θ_k − θ_{k−1}) − k_k(θ_k − θ_{k+1}) − c_{k−1}(θ̇_k − θ̇_{k−1}) − c_k(θ̇_k − θ̇_{k+1}) − c_abs,k θ̇_k.
  - Two-inertia natural frequency: f = (1/2π) √(k (J1+J2)/(J1 J2)).
  - Viscous damper (Houdaille): ring inertia J_d coupled by viscous torque c_d (θ̇_hub − θ̇_ring) only.
  - Tuned rubber damper: ring with k_d and c_d. Classical Den Hartog optimum: f_d/f_n = 1/(1+μ) and ζ_opt = √(3μ/(8(1+μ)³)), where μ = J_d/J_eq. These are from Den Hartog, not retrieved here.
- **Resonance speed.** rpm_res = 60·f_n / q, for excitation order q. For a four-stroke the orders are multiples of 1/2; the major orders are N/2 and its multiples for N cylinders. Example: an I4 mode at 400 Hz is excited by order 6 at 4000 rpm. This is derived and standard, but no page was retrieved.
- **Real-time feasibility.**
  - Semi-implicit Euler is stable for hω_n < 2. At 10 kHz (h = 100 µs), a 400 Hz mode has hω = 0.25, which is stable with about 25 steps per period. Modes up to about 1.5 kHz stay accurate enough at 35 kHz.
  - Each extra torsional DOF costs only a few flops in a reduced-coordinate model.
  - In engine-sim's maximal-coordinate solver, splitting the crankshaft body into several bodies joined by spring force generators is also feasible. Crank throws would map to the existing per-crankshaft bodies, which are currently locked together.
- **Recommended minimum model.** Three inertias (front damper ring, crank+throws lumped, flywheel) and one optional DMF secondary: 2–4 DOF total. The DMF arc spring can be piecewise-linear, with stage stiffnesses and a friction hysteresis torque (a Karnopp element, §7).
- **Typical values (unsourced estimates — flag for verification).** Passenger-car crank first torsional mode about 250–600 Hz without damper. Crank torsional stiffness between throws about 0.5–2 MN·m/rad. Damper ring inertia about 10–25% of the crank's inertia.

### Gaps
- No retrieved source gave numeric torsional stiffness (N·m/rad) per crank segment, damper ring inertias, or DMF arc-spring rates (N·m/deg) for passenger cars. The search for DMF stiffness returned none.
- Den Hartog's tuned-absorber optimum is quoted from standard theory, not a retrieved page.

---

## 4. Flywheel / rotational inertia estimates and engine speed fluctuation

### Takeaway
Speed fluctuation follows from the energy fluctuation over a cycle: δ = (ω_max − ω_min)/ω_mean ≈ ΔE_max/(J ω_mean²). Low-cylinder-count engines at idle need the largest J. engine-sim configs use crank+flywheel inertias of about 0.05–0.12 kg·m², which is in the plausible range for passenger engines.

### Cited Findings
- engine-sim example configurations set crankshaft `moment_of_inertia` = 0.12 kg·m², 0.2299×0.5 ≈ 0.115 kg·m², and 0.2299×0.2 ≈ 0.046 kg·m². Flywheel masses are 5–15 kg (e.g. 8 lb, 5 lb, 6.8 kg, 15 kg), and simulation frequencies are 15–35 kHz. — local files in `/home/user/engine-sim/assets/engines/**/*.mr` (grep of `moment_of_inertia`, `flywheel_mass`, `simulation_frequency`)
- One engine-sim config builds crank inertia from disks: `disk_moment_of_inertia(mass, radius)` for the crank (radius = stroke/2), the flywheel and a 10 kg × 6 cm element. — local file under `/home/user/engine-sim/assets/engines/` (same grep)
- In engine-sim, the crank rigid body mass is crank mass + flywheel mass and I = `getMomentOfInertia()`. Piston bodies get an arbitrary I = 1.0, which does not matter because pistons are not torqued. Driveshaft inertia is a fixed 0.05 kg·m². — `/home/user/engine-sim/src/piston_engine_simulator.cpp:79-81, 199-203, 112`; `/home/user/engine-sim/include/piston_engine_simulator.h:73`

### Inferences
- **Speed fluctuation.** Equation of motion for the crank: J_eff(θ) θ̈ + ½ J_eff'(θ) θ̇² = T_gas(θ) + T_inertia-free − T_friction − T_load. Integrating T − T_mean over the cycle gives the energy excursion ΔE_max, and δ ≈ ΔE_max/(J ω²).
  - Classic machine-design guideline values for δ are about 1/100–1/300 for vehicle engines. These are from memory and not retrieved — verify against Rao "Dynamics of Machinery".
- **Inertia estimates (estimates, unsourced).**
  - Solid-disk flywheel: J = ½ m R². A 9 kg, 0.14 m radius disk gives 0.088 kg·m².
  - Typical passenger-car totals (crank + flywheel + clutch): 0.1–0.3 kg·m² for 1.5–3 L engines, about 0.05–0.1 kg·m² with lightweight flywheels. Sport-bike engines are about 0.01–0.03 kg·m² referenced to the crank.
  - Add the reciprocating contribution J_rec(θ) from §1: mean ≈ ½ Σ m_B r². For a 4-cylinder with m_B = 0.5 kg and r = 0.043 m that is about 0.0019 kg·m², roughly 1–2% of J. It is small but produces a 2nd-order speed ripple.
- **Gearbox inertia.** Reflect gearbox and vehicle inertias to the crank by the square of the ratio: J_reflected = J_wheel_side/(G·i_diff)². A 1000 kg car with a 10" tyre (r = 0.254 m) in 1st gear (3.5 × 3.42) reflects m (r/(G i))² = 1000·(0.254/11.97)² ≈ 0.45 kg·m². engine-sim uses exactly this formula (§7).

### Gaps
- No retrieved source gave measured flywheel/crank inertias for specific production engines, or a cited δ guideline table.

---

## 5. Valvetrain: cam profiles, acceleration limits, spring surge and float, hydraulic lifters, simple dynamic models, VVT/VVL (VTEC, VANOS)

### Takeaway
For real time, keep a kinematic cam lift table (as engine-sim does). Optionally add a 1-DOF valve mass–spring–contact model per representative valve to predict float or bounce above a threshold rpm. Spring surge needs a multi-node spring model and is best represented by a simple frequency check, not full simulation.

### Cited Findings
- **Polydyne cams.** A polynomial lift curve is designed for oscillation-free operation at one chosen speed. At other speeds polydyne cams can be dynamically inferior to other acceleration profiles. An earlier jerk-free method ignored the spring force characteristic. — [US3981281A (cam for controlling IC engine valves)](https://patents.google.com/patent/US3981281A/cg-RX-API-DMAC10.html)
- **SDOF follower model.** Norton's *Design of Machinery* Ch.10 models a cam-follower as a single-DOF mass–spring–damper. Its example uses a return spring of 50 lb/in with 30 lb preload and damping of 0.05–0.1 of critical. — [Norton, Design of Machinery ch.10 PDF](https://designofmachinery.com/wp-content/uploads/2018/11/Chap10p307.pdf)
- **Spring surge.** A spring with distributed mass is treated like a rod with longitudinal natural frequencies. Surge occurs when the coils stop moving together. — [Vibrationdata blog: Spring Surge](https://blog.vibrationdata.com/?p=151029)
- Aggressive cam profiles with high jerk excite a broad frequency spectrum and make surge more likely. — [STK Valve Spring Failure Analysis](https://stk.co.za/valve-spring-failure-analysis/) (industry blog; moderate reliability)
- **Float rule of thumb.** Compare peak negative cam acceleration × valvetrain mass against spring force. All components flex and vibrate, so this is only a rule of thumb. — [TheSamba forum archive](https://thesamba.com/vw/forum/archive/index.php/o-t--t-589370--.html) (enthusiast forum; low reliability)
- **Multi-mass valvetrain models.** These represent the valve and retainer as one node and split the spring into several nodes. — [Springer chapter (valvetrain dynamics model)](https://link.springer.com/chapter/10.1007/978-3-030-70566-4_18)
- **engine-sim lobe shape.** engine-sim's "harmonic" cam lobe is lift(x) = L_max · (0.5 + 0.5 cos(k x))^γ for |x| < π/k, else 0. k is chosen so that lift = 50 thou at ±duration_at_50_thou/4 (cam angle), via k = acos((2·0.05"/L)^(1/γ) − 1)/(dur/4). — `/home/user/engine-sim/scripting/include/actions.h:450-466`
- engine-sim samples cam lift kinematically: `valveLift = sampleLobe(camAngle + lobeAngle)`, with cam angle = (crank angle + advance)/2. The advance is a static parameter, and there is no valve dynamics. — `/home/user/engine-sim/src/camshaft.cpp:41-55`
- **engine-sim VTEC.** The VTEC valvetrain switches to the high-lift camshafts when manifold pressure exceeds a set vacuum threshold and engine speed is above a minimum rpm. — `/home/user/engine-sim/src/vtec_valvetrain.cpp:63-64`

### Inferences
- **Profile families (implementation notes; standard Norton-style content, not quoted).**
  - Simple harmonic or cycloidal rise/return.
  - 3-4-5 or 4-5-6-7 polynomials in normalised angle u ∈ [0,1], e.g. 3-4-5: s = h(10u³ − 15u⁴ + 6u⁵). These give continuous acceleration (3-4-5) or jerk (4-5-6-7).
  - Polydyne: a polynomial whose coefficients are chosen so that, with a given spring rate and mass at a design speed, the valve follows the cam without vibration.
  - Real valve lobes add ramps (clearance take-up) of about 0.1–0.3 mm. These lobes are asymmetric with a positive acceleration flank, a long negative-acceleration nose and a closing ramp.
- **Acceleration in physical units.** a_valve = (dθ_cam/dt)² · d²L/dθ_cam² = (ω_crank/2)² · L''(θ_cam).
  - Normalised acceleration limits are often quoted per (cam degree)² — e.g. "mm/deg²". Convert with ω_cam in deg/s.
  - Check float onset: m_eq · |a_min| > F_spring(L) = F_preload + k_s L. Here m_eq = m_valve + m_retainer + m_locks + ⅓ m_spring + (rocker inertia)/(rocker arm)² (the ⅓-spring-mass rule is standard but not retrieved here).
  - The float rpm then follows from a_min ∝ ω² as rpm_float = rpm_ref · √(F_spring/(m_eq |a_min,ref|)), evaluated at the nose.
- **1-DOF dynamic valve model (real-time).** m_eq ẍ_v = −F_preload − k_s x_v − c ẋ_v + F_c, with contact force F_c = max(0, k_c (x_cam(θ) − x_v) + c_c (ẋ_cam − ẋ_v)).
  - Seat impact: when x_v < 0, apply a stiff seat spring (or a restitution impulse) to get bounce.
  - k_c is the valvetrain stiffness, typically tens of N/µm for a DOHC bucket-tappet train. This is an estimate, unverified.
  - Valve-train natural frequencies are about 1–3 kHz, so integrate at ≥ 20 kHz or sub-step. Alternatively use the kinematic table plus a float flag. One representative valve per camshaft is sufficient.
- **Spring surge frequency.** For a helical spring fixed at both ends, f_1 = ½ √(k/m_s) Hz (classic rod-analogy result). Surge risk is high when a strong cam harmonic n·(rpm/120) lands near f_1. Real-time approach: compute the cam lift harmonics once and flag rpm bands.
- **Hydraulic lifter.** Model it as zero lash plus a very stiff column (oil compressibility) with slow leak-down. For real time, treat it as kinematic zero-lash. Optionally include "pump-up" at float: a lifter that extends during float holds the valve off its seat (inference).
- **VVT / VVL representation.**
  - Cam phasing (VANOS, VVT-i): θ_cam = (θ_crank + φ_adv(t))/2, where φ_adv follows a first-order lag or a rate-limited actuator toward a map target from rpm and load. Phaser slew rates (order 100–300 crank deg/s for oil-pressure phasers) are unverified estimates.
  - Two-step lift (VTEC, MIVEC): switch between two lift tables when an rpm/load condition holds, with hysteresis and a short (one-cycle) switching delay.
  - Continuous VVL (Valvetronic): interpolate lift and duration tables by an actuator position.
  - engine-sim already supports a static advance and VTEC switching. A dynamic φ_adv(t) is a straightforward extension.

### Gaps
- I could not retrieve Norton's *Cam Design and Manufacturing Handbook* or SAE valvetrain-dynamics papers. Numerical acceleration limits (e.g. typical maximum valve acceleration in m/s² or mm/deg²), valvetrain stiffness, and hydraulic lifter parameters are unsourced.
- No primary source on VANOS or VTEC actuator dynamics (phaser rates, switching time) was found.

---

## 6. Starter motor torque–speed characteristics and cranking speeds

### Takeaway
Model the starter as a linear DC machine behind a pinion–ring-gear ratio, with battery and cable resistance limiting current. Cranking speeds of about 150–250 rpm, and ≥ 450 rpm before firing is expected in some strategies, are cited. engine-sim instead uses a "speed target with torque cap" constraint.

### Cited Findings
- Cold-start cranking example: 150–250 rpm, with spark delayed until the fuel has vaporised. — [US Patent 10107219 "Method and system for engine cold-start"](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10107219) (via search summary)
- One start-control patent uses a threshold engine speed N1 ≥ 450 rpm at which the engine can start firing, and says cranking should end quickly to limit noise. — [US Patent 10890122 "Method and device for controlling starting of engine"](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10890122) (via search summary; attribution of the figure to this specific patent is from the summary)
- The engine's required starting torque peaks at very low speed, about 50 min⁻¹, then falls. — [US Patent 6700212 "Method for controlling the starting torque and starting power of an IC engine"](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/6700212)
- Starter selection uses the torque needed at the minimum cranking speed and the lowest starting temperature. Manufacturers publish starter curves of torque, speed, power and current at +20 °C and −20 °C. Starter torque falls as temperature drops, and lower battery and cable resistance raises starter output. — [GCOE Ara Auto Electrical notes, Unit 3](https://mail.gcoeara.ac.in/learning_material/auto/TE/Unit%203-AEE.pdf)
- **engine-sim starter model.** A velocity constraint drives the crank toward a target speed (default −200 rpm), with constraint torque clamped to ±maxTorque (default 80 lb·ft in C++). The `.mr` engine defaults use 200–250 rpm and 100–200 lb·ft. It is not a torque–speed curve. — `/home/user/engine-sim/src/starter_motor.cpp`; `/home/user/engine-sim/es/objects/objects.mr:73-74,195-196`

### Inferences
- **Linear DC model (standard machine equations; not retrieved).**
  - T_m = k_t I, with I = (V_batt,oc − k_e ω_m)/(R_batt + R_cable + R_arm). This gives T_m(ω_m) = T_stall (1 − ω_m/ω_nl), with T_stall = k_t V/R and ω_nl = V/k_e.
  - At the crank: T_crank = G_p · η · T_m and ω_crank = ω_m/G_p. Pinion-to-ring-gear ratios of about 10–15:1 and gear-reduction starters with an extra 3–4:1 are unverified estimates.
  - Battery voltage sag: V = V_oc − I R_batt. Temperature: raise R_batt and engine friction at low temperature.
- **Expected behaviour.** The steady cranking speed is where T_crank(ω) equals the engine's mean motoring torque (friction plus pumping). The compression torque ripple is large at low speed, so cranking speed visibly oscillates per compression event, which is a desirable audible effect. A torque–speed model produces this naturally; engine-sim's speed-target constraint suppresses it whenever torque is below the cap.
- **Engagement.** Model pinion engagement as a one-way clutch (overrunning) so the starter disengages when the engine overruns.

### Gaps
- No retrieved source gave stall torque, no-load speed or pinion ratios for specific starters. Typical passenger starters are about 0.8–2 kW, but this is an estimate only.

---

## 7. Drivetrain and vehicle: clutch friction (Coulomb/viscous, Karnopp), gearbox efficiency, shaft compliance, differential, tyre force (Pacejka, slip ratio), rolling resistance (SAE J2452), drag, road load (A+Bv+Cv²), 0–100 km/h, shift dynamics, torque converter

### Takeaway
A real-time vehicle needs about 3–5 rotational DOF: engine, clutch/gearbox input, wheel(s), plus optional shaft-compliance DOFs and a vehicle translational DOF.
- Use a Karnopp-style clutch and a compliant halfshaft.
- Use Pacejka MF with slip ratio and relaxation length for low-speed robustness.
- Use road load A + Bv + Cv² (or J2452 + ½ρC_dAv²).
- Use a K-factor torque converter for automatics.

engine-sim currently lumps the vehicle into a single rotating inertia with no tyre slip, a constant rolling-resistance force, and an energy-preserving (non-physical) gear-change velocity reset.

### Cited Findings
- **Magic Formula coefficients.**
  - MathWorks gives typical constant coefficients for dry tarmac of B ≈ 10, C ≈ 1.9, D ≈ 1.0, E ≈ 0.97, and wet tarmac B 12, C 2.3, D 0.82, E 1. — [MathWorks: Tire-Road Interaction (Magic Formula)](https://www.mathworks.com/help/sdl/ref/tireroadinteractionmagicformula.html)
  - Another source says the formula's developers recommend C ≈ 1.65 for longitudinal and 1.3 for lateral, and D (peak μ) of 0.7–1.3 on dry roads, which conflicts with C = 1.9 above. — [arXiv 2504.20863 (Bayesian tyre parameter estimation)](https://arxiv.org/pdf/2504.20863); [arXiv 2005.00826 (racing MPC, uses B10/C1.9/D1 dry)](https://arxiv.org/pdf/2005.00826)
- **SAE J2452 rolling resistance.** F_RR = P^α · Z^β · (a + b V + c V²), with P = inflation pressure, Z = load and V = speed. The coefficients are fitted from coastdown-type test data per J2452. — [Wikipedia: SAE J2452](https://en.wikipedia.org/wiki/SAE_J2452); [MathWorks: Rolling Resistance block](https://www.mathworks.com/help/sdl/ref/rollingresistance.html)
  - ADVISOR ships fitted a, b, c, α, β for specific Michelin tyres in its m-files. — [ADVISOR: SAE J2452 Tire Model](https://adv-vehicle-sim.sourceforge.net/J2452_RR.html)
- **Road load and coastdown.** Under 40 CFR 1066, road-load force is characterised by on-road coastdown and expressed with target coefficients A, B, C (F = A + Bv + Cv²). The same coefficients are used to set chassis dynamometers. The unloaded-coastdown dyno check runs from about 80 mph to 10 mph. — [40 CFR 1066.301 (Cornell LII)](https://www.law.cornell.edu/cfr/text/40/1066.301); [40 CFR 1066.270 (eCFR)](https://www.ecfr.gov/current/title-40/chapter-I/subchapter-U/part-1066/subpart-C/section-1066.270)
  - EPA's "Data on Cars used for Testing Fuel Economy" lists target coefficients per model. — [PMC article referencing EPA test-car data](https://pmc.ncbi.nlm.nih.gov/articles/PMC7900940)
- **Torque converter.**
  - Torque ratio TR = T_turbine/T_pump; K-factor K = ω_pump/√T_pump; speed ratio SR = ω_t/ω_p. At a given SR a converter has one TR, K and efficiency, whatever the absolute torque or speed.
  - TR is maximal at stall, typically 1.5–2.5. K is roughly flat from stall to about SR 0.6, then rises steeply.
  - A Kotwicki model writes turbine/pump torque as a quadratic in pump and turbine speeds (α1 ω_p² + α2 ω_p ω_t + α3 ω_t²), regression-fitted per converter and per region. — [US Patent 9879769 "Torque converter clutch slip control"](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/9879769)
  - MathWorks' Torque Converter block uses a capacity factor in the form τ_I = ω²/K(SR)² (equivalently K* = τ/ω²). — [MathWorks: Torque Converter](https://www.mathworks.com/help/sdl/ref/torqueconverter.html)
  - EPA's procedure measures K at SR = 0.00–0.95, with finer steps above 0.60. — [40 CFR 1037.570 (eCFR)](https://www.ecfr.gov/current/title-40/chapter-I/subchapter-U/part-1037/subpart-F/section-1037.570)
- **Karnopp stick-slip.** Stick is declared when relative velocity is inside a small band |v| < Dv, which must be much smaller than typical system speeds. In stick, the friction force equals the force needed to keep the relative acceleration zero, saturated at the static limit; outside the band, kinetic (Coulomb + viscous) friction applies.
  - The resulting model is discontinuous but non-stiff during stick. — [UPB Scientific Bulletin paper on friction models](https://scientificbulletin.upb.ro/static/pdfs/rez541_643726.pdf); [Kikuuwe et al. robotics symposium paper](https://home.hiroshima-u.ac.jp/kikuuwe/pub/05_robosymp.pdf)
  - Drawbacks: drift-off from a constant velocity offset during long sticks, and an arbitrary band threshold. — [UPB paper](https://scientificbulletin.upb.ro/static/pdfs/rez541_643726.pdf); [Kikuuwe](https://home.hiroshima-u.ac.jp/kikuuwe/pub/05_robosymp.pdf)
  - Karnopp was chosen for a clutch model after reviewing alternatives. — [Swanson, Georgia Tech AIM'99](https://imdl.gatech.edu/davin/research/swanson_aim99final.pdf)
  - Haessig & Friedland found models of this kind preferable to the classical model. — [NJIT: On the modeling and simulation of friction](https://digitalcommons.njit.edu/fac_pubs/17572)
  - Original reference: Karnopp, J. Dyn. Sys. Meas. Control 107 (1985) 100–103. — [MATLAB File Exchange Karnopp model](https://mathworks.com/matlabcentral/fileexchange/155462-karnopp-s-model-stick-slip-friction-dynamics-in-simulink)
- **engine-sim drivetrain (current implementation).**
  - **Clutch.** A ClutchConstraint between the crank and the vehicle-equivalent body enforces equal angular velocity, with torque clamped to ±maxClutchTorque × clutchPressure. Default max is 1000 lb·ft. This is a velocity-level Coulomb clutch, so stick/slip arises from the LCP-style clamp. With `simulate_flex` the clutch becomes a spring force generator instead. — `/home/user/engine-sim/src/transmission.cpp:44-77`; `/home/user/engine-sim/es/objects/objects.mr` (vehicle/transmission defaults)
  - **Vehicle.** The vehicle is one rotating body with I = m_car·(r_tyre/(i_diff·i_gear))². On a gear change, ω is rescaled to conserve the *kinetic energy* of the rotating mass. Vehicle speed is computed as √(2E_rot/m), which loses the sign. — `/home/user/engine-sim/src/transmission.cpp:79-107`; `/home/user/engine-sim/src/vehicle.cpp:62-71`
  - **Drag.** A one-sided torque-limit constraint applies rolling resistance (a *constant* force, default 2000 N) + ½ρ C_d A v² + brake force, mapped to virtual torque via √(I/m). Defaults: m = 1000 kg, C_d = 0.25, A = (72 in)² ≈ 3.34 m², i_diff = 3.42, r_tyre = 10 in, drivetrain stiffness 50 lb·ft/deg (≈ 3.9 kN·m/rad), clutch stiffness 100 lb·ft/deg; flex off by default. — `/home/user/engine-sim/src/vehicle_drag_constraint.cpp`; `/home/user/engine-sim/es/objects/objects.mr` (vehicle node inputs, ~lines 826–862)

### Inferences
- **Recommended real-time drivetrain (all ODEs, integrate with the engine at the same h).**
  1. Engine/flywheel ω_e; clutch friction torque T_c.
  2. Gearbox input ω_in (clutch disc + input shaft inertia, about 0.005–0.02 kg·m², estimate). Gear ratio i_g with efficiency η_g: forward T_out = i_g η_g T_in, and on overrun use T_out = i_g T_in/η_g so power flows correctly.
  3. Compliant output/halfshaft: T_s = k_s (θ_out/i_d − θ_w) + c_s (ω_out/i_d − ω_w). This gives the driveline "shuffle" mode, typically 2–8 Hz in low gears (estimate; consistent with the Schaeffler statement that a DMF puts the driveline resonance at 7.5–15 Hz for the engine-side mode).
  4. Wheels: J_w ω̇_w = T_s − F_x R_e − T_brake. Vehicle: m v̇ = Σ F_x − F_RL(v) − m g sin(grade).
  5. Tyre: F_x = F_z · D sin(C atan(B κ − E(B κ − atan(B κ)))). Slip ratio κ = (ω R_e − v)/max(|v|, v_min). For robustness at low speed use a relaxation-length state: σ κ̇ + |v| κ = ω R_e − v, with σ ≈ 0.1–0.5 m (estimate, Pacejka-style). This avoids the 0/0 singularity at standstill. Traction limit: F_x ≤ μ F_z,drive, with load transfer ΔF_z = m a h_cg/L.
- **Clutch model options.**
  - (a) A velocity-level constraint with a torque clamp, as engine-sim does, is equivalent to an implicit Karnopp model and is unconditionally stable.
  - (b) An explicit Karnopp: if |Δω| < Δω_band and |T_required| ≤ T_static, lock (T_c = T_required); else T_c = sign(Δω)·(μ_k F_n r_eff n_surfaces) + c_v Δω.
  - Clutch capacity: T_max = μ F_n r_eff n, with r_eff = (2/3)(R_o³ − R_i³)/(R_o² − R_i²). This is standard but not retrieved; μ for organic facings is about 0.25–0.4 (estimate).
  - Option (a) fits engine-sim's solver. Add a separate static/kinetic μ (the `.mr` already declares coeff_static_friction/dynamic inputs).
- **Gear shift dynamics.** A shift is: clutch opens (torque ramps down over about 0.05–0.2 s), gear ratio changes with synchroniser slip, clutch re-engages under slip, and the engine is pulled to the new synchronous speed by T_c.
  - Physically, vehicle speed is continuous across a shift. Only the gearbox input changes speed, and the energy difference is dissipated in clutch/synchroniser slip.
  - engine-sim's KE-preserving ω rescale on the vehicle body (transmission.cpp:79-107) **changes vehicle speed on a shift**, because reflected inertia I ∝ 1/i_g². This is physically wrong and should be replaced by holding v constant: ω_new = ω_old · (i_new/i_old), with the clutch absorbing the mismatch.
- **Road load.** Either use coastdown A, B, C (from EPA test-car data) or F_RL = C_rr m g + ½ ρ C_d A v² (+ J2452 velocity terms).
  - engine-sim's constant 2000 N rolling resistance on a 1000 kg default car corresponds to C_rr ≈ 0.20. That is about 15–20× typical passenger-car values of about 0.008–0.015 (typical values are an estimate; no retrieved source). It probably compensates for the missing B·v and drivetrain losses.
- **0–100 km/h computation.** Integrate m_eff dv/dt = min(F_engine,wheel(v, gear), μ F_z,drive) − F_RL(v), where m_eff = m + J_w,total/R_e² + J_e (i_g i_d)²/R_e² (rotational mass factor, about 1.05–1.4 depending on gear; estimate). Include shift times (about 0.2–0.5 s manual, under 0.1 s DCT) and the launch-clutch slip phase. Report t at v = 27.78 m/s. Outputs: time, distance, shift points and wheelspin.
- **Torque converter (K-factor model).**
  - Pump torque T_p = (ω_p/K(SR))², turbine torque T_t = TR(SR)·T_p, with TR and K tabulated vs SR. Typical stall TR is 1.8–2.2 (within the cited 1.5–2.5); K is flat to SR ≈ 0.6, the coupling point is around SR ≈ 0.85–0.9 where TR → 1, and the lock-up clutch is a Karnopp/clamp clutch in parallel.
  - The converter adds no stiff dynamics because torque is algebraic in speeds, which is ideal for real time. Use the Kotwicki quadratic per region for better fidelity near coupling.
- **Differential.** An open differential splits torque equally, T_L = T_R = T_diff/2, with ω_carrier = (ω_L + ω_R)/2. For straight-line longitudinal simulation one lumped wheel is adequate. A limited-slip differential adds a Karnopp clutch between the two sides with a bias ratio (inference).

### Gaps
- No retrieved source gave numeric halfshaft or propshaft torsional stiffness (N·m/rad), gearbox mesh efficiencies, J2452 α/β/a/b/c values, or C_rr ranges. These values exist in ADVISOR m-files and Gillespie but were not accessible.
- Pacejka MF 5.2/6.x full coefficient sets (pCx1, pDx1, ...) for a real tyre were not retrieved. Only simplified constant B/C/D/E sets are cited, and the sources conflict on C (1.65 vs 1.9).
- No source retrieved for synchroniser timing or typical shift durations.

---

## 8. Numerical methods for constrained rigid multibody systems in real time (Baumgarte, sequential impulse/Gauss-Seidel, reduced coordinates, symplectic integrators, accuracy vs stability)

### Takeaway
For a single-loop mechanism like the slider-crank, a reduced (generalised) coordinate formulation (one DOF per crankshaft, plus optional torsional DOFs) is more accurate and cheaper than maximal coordinates with iterative constraint solving. It has no constraint drift, no iteration-count-dependent compliance, and no Baumgarte energy injection. Maximal-coordinate impulse solvers (Catto/Box2D style) are robust and general but trade accuracy for stability.

### Cited Findings
- Catto's GDC 2011 "Soft Constraints" tutorial derives the ODE engine's ERP/CFM parameters from the time step, spring stiffness and damping, i.e. constraint softness as an implicit spring-damper. — [Catto, Soft Constraints, GDC 2011 (PDF)](https://box2d.org/files/ErinCatto_SoftConstraints_GDC2011.pdf)
- **Baumgarte stabilisation.** Feed a fraction (about 0.2) of the position error, scaled by 1/dt, into the velocity constraint as a bias. Baumgarte can add energy and cause jitter; split impulses are a remedy. — [GameDev.net forum: collision resolution](https://gamedev.net/forums/topic/703356-question-about-collision-resolution/); [GameDev.net: split impulses](https://gamedev.net/forums/topic/705305-split-impulses-in-a-constraint-based-physics-engine); [Allen Chou: Game Physics – Contact Constraints](https://allenchou.net/2013/12/game-physics-resolution-contact-constraints)
- **Sequential impulses** solve constraints one at a time, like Gauss-Seidel, instead of all at once. — [GameDev.net: Understanding Constraint Resolution in Physics Engine](https://www.gamedev.net/articles/programming/math-and-physics/understanding-constraint-resolution-in-physics-engine-r4839/)
- Catto has said sequential impulses can handle stiffness and damping values larger than a true explicit spring could. — [Bullet forum thread quoting Catto](https://bulletphysics.org/Bullet/phpBB3/viewtopic.php?f=4&t=1354)
- **engine-sim's solver (local source).** Per substep, the OptimizedNsvRigidBodySystem:
  1. Builds the sparse Jacobian J (3 DOF per body).
  2. Forms q̇' = q̇ + M⁻¹F_ext·dt.
  3. Sets the bias b_err = (biasFactor/dt)·C with **biasFactor = 1.0**.
  4. Solves (J M⁻¹ Jᵀ) λ = −(J q̇' + v_bias + b_err) with projected Gauss-Seidel, using box limits for torque-limited constraints, warm-started from the previous λ.
  5. Converts λ/dt to constraint forces, then integrates with **semi-implicit (symplectic) Euler**: v += a·dt; x += v·dt.
  — `/home/user/engine-sim/dependencies/simple-2d-constraint-solver/src/optimized_nsv_rigid_body_system.cpp:6-10,120-275`; `/home/user/engine-sim/dependencies/simple-2d-constraint-solver/src/nsv_ode_solver.cpp:20-32`
- The Gauss-Seidel solver forms the dense matrix J W Jᵀ and stops at `m_maxIterations` (default 128, settable per engine via `max_sle_solver_steps`). It also stops when the max *relative* change of λ falls below `m_minDelta = 0.1`, i.e. 10%, a loose tolerance. — `/home/user/engine-sim/dependencies/simple-2d-constraint-solver/src/gauss_seidel_sle_solver.cpp:6-13,58-81,125-160`; `/home/user/engine-sim/src/engine.cpp:45,81`
- engine-sim's default system type is `NsvOptimized` with `GaussSeidelSleSolver`. The alternative `GenericRigidBodySystem` (Gaussian elimination + NSV) is the only path that uses per-constraint `ks`/`kd`. — `/home/user/engine-sim/src/simulator.cpp:31-45`; `/home/user/engine-sim/dependencies/simple-2d-constraint-solver/src/generic_rigid_body_system.cpp:119-177`

### Inferences
- **Reduced-coordinate crank model (recommended).** Generalised coordinate θ (crank angle), with Lagrange's equation:
  - Kinetic energy: T = ½ J(θ) θ̇². J(θ) = J_crank+flywheel+attached + Σ_i [m_A,i r² + m_B,i (ds_i/dθ)² + I_rod,cg,i (dβ_i/dθ)² + m_rod,i·(CG velocity terms)]; the exact rod terms are easily written from the kinematics.
  - Equation of motion: J(θ) θ̈ + ½ J'(θ) θ̇² = Σ_i F_gas,i(θ) ds_i/dθ − T_fric(θ, θ̇) − T_load + T_starter.
  - Momentum form for a symplectic step: p = J(θ) θ̇, ṗ = Q + ½ J'(θ) θ̇². Use semi-implicit Euler or Störmer–Verlet; J'(θ) is analytic or finite-differenced.
  - Cost: O(N_cyl) scalar ops per step, with no linear solve, no iteration and no constraint drift.
  - Piston, rod and bearing forces are recovered post hoc from kinematics (positions, accelerations, F_rod = F_p/cos β, side thrust, shaking forces of §2).
  - Multiple crankshafts (opposed-piston, W engines) are coupled by gear ratios as kinematic relations, or by springs if compliance is wanted.
- **Why reduced coordinates fit here.** The slider-crank has exactly one DOF and holonomic, non-redundant constraints. Maximal coordinates add 3 DOF per body (I4: 9 bodies, 27 DOF, about 22 constraint rows) only to remove them again by an iterative solver.
  - Any residual (loose GS tolerance, finite iterations, β = 1 bias) appears as *spurious compliance and energy noise* in exactly the signal of interest: crank torque and speed ripple.
  - Reduced coordinates give an exact energy balance: work in = ∫ T dθ. Combustion torque, friction and indicated vs brake work then become directly auditable, which matters for an "engineering tool" output (IMEP/BMEP consistency).
- **Integrator choice.** Semi-implicit Euler is 1st-order and symplectic, with bounded energy error for conservative parts, and is stable for hω < 2. Störmer–Verlet is 2nd-order at the same cost per step (one force evaluation).
  - At 10–35 kHz a rigid crank is dominated by gas-force stiffness in θ, which is not stiff, so explicit symplectic schemes are adequate.
  - Stiffness enters with torsional springs (§3), valvetrain contact (§5) and tyre/clutch stick. Handle these with velocity-level implicit clamps (as engine-sim does for the clutch), with sub-stepping, or with locally implicit spring updates.
- **Baumgarte at β = 1** (engine-sim's biasFactor) corrects the full position error in one step. That is the most aggressive setting: it maximises drift removal but injects the most energy. Catto-style practice uses β ≈ 0.1–0.2, or soft constraints with explicit (k, c) mapped to (β, γ). The Catto mapping (from the cited slides, which could not be fully retrieved; verify) is β = h k/(c + h k), γ = 1/(c + h k) (per unit effective mass).
- **Hybrid option.** Keep engine-sim's solver for the vehicle/clutch/dyno/starter constraints, where the torque clamps work well, but replace the crank–rod–piston bodies with a single crank body whose inertia is J(θ). Add the gas torque Σ F_gas ds/dθ and the inertia-coupling term −½ J'(θ) θ̇² as external torques. This removes 2N bodies and 5N constraint rows per engine.

### Gaps
- The Catto PDF, Baraff course notes and Featherstone's articulated-body papers could not be fetched; the ERP/CFM mapping above is from memory of the slides and should be verified.
- No published benchmark was found comparing maximal vs reduced coordinates specifically for slider-crank accuracy in real time. The comparison here is analytical, not measured.

---

## 9. How engine-sim models mechanics, and implications

### Takeaway
engine-sim uses a 2D maximal-coordinate impulse solver: velocity-level constraints with a full Baumgarte bias (β = 1), projected Gauss-Seidel with a loose 10% relative tolerance and up to 128 iterations, and semi-implicit Euler at 10–35 kHz.
- The `ks = 5000, kd = 10` set in `piston_engine_simulator.cpp` are **ignored by the default `NsvOptimized` system**. They only matter for the non-default `GenericRigidBodySystem`, so the joints are *not* spring-dampers in practice.
- The approach is robust and general but has inexact constraint satisfaction and a crank-drift "hack". It also offers no explicit energy audit.
- A reduced-coordinate crank (§8) would be more accurate and cheaper, and would make bearing loads, shaking forces and torque decomposition straightforward outputs.

### Cited Findings
- **Bodies and constraints per cylinder.**
  - The piston and the rod are each rigid bodies.
  - Two LinkConstraints join piston↔rod (wrist pin) and rod↔crank journal, each with 2 rows.
  - A cylinder-wall LineConstraint keeps the piston on the bore axis.
  - The combustion chamber is registered as a force generator acting on the piston.
  - The crank body is pinned to its bearing location with a FixedPositionConstraint.
  - A RotationFrictionConstraint applies a symmetric Coulomb friction torque of ±frictionTorque.
  - Multiple crankshafts are linked to the output crank by ClutchConstraints.
  - Constants `ks = 5000`, `kd = 10` are assigned to the crank, link and wall constraints.
  — `/home/user/engine-sim/src/piston_engine_simulator.cpp:57-98,143-211`
- `OptimizedNsvRigidBodySystem::processConstraints` never reads `ks`/`kd`. Error correction is only b_err = (m_biasFactor/dt)·C with m_biasFactor = 1.0. `ks`/`kd` are consumed only by `GenericRigidBodySystem`. — `/home/user/engine-sim/dependencies/simple-2d-constraint-solver/src/optimized_nsv_rigid_body_system.cpp:8,185-187`; `/home/user/engine-sim/dependencies/simple-2d-constraint-solver/src/generic_rigid_body_system.cpp:119-177`
- `LinkConstraint` computes C = world-position mismatch, the analytic J, and J̇. The default limits are ±DBL_MAX, i.e. equality constraints. — `/home/user/engine-sim/dependencies/simple-2d-constraint-solver/src/link_constraint.cpp`
- Each simulator step calls `m_system->process(timestep, 1)`, with timestep = 1/simulation_frequency (default 10 kHz; engine configs use 15–35 kHz). Step counts per frame adapt to audio-synthesiser latency. — `/home/user/engine-sim/src/simulator.cpp:16,79-122`
- After each step, every secondary crankshaft's angle is overwritten with the output shaft's angle: "Correct drift (temporary hack)". The angle-only ClutchConstraint (C = 0, velocity-only) lets multi-crank engines drift apart in angle. — `/home/user/engine-sim/src/simulator.cpp:126-133`; `/home/user/engine-sim/dependencies/simple-2d-constraint-solver/src/clutch_constraint.cpp`
- Starter and dynamometer are velocity motors with torque caps: v_bias = target speed, limits = ±maxTorque. The dyno can "hold" via two-sided limits. — `/home/user/engine-sim/src/starter_motor.cpp`; `/home/user/engine-sim/src/dynamometer.cpp:34-47`
- **Crank friction** is a constant Coulomb torque per crankshaft (configs use 1–10 lb·ft), independent of speed, load or temperature. — `/home/user/engine-sim/src/piston_engine_simulator.cpp:83-85`; engine `.mr` files (`friction_torque`)

### Inferences
- **The joints are effectively rigid, not spring-dampers.** The stated "ks = 5000, kd = 10" have no effect in the shipped configuration. Constraint compliance therefore comes from the bias (β = 1) and from incomplete Gauss-Seidel convergence. That compliance depends on dt, iteration count and system conditioning, and has no physical parameter.
  - The 10% relative-change stopping criterion lets GS exit early, at the expense of joint error and λ noise. The full-error-in-one-step bias then injects energy to correct it next step.
  - Both effects produce small high-frequency crank-speed noise. Since sound is synthesised from exhaust flow rather than mechanics, this is mostly invisible to the listener, but it contaminates any "engineering output" derived from instantaneous torque or speed.
- **Torque decomposition is implicit.** Gas force acts on the piston body, and crank torque emerges through the solved constraint forces. To report indicated, inertia and friction torque, or bearing loads, you must read constraint reactions (available in `constraint->F_x/F_y/F_t`) and these carry the solver error. A reduced model gives these directly and exactly (§1, §8).
- **Performance.** A dense GS matrix of size m_f × m_f is rebuilt every substep: for a V12 about 60–70 rows, so about 5k entries, up to 128 sweeps, at 10–35 kHz.
  - This is feasible but wasteful compared with a 1-DOF crank: O(N) work and no iterations. The savings would fund torsional DOFs, valve dynamics, or higher fluid-simulation step counts.
- **Cylinder wall constraint = side thrust.** The LineConstraint reaction is the piston side force F_n = F_p tan β from §1, so engine-sim can already output side thrust and wall friction inputs, with solver noise.
- **Crank drift hack.** Multi-crank engines (e.g. opposed or W layouts) need a position-level angular constraint (C = θ_2 − θ_1 − φ_0 with a bias) or a gear constraint. Overwriting θ silently breaks momentum and energy consistency.
- **Vehicle model limits** (from §7): single lumped rotating mass, no tyre slip or traction limit, unsigned speed, constant rolling force, and KE-preserving gear-change velocity jump. These are the highest-value drivetrain upgrades for a "0–100 km/h" engineering output.
- **Suggested migration path (low risk).**
  1. Set biasFactor to about 0.2 and tighten the GS tolerance, measuring CPU impact; or expose both as engine parameters.
  2. Add an energy-audit output: ∫ T_gas dθ vs ΔKE + friction work.
  3. Prototype a reduced-coordinate crank as an alternative `Simulator` system type behind a flag, keeping the existing clutch/dyno/starter constraints acting on the single crank body.
  4. Replace the vehicle lumped mass with wheel + vehicle DOFs and a tyre model.

### Gaps
- No upstream design document or issue explaining why `ks`/`kd` are set but unused in the optimized system was found, and no upstream discussion was retrieved because GitHub page fetches were blocked. Behaviour is inferred from source reading only.
- No measurements of constraint error magnitude or of the CPU split (solver vs fluid simulation) were made in this session. engine-sim records per-frame timings (`m_constraintSolveMicroseconds` etc.) that could quantify this.
