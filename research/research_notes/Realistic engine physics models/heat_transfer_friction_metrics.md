# In-Cylinder Heat Transfer, Mechanical Friction/Loss Models, and Engine Performance Metrics for Real-Time Cycle Simulation

> **Read this first: how far the sources were checked.**
> In this session WebFetch failed with DNS errors (`getaddrinfo ENOTFOUND`) on every host tried: dspace.mit.edu, saemobilus.sae.org, ansyshelp.ansys.com, libice-post.readthedocs.io, wahiduddin.net, ddisoftware.com, scribd.com and engr.colostate.edu. Only WebSearch result summaries were available. Findings are tagged by how far they could be checked:
> - **[V]**: confirmed by a web search result in this session (URL given).
> - **[R]**: taken from the named primary source or textbook as recalled by the researcher. The text could **not** be fetched to check the exact coefficients. These are the standard published forms, mostly as reproduced in Heywood's *Internal Combustion Engine Fundamentals* (McGraw-Hill, 1988; 2nd ed. 2018). The URL points to the primary reference (DOI). **Check every [R] coefficient against the primary source before treating it as authoritative.**
> - Statements under "Inferences" are the researcher's own engineering judgement.
>
> Primary references used throughout (DOIs):
> - Woschni 1967, SAE 670931: https://doi.org/10.4271/670931
> - Hohenberg 1979, SAE 790825: https://doi.org/10.4271/790825
> - Annand 1963, Proc. IMechE 177: https://doi.org/10.1243/PIME_PROC_1963_177_069_02
> - Chang et al. 2004, SAE 2004-01-2996: https://saemobilus.sae.org/content/2004-01-2996
> - Chen & Flynn 1965, SAE 650733: https://doi.org/10.4271/650733
> - Patton, Nitschke & Heywood 1989, SAE 890836: https://dx.doi.org/10.4271/890836
> - Sandoval & Heywood 2003, SAE 2003-01-0725: https://saemobilus.sae.org/papers/improved-friction-model-spark-ignition-engines-2003-01-0725
> - Sandoval MIT thesis: https://dspace.mit.edu/bitstream/1721.1/80657/2/52940875-MIT.pdf
> - Heywood, *Internal Combustion Engine Fundamentals* (McGraw-Hill 1988 / 2nd ed. 2018), cited below as "Heywood ICEF": https://www.accessengineeringlibrary.com/content/book/9781260116106

---

## Q1. In-cylinder convective heat transfer correlations (Woschni, Hohenberg, Annand, Eichelberg, Chang), inputs and wall area

### Takeaway
Woschni (1967) is the default zero-dimensional correlation. It costs a few flops per step, needs bore, mean piston speed, p, T, and a motored-pressure reference taken at IVC, and switches its constants by phase (gas exchange, compression, combustion/expansion). Hohenberg is preferred for DI diesels, Chang et al. (2004) for HCCI, and Annand adds an explicit radiation term (important in diesels). All of them are cheap enough to evaluate every integration step in real time. The real uncertainty is calibration: the overall multiplier is commonly tuned by roughly 0.5–2×.

### Cited Findings

**General form.** The heat flux to each wall surface i is `Q̇_i = h · A_i · (T_gas − T_wall,i)`, with T_gas the mass-averaged bulk temperature. All of these correlations come from Nusselt–Reynolds forced-convection relations for turbulent pipe flow — [Finol & Robinson review, Univ. of Bath](https://researchportal.bath.ac.uk/en/publications/thermal-modelling-of-modern-engines-a-review-of-empirical-correla/) [V]

**Woschni (1967), SI form as in Heywood ICEF eq. 12.19** [R, structure partly V]
```
h_c [W/m²K] = 3.26 · B^-0.2 · p^0.8 · T^-0.55 · w^0.8
   B = bore [m], p = cylinder pressure [kPa], T = bulk gas temperature [K]
w [m/s] = C1 · S̄p + C2 · (Vd · Tr / (pr · Vr)) · (p − pm)
   S̄p = mean piston speed = 2·L·N [m/s] (L stroke [m], N [rev/s])
   Vd = displaced volume per cylinder [m³]
   pr, Vr, Tr = cylinder state at a reference point, usually IVC
                (or the start of combustion) [kPa, m³, K]
   pm = motored pressure at the same crank angle [kPa]
Phase constants:
   Gas exchange (any valve open):     C1 = 6.18, C2 = 0
   Compression (valves closed):       C1 = 2.28, C2 = 0
   Combustion and expansion:          C1 = 2.28, C2 = 3.24e-3 [m/(s·K)]
With measured swirl (paddle-wheel swirl speed vs = B·ω_paddle/2):
   gas exchange: C1 = 6.18 + 0.417·vs/S̄p
   comp./exp.:   C1 = 2.28 + 0.308·vs/S̄p
```
- C1 = 6.18 applies when the inlet or exhaust valves are open and 2.28 when all valves are closed; a tangential swirl term appears in some versions — [FSU propulsion lecture notes](https://web1.eng.famu.fsu.edu/~shih/propulsion/lecture%20notes/Convective%20Heat%20Transfer.pdf); [Kharkiv (KhNADU) paper](https://at.khadi.kharkov.ua/article/download/337126/337092/811305) [V]
- The combustion-term coefficient is 0.00324 (labelled `C3comb` in libICEpost), with a separate compression value `C3comp`. The libICEpost implementation writes `u = C2'·Ūp + C3·(p − p_mot)·Vs·T0/(p0·V0)` and gets the motored pressure from a polytropic law `p_mot = p0·(V0/V)^n` — [libICEpost docs, Woschni model](https://libice-post.readthedocs.io/en/latest/autoapi/libICEpost/src/engineModel/HeatTransferModel/Woschni/index.html); also given as 3.24×10⁻³ with p0 = motoring pressure in [Rzeszów Univ. of Technology paper](https://czasopisma.prz.edu.pl/amme/article/download/1436/1090) [V]
- **Unit and exponent variants exist.** The prefactor appears as ~128–130 when p is in bar or MPa-scaled units, and 3.26 when p is in kPa. Woschni's original paper uses T^-0.53, while Heywood writes T^-0.55 — [libICEpost docs](https://libice-post.readthedocs.io/en/stable/autoapi/libICEpost/src/engineModel/HeatTransferModel/Woschni/index.html) and search summary [V for "prefactor differs with units"; exponent difference R]. As a check: 3.26 × 100^0.8 ≈ 130, so "130, p in bar" and "3.26, p in kPa" are the same correlation.
- For motored engines, one source reports that the measured coefficient was matched by multiplying Woschni by about 1.7 — [search summary of the FSU and Polish sources above](https://web1.eng.famu.fsu.edu/~shih/propulsion/lecture%20notes/Convective%20Heat%20Transfer.pdf) [V, as reported; underlying experiment not identified]
- **Woschni/Huber low-load revision** (Huber, Woschni & Zeilinger 1990, SAE 900252/905018-era). The velocity term becomes `w = C1·S̄p·[1 + 2·(Vc/V)²·imep^-0.2]` (imep in bar), and the model takes the larger of this and the standard Woschni w. It corrects the under-prediction near TDC at low load and when motored — [R; Huber, Woschni, Zeilinger, "Investigations on heat transfer in internal combustion engines under low load and motoring conditions" (1990)](https://doi.org/10.4271/905018)

**Hohenberg (1979)** [R]
```
h [W/m²K] = 130 · V^-0.06 · p^0.8 · T^-0.4 · (S̄p + 1.4)^0.8
   V = instantaneous cylinder volume [m³], p [bar], T [K], S̄p [m/s]
```
- No motored-pressure term and no phase switching. It was developed for DI diesels. A 2021 comparison found Hohenberg agreed well with experiment, especially for a turbocharged engine — [Archives of Thermodynamics 2021 (PAN)](https://www.czasopisma.pan.pl//Content/121382/PDF/art08_final.pdf) [V]. Ansys Chemkin IC-engine models offer it as an option — [Ansys Chemkin docs](https://ansyshelp.ansys.com/public/Views/Secured/corp/v252/en/chemkin_th/pgfId-1376572.html) [V that it exists; formula R]

**Annand (1963), convection plus radiation** [R]
```
q/A = a · (k/B) · Re^b · (T − Tw) + c · (T⁴ − Tw⁴)
   Re = ρ · S̄p · B / μ  (gas properties at bulk T), k = gas thermal conductivity
   a = 0.35 – 0.8 (grows with charge-motion intensity), b = 0.7
   c = 0.576·σ (diesel) and 0.075·σ (SI), σ = 5.67e-8 W/m²K⁴
```
- Radiation can be 20–40% of time-averaged cylinder heat flux in diesels — [Univ. of Bath, "Thermal modelling of modern diesel engines"](https://researchportal.bath.ac.uk/en/publications/thermal-modelling-of-modern-diesel-engines-proposal-of-a-new-heat/) and search summary [V]. Constants a, b and c are R (Heywood ICEF eq. 12.20). Annand's original radiation constant is quoted in some texts with a different T reference, so verify it.
- Gas properties for Annand: air-like correlations work, e.g. `k ≈ 2.5e-4·T^0.8` [W/mK] and `μ ≈ 3.3e-7·T^0.7` [kg/ms] (Heywood ICEF ch. 4/12) [R].

**Eichelberg (1939)** [R]
```
h = 2.1 · S̄p^(1/3) · (p·T)^(1/2)     original units: kcal/(m²·h·°C), p in at (kgf/cm²)
SI conversion: h [W/m²K] ≈ 2.43–2.46 · S̄p^(1/3) · (p[bar]·T)^0.5
             ≈ 7.8e-3 · S̄p^(1/3) · (p[Pa]·T)^0.5
```
- This is the oldest correlation and is known to be poor for high-speed engines. Treat the SI conversion constant as approximate: it is computed by the researcher from 1 kcal/m²h°C = 1.163 W/m²K and 1 at = 0.981 bar. Some codes quote 7.67e-3.

**Chang et al. (2004), HCCI** [V for source and qualitative change; coefficients R]
```
h [W/m²K] = 3.4 · L(t)^-0.2 · p^0.8 · T^-0.73 · v_tuned^0.8
   L(t) = instantaneous combustion-chamber height [m] (replaces bore)
   v_tuned = 2.28·S̄p + (0.00324/6)·(Vd·Tr/(pr·Vr))·(p − pm)
   p [kPa], T [K]; i.e. the combustion term is cut to 1/6 of Woschni's
```
- The source is Chang et al., SAE 2004-01-2996 (U. Michigan / GM R&D), which found Woschni inadequate for HCCI because of its flame-propagation (pressure-rise) term. The modified model matched measured surface heat flux in magnitude and phasing; the full text is in Chang's 2004 dissertation on Deep Blue — [SAE 2004-01-2996](https://saemobilus.sae.org/content/2004-01-2996); [U. Michigan Deep Blue](https://deepblue.lib.umich.edu/items/8905f499-61f2-43e5-8939-397e5ebf213f/full) [V]
- Independent validation is limited and mixed. One review says the Chang and Hensel modifications "have not been validated by others". A diesel-HCCI study found the modified Woschni most accurate, while a natural-gas HCCI CFD study found Woschni unusable — [IJAME, Effect of Different Heat Transfer Models on a Diesel HCCI Engine](https://journal.ump.edu.my/ijame/article/view/8144); [SAGE J. Automobile Eng. 2011](https://journals.sagepub.com/doi/full/10.1177/0954407011407559) [V]

**Wall area as a function of crank angle (Heywood ICEF eq. 2.4–2.7)** [R]
```
a = L/2 (crank radius), l = con-rod length, θ = crank angle from TDC
s(θ) = a·cosθ + sqrt(l² − a²·sin²θ)          (pin-to-crank-axis distance)
x(θ) = l + a − s(θ)                           (piston displacement from TDC)
V(θ) = Vc + (π·B²/4)·x(θ)                     (Vc = Vd/(rc − 1))
A_liner(θ) = π·B·(x(θ) + h_c)                 (h_c = clearance height ≈ Vc/(πB²/4) for a flat chamber)
A_piston   = (π·B²/4)·k_p                     (k_p ≈ 1 flat; 1.1–1.5 for bowl or dome)
A_head     = (π·B²/4)·k_h                     (k_h ≈ 1.1–1.4 for pent-roof)
A_total(θ) = A_head + A_piston + A_liner(θ)
```
- Instantaneous piston speed: `Sp(θ) = (π/2)·S̄p·sinθ·[1 + cosθ/sqrt(R² − sin²θ)]`, R = l/a (Heywood ICEF eq. 2.11) [R]. All the correlations above use the **mean** piston speed S̄p, not this instantaneous value.
- The k_p and k_h area multipliers are not standard published constants; they are geometry-dependent and the researcher's estimate.

### Inferences
- **Real-time recipe.** At IVC, store (p_r, V_r, T_r) and the polytropic index n (≈1.32–1.37). At each step compute `p_m = p_r·(V_r/V)^n`, then w, then h, then split the flux by area into head, piston and liner using each surface's own T_wall. This costs about 3 `pow()` calls per cylinder per step; at 10–50 kHz with 8 cylinders that is negligible. `pow(x,0.8)` can be replaced by `exp(0.8·log x)` or a lookup table if profiling demands it.
- **Choice per engine type:** SI default Woschni; DI diesel Hohenberg (or Woschni plus an Annand radiation term); HCCI Chang. Expose a user multiplier on h (default 1.0, range 0.5–2.0), since every source says correlations need tuning.
- Woschni's C2 term needs p_m to be continuous through combustion. Set the reference state at IVC, or at spark/SOI, and freeze it for the cycle.
- When valves are open, use C1 = 6.18; the ±swirl terms can be ignored unless the simulator models swirl.

### Gaps
- Exact coefficients for Woschni 1978 and Huber/Woschni 1990, Annand's c, Eichelberg's SI constant, and Chang's 3.4 / −0.73 / C2/6 could not be fetched from primary texts in this session (DNS failures). They are given from recall [R] and need verification.
- No source was found giving empirical head and piston area multipliers for common combustion-chamber shapes.

---

## Q2. Wall temperatures, coolant, lumped wall thermal models and warm-up

### Takeaway
Use per-cycle-constant wall temperatures per surface: head ~180–300 °C (hotter at the exhaust-valve bridge), piston crown ~200–350 °C, liner ~110–200 °C, coolant ~85–105 °C. Drive them with a few-node lumped RC model, whose time constants of seconds to minutes produce engine warm-up. Cycle-resolved surface swings are only a few to ~20 K and are not needed in real time.

### Cited Findings
- Typical warm-engine values (Heywood ICEF ch. 12, figs. 12-1 and 12-28 to 12-30) [R]:
  - Cylinder head, gas side: ~180–300 °C; the exhaust-valve bridge is hottest.
  - Exhaust valve head: ~650–800 °C (SI full load).
  - Aluminium piston crown: ~200–300 °C at the centre and higher in diesel bowl rims. Top ring groove is kept below ~220–250 °C to avoid oil coking.
  - Liner: ~150–200 °C at top-ring TDC position, falling to near coolant temperature at the bottom.
  - Coolant: ~85–100 °C pressurised (thermostat opening ~82–95 °C). Oil sump: ~90–120 °C.
- Combustion-chamber surface temperature swings within a cycle are small, on the order of 10 K and confined to ~1 mm penetration depth, so a cycle-constant wall temperature is a standard assumption (Heywood ICEF §12.6) [R].
- Heat transfer to the coolant is roughly 16–35% of fuel energy depending on load and speed. Peak in-cylinder heat flux is on the order of 1–10 MW/m² (Heywood ICEF ch. 12, Table 12.x energy balance) [R].
- The search found no authoritative published lumped wall model with standard constants; the model below is the researcher's engineering construction.

### Inferences
- **Lumped warm-up model** (researcher-constructed, standard RC network):
  ```
  For each wall node i ∈ {head, liner, piston}:
     C_i · dT_i/dt = Q̄_gas→i − (T_i − T_cool)/R_i,cool − (T_i − T_oil)/R_i,oil
  Coolant node:
     C_cool · dT_cool/dt = Σ (T_i − T_cool)/R_i,cool − ṁ_rad·cp·ε_rad·(T_cool − T_amb)·thermostat(T_cool)
  where Q̄_gas→i = cycle-averaged ∫ h·A_i·(T_gas − T_i) dt.
  Conduction resistance: R = t/(k·A) + 1/(h_cool·A)
     Al head: k ≈ 150–170 W/mK, gas-side wall t ≈ 6–10 mm
     cast-iron liner: k ≈ 45–55 W/mK, t ≈ 5–8 mm
     h_cool (forced-convection water/glycol) ≈ 2,000–10,000 W/m²K (nucleate boiling higher)
  Thermal capacitance C_i = ρ·c·V_i (Al: ρc ≈ 2.4 MJ/m³K; Fe: ≈ 3.6 MJ/m³K)
  ```
- The wall-surface time constants (about 1–30 s) are much faster than coolant/block warm-up, which takes minutes. The thermostat (a proportional valve between ~85 and ~95 °C) is what limits steady-state coolant temperature. Cold-start effects, such as higher friction from oil viscosity (see Q4) and higher heat loss, should key off T_oil and T_cool.
- In real time, update wall temperatures once per cycle using the cycle-integrated heat. That avoids stiffness and costs nothing.

### Gaps
- No primary source was retrieved for standard lumped thermal capacitances or coolant-side heat transfer coefficients of production engines; the values above are typical engineering numbers and should be treated as estimates.

---

## Q3. Exhaust port and manifold heat transfer, exhaust gas temperature

### Takeaway
Use Dittus–Boelter or Sieder–Tate pipe correlations with an enhancement factor for pulsating, entrance-region flow. Exhaust ports need a dedicated, much higher correlation such as Caton–Heywood. Exhaust temperatures range from ~400 °C at idle to ~900–1000 °C at SI full load, and ~500–700 °C for diesels.

### Cited Findings
- Diesel and engine heat-transfer correlations are rooted in turbulent tube forced-convection Nusselt correlations — [Finol & Robinson review](https://researchportal.bath.ac.uk/en/publications/thermal-modelling-of-modern-engines-a-review-of-empirical-correla/) [V]
- **Dittus–Boelter** [R; standard heat-transfer texts such as Incropera]:
  `Nu = 0.023·Re^0.8·Pr^n`, with n = 0.3 when the fluid is cooled (hot exhaust) and 0.4 when heated. Valid for Re > 10,000, 0.6 < Pr < 160, L/D > 10. `h = Nu·k/D`, Re = ρuD/μ, and Pr ≈ 0.7 for exhaust gas.
- **Sieder–Tate** [R]: `Nu = 0.027·Re^0.8·Pr^(1/3)·(μ/μ_w)^0.14`, for large property variation; same Re range.
- **Gnielinski** [R]: `Nu = (f/8)(Re − 1000)Pr / [1 + 12.7·(f/8)^0.5·(Pr^(2/3) − 1)]` with `f = (0.79 ln Re − 1.64)^-2`. Valid for 3,000 < Re < 5×10⁶, so it covers transitional flow at idle.
- **Exhaust port, Caton & Heywood (1981)** [R]: `Nu = 0.258·Re^0.8` (Re based on port diameter and port-flow velocity). This is about 2–3 times Dittus–Boelter because of separated, jet-like flow past the valve.
- Exhaust gas temperature at the exhaust port, as a temporal mean: SI full load ~800–950 °C (some turbo engines limit turbine inlet to 950–1050 °C); SI part load and idle ~400–600 °C; diesel full load ~500–700 °C (Heywood ICEF §6.5 and ch. 12) [R].

### Inferences
- For a real-time 1-D or 0-D manifold model, compute Q̇ = h·A·(T_gas − T_pipe) per pipe segment. Apply an empirical pulsating-flow enhancement factor of 1.5–3 to Dittus–Boelter for primaries close to the port. Give the pipe wall its own lumped thermal node: `C_pipe·dT_pipe/dt = Q̇_gas − h_ext·A·(T_pipe − T_amb) − εσA(T_pipe⁴ − T_amb⁴)`, with external natural convection plus radiation, ε ≈ 0.7–0.9 for oxidised steel. That produces glowing headers and realistic cooldown.
- Since gas properties change with temperature, evaluate k and μ at the gas bulk temperature with the same power-law fits as Q1.

### Gaps
- The Caton–Heywood coefficient (0.258) and the enhancement factors could not be verified online in this session.

---

## Q4. Friction mean effective pressure: Chen–Flynn, Patton–Nitschke–Heywood (PNH), and Sandoval–Heywood update

### Takeaway
For real time, the PNH/Sandoval component model is the best balance. It uses closed-form algebraic terms per component in B, S, N, bearing dimensions, valve count/type, intake pressure and oil viscosity, all evaluated once per cycle or at a low rate. Chen–Flynn (constant + peak pressure + piston speed + speed²) is the simplest calibratable alternative and is used widely in 1-D codes. FMEP rises roughly linearly to quadratically with speed. It falls with larger bore, because many terms scale as 1/B², and rises modestly with load through the ring gas-pressure term.

### Cited Findings

**Chen–Flynn (SAE 650733)**
- Structure: FMEP is a constant plus a term linear in peak cylinder pressure plus a quadratic function of speed. `FMEP = A + B·p_max + C·S̄p + D·S̄p²` with four calibration constants — [Pipitone, "A New Simple Friction Model for S.I. Engine", SAE 2009-01-1984](https://saemobilus.sae.org/content/2009-01-1984/) [V]
- Typical coefficient ranges, quoting the *Internal Combustion Engine Handbook* in a USPTO patent: constant ≈ 0.3–0.5 bar, p_max coefficient ≈ 0.004–0.006 (bar/bar), S̄p² coefficient ≈ 0.0006–0.0012 bar/(m/s)² — [USPTO patent 8511265 (variable stroke & CR engine)](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/8511265) [V]
- Pipitone reports that Chen–Flynn is less accurate than his load-based model, especially when peak pressure occurs late (retarded combustion), because p_max then poorly represents load — [SAE 2009-01-1984](https://saemobilus.sae.org/content/2009-01-1984/) [V]
- Recent calibration practice fits the Chen–Flynn terms by optimiser against measured or 1-D data, e.g. for a marine dual-fuel engine — [Tadros et al. 2025, Strathclyde](https://strathprints.strath.ac.uk/93447/) [V]
- A frequently quoted parameter set, `FMEP[bar] = 0.137 + 0.005·p_max[bar] + 0.162·S̄p[m/s]` (sometimes with a small S̄p² term), could **not** be verified. Its constant lies outside the handbook range above. Treat it as unverified.

**Patton–Nitschke–Heywood 1989 (SAE 890836)**
- The model predicts rubbing losses from crankshaft, reciprocating and valvetrain components, auxiliary losses from accessories, and pumping losses. It was calibrated against published fmep data — [SAE 890836](https://saemobilus.sae.org/papers/development-evaluation-a-friction-model-spark-ignition-engines-890836) [V]
- The crankshaft term has three parts: seal friction of the form `C·Db/(B²·S·nc)`, main-bearing hydrodynamic friction, and turbulent dissipation of oil in the journal bearings. The reciprocating term has a hydrodynamic piston-skirt part, a turbulent-dissipation part (con-rod bearings) and a piston-ring part — [search summary of SAE 2011-01-0610 and related docs](https://saemobilus.sae.org/content/2011-01-0610) [V for structure]
- **Full equation set** (Heywood ICEF 1st ed. §13.6, from PNH). Units: fmep in **kPa**; B, S, Db, Lb, Lv in **mm**; N in **rev/min**; S̄p in **m/s**; nc = number of cylinders [R]:
  ```
  Crankshaft:
   cfmep = 1.22e5·Db/(B²·S·nc)                       (front/rear main-bearing seals)
         + 3.03e-4·N·Db³·Lb·nb/(B²·S·nc)              (main bearings, hydrodynamic)
         + 1.35e-10·Db²·N²·nb/nc                      (turbulent oil dissipation)
     Db, Lb = main-bearing diameter, length; nb = number of main bearings

  Reciprocating (piston, rings, con-rod bearings):
   rfmep = 2.94e2·S̄p/B                               (piston skirt, hydrodynamic)
         + 4.06e4·(1 + 500/N)·1/B²                    (ring tension, mixed lubrication)
         + 3.03e-4·N·Db³·Lb·nb/(B²·S·nc)              (con-rod big-end bearings; rod-bearing Db, Lb, nb)
         + 6.89·(pi/pa)·[0.088·rc + 0.182·rc^(1.33 − K·S̄p)]   (gas-pressure loading of rings)
     pi = intake manifold pressure, pa = atmospheric pressure, rc = compression ratio,
     K = 2.38e-2 s/m

  Valvetrain:
   vfmep = 244·N·nb/(B²·S·nc)                         (camshaft bearings, hydrodynamic; nb = cam bearings)
         + C_ff·(1 + 1000/N)·nv/(S·nc)                (flat-follower, mixed lubrication)
         + C_rf·N·nv/(S·nc)                           (roller-follower rolling friction)
         + C_oh·Lv^1.5·N^0.5·nv/(B·S·nc)              (oscillating hydrodynamic)
         + C_om·(1 + 500/N)·Lv·nv/(S·nc)              (oscillating mixed)
     nv = total valves, Lv = max valve lift [mm]
     Use C_ff for flat-tappet engines and C_rf for roller engines (not both).

     Configuration              C_ff   C_rf     C_oh   C_om
     DOHC direct-acting bucket  200    0.0076   0.5    10
     SOHC finger follower       600    0.0227   0.2    42.8
     SOHC rocker arm            400    0.0151   0.5    21.4
     Cam-in-block (pushrod)     400    0.0151   0.5    32.1

  Auxiliary (oil pump, water pump, non-charging alternator):
   afmep = 8.32 + 1.86e-3·N + 7.45e-7·N²             [kPa]

  Total:  tfmep = cfmep + rfmep + vfmep + afmep (+ pmep)
  ```

**Sandoval & Heywood 2003 (SAE 2003-01-0725) update**
- The update re-evaluated the late-1980s PNH model against more recent engine friction data and improved it. It keeps the same breakdown (crankshaft, reciprocating, valvetrain, auxiliary, pumping); some terms come from lubrication theory and others are empirical — [MIT thesis, Sandoval](https://dspace.mit.edu/bitstream/1721.1/80657/2/52940875-MIT.pdf); [SAE 2003-01-0725](https://saemobilus.sae.org/papers/improved-friction-model-spark-ignition-engines-2003-01-0725) [V]
- The main changes as recalled [R; reproduced in Heywood ICEF 2nd ed. ch. 13]:
  1. Hydrodynamic terms (bearings, piston skirt, cam bearings, oscillating hydrodynamic) are scaled by **(μ/μ₀)^0.5**. μ is the instantaneous oil viscosity from a Vogel-type equation `μ = k·exp(θ1/(T_oil + θ2))`, and μ₀ is a reference viscosity (SAE 10W-30 at warm conditions). This term captures cold-start friction.
  2. The piston-skirt and ring terms were re-fitted, with a Stribeck-style transition between mixed and hydrodynamic regimes.
  3. Revised auxiliary term: `afmep = 6.23 + 5.22e-3·N − 1.79e-7·N²` [kPa].
- The thesis abstract says its predictions were compared with friction data collected 1980–1989 — [MIT thesis](https://dspace.mit.edu/bitstream/1721.1/80657/2/52940875-MIT.pdf) [V]

**Simple whole-engine correlations** [R]
- Motored SI engines, 4-cylinder, 0.845–2.0 L, WOT (Barnes-Moss / Bishop data in Heywood ICEF eq. 13.15): `tfmep[bar] = 0.97 + 0.15·(N/1000) + 0.05·(N/1000)²`. This includes pumping at WOT.
- Friction breakdown (Heywood ICEF fig. 13-x) [R]: piston, rings and con-rods are the largest share (~40–55% of mechanical friction); crank bearings ~20–30%; valvetrain ~7–15% (flat-tappet engines higher); auxiliaries ~10–20%. Total friction is ~10% of indicated power at WOT and grows to ~100% at idle.

### Inferences
- **Speed dependence:** the constant parts (seals, ring tension at high N) and the hydrodynamic terms (∝ N or S̄p) produce a characteristic FMEP(N) that rises from ~0.8–1 bar at 1000 rpm to ~2–3 bar at 6000–7000 rpm for automotive SI engines (consistent with the Bishop correlation). (1 + 500/N)-type terms make friction rise slightly toward very low speed, imitating the mixed or boundary regime.
- **Bore and stroke:** most terms ∝ 1/B² or 1/(B·S) per cylinder. Small-bore, many-cylinder engines therefore have higher FMEP, and a long stroke raises S̄p at a given N.
- **Load:** only the ring gas-pressure term (∝ pi/pa) and Chen–Flynn's p_max term are load-sensitive, giving roughly +0.1–0.3 bar from idle to WOT.
- **Real-time implementation:** evaluate the PNH/Sandoval terms once per engine revolution, or at 100 Hz, from the current N, MAP and T_oil, and apply friction torque `T_f = FMEP·Vd_total/(4π)` (four-stroke). This is trivially cheap. A Chen–Flynn option using the last cycle's p_max is a good "calibrate to dyno data" mode.
- **Crank-angle-resolved friction** (for torsional realism): spread the reciprocating part as `F_f(θ) ∝ |Sp(θ)|` plus a gas-load term ∝ p(θ), normalised so the cycle integral equals rfmep. This keeps the mean right and adds per-stroke torque ripple.

### Gaps
- The PNH coefficient table and the Sandoval revisions (exact viscosity exponents, Vogel constants, re-fitted piston and ring coefficients) are given from recall [R]. The MIT thesis and the SAE paper could not be fetched (DNS failure). **These must be verified against Heywood ICEF 2nd ed. ch. 13 or the thesis PDF before release.**
- The original Chen–Flynn coefficients (diesel, 1965) could not be retrieved.
- PNH's empirical intake/exhaust pumping sub-model coefficients were not recovered; this matters little, since the simulator computes PMEP directly (Q5).

---

## Q5. Pumping losses (PMEP): definition and computation from the simulated P–V loop

### Takeaway
PMEP is the net work of the exhaust and intake strokes divided by displacement. In a simulator that integrates p·dV it is exact and free: split the ∮p·dV integral at BDC and TDC.

### Cited Findings
- PNH/Sandoval treat pumping losses (intake and exhaust systems, including throttle and valves) as a separate component alongside rubbing and auxiliary friction — [MIT thesis abstract](https://dspace.mit.edu/bitstream/1721.1/80657/2/52940875-MIT.pdf); [SAE 890836](https://dx.doi.org/10.4271/890836) [V]
- Definitions (Heywood ICEF §2.4 and §5.x) [R]:
  ```
  W_c,ig  (gross indicated work)  = ∫ p dV over compression + expansion strokes (BDC → TDC → BDC, i.e. 180°–540° with TDC-firing at 360°)
  W_c,in  (net indicated work)    = ∮ p dV over the full 720° cycle
  imep_g = W_c,ig / Vd ;   imep_n = W_c,in / Vd
  pmep   = imep_g − imep_n = −(1/Vd)·∫_{exhaust + intake strokes} p dV   (positive when the engine does work on the gas)
  ```
- Typical magnitude [R, Heywood ICEF ch. 13]: throttled SI at light load ~0.3–1.0 bar (≈ pa − p_manifold plus valve and exhaust losses); NA WOT ~0.1–0.3 bar. On turbocharged engines PMEP can be negative (a pumping gain) when boost pressure exceeds exhaust back-pressure.

### Inferences
- **Implementation:** keep two accumulators per cylinder, `W_hp += p·dV` during the 360° of compression and expansion and `W_lp += p·dV` during exhaust and intake, then reset at the cycle boundary. That gives imep_g, imep_n and pmep at once. Use the trapezoidal rule, `0.5·(p_k + p_{k+1})·(V_{k+1} − V_k)`.
- **Sign convention:** Heywood defines mechanical efficiency against gross IMEP and puts PMEP inside "total friction" (tfmep = pmep + rfmep). Some industries (and GT-Power) report imep_n and fmep separately, excluding pumping. Pick one convention and show it in the UI.

### Gaps
- None material.

---

## Q6. Detailed piston-ring and bearing friction (Stribeck, hydrodynamic): worth it in real time?

### Takeaway
Physically based crank-angle-resolved models (Stribeck-curve rings, Petroff or short-bearing journal models) cost little if kept algebraic. Full Reynolds-equation ring-pack or bearing EHL solvers are not real-time viable and gain little for a game-grade or educational simulator. A middle path is a Stribeck friction coefficient per stroke, driven by the Hersey number.

### Cited Findings
- Simplified *instantaneous* (crank-angle-resolved) friction models of the piston–crank slider exist specifically for cycle simulation — [SAE 2011-01-0612, "Development of a Simplified Instantaneous Friction Model of the Piston-Crank-Slider Mechanism"](https://saemobilus.sae.org/articles/development-a-simplified-instantaneous-friction-model-piston-crank-slider-mechanism-internal-combustion-engines-2011-01-0612); and a simple physical model of mechanical losses — [SAE 2011-01-0610](https://saemobilus.sae.org/papers/a-simple-physical-model-ice-mechanical-losses-2011-01-0610) [V that they exist; content not retrieved]
- A diesel-specific friction model is in SAE 2006-01-0888 — [SAE 2006-01-0888](https://saemobilus.sae.org/papers/a-model-friction-losses-diesel-engines-2006-01-0888) [V that it exists]
- Lubrication basics [R; Heywood ICEF §13.3, standard tribology texts]:
  - **Stribeck curve:** the friction coefficient f is a function of the Hersey number `μ·U/W` (viscosity × sliding speed / load per unit length). Boundary regime f ≈ 0.08–0.15; mixed regime, falling to a minimum of ~0.001–0.01; hydrodynamic regime rising roughly ∝ (μU/W)^0.5 to ^1.
  - **Petroff journal-bearing law:** friction torque `T_f = 2π·μ·ω·R³·L/c` (R radius, L length, c radial clearance), power loss `T_f·ω`, valid for a lightly loaded, concentric bearing. Typical c/R ≈ 0.001.
  - Piston rings run near boundary or mixed lubrication at TDC and BDC (Sp → 0) and hydrodynamically mid-stroke. That gives friction-force spikes at the dead centres, largest near firing TDC because gas pressure loads the top ring.

### Inferences
- **A real-time-friendly ring model** (researcher-constructed): `F_ring(θ) = f_Stribeck(H(θ))·[F_tension + p_gas(θ)·π·B·h_ring,eff]`, with `H = μ(T_liner)·|Sp(θ)|/(F_load/(πB))` and an algebraic Stribeck fit such as `f = f_b·exp(−H/H₀) + c_h·H^0.5`. Cost is a few flops per step. Calibrate f_b, H₀ and c_h so the cycle mean matches the PNH rfmep.
- Bearings: Petroff with `μ(T_oil)` is enough. Eccentricity and short-bearing (Ocvirk) theory change results by a factor of order 1 but need load-direction tracking; skip unless bearing-load visualisation is a feature.
- **Recommendation:** use PNH/Sandoval for the mean level and the Stribeck ring model only to shape crank-angle-resolved torque ripple. Reynolds/EHL solvers (FRICTION-type ring-pack codes) take milliseconds to seconds per cycle and are not worth it in real time.

### Gaps
- Contents of SAE 2011-01-0610, 2011-01-0612 and 2006-01-0888 could not be retrieved; coefficients from those simplified instantaneous models are not provided.

---

## Q7. Accessory losses (oil pump, water pump, alternator, A/C) and motoring-torque data

### Takeaway
Accessories cost roughly 0.1–0.3 bar FMEP equivalent (afmep ≈ 8–25 kPa per PNH/Sandoval over 1000–6000 rpm). That is a few hundred W at idle rising to ~2–4 kW at high speed for a 2 L engine, plus the alternator load (electrical load / ~0.5–0.65 efficiency) and A/C (several kW when engaged). Motoring torque is FMEP·Vd/(4π).

### Cited Findings
- The PNH and Sandoval auxiliary terms cover oil pump, water pump and an uncharged alternator. Equations are in Q4: PNH `8.32 + 1.86e-3·N + 7.45e-7·N²` kPa; Sandoval `6.23 + 5.22e-3·N − 1.79e-7·N²` kPa [R]. That the model includes accessory losses is [V] — [MIT thesis](https://dspace.mit.edu/bitstream/1721.1/80657/2/52940875-MIT.pdf)
- Worked from the PNH equation (researcher's arithmetic): at 2000 rpm afmep ≈ 8.32 + 3.72 + 2.98 = 15.0 kPa; at 6000 rpm ≈ 8.32 + 11.2 + 26.8 = 46.3 kPa. Sandoval: 2000 rpm ≈ 6.23 + 10.4 − 0.72 = 15.9 kPa; 6000 rpm ≈ 6.23 + 31.3 − 6.4 = 31.1 kPa. The update therefore lowers high-speed accessory losses.
- Power conversion: `P_acc = afmep·Vd_total·N/(2·60)` (four-stroke, N in rpm). For a 2.0 L engine at 6000 rpm with Sandoval's ~31 kPa: P ≈ 31e3 × 2e-3 × 50 = 3.1 kW (researcher's arithmetic).
- No primary published measurements of individual oil-pump, water-pump or alternator power were retrieved in this session.

### Inferences
- **Suggested accessory sub-models** (engineering estimates, not sourced):
  - Alternator shaft power = P_elec/η_alt, with η_alt ≈ 0.5–0.65; idle electrical load ~300–600 W.
  - A/C compressor: 2–5 kW engaged.
  - Power-steering pump: ~0.3–1 kW (hydraulic).
  - Oil pump: P = Δp·Q/η, with Δp ≈ 2–5 bar, Q ∝ N until the relief valve opens.
  - Water pump: P ∝ N³ (fan law), ~0.2–1.5 kW across the speed range.

  Apply them as separate torque sinks for UI transparency. Make sure they do not double-count PNH's afmep: either use afmep or the individual models, not both.
- **Motoring torque:** `T_motor = (FMEP_total incl. PMEP)·Vd/(4π)` for a four-stroke. For example, a 2.0 L engine at 2000 rpm with tfmep ≈ 0.97 + 0.30 + 0.20 = 1.47 bar (Bishop/Heywood correlation) gives ≈ 1.47e5 × 2e-3/12.57 ≈ 23 N·m. This is a useful sanity check for "engine-off coast-down" and starter sizing.

### Gaps
- Measured motoring-torque curves and individual accessory power maps for specific production engines were not found or fetched; the values above are estimates.

---

## Q8. Engine performance metrics: definitions and formulas, plus heat-release analysis

### Takeaway
All metrics follow from cycle-integrated p·dV, brake torque, fuel and air mass flows, and LHV. The simulator should report imep_g, imep_n, pmep, fmep, bmep, η_m, η_i, η_b, BSFC, η_v (referenced to ambient and to intake manifold), λ/AFR, specific power, CA10/CA50/CA90 and an apparent heat-release curve. Heat release from simulated or measured pressure uses the single-zone first-law model (γ ≈ 1.3–1.35) or Rassweiler–Withrow.

### Cited Findings
All formulas below are from Heywood ICEF ch. 2 and §9.2 [R]. They are standard definitions found in every ICE text (Ferguson & Kirkpatrick; Stone; Guzzella & Onder).

```
nR = 2 (four-stroke), 1 (two-stroke); N in rev/s; Vd = total displacement [m³]
Brake power         P_b = 2π·N·T_b
Indicated work      W_c,i = ∮ p dV (per cylinder per cycle)
IMEP (gross/net)    imep = W_c,i / Vd,cyl     (gross: compr + exp strokes; net: full cycle)
BMEP                bmep = P_b·nR/(Vd·N) = 2π·nR·T_b/Vd = 4π·T_b/Vd (4-stroke)
                    handy: bmep[kPa] = 6.28·nR·T_b[N·m]/Vd[dm³]
FMEP                fmep = imep_g − bmep  (Heywood: includes pumping)  or  imep_n − bmep (excl. pumping)
PMEP                pmep = imep_g − imep_n
Mechanical eff.     η_m = P_b/P_i,g = bmep/imep_g
Fuel conv. eff.     η_f = P/(ṁ_f·Q_LHV)      (indicated or brake, by which P is used)
                    η_f = 3600/(sfc[g/kWh]·Q_LHV[MJ/kg])
BSFC                bsfc = ṁ_f/P_b   [g/kWh];  ISFC with P_i
Volumetric eff.     η_v = nR·ṁ_a/(ρ_a,i·Vd·N)
                    ρ_a,i = air density at inlet reference: ambient (overall η_v, includes
                    intake system losses) or intake manifold/port (η_v of cylinder + ports)
Air–fuel ratio      AFR = ṁ_a/ṁ_f; λ = AFR/AFR_s (AFR_s ≈ 14.6–14.7 gasoline, 14.5 diesel, 17.2 CH4, 9.0 ethanol)
Specific power      P_b/A_p (A_p = total piston area) [kW/m² or W/mm²] and P_b/Vd [kW/L]
Mean piston speed   S̄p = 2·L·N
```
- Typical ranges (Heywood ICEF 1st ed. Table 2.x/§2.7, now dated) [R]:
  - Max bmep: NA SI ~0.85–1.05 MPa; turbo SI ~1.25–1.7 MPa (modern downsized engines reach 2.0–2.5 MPa); NA diesel 0.7–0.9 MPa; turbo diesel 1.0–1.2 MPa (modern ≥2.0 MPa).
  - Best bsfc: SI ~250–270 g/kWh; DI diesel ~195–210 g/kWh.
  - η_v (NA): 0.8–0.9, exceeding 1.0 with tuned intakes.

**Heat-release analysis**
- **First-law, single-zone apparent net heat release** (Heywood ICEF eq. 9.27) [R]:
  `dQ_n/dθ = (γ/(γ−1))·p·dV/dθ + (1/(γ−1))·V·dp/dθ`, with γ ≈ 1.35 in compression, ~1.26–1.30 during and after combustion. Some codes use γ(T) from a linear fit, e.g. Gatowski et al. Gross heat release = net + wall heat transfer (from the Q1 correlation) + crevice losses.
- **Rassweiler–Withrow (1938)** mass-fraction-burned method [R]:
  ```
  Δp_c,i = p_{i+1} − p_i·(V_i/V_{i+1})^n        (pressure rise due to combustion over step i;
                                                n = polytropic index ≈ 1.3, fit from compression/expansion log-p–log-V)
  Δp*_c,i = Δp_c,i·(V_i/V_c)                    (refer to a common volume, e.g. TDC clearance volume)
  x_b(i) = Σ_{k=0..i} Δp*_c,k / Σ_{k=0..end} Δp*_c,k
  ```
- **Combustion phasing:** CA10, CA50 and CA90 are the crank angles where x_b = 0.1, 0.5 and 0.9. The burn duration is CA10–90. For SI engines, MBT timing typically puts CA50 at ~8–10° ATDC [R; Heywood ICEF ch. 9]. The Wiebe function `x_b = 1 − exp(−a·((θ−θ0)/Δθ)^(m+1))` with a ≈ 5, m ≈ 2 is the standard forward model (Heywood eq. 9.23) [R].

### Inferences
- Because the simulator knows the "true" heat release, show the "apparent" heat release computed from the simulated pressure next to the true fuel-burn rate. The difference is wall heat loss and γ error, which is an educational feature.
- Compute η_v twice: against ambient (p_amb, T_amb) and against intake manifold state. Label which is which, since sources and users conflate them.
- Use LHV for η_f by convention: gasoline ~43–44 MJ/kg, diesel ~42.5–43 MJ/kg [R].

### Gaps
- The typical-value tables come from the 1988 edition and understate modern downsized turbo engines; modern ranges are researcher estimates.

---

## Q9. Dynamometer correction standards: SAE J1349, ISO 1585, DIN 70020, EEC 80/1269, JIS D1001, SAE J607

### Takeaway
SAE J1349, ISO 1585, JIS D1001 and EEC 80/1269 share the reference condition of 25 °C and 99 kPa dry-air pressure. DIN 70020 uses 20 °C and 101.3 kPa total pressure, so it gives the highest corrected numbers. The SI-engine factor in J1349 is `CF = 1.18·(99/p_d)·(T/298)^0.5 − 0.18`, where 1.18/0.18 builds in an assumed ~85% mechanical efficiency. ISO 1585 / EEC uses `(99/p_d)^1.2·(T/298)^0.6`. DIN uses `(1013/p)·(T/293)^0.5`.

### Cited Findings
- J1349 reference: 25 °C (77 °F) and 99 kPa (990 mbar, 29.235 inHg) dry-air pressure, 0% humidity — [HP Academy forum](https://www.hpacademy.com/es/foro/introduccion-al-tuning-de-motores-1/show/standard-temperature-and-altitude); [Eng-Tips thread](https://www.eng-tips.com/threads/temperature-correction-on-a-dyno.305418/latest) [V]
- SAE J1349, ISO 1585, JIS D1001 and EWG 80/1269 all use 990 mbar and 25 °C; DIN 70020 uses different reference conditions and a different equation — [DDI Software tech note, ambient correction factors](https://ddisoftware.com/tech-archive/tt-dyno/the-ambient-correction-factors/index.html) [V]
- J1349 AUG2004 §5.1 limits validity to about 15–35 °C and 900–1050 mbar dry-air pressure — [Wahiduddin engine tuner's calculator notes](https://www.wahiduddin.net/calc/calc_hp.htm) [V]
- Dry-air pressure = absolute barometric pressure minus water-vapour partial pressure; this is how humidity enters — [Bikeboy dyno correction factors](https://bikeboy.org/dynocorrectionfactors.html) [V]
- DIN 70020 reference: dry air 101,325 Pa and 293 K, humidity neglected, temperature exponent m = 0.5 — [Sandu & Ungureanu 2019, Transilvania Univ. Braşov](https://webbut.unitbv.ro/index.php/Series_I/article/download/1085/969) [V]
- Comparing DIN 70020, SAE J1349, JIS D1001 and ISO 1585 against measured road acceleration, SAE J1349 matched best — [J. Braz. Soc. Mech. Sci. Eng. 2003 (Aston repository)](https://research.aston.ac.uk/en/publications/comparison-of-engine-power-correction-factors-for-varying-atmosph/) [V]
- ISO 1585:2020 has a dedicated "power correction factors" clause (paywalled) — [ISO 1585:2020 catalogue (iTeh)](https://iteh.eu/catalog/standards/iso/14b70ffa-8765-4249-9418-874a1e17cfae/iso-1585-2020) [V that the clause exists]
- **Formulas** [R; widely reproduced, not verifiable here against the paywalled standards]:
  ```
  p_d = dry-air pressure [kPa] = p_baro − p_vapour;  T = intake air temperature [K]

  SAE J1349 (1990/2004, SI engines):  CF = 1.18·(99/p_d)·(T/298)^0.5 − 0.18
  SAE J607 (small engines, older):    CF = (29.92/p_d[inHg])·(T[°R]/520)^0.5   (60 °F, 29.92 inHg dry)
  DIN 70020:                          CF = (1013/p[mbar])·(T/293)^0.5           (total pressure, 20 °C)
  ISO 1585 / EEC 80/1269 / JIS D1001 (SI engines):
                                      CF = (99/p_d)^1.2·(T/298)^0.6
     validity: 0.93 ≤ CF ≤ 1.07 (otherwise report corrected value and conditions)
  ISO 1585 (CI / diesel):             CF = f_a^f_m
     f_a = (99/p_d)·(T/298)^0.7             NA and mechanically supercharged
     f_a = (99/p_d)^0.7·(T/298)^1.5         turbocharged (with or without charge-air cooling)
     f_m = 0.036·(q/r) − 1.14, clipped to 0.2 ≤ f_m ≤ 1.2
       q = fuel delivery [mg per cycle per litre displacement], r = boost pressure ratio
  Corrected power: P_corr = CF·P_observed  (J1349 strictly corrects indicated power, then subtracts friction;
  the 1.18/−0.18 form is that idea with η_m ≈ 0.85 baked in)
  ```

### Inferences
- In a simulator, CF is a reporting layer only: physics already responds to ambient p and T. Let the user pick the standard (SAE J1349 / ISO 1585 / DIN 70020 / uncorrected) and display p_d, T and CF. Warn when outside J1349's 15–35 °C / 90–105 kPa window.
- DIN 70020 gives roughly 4–5% higher figures than J1349 for the same run. The researcher computed this: (1013/990)·(298/293)^0.5 ≈ 1.023·1.0085 ≈ 1.032, and the 1.18 multiplier effect amplifies it further.

### Gaps
- The exact equations could not be fetched from SAE J1349, ISO 1585 or DIN 70020 (paywalled; supporting web pages failed DNS). The formulas above, including the ISO diesel f_m definition and the 0.93–1.07 validity window, are from recall [R] and should be checked against the standard texts. The search did not confirm the ISO exponents 1.2 and 0.6.
