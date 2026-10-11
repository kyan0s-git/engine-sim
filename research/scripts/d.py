import numpy as np
R=8.314462618
# Peng-Robinson Z for pure components (Tc K, Pc Pa, omega) -- standard critical constants
comp={'N2':(126.2,33.98e5,0.037),'CO2':(304.13,73.77e5,0.225),'H2O':(647.1,220.64e5,0.344),'iC8H18':(543.9,25.7e5,0.304),'O2':(154.58,50.43e5,0.022)}
def Zpr(T,P,c):
    Tc,Pc,w=c; a=0.45724*R**2*Tc**2/Pc; b=0.07780*R*Tc/Pc
    k=0.37464+1.54226*w-0.26992*w**2; al=(1+k*(1-np.sqrt(T/Tc)))**2
    A=a*al*P/(R*T)**2; B=b*P/(R*T)
    r=np.roots([1,-(1-B),A-3*B**2-2*B,-(A*B-B**2-B**3)]); r=r[np.isreal(r)].real
    return max(r)
for (T,P,lab) in [(300,1e5,'intake'),(800,60e5,'SI TDC motored-ish'),(900,150e5,'diesel TDC high boost'),(1000,200e5,'diesel peak early'),(2000,100e5,'burned mid'),(2800,200e5,'peak burned extreme'),(2500,80e5,'SI peak')]:
    print(lab,T,"K",P/1e5,"bar: "+" ".join("%s Z=%.4f"%(k,Zpr(T,P,v)) for k,v in comp.items() if not (k=='iC8H18' and T>1000)))
