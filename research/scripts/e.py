exec(open('c.py').read().split('print("== gamma=cp/cv')[0])
def mix(lbl,X,Tlo,Thi):
    T=np.linspace(Tlo,Thi,300); cv=[];gm=[];u=[]
    for t in T:
        gas.TPX=t,1e5,X; cv.append(gas.cv_mass); gm.append(gas.cp/gas.cv); u.append(gas.int_energy_mass)
    cv=np.array(cv);gm=np.array(gm);x=(T-1000)/1000
    gas.TPX=300,1e5,X; Rm=ct.gas_constant/gas.mean_molecular_weight
    for deg in (1,2,3):
        p=np.polyfit(x,cv,deg); e=np.max(np.abs(np.polyval(p,x)-cv)/cv)
        print("%s cv_mass[J/kgK] deg%d in x=(T-1000)/1000, %g-%gK: coeffs(high->low)=[%s] maxerr=%.2f%%"%(lbl,deg,Tlo,Thi,", ".join("%.4g"%c for c in p),e*100))
    p=np.polyfit(T,gm,1); e=np.max(np.abs(np.polyval(p,T)-gm))
    print("%s gamma linear: gamma=%.4f + (%.4e)*(T-300)  maxabs err=%.4f ; R=%.2f J/kgK"%(lbl,np.polyval(p,300),p[0],e,Rm))
    p2=np.polyfit(x,gm,2); e2=np.max(np.abs(np.polyval(p2,x)-gm))
    print("%s gamma quad in x: [%s] maxabs err=%.4f"%(lbl,", ".join("%.5g"%c for c in p2),e2))
mix("AIR",air,300,3000)
mix("UNB phi1 (iC8H18 vap+air)",unburned(1.0),300,1100)
mix("UNB phi1 +10%mass stoich residual",None,300,1100) if False else None
gas.TPX=300,1e5,complete(1.0); Yb=gas.Y.copy()
gas.TPX=300,1e5,unburned(1.0); Yu=gas.Y.copy()
gas.TPY=300,1e5,0.9*Yu+0.1*Yb; Xr=gas.X.copy()
mix("UNB phi1 + 10% residual",Xr,300,1100)
mix("BURNED phi1 complete (frozen)",complete(1.0),300,3000)
mix("BURNED phi0.8 complete",complete(0.8),300,3000)
mix("BURNED phi1.2 complete+WGS",complete(1.2),300,3000)
# heat release per kg mixture & effective
for phi in (0.8,1.0,1.2):
    gas.TPX=298.15,1e5,unburned(phi); uu=gas.int_energy_mass; yf=gas['IC8H18'].Y[0]
    gas.TPX=298.15,1e5,complete(phi); ub=gas.int_energy_mass
    print("phi=%.1f fuel mass frac=%.5f  -du_comb(298K)=%.0f kJ/kg mixture; per kg fuel=%.2f MJ"%(phi,yf,(uu-ub)/1e3,(uu-ub)/yf/1e6))
# equilibrium effective (shifting) gamma: numeric dlnP/dlnrho at const s for eq mixture
for T,P in ((2000,50e5),(2500,50e5),(2800,100e5),(3000,100e5)):
    gas.TPX=T,P,complete(1.0); gas.equilibrate('TP'); s=gas.s; v=gas.v; P0=gas.P
    gas.SV=s,v*1.001; gas.equilibrate('SV'); P1=gas.P
    ge=-np.log(P1/P0)/np.log(1.001)
    gas.TPX=T,P,complete(1.0); gas.equilibrate('TP'); gf=gas.cp/gas.cv
    print("shifting-eq isentropic exponent phi1 T=%d P=%dbar: %.4f (frozen at eq comp %.4f)"%(T,P/1e5,ge,gf))
