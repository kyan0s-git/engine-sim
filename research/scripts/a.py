import numpy as np, re, sys
D=__import__('os').environ.get('THERMO_DATA', 'data') + '/'
L=open(D+'thermo.inp').read().splitlines()
def f(s): return float(s.replace('D','E'))
def nasa9(name):
    for i,l in enumerate(L):
        if l.startswith(name):
            h=L[i+1]; nint=int(h[0:2]); M=float(h[52:65]); Hf=float(h[65:80])
            rngs=[]
            for k in range(nint):
                b=i+2+3*k; r=L[b]; Tl,Th=float(r[0:11]),float(r[11:22])
                c=[f(L[b+1][j*16:(j+1)*16]) for j in range(5)]+[f(L[b+2][0:16]),f(L[b+2][16:32])]
                bb=[f(L[b+2][48:64]),f(L[b+2][64:80])]
                rngs.append((Tl,Th,c,bb))
            return M,Hf,rngs
def cp9(r,T):
    for Tl,Th,c,b in r:
        if T<=Th+1e-9: break
    return c[0]/T**2+c[1]/T+c[2]+c[3]*T+c[4]*T**2+c[5]*T**3+c[6]*T**4
def h9(r,T):
    for Tl,Th,c,b in r:
        if T<=Th+1e-9: break
    return -c[0]/T**2+c[1]*np.log(T)/T+c[2]+c[3]*T/2+c[4]*T**2/3+c[5]*T**3/4+c[6]*T**4/5+b[0]/T  # h/RT
def s9(r,T):
    for Tl,Th,c,b in r:
        if T<=Th+1e-9: break
    return -c[0]/T**2/2-c[1]/T+c[2]*np.log(T)+c[3]*T+c[4]*T**2/2+c[5]*T**3/3+c[6]*T**4/4+b[1]
R=8.314462618
# refit NASA7
def fit7(r,Tlo,Tmid,Thi):
    out=[]
    for a,bnd in ((Tlo,Tmid),(Tmid,Thi)):
        T=np.linspace(a,bnd,400)
        A=np.vstack([T**k for k in range(5)]).T
        y=np.array([cp9(r,t) for t in T])
        # constrain continuity loosely via least squares only
        co=np.linalg.lstsq(A,y,rcond=None)[0]
        # a6: h/RT = a1 + a2 T/2 + ... + a6/T, match at Tmid-ish point 
        Tm=bnd if a==Tlo else a
        Tm=1000.0
        hp=co[0]+co[1]*Tm/2+co[2]*Tm**2/3+co[3]*Tm**3/4+co[4]*Tm**4/5
        a6=(h9(r,Tm)-hp)*Tm
        sp=co[0]*np.log(Tm)+co[1]*Tm+co[2]*Tm**2/2+co[3]*Tm**3/3+co[4]*Tm**4/4
        a7=s9(r,Tm)-sp
        err=np.max(np.abs(A@co-y)/y)
        out.append((list(co)+[a6,a7],err))
    return out
for nm in ["C8H18,isooctane","C7H16,n-heptane","C2H5OH ","CH3OH ","CH4 ","C3H8 ","Air "]:
    M,Hf,r=nasa9(nm)
    res=fit7(r,300.,1000.,3000.)
    print(nm,"M=",M,"Hf298=",Hf)
    for co,err in res: print("  [",", ".join("%.8e"%x for x in co),"] maxrelerr cp=%.4f"%err)
    for T in (300,500,800,1000,1500,2000,2500,3000):
        print("   T=%d cp/R=%.3f"%(T,cp9(r,T)),end=';')
    print()
# LHV & latent
Hf={'CO2':-393510.,'H2O':-241826.,'H2Ol':-285830.}
for nm,ln,n,m,o in [("C8H18,isooctane","C8H18(L),isooct",8,18,0),("C7H16,n-heptane","C7H16(L),n-hept",7,16,0),("C2H5OH ","C2H5OH(L)",2,6,1),("CH3OH ","CH3OH(L)",1,4,1),("CH4 ",None,1,4,0),("C3H8 ",None,3,8,0)]:
    M,hg,_=nasa9(nm)
    hl=nasa9(ln)[1] if ln else None
    lhv_g=-(n*Hf['CO2']+m/2*Hf['H2O']-hg)/M  # kJ/kg (J/mol / g/mol)
    hhv_g=-(n*Hf['CO2']+m/2*Hf['H2Ol']-hg)/M
    a=n+m/4-o/2; afr=a*4.76*28.9651/M  # approximate
    afr2=a*(31.9988+3.76*28.0134)/M
    s="%s M=%.3f LHVgas=%.2f MJ/kg HHVgas=%.2f"%(nm,M,lhv_g/1000,hhv_g/1000)
    if hl is not None:
        hfg=(hg-hl)/M
        s+=" LHVliq=%.2f hfg298=%.1f kJ/kg (%.2f kJ/mol)"%((lhv_g-hfg)/1000,hfg,(hg-hl)/1000)
    s+=" AFRst(O2+3.76N2)=%.2f AFRst(air M=28.965,4.773)=%.2f"%(afr2,a*4.773*28.9651/M)
    print(s)
