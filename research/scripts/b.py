exec(open('a.py').read().split('# refit NASA7')[0])
def fit(r,a,bnd,Tm=1000.):
    T=np.linspace(a,bnd,600); x=T/1000.
    A=np.vstack([x**k for k in range(5)]).T
    y=np.array([cp9(r,t) for t in T])
    c=np.linalg.lstsq(A,y,rcond=None)[0]
    co=[c[k]/1000.**k for k in range(5)]
    hp=co[0]+co[1]*Tm/2+co[2]*Tm**2/3+co[3]*Tm**3/4+co[4]*Tm**4/5
    a6=(h9(r,Tm)-hp)*Tm
    sp=co[0]*np.log(Tm)+co[1]*Tm+co[2]*Tm**2/2+co[3]*Tm**3/3+co[4]*Tm**4/4
    a7=s9(r,Tm)-sp
    yy=sum(co[k]*T**k for k in range(5))
    return co+[a6,a7], np.max(np.abs(yy-y)/y)
for nm in ["C8H18,isooctane","C7H16,n-heptane","C2H5OH ","CH3OH ","Air "]:
    M,Hf,r=nasa9(nm)
    print(nm)
    for a,bnd in ((300.,1000.),(1000.,3000.)):
        co,err=fit(r,a,bnd)
        print("  %g-%g K: [%s] max cp err %.2f%%"%(a,bnd,", ".join("%.8e"%x for x in co),err*100))
        # check h at 298 and 2000
        def h7(T): return (co[0]+co[1]*T/2+co[2]*T**2/3+co[3]*T**3/4+co[4]*T**4/5+co[5]/T)*R*T
        Tc=500. if a<999 else 2000.
        print("     check h(%g) 7coef=%.1f J/mol vs 9coef=%.1f"%(Tc,h7(Tc),h9(r,Tc)*R*Tc))
