# Thermodynamic property scripts

Python scripts used to compute the thermodynamic data in
`research_notes/Realistic engine physics models/thermodynamics_gas_properties.md`
(NASA-7 refits, gamma(T) tables, fuel properties, fuel-air cycle efficiencies,
equilibrium compositions). They require `numpy` and, for the equilibrium
calculations, [Cantera](https://cantera.org) 3.x.

The scripts read their input data from the directory named by the
`THERMO_DATA` environment variable (default: `data/`):

| File | Source |
| --- | --- |
| `thermo.inp` | NASA CEA thermodynamic database (McBride, Gordon & Reno), `data/thermo.inp` in https://github.com/nasa/cea |
| `gri30.yaml` | GRI-Mech 3.0 as distributed with Cantera |
| `nDodecane_Reitz.yaml` | n-dodecane mechanism distributed with Cantera |

```
THERMO_DATA=/path/to/data python3 a.py
```
