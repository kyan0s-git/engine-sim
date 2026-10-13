# Engineering physics roadmap

This plan turns the findings in
[`reports/Realistic engine physics models.md`](reports/Realistic%20engine%20physics%20models.md)
into concrete work. Section numbers ("§") refer to that report; the research
notes behind it are in [`research_notes/`](research_notes/).

## Principles

1. **Two physics models side by side.** Every engine selects
   `physics_model: "legacy"` (default, the original tuned behaviour) or
   `"engineering"`. Legacy stays bit-for-bit compatible so that the shipped
   engines keep sounding the way they were tuned; new physics is developed in
   the engineering model and becomes the default only once it is validated.
   Fixes for outright bugs that are not compensated by script tuning apply to
   both models.
2. **Measure before changing.** Engineering outputs (IMEP, PMEP, BMEP, FMEP,
   BSFC, efficiencies, burn angles, VE) and a headless dyno sweep come first, so
   that every later change is quantified.
3. **Verify, then validate.** Each model gets unit tests against closed-form
   solutions (§ "Verify against closed forms"); whole-engine results are then
   compared with published data.
4. **Real time stays the constraint.** Everything except 1-D pipes costs a
   few polynomial or `pow` evaluations per cylinder step.

## Milestone A — thermodynamic core, heat transfer, friction and measurement (done)

| Item | Report phase | Status |
| --- | --- | --- |
| Per-cycle engine metrics: IMEP (gross/net), PMEP, BMEP, FMEP, indicated/brake/mechanical efficiency, BSFC, trapped air and fuel mass, VE (dry air, ambient reference), AFR/λ, CA10/50/90, peak pressure and its angle | 0 | done |
| Headless dyno sweep producing CSV (`engine-sim-dyno`) with SAE J1349 correction column | 0 | done |
| Deterministic random numbers (seeded per chamber) | 0 | done |
| `physics_model` engine switch threaded through intake, exhaust and cylinders | – | done |
| Element-conserving `react()` (O₂ is no longer consumed without fuel) | 1 | done |
| Equivalence ratio passed to the flame speed correlation (φ instead of λ) | 1 | done (engineering model) |
| Enthalpy-carrying flows (blowdown cools, filling heats) | 2 | done |
| Species tracking (fuel, O₂, N₂, Ar, CO₂, H₂O, CO, H₂) with NASA-7 properties: variable c_p, γ and molar mass | 2 | done |
| Real air (20.95 % O₂ + Ar), metered λ, mass-based fuel definitions (formula, LHV, AFR_st) and a fuel library | 2 | done |
| Complete-combustion stoichiometry with water–gas shift for rich mixtures; heat release from LHV minus unburned CO/H₂; η_HC instead of the random burning efficiency | 2 | done |
| Laminar flame speed updated every step from the unburned state | 4 (early) | done, plus quasi-dimensional entrainment (ρᵤ·A_f·S_T) |
| Woschni in-cylinder heat transfer with phase constants and motored pressure from IVC | 3 | done |
| Patton–Nitschke–Heywood friction (rubbing + auxiliary; pumping is simulated) with Chen–Flynn and legacy options | 3 | done |
| Verification tests: blowdown, filling, isentropic compression, choked flow, constant-volume combustion temperature, friction and Woschni magnitudes | – | done (Woschni through the reference engine: 13–20 % of the fuel energy to the walls) |
| Engineering reference engine and documentation | 8 (early) | done (`docs/engineering.md`) |

Findings from Milestone A that feed the next milestones:

* The gas work (p·dV per step) and the force on the pistons are integrated
  at different points of a time step. At 10 kHz this loses up to about
  1 bar of FMEP at high speed (0.1–0.6 bar at 20 kHz). A work-consistent
  coupling belongs to Milestone D.
* Engines tuned for the legacy model rely on its very rich idle circuit;
  under the engineering model, engines with large valve overlap need more
  idle air (`docs/engineering.md`, "Converting an engine").
* The engineering model costs about 1.5× the legacy model per step.
* Dissociation is not modelled; peak temperatures are 100–300 K high at
  stoichiometric conditions.

## Milestone B — combustion model

Wiebe "imposed" burn mode and a Blizard–Keck quasi-dimensional mode with flame
area tables (§ "Wiebe first, then quasi-dimensional entrainment"); residual
dilution in S_L; flame kernel delay; per-cycle Gaussian variability; misfire
and partial burn; unburned-zone temperature with a Livengood–Wu /
Douaud–Eyzat knock integral; MBT spark finder.

## Milestone C — gas exchange

Forward and reverse discharge coefficients versus L/D, Heywood throttle area
with leakage and idle air, stagnation temperature in the orifice law,
quasi-1-D finite-volume pipes with HLLC fluxes for runners and primaries,
junction volumes, pipe friction and heat transfer, exhaust temperature
(§ "One-dimensional pipes are the largest remaining fidelity gap").
Turbo/supercharger maps follow.

## Milestone D — mechanics and drivetrain

Reduced-coordinate crank model with J(θ) and torque decomposition, bearing
and shaking loads, optional torsional chain; Baumgarte factor and solver
tolerance exposed; wheel slip with a Magic Formula tyre, road load
A + Bv + Cv², gearbox efficiency, speed-continuous gear changes
(§ "Reduced-coordinate mechanics make torque auditable").

## Milestone E — acoustics

Tailpipe and intake dṁ/dt monopole sources from the 1-D solver, correct
delay lines (real simulation rate, c(T), fractional samples), band-limited
resampling, transfer-matrix mufflers and Levine–Schwinger termination, an
"engineering" audio mode without automatic gain
(§ "Physical sound means radiating the tailpipe mass-flow derivative").

## Milestone F — validation

A reference engine built from published Toyota A25A-FKS data
(SAE 2019-01-0249, EPA benchmarking) with an automated comparison of torque,
VE and BSFC (target ±5 %), sensitivity studies, two-zone burned gas with
Zeldovich NO, and a diesel combustion path.

## Values that must be verified before release

The report marks several coefficients as recalled rather than checked
against the primary source ([R]): the PNH friction coefficients, the
Woschni temperature exponent (−0.53 vs −0.55), the Metghalchi–Keck and
Gülder flame speed tables, and the SAE J1349 / ISO 1585 correction formulas.
They are used as defaults but are labelled in the code and documentation.
