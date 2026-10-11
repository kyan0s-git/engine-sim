# Thermodynamic and Thermochemical Models of the Working Fluid in SI/CI Engines (for a 10–50 kHz Real-Time Simulator)

> Method note. Outbound web access in this session was heavily restricted (Wikipedia, AFDC, LiU, SAE full texts and most university hosts were blocked). To get exact coefficients rather than paraphrased snippets, I downloaded two primary machine-readable databases from GitHub: **GRI-Mech 3.0 thermo (NASA 7-coefficient) as distributed with Cantera** ([gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml)) and **the NASA Glenn CEA thermodynamic database (NASA 9-coefficient, McBride/Gordon/Zehe)** ([nasa/cea data/thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp)). Many numbers below (LHV, h_fg, AFR, γ(T) tables, flame temperatures, fuel–air cycle efficiencies, equilibrium compositions, compressibility factors) are **computed by me** from those databases with Cantera 3.2.0 and short Python scripts. They are labelled "computed". They are reproducible derivations from cited primary data, not literature quotations. Textbook material (Heywood, Ferguson & Kirkpatrick, Stone) could not be fetched. Where these notes give textbook-style equations, they are my own derivations, are marked as such, and were checked against limiting cases.

---

## 1. Cylinder gas representation & governing ODEs (single-zone, two-zone, filling-and-emptying)

### Takeaway
For real-time use, the standard approach is an open single-zone control volume with mass-averaged properties during gas exchange and compression/expansion. During combustion it switches to a two-zone (unburned/burned) model if you need NOx/knock/flame temperature. All of these reduce to 2–4 explicit ODEs per cylinder, which is trivially cheap at 50 kHz. The single-zone heat-release form with a linear γ(T) (Gatowski et al. 1984) is the classic baseline, but a linear γ carries noticeable error. Klein & Eriksson report that their improved γ model has 15× lower error.

### Cited Findings
- Gatowski et al. (SAE 841359, 1984), "Heat Release Analysis of Engine Pressure Data", is a **one-zone model** that treats the cylinder contents with **a linear approximation for γ(T)** and includes heat transfer, crevice flow and fuel-injection effects, while keeping the analysis simple — [SAE 841359](https://saemobilus.sae.org/papers/heat-release-analysis-engine-pressure-data-841359)
- In later work the Gatowski γ model is written as **γ_lin(T) = γ_300 + b·(T − 300)**, with T the mean charge temperature. The single-zone Gatowski model "has been widely used". The linear approximation introduces noticeable model error — [Klein & Eriksson, LiU / SAE 2004-01-1464 summary](https://www.fs.isy.liu.se/en/Publications/Articles/SAE_04_MKLE.html); [SAE 2004-01-1464 "A Specific Heat Ratio Model for Single-Zone Heat Release Models"](https://saemobilus.sae.org/content/2004-01-1464)
- Klein & Eriksson (2004) report that their new γ model's error is **15× smaller** than the linear model originally suggested by Gatowski et al. — [SAE 2004-01-1464](https://saemobilus.sae.org/content/2004-01-1464)
- One heat-release method for high-boost, low-temperature combustion splits the cylinder into **fresh, burned and mixed zones with an arbitrary equation of state** — [Combust. Sci. Tech. abstract (informahealthcare)](https://informahealthcare.com/doi/abs/10.1080/00102200902857757)

### Inferences (my derivations; each checked against the closed, constant-property limit)

**Notation.** m = cylinder mass, V = volume, T = temperature, p = pressure. u and h are specific internal energy and enthalpy *including formation enthalpy* (NASA polynomials give exactly this). R = specific gas constant, c_v and c_p are frozen specific heats, γ = c_p/c_v. Q̇ is the heat *added* to the gas (wall heat loss is negative). ṁ_j is the mass flow through port j (positive into the cylinder), and h_j is the upstream stagnation enthalpy: the manifold for inflow, the cylinder for outflow. Y_k are species or "lumped species" mass fractions.

**(a) Open single-zone, temperature form (filling-and-emptying, the gas-exchange workhorse):**

- Mass: dm/dt = Σ_j ṁ_j (+ ṁ_fuel,injected for DI) − ṁ_blowby
- Energy: d(mu)/dt = Q̇ − p dV/dt + Σ_j ṁ_j h_j
- Expanding u = u(T, Y): m c_v dT/dt = Q̇ − p dV/dt + Σ_j ṁ_j (h_j − u) − m Σ_k (∂u/∂Y_k) dY_k/dt
- Ideal-gas pressure: p = m R T / V, so (1/p) dp/dt = (1/m) dm/dt + (1/R) dR/dt + (1/T) dT/dt − (1/V) dV/dt
- Species: d(m Y_k)/dt = Σ_j ṁ_j Y_k,j + ω̇_k·V (or a prescribed burn-rate source)
- With formation-inclusive u, the combustion "heat release" appears automatically through the −m Σ u_k dY_k/dt term. Alternatively, use **sensible-only u** and add Q̇_comb = η_c · LHV · ṁ_fuel,burned. This is cheaper and is what most real-time codes do.

**(b) Constant-property pressure form (Benson-type filling-and-emptying):** with u = c_v T and h = c_p T,

- dp/dt = [ (γ − 1)·(Q̇ + Σ_j ṁ_j h_j) − γ p dV/dt ] / V
- Closed limit: dp/dt = −γ (p/V) dV/dt + (γ − 1) Q̇/V, which is the polytropic/heat-release equation.

**(c) Single-zone apparent heat release (inverse of (b), Gatowski/Rassweiler style):**

- dQ_net/dθ = [γ/(γ − 1)] p dV/dθ + [1/(γ − 1)] V dp/dθ

**(d) Two-zone combustion (unburned u, burned b, equal p, closed cylinder, frozen composition in each zone, R_i and c_p,i per zone, ṁ_b = burn rate):**

- Unburned zone (mass leaves at h_u, so its own state is unaffected): m_u c_p,u dT_u/dt = V_u dp/dt + Q̇_u
- Burned zone: m_b c_p,b dT_b/dt = ṁ_b (h_u(T_u) − h_b(T_b)) + V_b dp/dt + Q̇_b
- Pressure, from pV = m_u R_u T_u + m_b R_b T_b:
  dp/dt = [ −p dV/dt + ṁ_b (R_b T_b − R_u T_u) + ((γ_b − 1)/γ_b) ṁ_b (h_u − h_b) + ((γ_u − 1)/γ_u) Q̇_u + ((γ_b − 1)/γ_b) Q̇_b ] / ( V_u/γ_u + V_b/γ_b )
- Check: with no burning and one zone this collapses to dp/dt = −γ p V̇/V + (γ − 1) Q̇/V. ✓
- Zone volumes follow from V_i = m_i R_i T_i / p.
- Initialisation at ignition: m_b ≈ 1 % of the charge at T_b ≈ the adiabatic flame temperature. This avoids the m_b → 0 singularity.

**(e) Real-time cost.**

- Per cylinder and step, (a)–(d) cost about 50–300 flops plus property evaluations.
- At 50 kHz × 8 cylinders, that is < 10⁸ flop/s, comfortably real-time on one core.
- Use explicit RK2/RK4 or semi-implicit Euler. The stiff part is not the thermodynamics. It is the port flow at small pressure ratios and small cylinder volumes; integrate those with sub-stepping.

### Gaps
- Could not retrieve the actual γ_300 and b values from Gatowski 1984 or Klein & Eriksson 2004 (SAE paywall; the LiU PDF host was blocked). §2 gives my own fitted values as a substitute.
- Could not retrieve the published crevice-model equations (Gatowski) or the Klein–Eriksson γ model coefficients.

---

## 2. Temperature- and composition-dependent properties: NASA polynomials, cheap fits, γ over the cycle

### Takeaway
NASA 7-coefficient polynomials for about six species (N₂, O₂, CO₂, H₂O, CO, H₂, plus Ar and fuel) evaluated per step are cheap: about 5 multiply-adds per species per property. They are accurate to <0.5 % against the NASA-9 reference and are the recommended baseline. Over a stoichiometric gasoline cycle, frozen γ runs from **~1.35 (unburned, 300 K)** → **~1.29 (unburned at 700 K, end of compression)** → **~1.25–1.26 (burned at 1800–2200 K)** → **~1.32 (burned at 600–800 K, exhaust)**. With shifting equilibrium at peak conditions, the *effective* isentropic exponent falls to **~1.17–1.20**.

### Cited Findings

**NASA 7-coefficient form** (Cantera NasaPoly1; coefficients ordered [a1…a7]) — [Cantera NasaPoly1 docs](https://cantera.org/3.2/cxx/d2/d2a/NasaPoly1_8h_source.html):
- c_p/R = a1 + a2 T + a3 T² + a4 T³ + a5 T⁴
- h/(R T) = a1 + a2 T/2 + a3 T²/3 + a4 T³/4 + a5 T⁴/5 + a6/T
- s°/R = a1 ln T + a2 T + a3 T²/2 + a4 T³/3 + a5 T⁴/4 + a7
- Derived quantities: u = h − R T; c_v = c_p − R. R = 8.314462618 J/mol/K. h includes the 298.15 K enthalpy of formation.

**GRI-Mech 3.0 NASA-7 coefficients** (exact values from [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml)). The low range is T_low–1000 K and the high range is 1000 K–T_high.

| Species | T range (K) | Low range a1…a7 | High range a1…a7 |
|---|---|---|---|
| **N₂** | 300–1000–5000 | 3.298677, 1.4082404e-03, −3.963222e-06, 5.641515e-09, −2.444854e-12, −1020.8999, 3.950372 | 2.92664, 1.4879768e-03, −5.68476e-07, 1.0097038e-10, −6.753351e-15, −922.7977, 5.980528 |
| **O₂** | 200–1000–3500 | 3.78245636, −2.99673416e-03, 9.84730201e-06, −9.68129509e-09, 3.24372837e-12, −1063.94356, 3.65767573 | 3.28253784, 1.48308754e-03, −7.57966669e-07, 2.09470555e-10, −2.16717794e-14, −1088.45772, 5.45323129 |
| **CO₂** | 200–1000–3500 | 2.35677352, 8.98459677e-03, −7.12356269e-06, 2.45919022e-09, −1.43699548e-13, −48371.9697, 9.90105222 | 3.85746029, 4.41437026e-03, −2.21481404e-06, 5.23490188e-10, −4.72084164e-14, −48759.166, 2.27163806 |
| **H₂O** | 200–1000–3500 | 4.19864056, −2.0364341e-03, 6.52040211e-06, −5.48797062e-09, 1.77197817e-12, −30293.7267, −0.849032208 | 3.03399249, 2.17691804e-03, −1.64072518e-07, −9.7041987e-11, 1.68200992e-14, −30004.2971, 4.9667701 |
| **CO** | 200–1000–3500 | 3.57953347, −6.1035368e-04, 1.01681433e-06, 9.07005884e-10, −9.04424499e-13, −14344.086, 3.50840928 | 2.71518561, 2.06252743e-03, −9.98825771e-07, 2.30053008e-10, −2.03647716e-14, −14151.8724, 7.81868772 |
| **H₂** | 200–1000–3500 | 2.34433112, 7.98052075e-03, −1.9478151e-05, 2.01572094e-08, −7.37611761e-12, −917.935173, 0.683010238 | 3.3372792, −4.94024731e-05, 4.99456778e-07, −1.79566394e-10, 2.00255376e-14, −950.158922, −3.20502331 |
| **Ar** | 300–1000–5000 | 2.5, 0, 0, 0, 0, −745.375, 4.366 | (same) |
| **OH** | 200–1000–3500 | 3.99201543, −2.40131752e-03, 4.61793841e-06, −3.88113333e-09, 1.3641147e-12, 3615.08056, −0.103925458 | 3.09288767, 5.48429716e-04, 1.26505228e-07, −8.79461556e-11, 1.17412376e-14, 3858.657, 4.4766961 |
| **H** | 200–1000–3500 | 2.5, 7.05332819e-13, −1.99591964e-15, 2.30081632e-18, −9.27732332e-22, 25473.6599, −0.446682853 | 2.50000001, −2.30842973e-11, 1.61561948e-14, −4.73515235e-18, 4.98197357e-22, 25473.6599, −0.446682914 |
| **O** | 200–1000–3500 | 3.1682671, −3.27931884e-03, 6.64306396e-06, −6.12806624e-09, 2.11265971e-12, 29122.2592, 2.05193346 | 2.56942078, −8.59741137e-05, 4.19484589e-08, −1.00177799e-11, 1.22833691e-15, 29217.5791, 4.78433864 |
| **NO** | 200–1000–6000 | 4.2184763, −4.638976e-03, 1.1041022e-05, −9.3361354e-09, 2.803577e-12, 9844.623, 2.2808464 | 3.2606056, 1.1911043e-03, −4.2917048e-07, 6.9457669e-11, −4.0336099e-15, 9920.9746, 6.3693027 |
| **CH₄** | 200–1000–3500 | 5.14987613, −1.36709788e-02, 4.91800599e-05, −4.84743026e-08, 1.66693956e-11, −10246.6476, −4.64130376 | 7.4851495e-02, 1.33909467e-02, −5.73285809e-06, 1.22292535e-09, −1.0181523e-13, −9468.34459, 18.437318 |
| **C₃H₈** | 300–1000–5000 | 0.93355381, 2.6424579e-02, 6.1059727e-06, −2.1977499e-08, 9.5149253e-12, −13958.52, 19.201691 | 7.5341368, 1.8872239e-02, −6.2718491e-06, 9.1475649e-10, −4.7838069e-14, −16467.516, −17.892349 |

**n-Dodecane (diesel surrogate) NASA-7** from the Cantera-distributed Reitz mechanism. Mid temperature is **1391 K**; the range is 300–1391–5000 K.
- Low range: −2.62181594, 0.147237711, −9.43970271e-05, 3.07441268e-08, −4.0360223e-12, −40065.4253, 50.0994626
- High range: 38.5095037, 0.0563550048, −1.914932e-05, 2.96024862e-09, −1.7124415e-13, −54884.3465, −172.670922
- Source: [nDodecane_Reitz.yaml](https://github.com/Cantera/cantera/blob/main/data/nDodecane_Reitz.yaml)

**NASA-9 (CEA) source data** for fuels. Form: c_p/R = a1 T⁻² + a2 T⁻¹ + a3 + a4 T + a5 T² + a6 T³ + a7 T⁴, with b1 and b2 as the enthalpy and entropy integration constants — [NASA CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp). Entries used:
- `C8H18,isooctane` (TRC 4/85): M = 114.22852, ΔH_f,298 = −224 010 J/mol (gas)
- `C8H18(L),isooct`: ΔH_f = −259 160 J/mol (liquid)
- `C7H16,n-heptane`: −187 780 J/mol (gas); liquid −224 350 J/mol
- `C2H5OH`: −234 950 J/mol (gas); `C2H5OH(L)` −277 510 J/mol
- `CH3OH`: −200 940 J/mol (gas); `CH3OH(L)` −238 910 J/mol
- `CH4`: −74 600 J/mol
- `C3H8`: −104 680 J/mol
- `Jet-A(g)` C₁₂H₂₃: M = 167.311, −249 657 J/mol; `Jet-A(L)` −303 403 J/mol
- `Air` mixture (mole %: N₂ 78.084, O₂ 20.9476, Ar 0.9365, CO₂ 0.0319): M = 28.9651159 g/mol. Its c_p/R is 3.500 at 300 K, 3.975 at 1000 K, 4.356 at 2000 K and 4.510 at 3000 K — [NASA CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp)

**Computed: NASA-7 refits of the CEA NASA-9 fuel data.** These are least-squares fits on T/1000. a6 and a7 are matched to the NASA-9 h and s at 1000 K, and enthalpy agreement is within ~10 J/mol. Max c_p error is shown per range. Data source: [CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp).

| Fuel (gas) | 300–1000 K: a1…a7 | 1000–3000 K: a1…a7 |
|---|---|---|
| **iso-octane C₈H₁₈** | −1.43984399, 8.96796087e-02, −2.38210565e-05, −2.52511610e-08, 1.55447845e-11, −30245.6829, 33.6027464 (0.11 %) | 12.8875565, 6.12991780e-02, −2.37067433e-05, 4.55959217e-09, −3.59231952e-13, −34692.8575, −43.0050693 (0.03 %) |
| **n-heptane C₇H₁₆** | 1.46737592, 6.11298247e-02, 1.60971894e-05, −5.94475485e-08, 2.67061396e-11, −25776.3209, 24.6581794 (0.49 %) | 12.6392515, 5.19921151e-02, −2.39057471e-05, 5.74088721e-09, −5.51395151e-13, −29890.6314, −38.2903202 (0.05 %) |
| **ethanol C₂H₅OH** | 1.30120451, 2.21880634e-02, 3.68751291e-06, −1.88316650e-08, 8.83875328e-12, −29631.4474, 19.7034692 (0.46 %) | 2.91680449, 2.30989084e-02, −1.12762177e-05, 2.68132394e-09, −2.51738732e-13, −30274.7086, 10.2159469 (0.04 %) |
| **methanol CH₃OH** | 3.82932266, −1.62463700e-03, 3.09670971e-05, −3.50010091e-08, 1.26145837e-11, −25447.2279, 6.41686696 (0.47 %) | 1.31954848, 1.51704908e-02, −7.25814840e-06, 1.69727368e-09, −1.57168046e-13, −25213.4895, 17.0314448 (0.03 %) |
| **dry air (lumped)** | 3.73575283, −1.90527849e-03, 4.68261602e-06, −3.32947493e-09, 7.90328412e-13, −1079.39549, 3.01566618 (0.05 %) | 2.75662059, 1.99255272e-03, −9.97962072e-07, 2.49807656e-10, −2.45442833e-14, −887.498933, 7.73235391 (0.04 %) |

iso-octane gas c_p/R from CEA: 22.79 at 300 K, 35.27 at 500 K, 48.49 at 800 K, 54.70 at 1000 K, 65.07 at 1500 K and 71.39 at 2000 K — computed from [CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp)

**Computed: frozen γ = c_p/c_v vs T.** Ideal gas, from GRI-Mech 3.0 species plus the iso-octane refit; "complete" means complete-combustion products. The "eq" columns are frozen γ evaluated *at* the equilibrium composition. Data: [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml), [CEA](https://github.com/nasa/cea/blob/main/data/thermo.inp).

| T (K) | Dry air | Unburned φ=1 (iC₈H₁₈ vapour + air) | Burned φ=1 complete | Burned φ=1 eq. @50 bar | Burned φ=1 eq. @1 bar | Burned φ=0.8 | Burned φ=1.2 (with WGS) |
|---|---|---|---|---|---|---|---|
| 300 | 1.4007 | 1.3555 | 1.3722 | 1.3722 | 1.3722 | 1.3773 | 1.3771 |
| 400 | 1.3942 | 1.3375 | 1.3597 | 1.3597 | 1.3597 | 1.3657 | 1.3663 |
| 500 | 1.3860 | 1.3212 | 1.3479 | 1.3479 | 1.3479 | 1.3545 | 1.3557 |
| 600 | 1.3762 | 1.3064 | 1.3364 | 1.3364 | 1.3364 | 1.3432 | 1.3448 |
| 700 | 1.3653 | 1.2930 | 1.3248 | 1.3248 | 1.3248 | 1.3317 | 1.3335 |
| 800 | 1.3542 | 1.2809 | 1.3136 | 1.3136 | 1.3136 | 1.3205 | 1.3223 |
| 1000 | 1.3355 | 1.2620 | 1.2945 | 1.2945 | 1.2945 | 1.3015 | 1.3031 |
| 1200 | 1.3238 | 1.2499 | 1.2816 | 1.2816 | 1.2816 | 1.2887 | 1.2898 |
| 1500 | 1.3110 | 1.2371 | 1.2677 | 1.2677 | 1.2677 | 1.2749 | 1.2754 |
| 1800 | 1.3023 | 1.2284 | 1.2582 | 1.2583 | 1.2585 | 1.2655 | 1.2655 |
| 2000 | 1.2979 | 1.2241 | 1.2536 | 1.2538 | 1.2545 | 1.2609 | 1.2607 |
| 2200 | 1.2944 | 1.2207 | 1.2499 | 1.2506 | 1.2525 | 1.2572 | 1.2568 |
| 2500 | 1.2901 | 1.2168 | 1.2458 | 1.2482 | 1.2541 | 1.2531 | 1.2525 |
| 2800 | 1.2868 | 1.2138 | 1.2427 | 1.2487 | 1.2623 | 1.2500 | 1.2493 |
| 3000 | 1.2849 | 1.2122 | 1.2411 | 1.2508 | 1.2719 | 1.2483 | 1.2476 |

- **Computed: shifting-equilibrium isentropic exponent** (−d ln p / d ln v at constant s, re-equilibrating) for stoichiometric burned gas:
  - 2000 K, 50 bar: 1.241 (frozen at the same composition: 1.254)
  - 2500 K, 50 bar: 1.199 (frozen: 1.248)
  - 2800 K, 100 bar: 1.184 (frozen: 1.248)
  - 3000 K, 100 bar: 1.175 (frozen: 1.249)
  - Data: [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml)
- **Computed mixture molar masses and gas constants:**
  - Stoichiometric iso-octane vapour + air: M = 30.37 g/mol, R = 273.8 J/kg/K
  - Stoichiometric complete-combustion products: M = 28.71 g/mol, R = 289.6 J/kg/K
  - Dry air: R = 287.1 J/kg/K
  - Computed from [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml)

**Computed: cheap real-time fits** (least squares; x = (T − 1000)/1000; c_v in J/kg/K; polynomial coefficients listed highest power first). Data: [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml).
- **Unburned φ=1 iso-octane/air, 300–1100 K**
  - Gatowski-form linear fit: γ = 1.3473 − 1.257e-4·(T − 300). Max abs error 0.0088.
  - Quadratic: γ = 0.077849x² − 0.078978x + 1.2621. Max error 0.0005.
  - c_v: [−83.46, 338.0, 1044]. Max error 0.52 %.
- **With 10 % (mass) stoichiometric residual:** γ = 1.3495 − 1.242e-4·(T − 300) (max error 0.0084); R = 275.4 J/kg/K.
- **Burned φ=1 complete, 300–3000 K**
  - Linear: γ = 1.3352 − 4.304e-5·(T − 300). Max error **0.037**. Too crude.
  - Quadratic: γ = 0.024954x² − 0.07548x + 1.3004. Max error 0.008.
  - c_v quadratic: [−66.6, 238.9, 975.0]. Max error 1.2 %.
- **Burned φ=0.8:** γ quadratic [0.024562, −0.074631, 1.3073]
- **Burned φ=1.2:** γ quadratic [0.024373, −0.075356, 1.3083]; R = 301.2 J/kg/K
- **Dry air, 300–3000 K:** γ quadratic [0.022167, −0.0689, 1.3413], max error 0.0073; linear max error 0.027

### Inferences
- **Recommended real-time property scheme: "lumped species".** Track three mass fractions per cylinder:
  - fresh air (incl. humidity),
  - fuel vapour,
  - burned products at the current φ.
  
  Precompute NASA-7 coefficients for each lump as mole-weighted sums of the species coefficients. This is exact for ideal-gas mixtures at frozen composition, because c_p/R is linear in the coefficients. Cost is about 3 polynomial evaluations per property per zone. Re-lump the burned-products coefficients only when φ changes appreciably.
- **Even cheaper:** 1-D lookup tables of u_sens(T), h_sens(T), c_v(T) and R for each lump, at 10 K spacing over 250–3500 K with linear interpolation. That is about 330 entries per lump and costs one multiply-add per property.
- **Single-zone γ.**
  - A linear γ(T) is acceptable for the unburned phase (≤0.009 error).
  - It is not adequate across the whole cycle: the burned-gas error reaches 0.037, consistent with the error Klein & Eriksson criticise.
  - Use a mass-fraction blend instead: γ = f(x_b, T) via c_v,mix = (1 − x_b)·c_v,u(T) + x_b·c_v,b(T), R_mix likewise, then γ = 1 + R_mix/c_v,mix. Composition-correct blending is more accurate than any single γ(T).
- **Over a stoichiometric SI cycle** (from the table): intake/early compression ≈1.35 → TDC unburned (~700–800 K) ≈1.28–1.29 → peak burned (~2500–2800 K) ≈1.24–1.25 frozen (≈1.18–1.20 shifting-equilibrium) → late expansion/exhaust (~1000–1300 K) ≈1.28–1.30. The "≈1.4 → ≈1.25" folklore applies to *air*. Fuel vapour lowers the unburned γ by ~0.04–0.07.
- **Diesel (CI) compression** is mostly air (+ residual/EGR), so γ ≈1.37–1.33 over 300–900 K (air column). Fuel is added as liquid near TDC, so its latent heat and vapour c_p enter at injection.

### Gaps
- Krieger & Borman (1966) and Heywood's own polynomial u(T, φ) fits: I could not retrieve the published coefficients. They are not reproduced here to avoid fabrication. The NASA lumped-species approach above supersedes them in accuracy at similar cost.
- Burcat's database (alternative iso-octane/gasoline-surrogate polynomials) was unreachable (host blocked). The iso-octane NASA-7 above is my refit of CEA data, not an official set.

---

## 3. Burned-gas composition vs φ; water–gas shift; dissociation; cheap equilibrium

### Takeaway
Complete-combustion stoichiometry plus a water–gas-shift (WGS) equilibrium for φ > 1 reproduces equilibrium major species to within ~0.001 mole fraction at T ≤ ~1700 K. This is the right model for exhaust/residual composition and for "sensible-energy" combustion accounting. Dissociation matters at peak burned-gas temperatures. At φ = 1 it lowers the constant-volume adiabatic flame temperature by ~300 K (3206 → 2907 K, computed), and CO+H₂ at 2800 K/100 bar is about 2.3 mol%. That affects peak pressure and NOx but has little effect on net work (recombination on expansion). For real time, use precomputed equilibrium tables in (T, p, φ) rather than an online Olikara–Borman solve.

### Cited Findings
- Olikara & Borman (SAE 750468, 1975) is a program that *rapidly* computes equilibrium mole fractions **and their partial derivatives with respect to T, p and φ** for products of any hydrocarbon fuel with air. A subroutine returns gas constant, enthalpy, internal energy and their partial derivatives — [SAE 750468](https://saemobilus.sae.org/content/750468); [TRID record](https://trid.trb.org/View/38050)
- A secondary source lists the Olikara–Borman ten products as CO₂, H₂O, N₂, O₂, CO, H₂, H, O, OH, NO. The method expresses the other species' mole fractions as functions of the CO, H₂, O₂ and N₂ mole fractions, so **only four equations are solved numerically**. Variants with 11 species (adding N) exist — [Thai PIT journal paper on Olikara–Borman 10-species method](https://ph03.tci-thaijo.org/index.php/pitjournal/article/view/837)

**Computed equilibrium vs complete-combustion composition** (iso-octane/air, mole fractions; GRI-Mech 3.0 thermo, Cantera TP equilibrium) — [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml):

| φ | State | CO₂ | H₂O | CO | H₂ | O₂ | OH | H | O | NO |
|---|---|---|---|---|---|---|---|---|---|---|
| 0.8 | complete | 0.1012 | 0.1138 | 0 | 0 | 0.0395 | – | – | – | – |
| 0.8 | eq 1700 K/10 bar | 0.1012 | 0.1138 | 0.0000 | 0.0000 | 0.0389 | 0.0001 | 0 | 0 | 0.0012 |
| 0.8 | eq 2500 K/50 bar | 0.0982 | 0.1111 | 0.0027 | 0.0005 | 0.0354 | 0.0037 | 0.0001 | 0.0004 | 0.0091 |
| 0.8 | eq 2800 K/100 bar | 0.0928 | 0.1081 | 0.0076 | 0.0013 | 0.0344 | 0.0070 | 0.0003 | 0.0010 | 0.0143 |
| 1.0 | complete | 0.1247 | 0.1403 | 0 | 0 | 0 | – | – | – | – |
| 1.0 | eq 1700 K/10 bar | 0.1244 | 0.1402 | 0.0002 | 0.0001 | 0.0001 | 0 | 0 | 0 | 0.0001 |
| 1.0 | eq 2500 K/50 bar | 0.1141 | 0.1362 | 0.0098 | 0.0019 | 0.0037 | 0.0023 | 0.0002 | 0.0001 | 0.0029 |
| 1.0 | eq 2800 K/100 bar | 0.1038 | 0.1321 | 0.0192 | 0.0035 | 0.0068 | 0.0052 | 0.0005 | 0.0005 | 0.0063 |
| 1.2 | complete + WGS (K=3.5) | 0.0905 | 0.1375 | 0.0516 | 0.0224 | 0 | – | – | – | – |
| 1.2 | eq 1700 K/10 bar | 0.0908 | 0.1372 | 0.0513 | 0.0227 | 0 | 0 | 0 | 0 | 0 |
| 1.2 | eq 2500 K/50 bar | 0.0835 | 0.1429 | 0.0585 | 0.0162 | 0.0001 | 0.0008 | 0.0005 | 0 | 0.0004 |

**Computed WGS equilibrium constant** K = (x_CO · x_H₂O)/(x_CO₂ · x_H₂) for CO₂ + H₂ ⇌ CO + H₂O, from GRI-Mech 3.0 Gibbs energies: 2.17 at 1400 K, 2.58 at 1500 K, 3.00 at 1600 K, **3.41 at 1700 K**, **3.82 at 1800 K**, 4.59 at 2000 K, 6.17 at 2500 K. The commonly used constant K ≈ 3.5 therefore corresponds to an assumed freezing temperature of ≈1720–1740 K — computed from [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml)

**Computed adiabatic flame temperatures** (iso-octane vapour/air) — [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml) + [CEA](https://github.com/nasa/cea/blob/main/data/thermo.inp):

| φ | T_ad, const-p, 1 atm, 298 K reactants: complete / equilibrium | T_ad, const-V from 700 K, 20 bar (SI-like TDC): complete / equilibrium (p_final) |
|---|---|---|
| 0.7 | 1896 / 1884 K | 2613 / 2538 K (76 bar) |
| 0.8 | 2073 / 2048 K | 2821 / 2700 K (81 bar) |
| 0.9 | 2242 / 2185 K | 3018 / 2826 K (86 bar) |
| 1.0 | 2404 / 2272 K | 3206 / 2907 K (89 bar) |
| 1.1 | 2313 / 2272 K | 3106 / 2935 K (92 bar) |
| 1.2 | 2226 / 2208 K | 3010 / 2913 K (93 bar) |
| 1.4 | 2061 / 2054 K | 2827 / 2787 K (93 bar) |

### Inferences

**Complete-combustion stoichiometry** for C_nH_mO_r + (a/φ)(O₂ + 3.773 N₂ + 0.0447 Ar), with a = n + m/4 − r/2 (Ar ratio from CEA air composition):
- **φ ≤ 1:** products are n CO₂ + (m/2) H₂O + a(1/φ − 1) O₂ + 3.773(a/φ) N₂ + 0.0447(a/φ) Ar.
- **φ > 1:** let c = moles of CO. Then:
  - CO₂ = n − c
  - H₂O = 2a/φ − 2n + c − r (from the O balance)
  - H₂ = m/2 − H₂O
  - Solve K·(n − c)·H₂ = c·H₂O. This is a quadratic in c with a closed-form root, so it costs no iterations.
- **Lean case:** to include a little CO at high T, use a single equilibrium CO₂ ⇌ CO + ½O₂. Otherwise set CO from an empirical exhaust map.

**When dissociation matters:**
- Below ~1700–1800 K: < 0.1 mol% change in the major species (table at 1700 K). Ignore it.
- At 2500–2800 K: CO + H₂ + OH reach 1.4–3.0 mol% at φ = 1. Peak T drops by 100–300 K versus frozen complete combustion (largest near φ = 0.9–1.0, small for φ ≥ 1.2).
- For a real-time sim focused on p(θ), torque and sound, dissociation changes peak pressure by ≈8 % at φ = 1 (97 → 89 bar in the constant-V case above). Most of that energy is returned during expansion (see the fuel–air cycle in §6).

**Cheapest physically consistent options, in order of cost:**
1. Frozen complete-combustion products with WGS (closed form). Error: overpredicts T_peak by up to ~300 K at φ ≈ 1.
2. 3-D lookup table of equilibrium burned-gas properties u(T, p, φ), R(T, p, φ) and ∂u/∂T, generated offline with Cantera/CEA. A grid of T 300–3500 K × 50 K (65), log p 0.5–300 bar (16) and φ 0.5–1.6 × 0.1 (12) gives ~12.5k nodes per property. Trilinear lookup is ~30 flops. This captures shifting-equilibrium γ (1.17–1.20 at peak).
3. Online Olikara–Borman (4×4 Newton, ~3–6 iterations) at combustion-phase steps only. That is probably ~10–50 µs per call on a CPU, which is borderline at 50 kHz × 8 cylinders, so prefer the table.
4. Full Gibbs minimisation (Cantera/CEA). Offline only.

**NO:** an equilibrium-NO table is useful only as an upper bound. Real NO is kinetically limited (extended Zeldovich), which is covered by other notes.

### Gaps
- Could not access the Olikara–Borman paper itself for its equilibrium-constant curve fits (log₁₀K = A ln(T/1000) + B/T + C + D T + E T² form; coefficients not verified). The Cantera table approach avoids needing them.
- No measured timing of an Olikara–Borman call was found; the µs estimate above is my guess.

---

## 4. Fuel properties (LHV, AFR_st, M, h_fg, density, octane)

### Takeaway
For pure compounds, LHV, stoichiometric AFR and latent heat follow exactly from formula and formation enthalpies. Values computed from the NASA CEA database agree with published tables to ~1 %. Commercial blends (gasoline, diesel, E10/E85, LPG, CNG) are variable, so a simulator should expose them as parameters, using the published ranges below.

### Cited Findings

**Computed from NASA CEA formation enthalpies.** CO₂ = −393.51 kJ/mol, H₂O(g) = −241.826 kJ/mol, H₂O(l) = −285.83 kJ/mol. Air is taken as 4.773 mol per mol O₂, M_air = 28.965. Data: [CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp).

| Fuel | Formula | M (g/mol) | LHV, gas fuel (MJ/kg) | LHV, liquid fuel (MJ/kg) | HHV, gas (MJ/kg) | h_fg at 298 K (kJ/kg) | AFR_st (mass) |
|---|---|---|---|---|---|---|---|
| iso-octane | C₈H₁₈ | 114.23 | 44.65 | **44.34** | 48.12 | **308** (35.15 kJ/mol) | **15.13** |
| n-heptane | C₇H₁₆ | 100.20 | 44.92 | 44.56 | 48.44 | 365 | 15.18 |
| ethanol | C₂H₅OH | 46.07 | 27.73 | **26.81** | 30.60 | **924** | **9.00** |
| methanol | CH₃OH | 32.04 | 21.10 | **19.92** | 23.85 | **1185** | **6.47** |
| methane (CNG proxy) | CH₄ | 16.04 | **50.03** | – | 55.51 | – | **17.24** |
| propane (LPG proxy) | C₃H₈ | 44.10 | **46.33** | – | 50.33 | – | **15.68** |
| Jet-A / diesel-like surrogate | C₁₂H₂₃ | 167.31 | – | **43.03** | – | 321 | 14.67 |
| hydrogen | H₂ | 2.016 | **119.96** | – | 141.79 | – | **34.29** |

- With the older "O₂ + 3.76 N₂" air (no Ar, M_air ≈ 28.85), the AFRs become iso-octane 15.03, ethanol 8.94, methanol 6.43, CH₄ 17.12 and C₃H₈ 15.57. These match the 8.94 and 6.43 in the literature table below. Use the Ar-inclusive values for consistency with the air thermo.
- A published simulation table gives:
  - gasoline: AFR_st 14.5, LHV 44.3 MJ/kg, RON 96.5
  - methanol: AFR_st 6.43, LHV 20 MJ/kg, RON 112
  - ethanol: AFR_st 8.94, LHV 27 MJ/kg, RON 111
  - Source: [IntechOpen, "Comparison of Ethanol and Methanol Blending with Gasoline Using Engine Simulation"](https://www.intechopen.com/chapters/64099)
- Tuning-industry stoichiometric AFR sheet: gasoline 14.7, diesel 14.6, methanol 6.4, ethanol 9, E85 9.8, propane 15.5, natural gas 17.2 — [HP Academy sheet](https://www.hpacademy.com/assets/Uploads/045852edfc/Stoichiometric-air-to-fuel-ratios-of-common-fuels-Sheet1-3.pdf)
- Ethanol **blending** RON is ≈120–135 and blending MON ≈100–106. These are higher than its neat RON/MON — [IEA-AMF ethanol fuel properties](https://iea-amf.org/content/fuel_information/ethanol/fuel_properties)
- E85 actually contains **51–83 % ethanol** depending on geography and season. Its LHV is **83,950–95,450 Btu/gal**, about 73–83 % of gasoline per volume — [AFDC via search summary; E85](https://en.wikipedia.org/wiki/E85); [AFDC properties notes](https://afdc.energy.gov/fuels/properties-notes)
- Energy per gallon relative to gasoline: diesel ≈113 %, propane ≈73 %, methanol ≈50 %. 1 kg H₂ ≈ 1 gallon gasoline equivalent — [AFDC fuel comparison chart (Stanford mirror)](http://large.stanford.edu/courses/2016/ph240/kountz2/docs/fuel_comparison_chart.pdf) (search-snippet level only)

### Inferences
- **Blends (mass-weighted).** AFR_blend = Σ w_i·AFR_i and LHV_blend = Σ w_i·LHV_i, with w_i the *mass* fractions. Assuming densities of 0.745 kg/L (gasoline) and 0.789 kg/L (ethanol), which are not verified here:
  - E10: w_EtOH ≈ 0.105, so AFR ≈ 13.9–14.1 (gasoline 14.5–14.7) and LHV ≈ 42.5 MJ/kg.
  - E85 (85 vol%): w_EtOH ≈ 0.857, so AFR ≈ 9.8 (matches the HP Academy sheet) and LHV ≈ 29 MJ/kg.
- **Gasoline surrogate.** Iso-octane (LHV 44.3 MJ/kg liquid, AFR 15.13) slightly overstates AFR versus market gasoline (14.5–14.7 because of aromatics/oxygenates). Either use a "C₇H₁₃"-like surrogate or override AFR_st as a parameter while keeping iso-octane thermo.
- **Diesel.** The C₁₂H₂₃ Jet-A surrogate (LHV 43.0 MJ/kg, AFR 14.67) is consistent with the HP Academy diesel AFR of 14.6. n-Dodecane NASA-7 is given in §2.
- **Energy per unit stoichiometric charge** (LHV/AFR_st, MJ per kg air) is nearly fuel-independent: iso-octane 2.93, ethanol 2.98, methanol 3.08, CH₄ 2.90, H₂ 3.50. This is why stoichiometric power density varies little apart from volumetric-efficiency effects (gaseous fuels displace air; alcohols' evaporative cooling densifies the charge).

### Gaps
- Not verified in this session (blocked sources), so typical values must be checked before use: gasoline density (~0.72–0.775 kg/L), diesel density (~0.82–0.85 kg/L), diesel LHV (~42.6–43 MJ/kg), diesel cetane (40–55), gasoline RON/MON (91–98 / 82–88), methane RON (~120+), propane RON (~105–112), hydrogen RON (>130, poorly defined), gasoline h_fg (~350 kJ/kg), hydrogen/CNG flammability limits.
- No primary AFDC table could be fetched (afdc.energy.gov blocked).

---

## 5. Combustion efficiency vs φ, unburned fuel, residual gas fraction

### Takeaway
For rich mixtures the dominant "inefficiency" is thermodynamic: oxygen is lacking, so CO and H₂ leave the cylinder. At φ = 1.2 the chemical energy actually released is only ~78 % of the LHV of the fuel supplied (computed). For lean/stoichiometric operation, real engines lose a further few percent to crevice/quench HC. Residual fraction is best computed by the filling-and-emptying model itself. The Fox–Cheng–Heywood correlation is the standard closed-form alternative, but its coefficients were not retrievable here.

### Cited Findings
- **Computed: energy released per kg fuel at 298 K** with complete combustion + WGS (K = 3.5):
  - φ = 0.8: 44.73 MJ/kg
  - φ = 1.0: 44.73 MJ/kg
  - φ = 1.2: **34.90 MJ/kg** (78 % of LHV)
  - Per kg mixture: 2247, 2774 and 2566 kJ/kg at φ = 0.8, 1.0 and 1.2. Mixture energy peaks near φ = 1.
  - Iso-octane fuel mass fraction: 0.0502, 0.0620 and 0.0735 at φ = 0.8, 1.0 and 1.2.
  - Computed from [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml) + [CEA](https://github.com/nasa/cea/blob/main/data/thermo.inp).
- A 2018 simulation of a 5.7 L V8 SI engine found combustion efficiency of **94.1–94.4 % lean** and **93.8–94.1 % rich** — [FME Transactions 2018 (DOAJ)](https://doaj.org/article/be5e16669c35471db01b629fedc79e8d). This is a single simulation study; treat as indicative only.
- Fox, Cheng & Heywood (SAE 931025, 1993) give a residual-gas-fraction model that explicitly accounts for **exhaust backflow during valve overlap**. It was calibrated against in-cylinder HC measurements across intake pressure, engine speed and overlap — [SAE 931025](https://saemobilus.sae.org/papers/a-model-predicting-residual-gas-fraction-spark-ignition-engines-931025)
- A University of Wisconsin thesis on an air-cooled utility engine found that **none of three residual-fraction models** (including Fox–Cheng–Heywood) predicted residual fraction accurately over the full range tested. Residual fraction was strongly affected by **valve overlap and engine speed** — [UW–Madison thesis (MINDS)](https://minds.wisconsin.edu/handle/1793/35297)

### Inferences
- **Combustion-efficiency model for real time:** η_c(φ) = min(1, 1/φ)·η_HC.
  - The first factor is a crude stand-in for the oxygen deficit. The exact released-energy ratio from the WGS calculation is lower, ≈0.78 at φ = 1.2 versus 1/φ = 0.83, because H₂ as well as CO is left unburned. Use the exact WGS calculation from §3 rather than 1/φ.
  - η_HC ≈ 0.95–0.98 accounts for crevice/quench HC and partial burn. It degrades towards the lean/dilution limit; tie it to cycle-to-cycle variability or the misfire model.
  - In a full energy-accounting model, set η_HC as a tunable parameter. The 94 % above is a lower-end reference.
- **Ideal-cycle residual estimate** (my derivation; a gas-exchange model is better). At EVC/IVO the clearance volume V_c = V_d/(r_c − 1) holds burned gas at p_e and T_r, so
  - x_r ≈ m_r / m_total ≈ (p_e V_c / (R_b T_r)) / (p_e V_c/(R_b T_r) + m_fresh)
  - With m_fresh ≈ η_v ρ_i V_d this becomes x_r ≈ [1 + η_v (r_c − 1)(p_i/p_e)(T_r/T_i)(R_b/R_u)]⁻¹
  - Example: r_c = 10, part load p_i/p_e = 0.4, T_r/T_i = 1000/320, η_v = 0.9 gives x_r ≈ 0.09. At WOT (p_i/p_e ≈ 1) x_r ≈ 0.04. Overlap backflow adds to this, which is why Fox–Cheng–Heywood includes an overlap-factor term.
- In a filling-and-emptying simulator, residual fraction emerges naturally from tracking burned-mass fraction through port backflow. No correlation is needed, only species (or burned-fraction) transport in manifold volumes.

### Gaps
- Heywood's measured "combustion efficiency vs φ" figure data (Fig. 4-26 in the 1st ed.) could not be retrieved. The common statement "95–98 % for lean, falling roughly as 1/φ when rich" could not be verified online.
- The Fox–Cheng–Heywood formula and its coefficients (an overlap-factor OF/N term and a p_e/p_i term) were not retrievable (SAE paywall).

---

## 6. Fuel–air cycle and ideal Otto/Diesel/dual efficiencies (validation targets)

### Takeaway
Constant-γ Otto efficiency with γ = 1.4 overstates achievable efficiency by ~10 percentage points. The computed iso-octane fuel–air cycle (variable properties + equilibrium) at r_c = 10, φ = 1 gives **η = 0.46**, versus 0.60 (γ = 1.4), 0.50 (γ = 1.3) and 0.44 (γ = 1.25). A real-time thermodynamic core with no heat loss and an instantaneous constant-volume burn should reproduce the fuel–air-cycle numbers below within ~1–2 points. That makes them good unit tests.

### Cited Findings
**Computed fuel–air cycle** (iso-octane vapour/air, no residual, 1 bar/350 K at BDC; frozen isentropic compression; constant-volume adiabatic **equilibrium** combustion; isentropic expansion with **shifting equilibrium**; η = net work / (m_fuel · 44.65 MJ/kg)) — [gri30.yaml](https://github.com/Cantera/cantera/blob/main/data/gri30.yaml), [CEA](https://github.com/nasa/cea/blob/main/data/thermo.inp):

| φ | r_c | T₂ (K) | p₂ (bar) | T₃ (K) | p₃ (bar) | T₄ (K) | **η_fuel-air** | Otto γ=1.4 | Otto γ=1.3 | Otto γ=1.25 |
|---|---|---|---|---|---|---|---|---|---|---|
| 0.8 | 8 | 697 | 15.9 | 2693 | 65 | 1656 | **0.450** | 0.565 | 0.464 | 0.405 |
| 0.8 | 10 | 746 | 21.3 | 2727 | 82 | 1584 | **0.485** | 0.602 | 0.499 | 0.438 |
| 0.8 | 12 | 787 | 27.0 | 2756 | 99 | 1528 | **0.512** | 0.630 | 0.525 | 0.463 |
| 0.8 | 16 | 856 | 39.1 | 2805 | 135 | 1443 | **0.552** | 0.670 | 0.565 | 0.500 |
| 1.0 | 6 | 627 | 10.7 | 2847 | 53 | 2038 | **0.377** | 0.512 | 0.416 | 0.361 |
| 1.0 | 8 | 683 | 15.6 | 2887 | 71 | 1935 | **0.425** | 0.565 | 0.464 | 0.405 |
| 1.0 | 10 | 729 | 20.8 | 2920 | 90 | 1856 | **0.460** | 0.602 | 0.499 | 0.438 |
| 1.0 | 12 | 768 | 26.3 | 2948 | 109 | 1793 | **0.487** | 0.630 | 0.525 | 0.463 |
| 1.0 | 14 | 802 | 32.1 | 2973 | 128 | 1741 | **0.509** | 0.652 | 0.547 | 0.483 |
| 1.0 | 16 | 833 | 38.1 | 2995 | 148 | 1697 | **0.527** | 0.670 | 0.565 | 0.500 |
| 1.2 | 8 | 670 | 15.3 | 2888 | 74 | 1761 | **0.355** | 0.565 | 0.464 | 0.405 |
| 1.2 | 10 | 714 | 20.4 | 2921 | 93 | 1681 | **0.383** | 0.602 | 0.499 | 0.438 |
| 1.2 | 12 | 751 | 25.7 | 2949 | 113 | 1618 | **0.405** | 0.630 | 0.525 | 0.463 |
| 1.2 | 16 | 812 | 37.1 | 2995 | 153 | 1525 | **0.437** | 0.670 | 0.565 | 0.500 |

(φ = 0.8 r_c = 6 gives 0.401, φ = 0.8 r_c = 14 gives 0.534, φ = 1.2 r_c = 6 gives 0.317, φ = 1.2 r_c = 14 gives 0.422.)

### Inferences
**Ideal-cycle formulas** (standard air-standard results; my statement, with r_c the compression ratio):
- Otto: η = 1 − r_c^(1−γ)
- Diesel, with cutoff ratio r_v = V₃/V₂: η = 1 − r_c^(1−γ) · (r_v^γ − 1) / (γ (r_v − 1))
- Dual (limited pressure), with α = p₃/p₂ and cutoff β:
  η = 1 − r_c^(1−γ) · (α β^γ − 1) / ((α − 1) + γ α (β − 1))
  - This reduces to Otto for β = 1 and to Diesel for α = 1.

**Fuel–air-cycle trends to reproduce in validation:**
- η rises as φ falls (φ = 0.8 is ~2.5–3 points above φ = 1 at the same r_c).
- Rich φ = 1.2 is ~7–9 points below φ = 1, because of CO/H₂ in the products when η is referenced to full LHV.
- An effective constant γ ≈ 1.28–1.30 reproduces the φ = 1 fuel–air η reasonably well (r_c = 10: 0.46 vs 0.48–0.50).

**Real engines:** the indicated efficiency of a real SI engine is typically ~80–85 % of the fuel–air-cycle value because of heat loss, finite burn duration and pumping. This is a common textbook rule of thumb that I could not verify in this session.

**Test recipe for the sim:** turn off heat transfer and blow-by, burn instantly at TDC, motor at 1 bar/350 K, and compare net work per cycle to this table. With frozen complete-combustion products (no dissociation), expect η slightly *higher* at φ = 1 (≈+0.5–1.5 points), because dissociation's lower peak-T losses are only partly recovered. Run once with frozen and once with equilibrium tables to bracket.

### Gaps
- Could not retrieve Heywood's or Ferguson's published fuel–air-cycle charts to cross-check numerically. They use different initial conditions (often including residual fraction ~0.05 and initial T ≈ 350 K, p = 1 atm), so small differences are expected.

---

## 7. Real-gas effects at in-cylinder conditions

### Takeaway
For the hot burned gas (≥2000 K) and SI compression (~60 bar, 800 K), the ideal-gas assumption errs by ~1–2 % in p·v/RT (computed with Peng–Robinson for N₂, which makes up ~70–75 mol% of the charge). That is well within other model uncertainties. Real-gas effects become noticeable (Z ≈ 1.04–1.06) only for high-boost diesel conditions (≥150 bar at ≤1000 K), and they matter mainly for heavy fuel vapour and for high-fidelity heat-release analysis. A real-time simulator can safely use the ideal-gas law, optionally with a covolume correction.

### Cited Findings
- Real-gas modelling predicted the compression-stroke pressure history more accurately than the ideal-gas law. Real-gas effects matter more at extreme pressures; the study considered a 300 bar case — [SAE 2010-01-0627 "Real Gas Effects in High-Pressure Engine Environment"](https://saemobilus.sae.org/articles/real-gas-effects-high-pressure-engine-environment-2010-01-0627)
- Under high-boost, low-temperature combustion the compressibility factor Z = p v/(R T) can be **significantly greater than 1.0** — [Combust. Sci. Tech. abstract](https://informahealthcare.com/doi/abs/10.1080/00102200902857757)
- In piston-cylinder (gas-spring) systems, ideal and real gas models gave nearly identical pressure and temperature oscillations for monatomic/diatomic gases (He, N₂). For heavier molecules such as propane, the ideal model overestimated T and p and underestimated thermal losses — [Univ. of Pretoria repository](https://repository.up.ac.za/handle/2263/61968)
- A diesel heat-release diagnostic study quantified fuel compressibility under engine conditions with four cubic equations of state. The magnitudes were not available in the abstract — [IAEA INIS record](https://inis.iaea.org/records/ps5es-md227)

**Computed Peng–Robinson Z** (pure-component values at the mixture T, p). Critical constants: N₂ Tc = 126.2 K, Pc = 33.98 bar, ω = 0.037; CO₂ 304.13 K, 73.77 bar, 0.225; H₂O 647.1 K, 220.64 bar, 0.344; O₂ 154.58 K, 50.43 bar, 0.022. These are standard handbook constants entered by me and not re-verified online this session.

| State | Z(N₂) | Z(O₂) | Z(CO₂) | Z(H₂O) |
|---|---|---|---|---|
| 300 K, 1 bar | 0.9996 | 0.9991 | 0.9946 | 0.985 (vapour would condense; irrelevant) |
| 800 K, 60 bar (SI TDC) | **1.020** | 1.014 | 1.009 | 0.951 |
| 900 K, 150 bar (boosted diesel TDC) | **1.046** | 1.034 | 1.032 | 0.929 |
| 1000 K, 200 bar | **1.056** | 1.043 | 1.048 | 0.947 |
| 2000 K, 100 bar | 1.014 | 1.012 | 1.016 | 1.009 |
| 2500 K, 80 bar (SI peak) | 1.009 | 1.008 | 1.010 | 1.007 |
| 2800 K, 200 bar (extreme peak) | 1.019 | 1.017 | 1.020 | 1.016 |

### Inferences
- **Mixture Z ≈ Z(N₂)** within ~±0.01, because N₂ dominates and H₂O/CO₂ partly cancel. Ideal-gas error in p(V, T) is therefore ≈2 % at SI TDC, ≈1 % at SI peak and ≈5 % at boosted-diesel TDC.
- A **cheap correction** for high-pressure CI: the Abel–Noble covolume form p = ρRT/(1 − bρ), with b ≈ 8.6e-4 m³/kg for air/N₂ (= b_PR/M = 0.0778·R·Tc/(Pc·M); the van der Waals b would give ~1.4e-3 m³/kg). It costs 2 flops. Check: at 800 K/60 bar, ρ ≈ 26 kg/m³, so Z = 1/(1 − bρ) ≈ 1.023, versus PR 1.020. Because the internal energy of an Abel–Noble gas equals the ideal-gas value, the energy equation is unchanged. This is an inference from the PR parameters, not from a cited engine paper.
- **Liquid fuel spray and fuel vapour near injection** are strongly non-ideal (iso-octane Z = 0.86 at 800 K/60 bar as a pure component). Only a small mole fraction is fuel, so the mixture effect is ≤1 %.

### Gaps
- Quantitative published error percentages for in-cylinder ideal-gas assumptions (from SAE 2010-01-0627 and the IAEA study) could not be retrieved.

---

## 8. Fresh-charge thermodynamics: humidity, fuel vapour, evaporative charge cooling, standard conditions

### Takeaway
Treat the fresh charge as a mixture of dry air (CEA "Air", M = 28.965), water vapour and fuel vapour, with additive NASA polynomials. Humidity reduces the O₂ available and slightly reduces γ. Fuel evaporation cools the charge by ~18 K (iso-octane), ~88 K (ethanol) or ~150 K (methanol) at stoichiometric, if fully evaporated adiabatically in the air. This is a large effect for alcohols and DI engines, and it is the main reason alcohols increase volumetric efficiency and knock margin.

### Cited Findings
- **Dry air composition** (mole %): N₂ 78.084, O₂ 20.9476, Ar 0.9365, CO₂ 0.0319, giving M = 28.9651159 g/mol (Gordon 1982, as encoded in the NASA CEA database) — [CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp)
- **Computed latent heats at 298.15 K** (ΔH_f,gas − ΔH_f,liquid): iso-octane 308 kJ/kg, n-heptane 365 kJ/kg, ethanol 924 kJ/kg, methanol 1185 kJ/kg, Jet-A (C₁₂H₂₃) 321 kJ/kg — [CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp)
- **Computed vapour c_p at 300 K:** iso-octane c_p/R = 22.79, which is 1.66 kJ/kg/K; ethanol 7.889, i.e. 1.42 kJ/kg/K; methanol 5.311, i.e. 1.38 kJ/kg/K. Dry air c_p = 1.005 kJ/kg/K (c_p/R = 3.500) — [CEA thermo.inp](https://github.com/nasa/cea/blob/main/data/thermo.inp)

### Inferences
- **Adiabatic evaporative cooling of a stoichiometric charge (my energy balance).**
  - ΔT ≈ (F/A)·h_fg / (c_p,air + (F/A)·c_p,fuel,vap), with F/A = 1/AFR_st.
  - Results: iso-octane 0.0661 × 308 / (1.005 + 0.110) ≈ **18 K**; ethanol 0.111 × 924 / (1.005 + 0.158) ≈ **88 K**; methanol 0.155 × 1185 / (1.005 + 0.213) ≈ **150 K**.
  - In practice, alcohol evaporation is limited by saturation and wall wetting (port injection: much of the h_fg is drawn from walls/valves). Expose a "fraction of evaporation drawing heat from charge" parameter (≈0.3–1.0) and an evaporation time constant.
- **DI implementation.** Fuel mass enters the cylinder energy balance as Σṁ_j h_j with h_j = h_liquid(T_fuel) = h_vapour(T_fuel) − h_fg. This automatically produces the cooling, given formation-inclusive NASA h for vapour plus the h_fg offset. For sensible-energy formulations, add −ṁ_evap·(h_fg + c_p,vap·(T − T_fuel)) to the energy equation.
- **Humidity.**
  - Specific humidity ω = 0.622·p_v/(p − p_v), with p_v = RH·p_sat(T).
  - A Magnus-type fit (p_sat ≈ 610.94·exp(17.625·t/(t + 243.04)) Pa, t in °C) is the usual cheap choice; these coefficients are from memory and need verification.
  - Humid air adds x_H₂O = p_v/p as an extra lump. Example: 30 °C at 60 % RH gives x_H₂O ≈ 2.5 %, which proportionally reduces O₂ moles and therefore maximum fuel at fixed λ.
- **Standard reference conditions** for power correction: SAE J1349 and DIN 70020 define different reference states (25 °C / 99 kPa dry vs 20 °C / 101.3 kPa). The exact values are unverified here; see Gaps.
- **Fuel vapour with port injection or premix.** It lowers mixture γ (unburned φ=1 iso-octane: γ_300 = 1.356 vs 1.401 for air) and displaces air. Gaseous fuels displace the most: the CH₄ volume fraction at stoichiometric is 1/(1 + 2·4.773) ≈ 9.5 %, and H₂ is ≈ 29.6 %. That causes the well-known power loss of PFI gas engines.

### Gaps
- Could not verify SAE J1349 / ISO 1585 / DIN 70020 reference conditions or correction-factor formulas online (sources blocked).
- No source retrieved for p_sat correlation coefficients (Magnus/Alduchov–Eskridge). Alternatively, compute p_sat from the CEA H₂O(L)/H₂O(g) Gibbs energies (both present in thermo.inp) via ln p_sat = −ΔG_vap/(R T).
- Gasoline h_fg and distillation behaviour (multi-component evaporation) were not covered.
