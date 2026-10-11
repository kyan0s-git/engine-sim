# Combustion models for a real-time IC engine cycle simulator (SI primary, CI secondary)

Scope note for the report writer: the browsing environment for this session blocked direct page fetches for nearly all publisher hosts (403 from the egress proxy; Scholar Gateway returned ACCESS_DENIED). Verification was therefore done through web-search result summaries plus a direct read of the local engine-sim source. Each item carries one of two tags. **[V]** means a search result in this session confirmed the value or form. **[R]** means the value is recalled from the cited primary paper or from Heywood (1988), the coefficients are standard and widely reproduced, but they were NOT text-verified in this session; check them against the original before relying on them. Heywood, *Internal Combustion Engine Fundamentals*, McGraw-Hill 1988, ISBN 0-07-028637-X, is abbreviated "Heywood"; link: [Heywood (WorldCat)](https://search.worldcat.org/search?q=bn:9780070286375).

Shared real-time budget used in the cost notes: 50 kHz × 12 cylinders = 6×10^5 cylinder-steps/s. Combustion is only active for about 60–100° of each 720° cycle, so roughly 1/8 of those steps. A model costing about 1 µs per active step therefore uses about 75 ms of CPU per wall-second, about 7.5% of one core.

---

## 1. Wiebe function (single/double): form, a & m, burn-duration scaling

### Takeaway
The Wiebe function x_b = 1 − exp[−a((θ−θ0)/Δθ)^(m+1)] with a≈5 (or 6.908) and m≈2 is the cheapest credible SI burn-rate model, at one `pow` and one `exp` per step. It has no predictive physics: Δθ and θ0 must come from correlations or from a cheap flame-speed estimate. The burn duration in crank angle grows only mildly with speed, because turbulence scales with piston speed, and dilution has the strongest effect on it.

### Cited Findings
- Form: x_b(θ) = 1 − exp{−a[(θ−θ0)/Δθ]^(m+1)}. The burn rate is dx_b/dθ = (a(m+1)/Δθ)·((θ−θ0)/Δθ)^m·(1−x_b). Heat release is dQ/dθ = η_comb·m_f·Q_LHV·dx_b/dθ. Heywood (Eq. 9.23) cites a = 5 and m = 2 as fitting SI data well [R] — [Heywood](https://search.worldcat.org/search?q=bn:9780070286375)
- "According to several references the values for a and m are 5 and 2"; "in most of the work the values of a is 5 or 6.908" [V]. a = 6.908 makes x_b = 0.999 at θ0+Δθ; a = 5 gives 0.9933 — [Unipa MFB paper](https://iris.unipa.it/retrieve/e3ad8928-cc78-da0e-e053-3705fe0a2b96/innovative_MFB_function-revised.pdf)
- Derived by direct computation in this session: for a = 5, m = 2, x_b = 10/50/90% occur at (θ−θ0)/Δθ = 0.276 / 0.518 / 0.772. So the 0–10% angle is 0.276Δθ, the 10–90% angle is 0.496Δθ, and CA50 is θ0 + 0.518Δθ. For a = 6.908 the same points are 0.248 / 0.465 / 0.693. This lets you build Δθ from a measured or estimated 10–90% duration: Δθ ≈ 2.0 × Δθ_10–90 for a = 5 (math on the cited form).
- Definitions (Heywood ch. 9) [R]: the flame-development angle Δθ_d runs from spark to 10% mass fraction burned (MFB); the rapid-burning angle Δθ_b runs from 10% to 90% MFB. Typical values at mid speed and load: Δθ_d ≈ 10–30° CA and Δθ_b ≈ 20–40° CA, with longer values at light load, high dilution and lean mixtures. Total 0–100% is about 50–90° CA — [Heywood](https://search.worldcat.org/search?q=bn:9780070286375)
- The 10–90% duration is "the crank angle interval required to burn the bulk of the charge" [V] — [US patent 8006656](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/8006656)
- Speed trend: in a methane SI engine, combustion takes longer on a crank-angle basis as speed rises, while the maximum burn rate "remains relatively unaffected" [V] — [Alam et al. 2021 (Depcik, KU)](https://depcik.ku.edu/sites/depcik/files/images/Alam2021.pdf)
- Wiebe fits on a VVT SI engine correlated the 0–90% burn angle and the form factor m against speed, dilution, spark timing and charge density. Dilution had the strongest influence on burn rate and profile [V] — [Burn angles and form factors for Wiebe fits, VVT SI engine (ResearchGate)](https://www.researchgate.net/publication/245395016_Burn_angles_and_form_factors_for_Wiebe_function_fits_to_mass_fraction_burned_curves_of_a_spark_ignition_engine_with_variable_valve_timing)
- Correlations for Wiebe parameters also exist for small two-stroke engines [V, title only] — [Correlations for Wiebe parameters, two-stroke (ResearchGate)](https://www.researchgate.net/publication/238509896_Correlations_for_Wiebe_function_parameters_for_combustion_simulation_in_two-stroke_small_engines)
- Double Wiebe (sum of a premixed and a diffusion Wiebe, weighted by β). This is the standard form for diesel (Watson, see §9), and it is also used for SI engines with a fast initial phase plus a slow wall-quench tail [V] — [Wiebe function review (ResearchGate)](https://www.researchgate.net/publication/235973540_Review_of_the_development_and_applications_of_the_Wiebe_function_A_tribute_to_the_contribution_of_Ivan_Wiebe_to_engine_research); [Single and double Wiebe, HD diesel converted to NG-SI (ScienceDirect)](https://www.sciencedirect.com/science/article/abs/pii/S0306261919307652)

### Inferences
- Speed scaling, from first principles: burn time t_b ≈ L_flame/S_T. With S_T ≈ C·u' and u' ∝ S̄p ∝ N, t_b ∝ 1/N, so the burn angle is constant in CA. With S_T = S_L + C·u', the S_L share makes the CA duration rise with N, most strongly in the 0–10% kernel phase, which is close to laminar. For a Wiebe-only simulator, a practical form is Δθ = Δθ_ref·(N/N_ref)^k·f_φ(φ)·f_dil(x_r+EGR)·f_load. Take k ≈ 0.2–0.5 for the 10–90% duration and slightly more for 0–10%, with f_φ = S_L(φ_ref)/S_L(φ) built from §3. **The exponent k is an engineering assumption, not a sourced value.**
- An even cheaper option that is physically grounded: compute S_L (§3) and u' (§2) once at spark, set Δθ_10–90 ∝ (6N)·B/S_T(u', S_L), and calibrate one constant per engine.
- Cost: about 50–100 ns per active step (one `pow` and one `exp`), negligible. A 1-D lookup table on the normalized angle removes the transcendentals entirely.

### Gaps
- The Csallner/Witt-type multiplicative burn-duration correlations (Δθ vs speed, load, AFR, spark timing, EGR) and their exponents could not be retrieved. Sources were blocked.
- No accessible source gave Heywood's quantitative Δθ_d and Δθ_b vs speed plots. The ranges above are [R].

---

## 2. Quasi-dimensional turbulent flame models: Blizard-Keck entrainment, fractal, turbulence sub-models

### Takeaway
Quasi-dimensional (QD) models predict the burn rate from flame geometry, u', the integral length L and S_L, with no burn-duration correlation. Two variants matter. Entrainment/burn-up (Blizard-Keck, Tabaczynski) adds one extra ODE per cylinder. Fractal (Gatowski/Heywood, Matthews-Chin, North-Santavicca D3) needs no extra ODE. Both cost about 0.3–1 µs per step if the flame area A_f(r_f, piston position) is precomputed as a 2-D table. That makes them viable for real time and the recommended upgrade over Wiebe.

### Cited Findings
- Blizard & Keck (SAE 740191) eddy-entrainment model, in the Tabaczynski form (Heywood §9.5) [R]:
  - dm_e/dt = ρ_u·A_f·(u' + S_L) (entrainment into the flame zone)
  - dm_b/dt = (m_e − m_b)/τ_b + ρ_u·A_f·S_L (burn-up of entrained eddies)
  - τ_b = λ_T/S_L, where λ_T is the Taylor microscale, λ_T/L ≈ (15/Re_L)^(1/2) and Re_L = u'L/ν
  - Turbulence after IVC follows rapid distortion with angular-momentum conservation: u' ∝ ρ_u^(1/3), L ∝ ρ_u^(−1/3)
  - Blizard & Keck tied the initial u' to the mean inlet gas velocity
  - Source: [Blizard & Keck SAE 740191](https://www.sae.org/publications/technical-papers/content/740191/); Tabaczynski, Ferguson & Radhakrishnan, SAE 770647 [R]
- Fractal model, general form [V for structure]: the burning rate is ρ_u·A_L·S_L·(ε_o/ε_i)^(D3−2), where ε_o and ε_i are the outer and inner cutoff scales and D3 is the fractal dimension. D3 is "expressed as a function of the density ratio and u'/S_L". The fractal dimension of premixed flames "varies between 2.1 and 2.7" (2 is the laminar limit) — [Fractal QD model for HCNG (ResearchGate)](https://www.researchgate.net/publication/257174262_A_fractal-based_quasi-dimension_combustion_model_for_SI_engines_fuelled_by_hydrogen_enriched_compressed_natural_gas)
- Yoshiyama et al. (SAE 2001-01-3603) [V]: D3 is a function of the non-dimensional turbulence intensity and mixture density. The inner cutoff is a function of the Karlovitz number, and the outer cutoff equals the flame radius — [SAE 2001-01-3603](https://saemobilus.sae.org/papers/measurement-simulation-turbulent-flame-propagation-a-spark-ignition-engine-using-fractal-burning-model-2001-01-3603)
- Wall treatment [V]: models switch from the fractal law to a wall-combustion law (exponential decay of the burn rate) once the flame-to-wall distance falls below about half the maximum length scale — [Fractal QD with flame-wall interaction (Fuel 2018)](https://www.sciencedirect.com/science/article/abs/pii/S0016236118315254)
- Commonly used D3 closure (North & Santavicca 1990, used in Bozza/Matthews-Chin type models) [R]: D3 = 2.35·u'/(u'+S_L) + 2.05·S_L/(u'+S_L). With L_max = L (integral scale) and L_min = Kolmogorov scale η ≈ L·Re_L^(−3/4) — [Matthews & Chin SAE 910079](https://www.sae.org/publications/technical-papers/content/910079/)
- K-k (mean-flow and turbulent kinetic energy) 0-D turbulence model (Poulos & Heywood SAE 830334; Heywood §14.4) [R]:
  - dK/dt = ½ṁ_in·v_in² − P − K·ṁ_out/m
  - dk/dt = P − m·ε − k·ṁ_out/m
  - P = 0.3307·c_β·(K/L)·√(k/m), ε = u'^3/L, u' = √(2k/(3m)), L ≈ min(clearance height, B/2), c_β ≈ 0.3–1 (tuning)
  - Source: [Poulos & Heywood SAE 830334](https://www.sae.org/publications/technical-papers/content/830334/)
- Algebraic shortcut used in Heywood [R]: for open chambers without swirl, the turbulence intensity at TDC is about u'_TDC ≈ 0.5·S̄p, where S̄p is the mean piston speed. The integral scale is about 0.2 × clearance height, roughly a few mm. With swirl or tumble, u' can reach (0.75–1)·S̄p — [Heywood](https://search.worldcat.org/search?q=bn:9780070286375)

### Inferences
- A real-time QD recipe that needs no CFD:
  1. Set u'(θ) = c_u·S̄p·(ρ_u/ρ_u,spark)^(1/3) with c_u ≈ 0.5 (tunable for tumble or swirl).
  2. Precompute A_f(r_f, h_clearance) per chamber geometry, plugging the spark location into a 32×32 table.
  3. Integrate r_f with dr_f/dt = (ρ_u/ρ_b)·S_T for the spherical expansion.
  4. Add the burn-up lag τ_b, or use the fractal factor.
  This adds about 2 ODE states per cylinder and about 5 transcendentals per step, under 1 µs.
- The K-k model adds 2 states per cylinder and needs the valve mass flows the simulator already computes. It captures tumble and port effects that the algebraic u' ∝ S̄p misses, at negligible cost.

### Gaps
- The exact constants of Blizard-Keck's original u' correlation (inlet-velocity proportionality and density exponent) could not be verified.
- Original texts of Matthews-Chin, North-Santavicca and Gatowski-Heywood could not be accessed. D3 = 2.35 is [R].

---

## 3. Laminar flame speed correlations (Metghalchi-Keck, Rhodes-Keck, Gülder; H2, ethanol)

### Takeaway
Use the Metghalchi-Keck power-law form, S_L = S_L0(φ)·(T_u/298)^α·(p/1 atm)^β·(1 − c·x_dil^d). For gasoline, use the Rhodes-Keck coefficients that engine-sim already contains, plus the residual term engine-sim omits. Cost is 3 `pow` per evaluation and it is valid for φ ≈ 0.8–1.5. Hydrogen needs a dedicated correlation (Verhelst).

### Cited Findings
- Metghalchi's 1977 MIT thesis measured S_L for propane, isooctane, methanol and indolene (a gasoline surrogate) at 0.4–40 atm, 298–750 K and φ = 0.8–1.5, and fitted power-law relations [V] — [Metghalchi 1977 thesis](https://james-keck-memorial-collection.unibs.it/ThesisAdvisor/Thesis-Metghalchi-1977.pdf). A 1980 thesis extends the work to isooctane, methane and methanol at high T and p [V] — [Metghalchi 1980 thesis](https://james-keck-memorial-collection.unibs.it/ThesisAdvisor/Thesis-Metghalchi-1980.pdf)
- "Most correlations use the following form to express the influence of equivalence ratio, pressure, temperature and residual gas content, as originally proposed by Metghalchi and Keck" [V] — [Verhelst et al., H2 S_L correlation, IJHE (ScienceDirect)](https://www.sciencedirect.com/science/article/abs/pii/S0360319910020938)
- Metghalchi & Keck (Combust. Flame 48:191, 1982) [R]:
  - S_L0 = B_m + B_φ(φ − φ_m)²
  - α = 2.18 − 0.8(φ − 1), β = −0.16 + 0.22(φ − 1)
  - Dilution factor (1 − 2.1·x_b)
  - Coefficients (Heywood Table 9.3, cm/s): methanol φ_m = 1.11, B_m = 36.9, B_φ = −140.5; propane 1.08, 34.2, −138.7; isooctane 1.13, 26.3, −84.7; indolene 1.13, 27.6, −78.3 [R, flag]
  - Source: [Metghalchi & Keck 1982, doi:10.1016/0010-2180(82)90127-4](https://doi.org/10.1016/0010-2180(82)90127-4)
- Rhodes & Keck (SAE 850047, gasoline/RMFD-303) [R for attribution; the numbers are confirmed as present in engine-sim]:
  - φ_m = 1.21, B_m = 30.5 cm/s, B_φ = −54.9 cm/s
  - α = 2.4 − 0.271·φ^3.51, β = −0.357 + 0.14·φ^2.77
  - Residual factor (1 − 2.06·x_dil^0.77) [R]
  - These exact numbers appear in engine-sim `src/fuel.cpp::laminarBurningVelocity` [V, local code] — [engine-sim fuel.cpp](https://github.com/ange-yaghi/engine-sim/blob/master/src/fuel.cpp); [Rhodes & Keck SAE 850047](https://www.sae.org/publications/technical-papers/content/850047/)
- Gülder (SAE 841000) provides correlations "suitable for cycle simulation" as functions of mixture strength, T_u, p and residual fraction, for ethanol, methanol, alcohol/water blends, isooctane/alcohol blends, propane and isooctane [V] — [Gülder SAE 841000](https://www.sae.org/publications/technical-papers/content/841000/)
- Gülder form [R]: S_L0 = Z·W·φ^η·exp[−ξ(φ − 1.075)²], scaled as (T_u/T0)^α·(p/p0)^β·(1 − 2.1·x_r). Commonly quoted coefficients (W in cm/s), **flagged as uncertain** because the table could not be retrieved:
  - isooctane: W = 46.58, η = −0.326, ξ = 4.48, α = 1.56, β = −0.22
  - ethanol: W = 46.50, η = 0.25, ξ = 6.34, α = 1.75, β = −0.17
  - methane: W = 42.2, η = 0.15, ξ = 5.18, α = 2.0, β = −0.5
  - propane: W = 44.6, η = 0.12, ξ = 4.95, α = 1.77, β = −0.20
  - A ResearchGate table headed "Coefficients for Gülder's exponential formulation for different fuels" confirms that W, η and ξ are fuel constants, but the values were not visible [V] — [Gülder coefficients table (ResearchGate)](https://www.researchgate.net/figure/Coefficients-for-Guelders-exponential-formulation-for-different-fuels_tbl1_318794643)
- Methanol: a temperature exponent α = 1.32 − 1.32(φ − 1) has been reported [V] — [Methanol-air S_L at elevated T (ResearchGate)](https://www.researchgate.net/publication/245159762_Laminar_burning_velocities_for_mixtures_of_methanol_and_air_at_elevated_temperatures). Dedicated methanol and ethanol engine-condition correlations also exist [V, title only] — [Methanol/ethanol S_L correlations valid at SI conditions (ResearchGate)](https://www.researchgate.net/publication/278390971_Laminar_Burning_Velocity_Correlations_for_Methanol-Air_and_Ethanol-Air_Mixtures_Valid_at_SI_Engine_Conditions)
- Gasoline at elevated pressure up to 3.0 MPa: an empirical correlation tied to RON exists [V] — [Gasoline S_L at elevated pressures (Combust. Flame 2015)](https://www.sciencedirect.com/science/article/abs/pii/S0010218015000073)
- Hydrogen: Verhelst & Sierens note that no older correlation covered the full T, p and composition range of SI engines, and that residual-gas data were missing. Their work targets H2/air/residual at engine conditions [V] — [Verhelst & Sierens FISITA 2004](https://go.fisita.com/store/papers/barcelona2004/F2004V004-paper); later correlation: [Verhelst et al. IJHE 2011](https://www.sciencedirect.com/science/article/abs/pii/S0360319910020938)
- H2 blends: Ji et al. built correlations for hydrogen-enriched gasoline (φ 0.6–1.5, residual 0–20%) and hydrogen-enriched ethanol [V] — [BJUT record](https://ir.bjut.edu.cn/item/13068)
- Sensitivity: the choice of S_L correlation measurably changes QD model output [V, title] — [Impact of the laminar flame speed correlation on QD SI model results (Energy Procedia 2018)](https://www.sciencedirect.com/science/article/pii/S1876610218304466)

### Inferences
- Worked numbers, from the Rhodes-Keck coefficients above:
  - stoichiometric, 298 K, 1 atm: S_L0 = 30.5 − 54.9·(0.21)² ≈ 28.1 cm/s
  - at a typical spark condition (≈ 600 K, 8 atm, φ = 1): α ≈ 2.13, β ≈ −0.217, so S_L ≈ 28.1·(2.01)^2.13·8^(−0.217) ≈ 0.79 m/s
  - at TDC (700 K, 15 atm): S_L ≈ 0.96 m/s
  - with 10% residual: ×(1 − 2.06·0.1^0.77) ≈ ×0.65
- The φ-polynomial goes negative outside about φ ∈ [0.5, 1.9], so clamp S_L ≥ 0 and treat φ outside 0.7–1.6 as near-misfire (§8).
- Cost is 3 `pow` (about 60–100 ns). Calling it every step for the unburned zone is fine.
- Hydrogen S_L is roughly 2–3 m/s near stoichiometric at ambient conditions, about 7–10× gasoline [R], and it is strongly affected by thermo-diffusive instability. A Metghalchi-Keck form fitted to H2 underpredicts lean flames unless a Markstein or instability correction is added.

### Gaps
- The verified numeric tables of Metghalchi-Keck 1982 and Gülder 1984 were not retrievable. Treat the listed numbers as [R] and verify against Heywood Table 9.3 or the SAE paper.
- Verhelst's H2 correlation coefficients and the Gülder ethanol and methanol coefficients could not be retrieved.

---

## 4. Turbulent flame speed relations

### Takeaway
For engine QD models, simple forms suffice: S_T/S_L = 1 + C·(u'/S_L)^n with n ≈ 0.5–1, or Damköhler's linear form S_T = S_L + C·u'. Peters', Zimont's and Gülder's correlations converge on similar values for IC-engine regimes. All cost a few flops.

### Cited Findings
- A comparison study found that the Peters, Liu, Zimont, Kobayashi and Ronney correlations "converged on similar results" for turbulence-flame interactions relevant to ICEs [V] — [Comparison of turbulent flame speed correlations at elevated pressure (ResearchGate)](https://www.researchgate.net/publication/309359406_A_Comparison_of_Turbulent_Flame_Speed_Correlations_for_Hydrocarbon_Fuels_at_Elevated_Pressures)
- A Zimont-type closure modified for pressure and low turbulence gave S_T ∝ u'^0.55, similar to Bradley's data fits [V]. Peters' correlation is a widely used reference [V] — [Data-driven turbulent flame speed across engine regimes (Energies 2021)](https://www.mdpi.com/1996-1073/14/14/4210)
- At high turbulence, S_T/S_L scales roughly as (u'/S_L)^0.5 for H2-air [V] — [Turbulent burning velocity of H2-air at elevated pressure (ResearchGate)](https://www.researchgate.net/publication/222128548_Turbulent_burning_velocity_of_hydrogen-air_premixed_propagating_flames_at_elevated_pressures)
- Gülder's scaling is fractal-wrinkling based and emphasizes small-scale turbulence for u'/S_L > 7 [V] — [same comparison study](https://www.researchgate.net/publication/309359406_A_Comparison_of_Turbulent_Flame_Speed_Correlations_for_Hydrocarbon_Fuels_at_Elevated_Pressures)
- Standard forms [R]:
  - Damköhler (large scale): S_T/S_L = 1 + u'/S_L
  - Zimont (1979): S_T = A·u'^(3/4)·S_L^(1/2)·a_u^(−1/4)·L^(1/4) = A·u'·Da^(1/4), with A ≈ 0.52 and a_u the unburned thermal diffusivity
  - Gülder (1990): S_T/S_L = 1 + 0.62·(u'/S_L)^(1/2)·Re_η^(1/4)
  - Peters (1999), thin reaction zones: S_T/S_L = 1 − (a4·b3²/2b1)(L/l_F) + {[(a4·b3²/2b1)(L/l_F)]² + a4·b3²·u'L/(S_L·l_F)}^(1/2), with a4 = 0.78, b1 = 2.0, b3 = 1.0 and l_F = a_u/S_L
  - Source: [Peters 1999, J. Fluid Mech. 384, doi:10.1017/S0022112098004212](https://doi.org/10.1017/S0022112098004212)

### Inferences
- For a real-time simulator, use S_T = S_L·[1 + C_T·(u'/S_L)^n] with n ≈ 0.7 and C_T ≈ 1.5–2.5, calibrated so the 10–90% duration lands at 20–40° CA. Alternatively use Zimont with A ≈ 0.52. Both cost one `pow`.
- Typical engine values: u' ≈ 2–8 m/s and S_L ≈ 0.4–1 m/s give u'/S_L ≈ 3–15. Correlations give S_T/S_L ≈ 3–10, so S_T ≈ 3–8 m/s. In the expanding spherical flame, the burned-gas expansion multiplies the lab-frame front speed by about ρ_u/ρ_b ≈ 4–7 early in the burn.

### Gaps
- None of the original Zimont, Gülder 1990 or Peters equations could be text-verified in this session. The constants above are [R].

---

## 5. Spark ignition: kernel, ignition delay, MIE, MBT timing

### Takeaway
Model the spark-to-1% period as a kernel delay, a few degrees to about 15° CA that grows at low speed, lean, dilute or cold conditions, followed by flame propagation. At MBT, CA50 sits at about 8–10° ATDC and peak pressure at about 15–16° ATDC. That gives a cheap closed-loop "MBT finder" for an auto-calibrated spark map.

### Cited Findings
- MBT rules [V]: "a peak cylinder pressure usually occurs around 15° ATDC, and the 50 percent MFB location generally occurs from 8° to 10° ATDC". The maximum acceleration of MFB sits at about TDC. Controllers target a burn center around 10° ATDC. These are empirical observations that need per-engine calibration — [US7290442 (MBT from ionization)](https://patents.google.com/patent/US7290442); [US4867123 (adaptive spark timing)](https://patents.google.com/patent/US4867123)
- Kernel development [R]: Herweg & Maly (SAE 922243) give a physically based kernel model covering spark energy transfer, stretch and the transition from laminar to turbulent, with early growth near laminar speed. Heywood ch. 9 shows the flame-development angle is sensitive to mixture state at the plug and to near-plug flow — [Herweg & Maly SAE 922243](https://www.sae.org/publications/technical-papers/content/922243/)
- Minimum ignition energy (MIE) [R, Lewis & von Elbe]: about 0.25–0.3 mJ for stoichiometric propane- or methane-air at 1 atm, rising steeply toward the lean and rich limits. Hydrogen-air is about 0.017–0.02 mJ. Production inductive systems deliver tens of mJ (about 30–100 mJ stored), so energy rarely limits ignition except at very lean or dilute conditions — [Heywood](https://search.worldcat.org/search?q=bn:9780070286375)

### Inferences
- Cheap kernel model: θ_kernel = C_k·(6N)·δ_k/S_L(p, T, φ, x_r) with δ_k ≈ 1–2 mm, or equivalently ignition-to-1% time ≈ δ_k/S_L. Below u'/S_L ≈ 1 the delay balloons, which naturally gives misfire and late burns at lean or dilute idle.
- Spark auto-map: in a real-time simulator, MBT can be computed per operating point by iterating the spark until CA50 ≈ 9° ATDC. The knock-limited spark is the earliest spark whose Livengood-Wu integral (§6) stays below 1 with margin.
- Typical calibrated advance, from practice (not sourced): idle 5–15° BTDC; part load at 2000–4000 rpm, 30–45° BTDC; WOT 15–30° BTDC, usually knock-limited at low speed. For comparison, engine-sim's LFA V10 asset uses 12° at 0 rpm ramping to 40° from 4000 rpm [V, local file `assets/engines/atg-video-2/10_lfa_v10.mr`].

### Gaps
- No accessible source gave quantitative MBT tables vs speed and load. The ranges above are practitioner-level, not cited.
- The Herweg-Maly equations were not retrieved.

---

## 6. Knock: Livengood-Wu, Douaud-Eyzat, intensity, acoustics (Draper)

### Takeaway
The standard real-time knock predictor integrates 1/τ along the unburned-zone (p, T_u) history and declares autoignition when the integral reaches 1 (Livengood-Wu, 1955). τ comes from Douaud-Eyzat: τ = 17.68 ms·(ON/100)^3.402·p^(−1.7)·exp(3800/T_u), with p in atm and T in K. The cost is one `exp` and one `pow` per step. The acoustic signature is a set of cylinder-cavity modes, f_mn = ρ_mn·c/(πB), at about 5–18 kHz. These lie above the 10–25 kHz Nyquist of a 10–50 kHz physics step, so they must be synthesized in the audio domain.

### Cited Findings
- Livengood-Wu [V]: the knock criterion is ∫_{t_IVC or spark}^{t_knock} dt/τ(p, T_u) = 1. Ignition delay is often written τ = τ0·f(T)·(p/p0)^(−n), with n ≈ 1.5 for primary reference fuels (PRF) and about 1 or less for non-paraffins — [Princeton CEFRC slides](https://cefrc.princeton.edu/document/420). If unburned p and T are known, the integral can be evaluated vs crank angle — [KAUST, "Knock prediction using a simple model for ignition delay" (SAE 2016-01-0702)](https://cpc.kaust.edu.sa/publications/detail/pub-2016-kpu). A transported L-W integral handles single- and two-stage ignition at about 10× lower cost than detailed chemistry — [ORNL](https://www.ornl.gov/publication/transported-livengood-wu-integral-model-knock-prediction-computational-fluid-dynamics). Original: Livengood & Wu, 5th Symp. (Int.) Combust., 1955 [R]
- Douaud-Eyzat (SAE 780080) [V]: τ = C1·(ON/100)^C2·p^(−C3)·exp(C4/T), with C1 = 17.69 [ms], C2 = 3.402, C3 = 1.7, C4 = 3800 K — [Predictive knock-onset model with cooled EGR (Energy Convers. Manag. 2015)](https://www.sciencedirect.com/science/article/abs/pii/S0196890414007262). The same correlation appears as 0.01869 s, an octane exponent of 3.4017, T_A = 3800 K, p_ref = 1×10^5 Pa and a pressure exponent of 1.7 — [Energy & Fuels 2025](https://pubs.acs.org/doi/10.1021/acs.energyfuels.4c04775). Coefficients were fitted to primary reference fuels — [Energies 12(5):844](https://www.mdpi.com/1996-1073/12/5/844). It is a built-in Fluent knock option — [ANSYS Fluent 12 theory guide](https://www.afs.enea.it/project/neptunius/docs/fluent/html/th/node203.htm). **Discrepancy:** 17.68 (Heywood, [R]) vs 17.69 vs 18.69 (after unit and pressure-reference conversion). Validity is quoted as 80 < ON < 100 (forum-level source, weak).
- Limitation [V]: Douaud-Eyzat fails to predict knock onset as cooled EGR increases. The cited study adds the ratio of specific heats as a variable — [same ScienceDirect paper](https://www.sciencedirect.com/science/article/abs/pii/S0196890414007262)
- Draper modes [V]: f = ρ_mn·c/(πB), where ρ_mn is a Bessel-root vibration-mode constant, c is the sound speed (burned gas) and B is the bore — [US 10364760](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10364760). The full cavity form adds axial modes, f = (c/π)·√((ρ_mn/B)² + (pπ/L)²), and knock peaks fall at 6–20 kHz depending on the mode — [Knock characterization, turbocharged SI (Energy Convers. Manag. 2014)](https://www.sciencedirect.com/science/article/abs/pii/S0196890414004658)
- Mode constants for a rigid wall are the zeros of J_m′. These are standard mathematical constants [V via the search note, math identity]: ρ_10 = 1.841 (first circumferential), ρ_20 = 3.054, ρ_01 = 3.832 (first radial), ρ_30 = 4.201, ρ_11 = 5.331.
- Computed in this session with c = 950 m/s: B = 86 mm gives f_10/f_20/f_01/f_30 ≈ 6.5/10.7/13.5/14.8 kHz; B = 100 mm gives 5.6/9.2/11.6/12.7 kHz; B = 70 mm gives 8.0/13.2/16.6/18.1 kHz (math on the cited formula).

### Inferences
- Implementation, per cylinder:
  1. Track the unburned zone with isentropic compression: T_u = T_u,ref·(p/p_ref)^((γ_u−1)/γ_u), with γ_u ≈ 1.32–1.35.
  2. Accumulate I += dt/τ(p, T_u, ON) from IVC or spark. Each step costs one `exp` and one `pow`; a 2-D lookup table on (ln p, 1/T) removes them.
  3. When I ≥ 1 with x_b < x_knock,max: release a fraction of the remaining unburned mass over about 0.1–0.3 ms. That gives a pressure spike, plus heat-transfer augmentation if modeled.
  4. Set knock intensity (KI) ∝ (1 − x_b at onset)·m_charge·Q_LHV/V_cyl as a proxy for MAPO (maximum amplitude of pressure oscillation).
  5. Feed KI into the audio synth as 2–4 damped sinusoids at f_mn. Use amplitudes of about 1 : 0.5 : 0.3 for modes 10 : 20 : 01, and decay time constants of about 0.5–2 ms.

  These amplitude ratios and decay constants are **unsourced engineering choices**. Calibrate them by ear or against published knock spectra.
- Because 6–18 kHz exceeds the Nyquist frequency of a 10 kHz physics step (engine-sim assets typically use `simulation_frequency` 10000), knock "ping" cannot come from the physics pressure trace. It must be injected at the audio sample rate.
- ON dependence: from τ ∝ (ON/100)^3.4, going from RON 91 to 98 raises τ by about 29%, which lets you advance spark by a few degrees. This is consistent in sign with practice, but not calibrated.

### Gaps
- No accessible source gave a quantitative mapping from Livengood-Wu or Douaud-Eyzat to MAPO, or typical MAPO thresholds (e.g. light vs heavy knock in bar).
- Knock-mode damping rates were not found.
- The original Livengood-Wu (1955) and Douaud-Eyzat (1978) papers were not accessible. The constants are confirmed only via secondary sources.

---

## 7. Cycle-to-cycle variability (CCV)

### Takeaway
CCV mainly comes from variation in the early flame kernel (local flow, turbulence, mixture and residual at the plug), so it shows up chiefly as variation in combustion phasing and burn rate, not in total fuel burned. COV(IMEP) under about 2–3% is normal, and about 10% is the drivability limit (Heywood). Cheap stochastic models perturb kernel or turbulence parameters per cycle, plus residual feedback.

### Cited Findings
- The drivability threshold of COV_IMEP ≈ 10% is attributed to Heywood. Beyond it the engine becomes "unstable, jittery and even misfired", and instability can occur below 10% depending on the engine [V] — [IOP Conf. Ser. MSE 1303 012017](https://iopscience.iop.org/article/10.1088/1757-899X/1303/1/012017/pdf)
- A 2% misfire rate is enough to push emissions over the OBD-II threshold. One study set a 0.2% misfire limit to keep COV_IMEP < 10% [V] — [Cycle-to-cycle feedback control of spark advance at the misfire limit (ResearchGate)](https://www.researchgate.net/publication/323975929_Cycle-to-Cycle_Feedback_for_Combustion_Control_of_Spark_Advance_at_the_Misfire_Limit)
- Mechanisms and influencing factors are reviewed in [Duan et al. 2024, Energy Sci. Eng.](https://scijournals.onlinelibrary.wiley.com/doi/10.1002/ese3.1879) [V, title]. The machine-learning analysis of the early flame kernel ties CCV to kernel development [V, title] — [Proc. Combust. Inst. (ScienceDirect)](https://www.sciencedirect.com/science/article/abs/pii/S1540748920300778). Correlations between CCV and combustion parameters are also reported [V, title] — [Applied Thermal Eng. 2004](https://www.sciencedirect.com/science/article/abs/pii/S1359431104000614)
- CFD-based CCV estimation needed more than 500 cycles to converge COV [V] — [Energies 12:4131](https://doi.org/10.3390/en12214131)
- QD CCV simulation for gasoline-H2 blends perturbs the inputs per cycle. CCV decreased with H2 addition for lean operation (φ < 0.85) [V] — [IJHE 2016 (ScienceDirect)](https://www.sciencedirect.com/science/article/abs/pii/S0360319916302282)
- Residual-coupled deterministic and stochastic maps reproduce lean-limit CCV patterns, including the prior-cycle effect [R] — Daw et al. 1996, "A simple model for cyclic variations in a spark-ignition engine", SAE 962086 — [SAE 962086](https://www.sae.org/publications/technical-papers/content/962086/) (paper number [R], not verified)

### Inferences
- Recommended real-time CCV model: each cycle, draw ξ ~ N(0, 1) (Box-Muller or a precomputed table) and scale the kernel delay and/or u' by (1 + σ·ξ), with σ ≈ 0.05–0.15.
  - Make σ grow with low u'/S_L, high dilution and lean φ, for example σ = σ0·(S_L,ref/S_L)^k.
  - Add residual feedback: incomplete burn in cycle i raises fuel and lowers temperature in cycle i+1 (Daw-type map). Most simulators already carry residual mass, so this falls out of the gas exchange model.
  - Target COV_IMEP of about 1–3% at mid load and 5–10% at idle or high EGR.
- Cost: one RNG draw per cycle per cylinder, negligible.

### Gaps
- No primary-source numbers were found for typical σ of CA50 (degrees) vs operating point, or for the 2–3% "good" COV figure. That figure is practitioner-level, [R] from Heywood.

---

## 8. Lean/rich limits, misfire, partial burn, flammability vs φ and dilution

### Takeaway
Partial burn happens when the flame is too slow to finish before expansion quenches it, as T_u falls, S_L drops and the flame reaches the wall late. Misfire happens when the kernel fails. A QD model with S_L(φ, x_dil) and a quench criterion produces both automatically. Without one, a simple threshold on S_L (or on Δθ_burn vs the time available) works.

### Cited Findings
- CCV and its link to misfire at the limit: see §7 [V] — [ResearchGate misfire-limit paper](https://www.researchgate.net/publication/323975929_Cycle-to-Cycle_Feedback_for_Combustion_Control_of_Spark_Advance_at_the_Misfire_Limit)
- Engine burn angles are most sensitive to dilution [V] — [VVT Wiebe-fit study](https://www.researchgate.net/publication/245395016_Burn_angles_and_form_factors_for_Wiebe_function_fits_to_mass_fraction_burned_curves_of_a_spark_ignition_engine_with_variable_valve_timing)
- Typical limits [R, Heywood ch. 9 & 4]:
  - conventional gasoline SI lean operating limit about φ ≈ 0.65–0.75 (λ ≈ 1.3–1.5), extended with high tumble or strong ignition
  - rich limit for stable operation about φ ≈ 1.5–1.7
  - dilution (EGR + residual) tolerance about 15–25% before COV rises sharply
  - gasoline-air flammability at ambient conditions about 1.4–7.6 vol% fuel
  - H2-air 4–75 vol%, i.e. lean φ ≈ 0.1
  - Source: [Heywood](https://search.worldcat.org/search?q=bn:9780070286375)

### Inferences
- Cheap partial-burn and misfire logic:
  - **Misfire:** if S_L at spark < S_L,min (about 0.1–0.15 m/s at engine conditions), or the kernel delay exceeds the time to about 40° ATDC, set the burn to 0.
  - **Partial burn:** if the flame is still burning when T_u drops so far that S_L(T_u, p) < S_L,quench, or past EVO, freeze x_b. The unburned fuel exits as HC, which also makes "afterfire" and pops possible in the exhaust model.
- Combustion efficiency at stoichiometric and lean conditions is about 95–98%. On the rich side it drops roughly as 1/φ because of the O2 deficit [R, Heywood Fig. 3-9]. This should be set by chemistry (an O2-limited burn), not by a fixed "max burning efficiency".

### Gaps
- No primary-source values were retrieved for engine lean limits vs tumble or ignition energy.

---

## 9. Diesel (CI): ignition delay (Hardenberg-Hase), Watson double-Wiebe, injection rate

### Takeaway
For a real-time CI model: compute the ignition delay with Hardenberg-Hase (one `exp` and one `pow`), then apply the Watson double Wiebe. The premixed fraction β falls as the delay shortens and the load rises. Fuel injection rate comes from a simple orifice equation. Everything costs well under 1 µs per step.

### Cited Findings
- Hardenberg & Hase (SAE 790493) [V for structure and constants via secondary sources]:
  - τ_id[°CA] = (0.36 + 0.22·S̄p)·exp[E_A·(1/(R̃·T) − 1/17190)·(21.2/(p − 12.4))^0.63]
  - E_A = 618840/(CN + 25) J/mol, R̃ = 8.3143 J/mol·K, p in bar, T in K at TDC or SOI. The conversion is τ[ms] = τ[°CA]/(0.006·N)
  - It was developed for DI diesel, does not work for low-temperature combustion, and has limited predictive accuracy
  - A variant uses (22.1/p)^0.62 instead
  - Sources: [OSTI, Ignition delay in LTC](https://www.osti.gov/servlets/purl/1462880); [PMC4897468](https://pmc.ncbi.nlm.nih.gov/articles/PMC4897468); [Biofuel ignition delay methods (ScienceDirect)](https://www.sciencedirect.com/science/article/abs/pii/S1743967118300503)
- Watson, Pilley & Marzouk (SAE 800029) [V for structure; c_p2 = 5000 confirmed]:
  - x(τ) = β·f1 + (1 − β)·f2, with β the premixed fraction
  - f1 = 1 − (1 − τ^K_p1)^K_p2, f2 = 1 − exp(−K_d1·τ^K_d2), with τ the normalized time from start of combustion
  - The premixed-phase shape constant K_p2 is 5000 — [Wiebe review](https://www.researchgate.net/publication/235973540_Review_of_the_development_and_applications_of_the_Wiebe_function_A_tribute_to_the_contribution_of_Ivan_Wiebe_to_engine_research); [Diesel HiL combustion model (ResearchGate)](https://www.researchgate.net/publication/270984305_Diesel_engine_combustion_modeling_for_hardware_in_the_loop_applications_Effects_of_ignition_delay_time_model)
- Watson correlations as commonly quoted [R, flag]:
  - β = 1 − 0.926·φ^0.37/τ_id^0.26, with τ_id in ms
  - K_p1 = 2 + 1.25×10^(−8)·(τ_id·N)^2.4
  - K_d1 = 14.2/φ^0.644, K_d2 = 0.79·K_d1^0.25
  - τ normalized over about 125° CA
  - Source: [Watson et al. SAE 800029](https://www.sae.org/publications/technical-papers/content/800029/)
- Double- and multi-Wiebe extensions exist for multi-injection, RCCI, DME and H2-assisted CI [V, titles] — [ACS Omega multi-Wiebe](https://pubs.acs.org/doi/10.1021/acsomega.1c06858); [ASME double-Wiebe RCCI](https://asmedigitalcollection.asme.org/energyresources/article/144/11/112301/1137859/A-Double-Wiebe-Function-for-Reactivity-Controlled); [MDPI 18(21):5622](https://www.mdpi.com/1996-1073/18/21/5622)
- Injection rate [R, Heywood ch. 10]: ṁ_inj = C_d·A_n·√(2ρ_f·Δp), with C_d ≈ 0.6–0.8. Typical diesel ignition delay is about 0.4–1 ms at warm, loaded conditions — [Heywood](https://search.worldcat.org/search?q=bn:9780070286375)

### Inferences
- Simplest real-time CI: an injection schedule from a load map, then a Hardenberg-Hase delay, then Watson β, then a double Wiebe whose diffusion phase is stretched to the injection duration. A mixing-controlled (Chmela-type) model, dQ/dt = C·(m_f,avail)·√k/V^(1/3), is a next-step option at similar cost (not researched here).
- Diesel "knock" is the premixed-burn pressure-rise rate. It can drive the same audio synth (§6) via dp/dθ.

### Gaps
- The original SAE 800029 β and K constants could not be text-verified.
- The Chmela/Pischinger mixing-controlled combustion formulas were not researched.

---

## 10. Pollutant formation (cheap formulations)

### Takeaway
Thermal NO via the extended Zeldovich mechanism needs one ODE in the burned zone and three Arrhenius rates. CO is close to a frozen-equilibrium function of φ. HC can be approximated as crevice-stored plus unburned (partial-burn) fuel. All are cheap enough for real time if updated at a reduced rate (e.g. every 4th step).

### Cited Findings
- The extended Zeldovich mechanism (N2 + O ⇌ NO + N; N + O2 ⇌ NO + O; N + OH ⇌ NO + H) was formulated by Lavoie, Keck and Heywood (1970). The third reaction matters under rich conditions [V]. **Note:** Wikipedia's rate fits (k1f = 1.47×10^13·T^0.3·exp(−75286.81/RT) [cal/mol], k2f = 6.40×10^9·T·exp(−6285.5/RT), k3f = 3.80×10^13) differ from Heywood's, and Wikipedia itself flags unit problems — [Wikipedia: Zeldovich mechanism](https://en.wikipedia.org/wiki/Zeldovich_mechanism); [MIT thesis on NO formation in SI engines](https://dspace.mit.edu/bitstream/handle/1721.1/89261/45594168-MIT.pdf?sequence=2&isAllowed=y)
- Heywood ch. 11 values [R, flag]:
  - k1+ = 7.6×10^13·exp(−38000/T)
  - k2+ = 6.4×10^9·T·exp(−3150/T)
  - k3+ = 4.1×10^13 (cm³/mol·s)
  - initial formation rate d[NO]/dt = (6×10^16/T^(1/2))·exp(−69090/T)·[O2]_e^(1/2)·[N2]_e (mol/cm³·s)
  - characteristic time τ_NO = 8×10^(−16)·T·exp(58300/T)/p^(1/2) (s, p in atm)
  - Source: [Heywood](https://search.worldcat.org/search?q=bn:9780070286375)
- HC sources [R]: Cheng et al. (SAE 932708) apportion SI engine-out HC roughly as crevices ≈ 38%, liquid fuel ≈ 20%, oil layers ≈ 16%, deposits ≈ 16%, flame quench ≈ 5% and valve leakage ≈ 5% — [SAE 932708](https://www.sae.org/publications/technical-papers/content/932708/)

### Inferences
- Real-time NO: solve d[NO]/dt with the full forward/backward Zeldovich expression, using equilibrium [O], [O2], [N2] and [OH] from a temperature lookup table in the hottest burned zone. Because of exp(−69090/T), even a 1-zone model underpredicts NO. A 2-zone model (burned/unburned) with an adiabatic core is the minimum.
- CO: use frozen equilibrium at T_freeze ≈ 1700–1800 K, or, for φ > 1, an O2-deficit carbon balance with water-gas shift K ≈ 3.5. The lookup is φ → x_CO.
- HC: crevice mass ≈ V_crevice·p_peak/(R·T_wall) per cycle, released on expansion, plus any partial-burn unburned fuel the model already tracks.

### Gaps
- Heywood's NO rate constants could not be text-verified, and the published fits disagree (see Wikipedia vs Heywood).
- Cheng et al. percentages are [R].

---

## 11. How engine-sim models combustion, compared with the literature

### Takeaway
engine-sim uses a single-zone, geometric "cylindrical flame" model. The flame speed is computed once at spark: S_T = f(u'/S_L)·S_L with u' = 0.5·S̄p, a Rhodes-Keck S_L, and a default f(x) ≈ 1.5x. A random "burning efficiency" scales how much of the swept charge actually reacts. The turbulence premise matches Heywood (u'_TDC ≈ 0.5·S̄p). Main departures from the literature:
- the equivalence ratio passed to S_L is inverted (λ used as φ)
- the flame speed is frozen at spark and ignores the large rise in T_u during the burn
- residual dilution is absent from S_L
- CCV is modeled as a variable fraction of fuel that never burns, not as phasing variation
- there is no ignition delay, knock or quench physics

### Cited Findings (local code, engine-sim at commit f3e9a0a; upstream [ange-yaghi/engine-sim](https://github.com/ange-yaghi/engine-sim))
- **Turbulence.** `scripting/include/engine_node.h` builds `meanPistonSpeedToTurbulence` as a 30-sample function with u' = 0.5·S̄p for S̄p = 0..29 m/s. `calculateMeanPistonSpeed()` averages |v_piston| over a 256-bin per-cycle buffer, which gives the true cycle-mean piston speed. **Literature:** this matches Heywood's u'_TDC ≈ 0.5·S̄p [R].
- **Flame speed.** `Fuel::flameSpeed` returns `turbulenceToFlameSpeedRatio(u'/S_L)·S_L`. The default ratio function in `es/objects/objects.mr` is f(0) = 3, then f(x) = 1.5x for x = 5..45. Some assets override it: `07_gm_ls.mr` uses 1.5x→1.75x→2.0x, and overrides also exist in the EJ25 EH/UH, F136 and 412 T2 assets. So S_T ≈ max(3·S_L, 1.5·u') = 0.75·S̄p at typical u'/S_L. **Literature:** this is a Damköhler-like linear S_T ∝ u' with no S_L additive term (§4). Plausible magnitude, but no density or scale dependence.
- **Laminar speed.** `Fuel::laminarBurningVelocity` hard-codes the Rhodes-Keck gasoline constants (φ_m = 1.21, B_m = 30.5 cm/s, B_φ = −54.9 cm/s, α = 2.4 − 0.271·er^3.51, β = −0.357 + 0.14·er^2.77) and has no residual dilution term. **Literature:** Rhodes-Keck includes (1 − 2.06·x_dil^0.77) [R]. For every fuel the code assumes gasoline ("Assuming fuel is gasoline" comment), even when an asset defines another fuel.
- **φ/λ inversion (likely bug).**
  - In `CombustionChamber::ignite()`, `afr = p_o2/p_fuel` (O2:fuel mole ratio) and `equivalenceRatio = afr / molecularAfr`, where molecularAfr = 12.5, the stoichiometric O2/C8H18 mole ratio. That quantity is λ (>1 is lean), not φ.
  - The same λ is passed into `laminarBurningVelocity` as `er`, and the correlation expects φ.
  - Consequence, worked from the code formula: λ = 1.2 (lean, true φ = 0.83) gives S_L0 = 30.5 cm/s, the peak, where the correct value is ≈ 22.6 cm/s. λ = 0.85 (rich, φ = 1.18) gives 23.4 cm/s where the correct value is ≈ 30.4.
  - So lean mixtures burn fastest in engine-sim, the opposite of the literature, where the peak is at φ ≈ 1.1–1.2.
  - The ignitability window in `ignite()` (λ ∈ [0.5, 1.9]) is likewise expressed in λ.
- **Frozen flame speed.** S_T is computed once in `ignite()` from `m_system.temperature()` and `pressure()` at spark time, and stays constant during the burn. **Literature:** T_u rises by roughly 100–200 K during the burn because of compression by the burned gas. S_L ∝ T^~2.1 therefore grows substantially, and QD models update S_L, u' and A_f every step (§2, §3).
- **Geometry.** In `CombustionChamber::flow()`, a cylinder of burned gas grows radially (travel_x up to B/2) and axially (travel_y up to V/A_bore, rescaled by volume expansion), both at S_T. Burned volume = π·r²·h, and each step reacts (ΔV_burned/V)·n·efficiency moles. **Literature:** QD models use a spherical front from the plug, clipped by the chamber, with entrainment (§2). The engine-sim front starts on the cylinder axis, its axial extent saturates within a fraction of a millisecond near TDC, and x_b ∝ (r/R)² gives a Wiebe-like S-curve with m ≈ 1 and no slow wall-quench tail.
- **Burn efficiency and CCV.** In `ignite()`:
  - `mixingFactor = 1 − clamp(u'/maxTurbulenceEffect)·clamp(1 − dilution/maxDilutionEffect)`, with defaults maxTurbulenceEffect = 2 m/s and maxDilutionEffect = 10
  - `rand_s = lowEfficiencyAttenuation·((1 − randomness) + randomness·U[0,1])`, with defaults 0.6 and 0.5, using C `rand()` (uniform)
  - `efficiency = (mixingFactor·rand_s + 1 − mixingFactor)·maxBurningEfficiency`, with default 0.8 and per-asset values of 0.75–1.0
  - So randomness is active only when u' < 2 m/s (S̄p < 4 m/s, i.e. idle or low rpm) or at high dilution. Above that, the efficiency is a deterministic 0.8 by default.
  - **Literature:** CCV is primarily kernel-driven variation in phasing and burn rate (§7), and real combustion efficiency is about 95–98% at stoichiometric (§8). engine-sim's 20% default "unburned" fraction likely compensates for missing heat-transfer and other losses. Its CCV qualitatively matches the literature trend (worse at idle and with dilution) but uses the wrong mechanism (amount burned, not timing). The uniform distribution is not Gaussian, and `rand()` is not thread-safe or reproducible.
- **Termination.** The flame is extinguished if any intake flow occurs while lit (valve overlap). Otherwise it ends when both travel limits are reached. There is no expansion quench, no knock, no ignition delay and no pollutant model.

### Inferences
- **Burn duration check** (rough calculation using the default curve, 86 mm bore and stroke, 3000 rpm):
  - S̄p = 8.6 m/s, so u' = 4.3 m/s
  - S_L at spark (≈ 600 K, 8 atm, λ = 1) ≈ 0.8–0.95 m/s, so u'/S_L ≈ 4.5–5.4, f ≈ 7–8 and S_T ≈ 6.5–7 m/s
  - Radial travel of 43 mm takes about 6.3 ms ≈ 113° CA
  - With x_b ≈ (r/R)², the 10%/50%/90% points come at about 36°/80°/108° CA after spark, so the 10–90% duration ≈ 72° CA
  - That is about 2× the literature's typical 20–40° CA (§1)
  - Burn duration in CA is also nearly speed-independent, because S_T ∝ S̄p
  - Asset-specific curves (e.g. 2.0x in the LS asset) and early spark advance partly compensate. **This estimate depends on assumed T and p at spark and should be checked by instrumenting the sim.**
- **Recommended upgrade path**, cost-ranked and each step individually suitable for 12 cylinders at 50 kHz:
  1. Fix the λ→φ inversion: `phi = m_fuel->getMolecularAfr() / afr`. Add the residual term (1 − 2.06·x_r^0.77).
  2. Recompute S_L(T_u, p) and u'(ρ_u) every step instead of freezing them at spark. Cost is about 3 `pow` per active step.
  3. Replace the cylinder geometry with a spherical-cap flame from the plug and a precomputed A_f(r, h) table. Add the Blizard-Keck burn-up lag or the fractal factor (§2). This is about 1 µs per active step.
  4. Move CCV to per-cycle Gaussian perturbation of kernel delay and u' (§7). Raise the max efficiency to about 0.97, compensating instead with proper wall heat transfer.
  5. Add an unburned-zone temperature, the Livengood-Wu integral with Douaud-Eyzat, and audio-domain Draper-mode synthesis (§6).
  6. Optionally add fuel-specific S_L (Gülder or Verhelst), a Hardenberg-Hase/Watson CI path (§9) and Zeldovich NO in a 2-zone model (§10).
- Total incremental CPU for steps 1–5: on the order of 1 µs × 6×10^5 × 1/8 ≈ 0.1 core-seconds per second or less. Feasible.

### Gaps
- No runtime profiling of engine-sim was done to confirm actual burn durations. The 72° CA estimate is analytic.
- Upstream issues or discussions about the φ/λ convention were not checked; the network blocked GitHub web pages other than raw content.
