# Validation and Engineering Practice for Engine Simulation Tools

Research notes on how professional engine simulation tools are built, validated and used, and what an open-source real-time engine simulator (engine-sim) needs to become a credible engineering tool.

Method note: about 20 web searches and fetch attempts (2026-10-11). Direct page fetches to epa.gov, dspace.com, data.gov and ddisoftware.com failed with DNS errors in this sandbox. Some findings therefore rest on search-result excerpts of those pages, not full reads, and are flagged where it matters. Analytical formulas that are standard textbook material, but were not confirmed from a fetched source in this session, are placed under "Inferences" and labelled "(textbook, not re-verified this session)".

---

## 1. Industry-standard tools, model hierarchies and workflows

### Takeaway
Commercial tools (GT-POWER, Ricardo WAVE, AVL BOOST, Lotus Engine Simulation) are all 1D gas-dynamics pipe networks coupled to 0D (single- or two-zone) cylinder models. Combustion is either imposed (Wiebe, or a burn rate taken from measured pressure) or predictive (quasi-dimensional). The same vendors derive faster models (GT "FRM", mean-value) from the detailed model for real-time/HiL use. Open-source options are limited: OpenWAM (1D, GPLv3, dormant since 2014), Cantera's 0D reactor engine example (illustration only), and OpenFOAM (3D CFD).

### Cited Findings
**GT-POWER / GT-SUITE (Gamma Technologies)**
- GT-POWER provides "Three-Pressure Analysis" (TPA). It takes measured intake, cylinder and exhaust pressure traces to analyze gas exchange and combustion, and it "results in accurate calculation of cylinder contents at IVC" (trapped mass, residuals). — [GT-SUITE Cylinder Pressure Analysis](https://www.gtisoft.com/gt-suite-applications/propulsion-systems/cylinder-pressure-analysis/?mode=grid)
- Measured cylinder pressure is analyzed directly in GT-POWER, and the resulting burn rates are computed "using the same solution methodology as the engine model". This keeps the measured burn rate consistent with the simulation. GT also offers a "measured+predicted" analysis to calibrate predictive combustion against measured data. — [GT Cylinder Pressure Analysis](https://www.gtisoft.com/gt-suite-applications/propulsion-systems/cylinder-pressure-analysis/?mode=grid)
- GT training covers TPA, in-cylinder flow, predictive SI combustion model calibration, knock, emissions and cycle-to-cycle variation (CCV) as one workflow. — [GT event: Advanced topics in predictive SI combustion](https://www.gtisoft.com/event/advanced-topics-in-predictive-si-combustion/)
- Typical GT calibration workflow (PoliTo thesis, light-duty diesel):
  1. Run a non-predictive "Cylinder Pressure Only Analysis" (CPOA) to get burn rates from measured pressure.
  2. Use those imposed burn rates as targets for calibrating the predictive (DiPulse) combustion model on a subset of operating points, minimizing burn-rate RMS error and IMEP error.
  — [PoliTo thesis](https://webthesis.biblio.polito.it/16281/1/tesi.pdf); see also [TPA automatic calibration methodology thesis](https://webthesis.biblio.polito.it/7906/)
- A KTH/CCGEx SI study calibrated a GT-Power model on 22 calibration points and checked it on 12 separate verification points. The target function minimized burn-rate error. — [KTH CCGEx day 2017](https://www.ccgex.kth.se/polopoly_fs/1.752537.1600688541!/SenthilKMahendar_2017_CCGEXday.pdf)
- GT "Fast Running Models" (FRMs) are derived from detailed models for integrated, real-time, SiL and HiL simulation, and run much faster than the detailed GT-POWER model. — [GT training: FRMs for Integrated Simulations](https://www.gtisoft.com/training_catalog/fast-running-engine-models-frms-for-integrated-simulations-including-real-timesilhil/); [GT event "From 1D to 0D FRM"](https://www.gtisoft.com/event/from-1d-to-0d-frm-gt-power-model-for-driving-cycles-and-controls-milsilhil-free-7/)

**Ricardo WAVE / AVL BOOST / Lotus**
- AVL BOOST is widely used with a "two-zone Wiebe" 0D combustion model calibrated against test data. Several academic papers use BOOST's Wiebe-2-Zone model to predict SI performance. — [UPB Scientific Bulletin](https://scientificbulletin.upb.ro/static/pdfs/full7cc_457107.pdf); [UPB Scientific Bulletin 2](https://www.scientificbulletin.upb.ro/static/pdfs/reze37_840862.pdf)
- Alqahtani et al. (2015) directly compare AVL BOOST and Ricardo WAVE for simulating HCCI and SI GDI engines. — [Alqahtani et al. 2015 (PDF)](https://yadda.icm.edu.pl/baztech/element/bwmeta1.element.baztech-44b25b10-5d58-4657-8e89-6b641d5b3170/c/alqahtani_therodynamic_ce_2_2015.pdf)
- Software-comparison studies (Lotus, AVL BOOST, Ricardo WAVE) find that in-cylinder pressure and temperature profiles differ between packages. The differences come from different submodel implementations, mainly combustion and heat transfer. — [Comparison of Engine Simulation Software for Development of Control System (academia.edu)](https://www.academia.edu/57739777/Comparison_of_Engine_Simulation_Software_for_Development_of_Control_System); [Scribd: chapter 4 engine simulation](https://www.scribd.com/document/645081272/chapter-4-engine-simulation)
- Lotus Engine Simulation is a commercial performance-prediction tool in a suite with Lotus Vehicle Simulation and Lotus Concept Valve Train. — [Manufacturing Management: Lotus thrusts out CAE packages](https://www.manufacturingmanagement.co.uk/news/lotus-thrusts-out-cae-packages)
- Forum users (c.2002–2005) report that a freeware Lotus version was limited to single-cylinder models but still covered combustion, friction, airflow, scavenging, intake/exhaust effects and heat transfer. Current terms are unverified, and this is a low-quality source. — [f1technical forum](https://www.f1technical.net/forum/viewtopic.php?p=15008); [eng-tips](https://eng-tips.com/threads/engine-software.141413)
- Performance Trends ("Engine Analyzer Pro" vendor) sells hobbyist/race engine simulation software. — [Performance Trends Engine Simulation](https://performancetrends.com/Engine-Simulation.htm)

**Open-source tools**
- OpenWAM is a free, open-source 1D gas-dynamics code for ICEs from CMT-Motores Térmicos (UPV, Valencia).
  - Origins and method: started in 1984 with the Method of Characteristics, later replaced by "a finite difference scheme that is faster and less diffusive".
  - Scope: air management, compressible flow, turbocharging, chemical species tracking.
  - Implementation and license: C++, GPLv3.
  - Latest listed release: v2.1, dated 2014-12-24.
  — [OpenWAM on SourceForge](https://www.sourceforge.net/projects/openwam); [OSDN releases](https://osdn.net/projects/sfnet_openwam/releases); [OpenWAM docs (UPV)](https://openwam.webs.upv.es/docs/)
- Cantera's `ic_engine.py` is a 0D reactor-network diesel-type engine using n-dodecane injected near TDC (3000 rpm, 0.5 L, CR 20). The docs say it "uses numerous simplifying assumptions and thus serves for illustration purposes only". Older versions used propane at an unrealistic CR of 50. — [Cantera 3.2 ic_engine example](https://www.cantera.org/3.2/examples/python/reactors/ic_engine.html); [Cantera 2.3 example](https://cantera.org/2.3/sphinx/html/cython/examples/reactors_ic_engine.html)

### Inferences
- The common architecture across all vendors has five parts:
  1. A 1D pipe network (intake, exhaust and runners), solved with finite-volume or finite-difference methods.
  2. Junctions and valves with discharge coefficients.
  3. A 0D cylinder model: single-zone for gas exchange and TPA, two-zone (burned/unburned) for knock and NOx.
  4. A combustion submodel hierarchy: imposed Wiebe → measured burn rate (TPA) → predictive quasi-dimensional (turbulent flame).
  5. Mechanical friction correlations.
  engine-sim already has a pipe/plenum gas system and a single-zone cylinder. The fastest route to credibility is to (a) add two-zone and Wiebe/imposed-burn-rate options, and (b) add a measured-pressure-analysis (reverse heat-release) mode that uses the same solver, mirroring GT's TPA/CPOA philosophy.
- Workflows engineers run with these tools: steady-state WOT/part-load speed sweeps ("virtual dyno"), cam and runner-length DoE, optimization, transient (load step, tip-in), and FRM export for HiL. Of these, only FRM/HiL use and the GT calibration workflow are directly evidenced above. DoE and optimization are generally known vendor workflows that I did not confirm from a fetched source.
- OpenWAM is dormant (last release 2014), so a maintained open-source real-time 1D engine code would fill a genuine niche.

### Gaps
- Ricardo WAVE and AVL BOOST/CRUISE official model lists and documentation were not retrieved. Their documentation is behind customer portals.
- No primary source was fetched for OpenFOAM engine solvers (e.g. `engineFoam`), MATLAB/Simulink engine models, Enginuity, or Engine Analyzer Pro internals.
- Lotus freeware availability (lesoft.co.uk) is currently unverified.

---

## 2. Verification tests with analytical solutions

### Takeaway
A credible engine code should pass a suite of code-verification tests against closed-form solutions before it is validated against engine data:
- isentropic compression
- ideal Otto/Diesel efficiency
- choked and subsonic orifice flow
- the Sod shock tube (1D Euler)
- Helmholtz and quarter-wave resonance
- crank-slider kinematics
- exact mass and energy conservation in closed and adiabatic systems

The Sod problem is the canonical 1D gas-dynamics check. Reference setups (γ=1.4, 100–200 cells, t=0.2) and analytical Riemann solutions are widely published.

### Cited Findings
- The Sod (1978) shock tube is "often used as a one-dimensional test case to check the ability of a compressible code to capture shocks, contact surfaces, and rarefaction waves". The Cholla reference setup uses γ=1.4, 100 cells and output at t=0.2. — [Cholla 1D Sod docs](https://cholla.readthedocs.io/en/latest/ChollaExamples/1D-Sod/overview.html)
- A finite-volume HLLC solver with minmod and superbee limiters matched the analytical Riemann solution at N=200 cells. Superbee was least diffusive but overshot slightly at the contact discontinuity, which shows how limiter choice can be judged on Sod. — [Preprints.org Sod FV benchmark](https://www.preprints.org/frontend/manuscript/1e397aff66898b0dec0c4533b9074d6d/download_pub)
- E.F. Toro's lab material, *Riemann Solvers and Numerical Methods for Fluid Dynamics*, provides the exact Riemann solver and standard test suite used to verify 1D Euler codes. — [Toro lab session (UMA)](https://edanya.uma.es/NSPDE/images/ef%20toro/lab%20session.pdf)
- MindSpore's CFD tutorial gives the same Euler formulation and time-marching loop for Sod, useful as a second reference implementation. — [MindFlow Sod tube](https://www.mindspore.cn/mindflow/docs/en/r0.3/cfd_solver/sod_tube.html)
- OpenWAM's own move from the Method of Characteristics to a "faster and less diffusive" finite-difference scheme shows that numerical diffusion of pressure waves is the key quality metric for engine 1D codes. — [OpenWAM SourceForge](https://www.sourceforge.net/projects/openwam)

### Inferences
Proposed verification suite. Formulas are textbook (Heywood; Anderson; Kinsler), not re-verified this session.

| # | Test | Expected / analytical result | Pass criterion (suggested) |
|---|------|-----------------------------|----------------------------|
| V1 | Adiabatic, frictionless, sealed motored cylinder | p·V^γ = const (constant γ); T·V^(γ−1) = const; motored IMEP = 0 and p-V loop closes exactly | Peak p within 0.1%; net loop work ≈ 0 to machine tolerance × steps |
| V2 | Ideal Otto (instant heat addition at TDC, constant γ) | η = 1 − r^(1−γ); r=10, γ=1.4 → 0.602 | Within 0.5% |
| V3 | Ideal Diesel / dual cycle | η = 1 − (1/r^(γ−1))·(rc^γ − 1)/(γ(rc − 1)) | Within 0.5% |
| V4 | Choked orifice | ṁ = Cd·A·p0/√(R·T0)·√γ·(2/(γ+1))^((γ+1)/(2(γ−1))); choking at p*/p0 = 0.528 (γ=1.4) | Within 0.5% for ratios < 0.528; flow independent of downstream p |
| V5 | Subsonic orifice | Compressible isentropic nozzle equation | Within 1% across pressure ratio 0.53–1.0 |
| V6 | Sod shock tube | Exact Riemann solution (Toro) | L1 density error decreasing with first-/second-order convergence; shock position within 1–2 cells |
| V7 | Helmholtz resonator | f = (c/2π)·√(A/(V·L_eff)), with end correction | Peak frequency within 2–3% |
| V8 | Quarter-wave closed–open pipe | f_n = (2n−1)·c/(4·L_eff), L_eff ≈ L + 0.6·r (unflanged) | Within 2% |
| V9 | Half-wave open–open pipe | f_n = n·c/(2L_eff) | Within 2% |
| V10 | Crank-slider kinematics | x(θ) = r·cosθ + √(l² − r²·sin²θ); V(θ) = Vc + (π·B²/4)·(l + r − x); dV/dθ analytic | Exact to round-off |
| V11 | Global conservation | Closed network: Σm constant; adiabatic, rigid: Σ(m·u) + ΣW constant | Relative drift < 1e−6 per cycle (FV conservative scheme) |
| V12 | Constant-volume equilibration of two plenums | Final p, T from mass and energy balance | Exact to tolerance |
| V13 | Steady pipe friction / Fanno flow | Fanno relations | Within 1–2% |
| V14 | Time-step / grid convergence | Observed order of accuracy (Richardson) | Matches scheme's design order |

- These tests suit automated unit tests because each has a scalar pass/fail metric. V6–V9 exercise the gas system directly, which is the part of engine-sim most responsible for its sound and VE behaviour.

### Gaps
- I found no engine-specific published verification suite (e.g. a GT or WAVE "verification manual") with tolerances. The tolerances above are suggested engineering practice, not cited standards.
- Sources for the Helmholtz/quarter-wave end-correction constants and the Fanno relations were not fetched this session. Use Kinsler & Frey (*Fundamentals of Acoustics*) and Heywood.

---

## 3. Validation datasets: public measured engine data

### Takeaway
The best open validation source is the US EPA (NVFEL/ATAB) engine benchmarking program. It publishes:
- complete ALPHA BSFC maps (spreadsheet, images and MATLAB file)
- raw steady-state benchmarking test-data packages for modern production engines, e.g. Toyota A25A-FKS 2.5L, Mazda 2.5L Skyactiv-G, Honda 1.5L L15B7 turbo, Ford 1.6L EcoBoost

These come with SAE papers describing the test setup. For crank-angle-resolved cylinder pressure, the Sandia Engine Combustion Network (ECN) hosts pressure-trace files. Coverage there is mostly spray and optical-engine conditions, not full production engine maps.

### Cited Findings
**EPA ALPHA maps and benchmarking packages**
- EPA publishes zipped "ALPHA Map Package" files for specific engines, in Tier 2 and/or Tier 3 fuel versions:
  - 2018 Mazda 2.5L Skyactiv-G
  - 2018 Toyota 2.5L A25A-FKS
  - 2016 Honda 1.5L L15B7
  - 2013 Ford 1.6L EcoBoost
  Each package includes map images, a spreadsheet of tabular data, a document describing how the data were combined, and the MATLAB input file used in ALPHA. The maps cover the full operating boundary: idle, WOT, minimum motoring torque and maximum speed. Users are asked to cite the citation included in each document. — [EPA: Combining Data into Complete Engine ALPHA Maps (snapshot)](https://19january2025snapshot.epa.gov/vehicle-and-fuel-emissions-testing/combining-data-complete-engine-alpha-maps); [EPA NVFEL Science & Technology Development](https://epa.gov/vehicle-and-fuel-emissions-testing/science-and-technology-development-national-vehicle-and-fuel)
- Raw partial-map test data are released separately as "benchmarking test data packages", which include at least partial steady-state engine maps. — [EPA: Combining data (snapshot)](https://19january2025snapshot.epa.gov/vehicle-and-fuel-emissions-testing/combining-data-complete-engine-alpha-maps); [EPA Benchmarking Advanced Low Emission LD Vehicle Technology](https://19january2021snapshot.epa.gov/vehicle-and-fuel-emissions-testing/benchmarking-advanced-low-emission-light-duty-vehicle-technology_.html)
- EPA lists a "2018 Toyota 2.5L A25A-FKS Engine Tier 2 Fuel – Test Data Package – Dated 07-28-20 (ZIP)" and a Tier 3 equivalent.
  - The engine is a 4-cylinder, 2.5 L, naturally aspirated Atkinson-cycle engine with cooled EGR.
  - Recorded channels include engine and transmission torque, fuel flow, key temperatures and pressures, OBD data and CAN bus data.
  - It is documented in SAE 2019-01-0249 (Kargul, Stuhldreher, Barba, Schenk et al.), which EPA hosts as a free PDF.
  — [SAE 2019-01-0249 PDF on epa.gov](https://www.epa.gov/sites/default/files/2020-08/documents/sae-2019-01-0249.pdf); [SAE Mobilus 2019-01-0249](https://saemobilus.sae.org/content/2019-01-0249); [EPA presentation](https://19january2021snapshot.epa.gov/sites/static/files/2019-04/documents/sae-2019-01-0249-benchmarking-2018-toyota-camry-2.5-liter-atkinson-cycle-engine-cooled-egr-presentation.pdf)
- The dataset is also registered on Data.gov as "Data package from benchmarking a Toyota A25A-FKS engine at EPA NVFEL", linked to DOI 10.4271/2019-01-0249. — [catalog.data.gov entry](https://catalog.data.gov/dataset/data-package-from-benchmarking-a-toyota-a25a-fks-engine-at-epa-nvfel)
- EPA also published a procedure for generating engine fuel-consumption maps for future Atkinson engines. — [EPA PDF: procs gen eng fuel cons map fut atkinson eng](https://19january2021snapshot.epa.gov/sites/static/files/2016-11/documents/procs-gen-eng-fuel-cons-map-fut-atkinson-eng.pdf)
- The companion SAE 2020-01-1286 (UB80E 8-speed transmission) used the same A25A-FKS engine in an engine dyno cell. — [EPA PDF SAE 2020-01-1286](https://www.epa.gov/sites/default/files/2020-05/documents/sae-2020-01-1286-benchmark-2018-toyota-camry-ub80e-eight-speed-auto-trans.pdf)

**Sandia Engine Combustion Network (ECN)**
- ECN is "an internet library for data storage intended to be used for CFD model improvement and validation", started by Lyle Pickett (Sandia) and modelled on the TNF workshop. — [ECN2 intro (Sandia)](https://ecn.sandia.gov/workshop/ECN2/ECN2Intro.pdf); [DOE DEER 2009 Pickett](https://www.energy.gov/sites/prod/files/2014/03/f8/deer09_pickett.pdf)
- ECN has a "Measurements" section with "Pressure Trace Data Files" (e.g. motored file `mot_100.45kPa_anl.xls`), Diesel, Gasoline and CFD data searches, Spray G/M data, and a hydrogen-engine section with a cylinder-head solid model. — [ECN page p=370](https://ecn.sandia.gov/?p=370); [ECN page p=364](https://ecn.sandia.gov/?p=364); [ECN hydrogen engine head model](https://ecn.sandia.gov/engines/hydrogen-engine/cylinder-head-solid-model/)

### Inferences
- **Licensing:** EPA data are US federal government works, generally public domain within the US, with a citation request in each package. ECN data are publicly posted for model validation. I did not find an explicit licence statement for either; check the package README before redistributing in the repo.
- **Recommended validation ladder for engine-sim:**
  1. **Motored pressure traces** (ECN motored files or EPA data where available) to validate compression, heat transfer and blow-by without combustion.
  2. **EPA A25A-FKS WOT torque curve, VE (from airflow) and BSFC map.** This is a modern NA port/DI engine with published geometry in SAE 2019-01-0249.
  3. **EPA turbo engine** (Ford 1.6 EcoBoost or Honda L15B7) once boosting exists.
  4. **Textbook figures** (Heywood) for qualitative trends (VE vs speed and runner length, η vs CR).
- An EPA-derived engine definition file (e.g. `assets/engines/toyota_a25a.mr`) plus a regression test comparing simulated WOT torque/BSFC against the EPA map would be a concrete, citable validation case.

### Gaps
- I could not open the EPA ZIPs (DNS blocked), so these are unconfirmed: whether any EPA package contains crank-angle-resolved cylinder pressure, the exact file formats, and the current list of packages on the live page (the 2025 snapshot may list fewer than live).
- The "GM LS" and "Toyota 2AR-FE" public datasets mentioned in the brief were not found in this session.
- University open engine datasets (e.g. ETH, TU) were not located.
- Heywood's textbook data tables are copyrighted. Figures can be cited, but they cannot be redistributed as data files.

---

## 4. Accuracy expectations and calibration practice

### Takeaway
Calibrated 1D models (GT-POWER, BOOST) typically match measured torque, power, VE and BSFC to within roughly 2–5%, with worst points up to about 7%. Errors above about 10% are considered poor. Mean-value models can reach about ±2% on steady maps (Hendricks). Calibration usually tunes:
- combustion (Wiebe m, duration, CA50/CA90, or predictive-model constants)
- heat-transfer (Woschni) constants and multipliers
- valve and port discharge coefficients (from flow-bench data)
- friction (FMEP) correlations
Calibration fits on a subset of points and is checked on held-out points.

### Cited Findings
- A calibrated GT-Power model of an inline-4 SI engine (SAE 2019-36-0170) reported errors for power, torque, VE and fuel consumption no greater than 7%. The authors describe 1D models as typically having errors around 5%. — [SAE 2019-36-0170](https://saemobilus-test.cld.sae.org/papers/numerical-model-si-engine-using-gt-power-code-2019-36-0170)
- A 1.6 L 4-cylinder SI GT-Power model, after correlation, achieved VE, manifold pressure, back pressure and BSFC within 4% of test data. — [UKM repository](https://ptsldigital.ukm.my/handle/123456789/394137?mode=full)
- Counter-example: a turbocharged diesel GT-Power study reported RMS errors of 10% (power) and 39% (torque), showing that "validated" claims vary widely in quality. — [Jurnal Teknologi (UTM)](https://journals.utm.my/jurnalteknologi/article/view/9197)
- Hendricks & Sorenson (SAE 900616): a three-state nonlinear mean-value SI engine model matched dyno measurements to about ±2% across the full speed/load range, with similar accuracy in fast transients, and few parameters. — [SAE 900616](https://saemobilus.sae.org/content/900616)
- Mean-value diesel models reach about 6% max error on mass flow and manifold pressures (static verification). An extended model cut mean intake manifold pressure error from 5.8% to 3.0%. — [Linköping report 04_R_2666_JB](https://www.fs.isy.liu.se/Publications/Reports/04_R_2666_JB.pdf); [Linköping MSc 03_EX_3443](https://www.fs.isy.liu.se/Publications/MSc/03_EX_3443_OF_MG.html)
- An open-source mean-value heavy-duty diesel model with validation was published in Simulation Notes Europe (2018). — [SNE 28(4)](https://sne-journal.org/sne-volumes/volume-28/sne-284-articles/modeling-and-validation-of-an-open-source-mean-value-heavy-duty-diesel-engine-model)
- Calibration targets and parameters:
  - Combustion and heat-transfer model parameters were used as decision variables in an optimization. Calibrated pressure traces reached R² > 0.99 on three engines. — [Aston: cylinder pressure based calibration](https://research.aston.ac.uk/en/publications/cylinder-pressure-based-calibration-model-for-engines-using-ethan)
  - The recommended Wiebe tuning strategy adjusts the shape parameter m, CA90 and a correction parameter b. — [Aston publication](https://publications.aston.ac.uk/id/document/81427)
  - Wiebe (burn) and Woschni (heat-transfer) constants must be identified per engine. Differential evolution was used to fit them jointly. — [ABCM CIT20 0563](https://abcm.org.br/proceedings/view/CIT20/0563); [bibliotekanauki](https://bibliotekanauki.pl/articles/949481.pdf)
  - Four Wiebe parameters are needed per operating point, and they were correlated to 1D outputs for two-stroke engines. — [UPV RiuNet 10251/57537](https://riunet.upv.es/handle/10251/57537)
- GT calibration practice uses TPA/CPOA burn rates as targets, minimizes burn-rate RMS and IMEP error, and keeps calibration and verification point sets separate (22 vs 12 points in the KTH study). — [PoliTo thesis](https://webthesis.biblio.polito.it/16281/1/tesi.pdf); [KTH CCGEx](https://www.ccgex.kth.se/polopoly_fs/1.752537.1600688541!/SenthilKMahendar_2017_CCGEXday.pdf)

### Inferences
- **Suggested acceptance targets for engine-sim's validation report:**
  - WOT torque and VE: within ±5% at each speed, aiming for ±3%.
  - BSFC: within ±5%.
  - Peak cylinder pressure: within ±5%.
  - Location of peak pressure: within ±2 °CA.
  - Exhaust temperature: within ±30–50 K.
  - Report RMS and maximum error. Never report only the best point.
- **Calibration parameter set, in order:**
  1. Geometry, which must come from measurement and not be tuned.
  2. Valve Cd/flow-bench curves and lift profiles.
  3. Pipe friction and wall heat transfer.
  4. In-cylinder heat transfer multiplier (Woschni C1/C2 scale).
  5. Combustion (Wiebe a, m, duration, SOC/CA50).
  6. Friction (Chen-Flynn FMEP constants).
  7. Blow-by.
  Tuning more than a handful of parameters per point overfits. Fit on a subset and verify on the rest.

### Gaps
- No vendor-published official accuracy specification (e.g. "GT-POWER predicts VE within X%") was found. The figures above come from individual studies.
- No source specifically on discharge-coefficient sensitivity was retrieved.

---

## 5. Uncertainty and sensitivity: which inputs matter most

### Takeaway
Published calibration studies consistently identify a small set of high-leverage inputs:
- combustion phasing and shape (Wiebe m, CA50, CA90)
- in-cylinder heat-transfer constants (Woschni)
- trapped mass, which depends on valve timing/lift and discharge coefficients
- residual gas fraction
Model-to-model differences between commercial packages are driven mainly by the combustion and heat-transfer submodels.

### Cited Findings
- Simulated pressure and temperature profiles differ between commercial packages (Lotus, BOOST, WAVE), mainly because of differing submodel implementations. — [academia.edu comparison](https://www.academia.edu/57739777/Comparison_of_Engine_Simulation_Software_for_Development_of_Control_System); [Scribd chapter](https://www.scribd.com/document/645081272/chapter-4-engine-simulation)
- Sensitivity to combustion phasing (via the Wiebe shape factor at a fixed efficiency factor) dominates predicted performance. Fuel-specific coefficients can cover a broad range of operating conditions. — [IISc producer-gas sensitivity paper](https://cgpl.iisc.ac.in/dasappa/img/pdf/papers/Sensitivity%20analysis%20of%20mixture%20quality%20on%20combustion.pdf)
- TPA exists because accurate trapped mass and residuals at IVC are the prerequisite for any combustion calibration. — [GT Cylinder Pressure Analysis](https://www.gtisoft.com/gt-suite-applications/propulsion-systems/cylinder-pressure-analysis/?mode=grid)
- Woschni constants and Wiebe constants "are dependent on constants that can be adjusted to a specific engine". — [ABCM CIT20 0563](https://abcm.org.br/proceedings/view/CIT20/0563)

### Inferences
- engine-sim should provide a built-in one-at-a-time (OAT) sensitivity sweep, plus optionally Morris or Sobol screening. It would perturb each input by ±5–10% and report ∂(torque, VE, BSFC)/∂input. This is cheap if the simulator can run faster than real time headless.
- Measured-data uncertainty also matters:
  - cylinder pressure pegging and TDC phasing errors (a 1 °CA TDC error noticeably shifts IMEP)
  - fuel flow and torque transducer accuracy
  - ambient correction
  This is textbook practice; I found no fetched source quantifying it this session.

### Gaps
- No formal global sensitivity study (Sobol) of a 1D engine model was retrieved.
- A quantified TDC-phasing error vs IMEP source was not retrieved.

---

## 6. Engineering outputs and workflows a credible tool needs

### Takeaway
Engineers expect four kinds of output from an engine tool:
- **Crank-angle-resolved diagnostics:** P-θ, log P–log V, heat release / MFB, CA10/50/90.
- **Cycle-integrated metrics:** IMEP gross/net, PMEP, FMEP, BMEP, VE, trapping and scavenging ratios, BSFC, indicated and brake efficiency, exhaust temperature, knock margin, emissions estimates.
- **Automated steady-state sweeps:** speed sweeps and maps with CSV export.
- **Standard ambient correction:** SAE J1349 at 99 kPa dry air and 25 °C.

### Cited Findings
- GT-POWER's cylinder pressure analysis outputs burn rates, trapped conditions, and combustion metrics such as knock and CCV in its predictive-combustion workflow. — [GT Cylinder Pressure Analysis](https://www.gtisoft.com/gt-suite-applications/propulsion-systems/cylinder-pressure-analysis/?mode=grid); [GT predictive SI event](https://www.gtisoft.com/event/advanced-topics-in-predictive-si-combustion/)
- EPA ALPHA maps cover fuel consumption across the full operating envelope (idle, WOT, minimum motoring torque and maximum speed). This is the form downstream vehicle models consume. — [EPA combining data (snapshot)](https://19january2025snapshot.epa.gov/vehicle-and-fuel-emissions-testing/combining-data-complete-engine-alpha-maps)
- SAE J1349 reference conditions are 99 kPa dry-air pressure and 25 °C inlet air, with recommended test-range limits of 90–105 kPa and 15–40 °C. — [eng-tips thread citing J1349](https://www.eng-tips.com/threads/temperature-correction-on-a-dyno.305418/post-7496838); [wahiduddin.net HP correction](https://wahiduddin.net/calc/calc_hp_dp.htm)
- The J1349 correction is not intended for extreme ranges; the intended dry-air range is 900–1050 mbar. The dyno correction factor is the reciprocal of relative horsepower. — [wahiduddin.net](https://wahiduddin.net/calc/calc_hp_abs_dp.htm); [wahiduddin cf](https://wahiduddin.net/calc/calc_cf.htm)
- J1349 (2004+) uses dry-air pressure, so it accounts for humidity, and uses 77 °F/537 °R (298 K) as reference, unlike older 60 °F standards. Simplified calculators can differ by 1–2 percentage points in humid weather. — [calculover dyno correction](https://calculover.com/fr/automotive/performance/dyno-correction/)
- Some dyno shops reportedly still use the older SAE J607 "STD" factor, which gives higher numbers (unverified forum claim). — [eng-tips thread](https://www.eng-tips.com/threads/temperature-correction-on-a-dyno.305418/latest)

### Inferences
- **J1349 formula:** CF = 1.18 × (99 / p_d[kPa]) × √((T[°C] + 273) / 298) − 0.18. The 1.18/−0.18 form assumes 85% mechanical efficiency. It is widely reproduced, but I could not confirm it from a fetched copy of the standard this session; verify against SAE J1349 before shipping.
- **Minimal output set for engine-sim's "engineering mode":**
  - **Per cycle and per cylinder:**
    - IMEPg, IMEPn, PMEP, FMEP, BMEP (= IMEPn − FMEP)
    - Pmax and θ@Pmax; CA10/50/90; burn duration
    - VE: referenced to ambient and to intake manifold, plus trapped mass
    - Residual fraction; AFR/λ
    - Indicated and brake efficiency; ISFC/BSFC
    - Exhaust gas temperature; heat loss fraction
    - Knock index (e.g. Livengood-Wu integral); COV of IMEP
  - **Plots:** P-θ, P-V and log-log P-V, dQ/dθ and MFB, valve lift and flow, runner pressure waves.
  - **Workflows:**
    - Automated WOT speed sweep: settle → average N cycles → record → export CSV.
    - Part-load map: throttle or load target.
    - Parameter sweep / DoE (cam timing, runner length) and batch/headless CLI mode.
    - Corrected power (J1349, plus DIN 70020/ISO 1585 options).
  - **Emissions estimates** (Zeldovich NO from two-zone burned gas, simple CO/HC correlations) are "nice to have" and require two-zone first.

### Gaps
- An authoritative copy of SAE J1349 (paywalled) was not accessed.
- No primary source for the Livengood-Wu knock integral or the Chen-Flynn friction correlation was fetched this session (both standard in Heywood).

---

## 7. Numerical best practices: time step, convergence, conservation, determinism, testing

### Takeaway
Engine 1D codes use explicit finite-volume or finite-difference schemes with time steps bounded by the CFL condition. They run multiple cycles until cycle-to-cycle convergence of key outputs, and judge quality by numerical diffusion of pressure waves. FRMs gain speed by enlarging the time step and reducing computations per step. A credible open-source tool should add:
- explicit convergence criteria
- conservation-error monitors
- deterministic, seedable runs
- a layered test suite: unit → analytical verification → regression against stored reference results → validation against measured data

### Cited Findings
- FRMs gain speed by enlarging the simulation time step and reducing the number of calculations per step. The user chooses the accuracy-vs-runtime trade-off. — [Chalmers thesis: Real Time Fast Running Engine Modelling in GT Power](https://odr.chalmers.se/items/ceb18c2f-d5ae-413b-9c66-bffae4f018d4/full)
- OpenWAM's finite-difference scheme replaced the Method of Characteristics because it was faster and less diffusive. Diffusion and dispersion of waves is the critical numerical quality metric. — [OpenWAM SourceForge](https://www.sourceforge.net/projects/openwam)
- Limiter choice trades diffusion against overshoot (minmod vs superbee) at contact discontinuities on the Sod test. — [Preprints.org Sod FV benchmark](https://www.preprints.org/frontend/manuscript/1e397aff66898b0dec0c4533b9074d6d/download_pub)
- Results from unstable wave problems may not reproduce across codes, so verification should use stable, well-posed cases such as Sod. — [Caltech EDL supplemental](https://shepherd.caltech.edu/EDL/publications/reprints/LIFSupplemental.pdf)

### Inferences
- **Time step:** Δt ≤ CFL·Δx / (|u| + c), with CFL ≈ 0.5–0.9 for explicit schemes. Also cap Δθ (e.g. ≤ 0.5–1 °CA) so valve events and combustion are resolved. At 6000 rpm, 1 °CA = 27.8 µs. engine-sim's fixed audio-rate stepping, around 10–20 kHz, is coarser than typical 1D codes, so it should report the effective CFL and °CA per step.
- **Steady-state convergence:** run until consecutive-cycle change is below a threshold, e.g. |ΔIMEP|/IMEP < 0.1–0.5% and |ΔVE| < 0.2% over 3 consecutive cycles, with a maximum cycle cap. Then average the next N cycles for reporting. Report the convergence status in the CSV.
- **Conservation monitors:** track cumulative mass imbalance (Σinflow − Σoutflow − Δm_stored) and energy imbalance per cycle, and flag when they exceed 0.1% of trapped mass or fuel energy.
- **Determinism:** any stochastic elements (e.g. engine-sim's turbulence/CCV noise for sound) must use seeded PRNGs, with a "deterministic engineering mode" that disables noise or fixes the seed. This makes regression tests bit-reproducible on a given platform and tolerance-reproducible across compilers.
- **Testing strategy:**
  1. Unit tests for pure functions (kinematics, gas properties, orifice flow).
  2. Analytical verification tests V1–V14 (Section 2).
  3. Golden-file regression tests: stored CSV of a reference engine sweep, compared with relative tolerances. Run in CI.
  4. Validation tests against EPA data, reported as a dashboard, not hard failures.
  5. Grid and time-step convergence studies documented per release.

### Gaps
- No vendor documentation on GT-POWER's default time-step/discretization rules or convergence criteria was retrieved. GT's guidance (e.g. discretization length ~0.4×bore for intake, ~0.55×bore for exhaust) is commonly cited but was not verified this session.
- No published unit-testing strategy specific to engine codes was found.

---

## 8. Real-time vs offline: MVEM, crank-angle real-time models, HiL

### Takeaway
Industry practice has explicitly separate fidelity tiers:
- detailed crank-angle 1D models for offline design
- FRMs (simplified 1D/0D) that run near or above real time for SiL/HiL
- mean-value engine models (MVEM) with crank-angle torque modulation for ECU HiL (dSPACE ASM)
- optionally, a 0D in-cylinder pressure model running in real time for pressure-sensor-based ECUs

A dual "high-fidelity offline mode" plus "real-time mode" architecture is therefore standard and credible.

### Cited Findings
- **dSPACE ASM mean-value engine models** calculate averaged state variables of the whole engine process per cylinder. Angle-dependent quantities like torque are added by modulation applied to the mean values, giving strong real-time performance. — [dSPACE ASM Engine Models](https://dspace.com/en/inc/home/products/sw/automotive_simulation_models/produkte_asm/asm_engine_models.cfm)
- **dSPACE ASM Diesel** uses a mean-value model with crank-angle-based torque modulation. — [dSPACE ASM Diesel Engine Simulation](https://dspace.com/en/ltd/home/products/sw/automotive_simulation_models/produkte_asm/asm_engine_models/asm_diesel_engine_simulation.cfm)
- **ASM Gasoline Engine InCylinder** simulates in-cylinder pressure in real time with a zero-dimensional thermodynamic approach. Its air and exhaust paths are mean-value systems with manifold p, T and mass. — [dSPACE ASM InCylinder](https://dspace.com/en/inc/home/products/sw/automotive_simulation_models/produkte_asm/asm_engine_models/asmincylinder.cfm)
- dSPACE authors (2007 SAE) extended a Simulink MVEM to compute crank-angle-based in-cylinder pressure and temperature for HiL with in-cylinder pressure sensors. — [dSPACE abstract DocID=804](https://www.dspace.com/en/ltd/home/medien/abstract.cfm?DocID=804)
- A simplified GT FRM on a dSPACE simulator gave consistent results for most variables at a 0.7–0.9 real-time factor (i.e. faster than real time) over 1000–5000 rpm. Remaining differences, e.g. fuel consumption, came from simplification and discretization. — [SAE 2016-01-0549](https://saemobilus.sae.org/content/2016-01-0549)
- Other FRM conversions:
  - A Volvo heavy-duty diesel FRM was checked against test-cell data. — [Chalmers thesis](https://odr.chalmers.se/items/ceb18c2f-d5ae-413b-9c66-bffae4f018d4/full)
  - A Wärtsilä medium-speed engine crank-angle 1D model was reduced to a real-time FRM. — [Univ. of Vaasa thesis](https://osuva.uwasa.fi/bitstreams/be9ab830-3df9-4e39-8890-d92e6f0a3e04/download)
  - An IFAC 2020 paper compares an FRM with a Simulink MVEM for HiL. — [IFAC 2020](https://ifatwww.et.uni-magdeburg.de/ifac2020/media/pdfs/3334.pdf)
- A semi-predictive combustion model limited real-time run time, and the team moved to a mean-value model for the real-time version. Combustion-model complexity is often the bottleneck. — [PoliTo thesis 14730](https://webthesis.biblio.polito.it/14730/1/tesi.pdf)
- **Hendricks MVEM:** a three-state model (manifold pressure, crankshaft speed, fuel film), ±2% steady accuracy, few parameters. — [SAE 900616](https://saemobilus.sae.org/content/900616)

### Inferences
engine-sim is unusual because it is already a crank-angle-resolved, wave-dynamic, real-time model, closer to an FRM than to an MVEM. A credible architecture would:
- **Real-time mode:** keep the current fixed-step audio-synchronous solver, with simplified combustion and noise enabled.
- **Offline/high-fidelity mode:** same code path, run headless faster or slower than real time, with:
  - finer Δt (CFL-controlled)
  - more pipe cells
  - two-zone combustion
  - deterministic seed
  - convergence-controlled cycle averaging
  - CSV/JSON output
- **Consistency check:** a documented cross-check that both modes agree within stated tolerances on WOT torque/VE, the same way FRMs are validated against their parent detailed model (SAE 2016-01-0549 pattern).
- **Optional MVEM export:** a mean-value model fitted from offline sweeps (VE map, torque map) for very fast control or vehicle studies, mirroring the GT "1D → 0D FRM" workflow.

### Gaps
- dSPACE pages could not be fetched directly (DNS), so step sizes and accuracy figures for ASM were not obtained. The descriptions above come from search excerpts of the official pages.
- No quantitative accuracy comparison between crank-angle real-time models and MVEMs was retrieved, beyond the FRM study (SAE 2016-01-0549) and Hendricks' ±2%.
