import cantera as ct, numpy as np
D=__import__('os').environ.get('THERMO_DATA', 'data') + '/'
gri=open(D+'gri30.yaml').read()
import yaml
g=yaml.safe_load(gri)
for _s in g['species']:
    if _s['name'] is False: _s['name']='NO'
keep=['N2','O2','CO2','H2O','CO','H2','OH','H','O','NO','AR']
sp=[s for s in g['species'] if s['name'] in keep]
ic8={'name':'IC8H18','composition':{'C':8,'H':18},'thermo':{'model':'NASA7','temperature-ranges':[300.,1000.,3000.],
 'data':[[-1.43984399e+00, 8.96796087e-02, -2.38210565e-05, -2.52511610e-08, 1.55447845e-11, -3.02456829e+04, 3.36027464e+01],
         [1.28875565e+01, 6.12991780e-02, -2.37067433e-05, 4.55959217e-09, -3.59231952e-13, -3.46928575e+04, -4.30050693e+01]]}}
sp.append(ic8)
doc={'phases':[{'name':'gas','thermo':'ideal-gas','elements':['O','H','C','N','Ar'],'species':[s['name'] for s in sp],'state':{'T':300,'P':101325}}],'species':sp}
gas=ct.Solution(yaml=yaml.safe_dump(doc))
air='O2:0.21,N2:0.78,AR:0.01'
air='O2:1,N2:3.7274,AR:0.0447'  # 20.95/78.08/0.93 approx
def unburned(phi):
    gas.set_equivalence_ratio(phi,'IC8H18',air); return gas.X.copy()
def complete(phi):
    # complete-combustion products for C8H18 (lean/stoich), rich -> CO,H2 with WGS K
    a=12.5
    nO2=a/phi; n={'N2':3.7274*nO2,'AR':0.0447*nO2}
    if phi<=1: n.update({'CO2':8,'H2O':9,'O2':nO2-a})
    else:
        K=3.5; # solve WGS
        # c=CO: CO2=8-c, H2O=2nO2-16+c ... O balance: 2(8-c)+c + w = 2nO2 ; H: 2w+2h=18
        def fK(c):
            co2=8-c; w=2*nO2-2*co2-c; h=9-w
            return c*w-K*co2*h
        lo,hi=1e-9,min(8-1e-9,2*nO2-16+9-1e-9)
        for _ in range(200):
            mid=(lo+hi)/2
            if fK(lo)*fK(mid)<=0: hi=mid
            else: lo=mid
        c=mid; co2=8-c; w=2*nO2-2*co2-c; h=9-w
        n.update({'CO2':co2,'CO':c,'H2O':w,'H2':h})
    return n
print("== gamma=cp/cv vs T (ideal gas, frozen composition unless noted)")
print("T  air  unb_phi1(vap)  burned_complete_phi1  burned_eq_phi1_50bar  burned_eq_phi1_1bar burned_complete_phi0.8 burned_complete_phi1.2")
for T in [300,400,500,600,700,800,1000,1200,1500,1800,2000,2200,2500,2800,3000]:
    r=[]
    gas.TPX=T,1e5,air; r.append(gas.cp/gas.cv)
    gas.TPX=T,1e5,unburned(1.0); r.append(gas.cp/gas.cv)
    gas.TPX=T,1e5,complete(1.0); r.append(gas.cp/gas.cv)
    for P in (50e5,1e5):
        gas.TPX=T,P,complete(1.0); gas.equilibrate('TP'); 
        # equilibrium (shifting) gamma: use finite difference of u,h? report frozen at eq comp + effective
        r.append(gas.cp/gas.cv)
    gas.TPX=T,1e5,complete(0.8); r.append(gas.cp/gas.cv)
    gas.TPX=T,1e5,complete(1.2); r.append(gas.cp/gas.cv)
    print(T," ".join("%.4f"%x for x in r))
gas.TPX=300,1e5,unburned(1.0); print("unburned phi=1 mean M=%.3f"%gas.mean_molecular_weight, "R=%.1f"%(ct.gas_constant/gas.mean_molecular_weight))
gas.TPX=300,1e5,complete(1.0); print("burned complete phi=1 mean M=%.3f"%gas.mean_molecular_weight)
print("== WGS K=(CO2*H2)/(CO*H2O) from thermo")
for T in [1400,1500,1600,1700,1800,2000,2500]:
    gt=0
    gibbs=lambda s:(gas.species(s).thermo.h(T)-T*gas.species(s).thermo.s(T))
    dG=gibbs('CO2')+gibbs('H2')-gibbs('CO')-gibbs('H2O')
    print(T,"K=%.3f"%np.exp(-dG/(ct.gas_constant*T)))
print("== equilibrium composition (mole fr.) at T,P vs complete")
for phi in (0.8,1.0,1.2):
  for T,P in ((2500,50e5),(2800,100e5),(1700,10e5)):
    gas.TPX=T,P,complete(phi); gas.equilibrate('TP')
    print("phi=%.1f T=%d P=%dbar "%(phi,T,P/1e5)+" ".join("%s=%.4f"%(s,gas[s].X[0]) for s in ['CO2','H2O','CO','H2','O2','OH','H','O','NO']))
  gas.TPX=300,1e5,complete(phi); print("   complete: "+" ".join("%s=%.4f"%(s,gas[s].X[0]) for s in ['CO2','H2O','CO','H2','O2']))
print("== adiabatic flame T")
for phi in (0.7,0.8,0.9,1.0,1.1,1.2,1.4):
    gas.TPX=298.15,101325,unburned(phi); h0=gas.h
    # frozen complete
    gas.TPX=298.15,101325,complete(phi)
    gas.TPX=1000,101325,complete(phi); gas.HP=h0,101325
    Tc=gas.T
    gas.TPX=298.15,101325,unburned(phi); gas.equilibrate('HP'); Te=gas.T
    # constant volume from 700K, 20 bar
    gas.TPX=700,20e5,unburned(phi); u0=gas.u; v0=gas.v
    gas.TPX=2000,20e5,complete(phi); gas.UV=u0,v0; Tcv=gas.T; Pcv=gas.P
    gas.TPX=700,20e5,unburned(phi); gas.equilibrate('UV'); Tev=gas.T; Pev=gas.P
    print("phi=%.1f Tad_p(1atm,298K): complete=%.0f eq=%.0f | Tad_v(700K,20bar): complete=%.0f (%.0f bar) eq=%.0f (%.0f bar)"%(phi,Tc,Te,Tcv,Pcv/1e5,Tev,Pev/1e5))
print("== fuel-air cycle (no residual, start 1 bar 350K, frozen compression, UV eq comb, eq isentropic expansion)")
LHV=44.65e6
for phi in (0.8,1.0,1.2):
  for rc in (6,8,10,12,14,16):
    gas.TPX=350,1e5,unburned(phi); m=1.0; Y=gas.Y; yf=gas['IC8H18'].Y[0]
    u1=gas.u; s1=gas.s; v1=gas.v
    gas.SV=s1,v1/rc; u2=gas.u; v2=gas.v; T2=gas.T; P2=gas.P
    gas.equilibrate('UV'); T3=gas.T;P3=gas.P; s3=gas.s; u3=gas.u
    gas.SV=s3,v1; gas.equilibrate('SV'); u4=gas.u; T4=gas.T
    W=(u3-u4)-(u2-u1); eta=W/(yf*LHV)
    # frozen expansion variant
    gas.TPX=T3,P3,gas.X; 
    print("phi=%.1f rc=%2d T2=%.0f P2=%.1fbar T3=%.0f P3=%.0fbar T4=%.0f eta_fa=%.3f  Otto1.4=%.3f Otto1.3=%.3f Otto1.25=%.3f"%(phi,rc,T2,P2/1e5,T3,P3/1e5,T4,eta,1-rc**-0.4,1-rc**-0.3,1-rc**-0.25))
