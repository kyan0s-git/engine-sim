#include "../include/thermodynamics.h"

#include "../include/constants.h"

#include <algorithm>
#include <cmath>

namespace thermo {

namespace {

    constexpr double R = constants::R;

    constexpr double M_C = 12.011e-3;
    constexpr double M_H = 1.008e-3;
    constexpr double M_O = 15.999e-3;

    // GRI-Mech 3.0 coefficients (low range T_low-1000 K, high range
    // 1000 K-T_high), from Cantera's gri30.yaml
    constexpr NasaPolynomial Poly_N2 = {
        { 3.298677, 1.4082404e-03, -3.963222e-06, 5.641515e-09, -2.444854e-12, -1020.8999, 3.950372 },
        { 2.92664, 1.4879768e-03, -5.68476e-07, 1.0097038e-10, -6.753351e-15, -922.7977, 5.980528 }
    };

    constexpr NasaPolynomial Poly_O2 = {
        { 3.78245636, -2.99673416e-03, 9.84730201e-06, -9.68129509e-09, 3.24372837e-12, -1063.94356, 3.65767573 },
        { 3.28253784, 1.48308754e-03, -7.57966669e-07, 2.09470555e-10, -2.16717794e-14, -1088.45772, 5.45323129 }
    };

    constexpr NasaPolynomial Poly_CO2 = {
        { 2.35677352, 8.98459677e-03, -7.12356269e-06, 2.45919022e-09, -1.43699548e-13, -48371.9697, 9.90105222 },
        { 3.85746029, 4.41437026e-03, -2.21481404e-06, 5.23490188e-10, -4.72084164e-14, -48759.166, 2.27163806 }
    };

    constexpr NasaPolynomial Poly_H2O = {
        { 4.19864056, -2.0364341e-03, 6.52040211e-06, -5.48797062e-09, 1.77197817e-12, -30293.7267, -0.849032208 },
        { 3.03399249, 2.17691804e-03, -1.64072518e-07, -9.7041987e-11, 1.68200992e-14, -30004.2971, 4.9667701 }
    };

    constexpr NasaPolynomial Poly_CO = {
        { 3.57953347, -6.1035368e-04, 1.01681433e-06, 9.07005884e-10, -9.04424499e-13, -14344.086, 3.50840928 },
        { 2.71518561, 2.06252743e-03, -9.98825771e-07, 2.30053008e-10, -2.03647716e-14, -14151.8724, 7.81868772 }
    };

    constexpr NasaPolynomial Poly_H2 = {
        { 2.34433112, 7.98052075e-03, -1.9478151e-05, 2.01572094e-08, -7.37611761e-12, -917.935173, 0.683010238 },
        { 3.3372792, -4.94024731e-05, 4.99456778e-07, -1.79566394e-10, 2.00255376e-14, -950.158922, -3.20502331 }
    };

    constexpr NasaPolynomial Poly_Ar = {
        { 2.5, 0, 0, 0, 0, -745.375, 4.366 },
        { 2.5, 0, 0, 0, 0, -745.375, 4.366 }
    };

    constexpr NasaPolynomial Poly_CH4 = {
        { 5.14987613, -1.36709788e-02, 4.91800599e-05, -4.84743026e-08, 1.66693956e-11, -10246.6476, -4.64130376 },
        { 7.4851495e-02, 1.33909467e-02, -5.73285809e-06, 1.22292535e-09, -1.0181523e-13, -9468.34459, 18.437318 }
    };

    constexpr NasaPolynomial Poly_C3H8 = {
        { 0.93355381, 2.6424579e-02, 6.1059727e-06, -2.1977499e-08, 9.5149253e-12, -13958.52, 19.201691 },
        { 7.5341368, 1.8872239e-02, -6.2718491e-06, 9.1475649e-10, -4.7838069e-14, -16467.516, -17.892349 }
    };

    // NASA-7 refits (300-1000-3000 K) of the NASA CEA (NASA-9) fuel vapour
    // data; see the research notes on gas properties
    constexpr NasaPolynomial Poly_IsoOctane = {
        { -1.43984399, 8.96796087e-02, -2.38210565e-05, -2.52511610e-08, 1.55447845e-11, -30245.6829, 33.6027464 },
        { 12.8875565, 6.12991780e-02, -2.37067433e-05, 4.55959217e-09, -3.59231952e-13, -34692.8575, -43.0050693 }
    };

    constexpr NasaPolynomial Poly_NHeptane = {
        { 1.46737592, 6.11298247e-02, 1.60971894e-05, -5.94475485e-08, 2.67061396e-11, -25776.3209, 24.6581794 },
        { 12.6392515, 5.19921151e-02, -2.39057471e-05, 5.74088721e-09, -5.51395151e-13, -29890.6314, -38.2903202 }
    };

    constexpr NasaPolynomial Poly_Ethanol = {
        { 1.30120451, 2.21880634e-02, 3.68751291e-06, -1.88316650e-08, 8.83875328e-12, -29631.4474, 19.7034692 },
        { 2.91680449, 2.30989084e-02, -1.12762177e-05, 2.68132394e-09, -2.51738732e-13, -30274.7086, 10.2159469 }
    };

    constexpr NasaPolynomial Poly_Methanol = {
        { 3.82932266, -1.62463700e-03, 3.09670971e-05, -3.50010091e-08, 1.26145837e-11, -25447.2279, 6.41686696 },
        { 1.31954848, 1.51704908e-02, -7.25814840e-06, 1.69727368e-09, -1.57168046e-13, -25213.4895, 17.0314448 }
    };

    SpeciesData makeSpecies(const char *name, double molarMass, const NasaPolynomial &poly) {
        SpeciesData data;
        data.name = name;
        data.molarMass = molarMass;
        data.poly = poly;
        data.energy = makeEnergyPolynomial(poly);

        return data;
    }

    struct SpeciesTable {
        SpeciesData data[SpeciesCount];

        SpeciesTable() {
            // The fuel entry is a placeholder; fuel properties come from the
            // FuelDefinition
            data[Fuel] = makeSpecies("fuel", 114.23e-3, Poly_IsoOctane);
            data[O2] = makeSpecies("O2", 31.9988e-3, Poly_O2);
            data[N2] = makeSpecies("N2", 28.0134e-3, Poly_N2);
            data[Ar] = makeSpecies("Ar", 39.948e-3, Poly_Ar);
            data[CO2] = makeSpecies("CO2", 44.0095e-3, Poly_CO2);
            data[H2O] = makeSpecies("H2O", 18.01528e-3, Poly_H2O);
            data[CO] = makeSpecies("CO", 28.0101e-3, Poly_CO);
            data[H2] = makeSpecies("H2", 2.01588e-3, Poly_H2);
        }
    };

    const SpeciesTable &speciesTable() {
        static const SpeciesTable table;
        return table;
    }

    double evaluate(const double *c, double T) {
        return c[0] + T * (c[1] + T * (c[2] + T * (c[3] + T * (c[4] + T * c[5]))));
    }

    double evaluateDerivative(const double *c, double T) {
        return c[1] + T * (2 * c[2] + T * (3 * c[3] + T * (4 * c[4] + T * 5 * c[5])));
    }

    FlameSpeedParameters metghalchiKeck(double phi_m, double B_m_cm_s, double B_phi_cm_s) {
        FlameSpeedParameters params;
        params.correlation = FlameSpeedParameters::Correlation::MetghalchiKeck;
        params.phi_m = phi_m;
        params.B_m = B_m_cm_s / 100.0;
        params.B_phi = B_phi_cm_s / 100.0;

        return params;
    }

} /* namespace */

const char *speciesName(int s) {
    return species(s).name;
}

double NasaPolynomial::cp_R(double T) const {
    const double *a = (T < T_mid) ? low : high;
    return a[0] + T * (a[1] + T * (a[2] + T * (a[3] + T * a[4])));
}

double NasaPolynomial::h_RT(double T) const {
    const double *a = (T < T_mid) ? low : high;
    return a[0]
        + T * (a[1] / 2 + T * (a[2] / 3 + T * (a[3] / 4 + T * a[4] / 5)))
        + a[5] / T;
}

double NasaPolynomial::s_R(double T) const {
    const double *a = (T < T_mid) ? low : high;
    return a[0] * std::log(T)
        + T * (a[1] + T * (a[2] / 2 + T * (a[3] / 3 + T * a[4] / 4)))
        + a[6];
}

double NasaPolynomial::h(double T) const {
    return h_RT(T) * R * T;
}

NasaPolynomial NasaPolynomial::scaled(double s) const {
    NasaPolynomial result = *this;
    for (int i = 0; i < 7; ++i) {
        result.low[i] *= s;
        result.high[i] *= s;
    }

    return result;
}

void EnergyPolynomial::clear() {
    *this = EnergyPolynomial();
}

void EnergyPolynomial::add(const EnergyPolynomial &p, double weight) {
    for (int i = 0; i < 6; ++i) {
        low[i] += weight * p.low[i];
        high[i] += weight * p.high[i];
    }

    cv_ref_R += weight * p.cv_ref_R;
    chem += weight * p.chem;
}

double EnergyPolynomial::e(double T) const {
    if (T < T_ref) {
        return R * cv_ref_R * T;
    }
    else if (T < T_mid) {
        return R * evaluate(low, T);
    }
    else if (T <= T_max) {
        return R * evaluate(high, T);
    }
    else {
        return R * (evaluate(high, T_max) + evaluateDerivative(high, T_max) * (T - T_max));
    }
}

double EnergyPolynomial::cv(double T) const {
    if (T < T_ref) {
        return R * cv_ref_R;
    }
    else if (T < T_mid) {
        return R * evaluateDerivative(low, T);
    }
    else {
        return R * evaluateDerivative(high, std::fmin(T, T_max));
    }
}

double EnergyPolynomial::temperature(double e_target, double T_guess, double *cv_out) const {
    if (e_target <= 0 || cv_ref_R <= 0) {
        if (cv_out != nullptr) *cv_out = R * cv_ref_R;
        return 0;
    }

    const double e_ref = R * cv_ref_R * T_ref;
    if (e_target <= e_ref) {
        if (cv_out != nullptr) *cv_out = R * cv_ref_R;
        return e_target / (R * cv_ref_R);
    }

    const double e_max = e(T_max);
    if (e_target >= e_max) {
        const double cv_max = cv(T_max);
        if (cv_out != nullptr) *cv_out = cv_max;
        return T_max + (e_target - e_max) / cv_max;
    }

    // Newton iteration; e(T) is monotonic and nearly linear so this
    // converges in a few iterations from a nearby guess
    double T = (T_guess < T_ref) ? T_ref : ((T_guess > T_max) ? T_max : T_guess);
    for (int i = 0; i < 32; ++i) {
        const double *c = (T < T_mid) ? low : high;
        const double f = R * evaluate(c, T) - e_target;
        const double df = R * evaluateDerivative(c, T);
        const double dT = f / df;
        T -= dT;
        T = (T < T_ref) ? T_ref : ((T > T_max) ? T_max : T);

        if (std::abs(dT) < 1E-7 * T) {
            // cv barely changes over the last step
            if (cv_out != nullptr) *cv_out = df;
            return T;
        }
    }

    if (cv_out != nullptr) *cv_out = cv(T);
    return T;
}

EnergyPolynomial makeEnergyPolynomial(const NasaPolynomial &poly) {
    EnergyPolynomial p;

    const double cv_ref_R = poly.cp_R(T_ref) - 1;
    const double h_ref_R = poly.h_RT(T_ref) * T_ref;
    const double offset = -h_ref_R + T_ref + cv_ref_R * T_ref;

    p.cv_ref_R = cv_ref_R;
    p.chem = R * (h_ref_R - T_ref - cv_ref_R * T_ref);

    const double *ranges[] = { poly.low, poly.high };
    double *targets[] = { p.low, p.high };
    for (int r = 0; r < 2; ++r) {
        const double *a = ranges[r];
        double *c = targets[r];
        c[0] = a[5] + offset;
        c[1] = a[0] - 1;
        c[2] = a[1] / 2;
        c[3] = a[2] / 3;
        c[4] = a[3] / 4;
        c[5] = a[4] / 5;
    }

    // Make the energy exactly continuous at the range boundary
    p.high[0] += evaluate(p.low, T_mid) - evaluate(p.high, T_mid);

    return p;
}

const SpeciesData &species(int s) {
    return speciesTable().data[s];
}

double formationEnthalpy(int s) {
    return species(s).poly.h(T_ref);
}

double FuelDefinition::stoichiometricAfr() const {
    return stoichiometricO2() * air::molesPerO2 * air::molarMass / molarMass;
}

FuelDefinition makeFuel(
    const std::string &name,
    double C, double H, double O,
    double lhv,
    double latentHeat,
    double density,
    const NasaPolynomial &shape,
    double shapeMolarMass,
    const FlameSpeedParameters &flameSpeed)
{
    FuelDefinition fuel;
    fuel.name = name;
    fuel.C = C;
    fuel.H = H;
    fuel.O = O;
    fuel.molarMass = C * M_C + H * M_H + O * M_O;
    fuel.lhv = lhv;
    fuel.latentHeat = latentHeat;
    fuel.density = density;
    fuel.flameSpeed = flameSpeed;

    // Same heat capacity per unit mass as the reference molecule
    fuel.poly = shape.scaled(fuel.molarMass / shapeMolarMass);

    // Enthalpy of formation that reproduces the heating value:
    // C_x H_y O_z + a O2 -> x CO2 + y/2 H2O,  dH = -LHV * M
    const double h_f =
        C * formationEnthalpy(CO2)
        + (H / 2) * formationEnthalpy(H2O)
        - fuel.stoichiometricO2() * formationEnthalpy(O2)
        + fuel.vapourLhv() * fuel.molarMass;
    const double correction = (h_f - fuel.poly.h(T_ref)) / R;
    fuel.poly.low[5] += correction;
    fuel.poly.high[5] += correction;

    fuel.energy = makeEnergyPolynomial(fuel.poly);

    return fuel;
}

FuelDefinition blendFuels(
    const std::string &name,
    const FuelDefinition &a,
    const FuelDefinition &b,
    double volumeFractionB)
{
    const double v_b = std::fmin(std::fmax(volumeFractionB, 0.0), 1.0);
    const double m_a = (1 - v_b) * a.density;
    const double m_b = v_b * b.density;
    const double w_b = m_b / (m_a + m_b);
    const double n_a = m_a / a.molarMass;
    const double n_b = m_b / b.molarMass;
    const double x_b = n_b / (n_a + n_b);
    const double x_a = 1 - x_b;

    FuelDefinition fuel;
    fuel.name = name;
    fuel.C = x_a * a.C + x_b * b.C;
    fuel.H = x_a * a.H + x_b * b.H;
    fuel.O = x_a * a.O + x_b * b.O;
    fuel.molarMass = x_a * a.molarMass + x_b * b.molarMass;
    fuel.lhv = (1 - w_b) * a.lhv + w_b * b.lhv;
    fuel.latentHeat = (1 - w_b) * a.latentHeat + w_b * b.latentHeat;
    fuel.density = (1 - v_b) * a.density + v_b * b.density;
    fuel.flameSpeed = (v_b < 0.5) ? a.flameSpeed : b.flameSpeed;
    fuel.leanLimit = (1 - v_b) * a.leanLimit + v_b * b.leanLimit;
    fuel.richLimit = (1 - v_b) * a.richLimit + v_b * b.richLimit;

    for (int i = 0; i < 7; ++i) {
        fuel.poly.low[i] = x_a * a.poly.low[i] + x_b * b.poly.low[i];
        fuel.poly.high[i] = x_a * a.poly.high[i] + x_b * b.poly.high[i];
    }

    fuel.energy = makeEnergyPolynomial(fuel.poly);

    return fuel;
}

namespace {

    struct FuelLibrary {
        std::vector<FuelDefinition> fuels;

        FuelLibrary() {
            const FlameSpeedParameters rhodesKeck;
            const NasaPolynomial &isoOctane = Poly_IsoOctane;
            constexpr double M_isoOctane = 8 * M_C + 18 * M_H;

            // Lower heating values of the liquid fuels and latent heats
            // follow from the CEA formation enthalpies (see the research
            // notes). Flame speed coefficients are Metghalchi & Keck's where
            // available; the ones marked [E] are estimates.

            // Market gasoline: C7H13 surrogate (H/C = 1.86, AFR 14.6) with
            // the heat capacity of iso-octane
            const FuelDefinition gasoline = makeFuel(
                "gasoline", 7, 13, 0,
                43.4e6, 350e3, 745.0,
                isoOctane, M_isoOctane,
                rhodesKeck);
            fuels.push_back(gasoline);

            fuels.push_back(makeFuel(
                "iso-octane", 8, 18, 0,
                44.34e6, 308e3, 692.0,
                isoOctane, M_isoOctane,
                metghalchiKeck(1.13, 26.3, -84.7)));

            fuels.push_back(makeFuel(
                "n-heptane", 7, 16, 0,
                44.56e6, 365e3, 684.0,
                Poly_NHeptane, 7 * M_C + 16 * M_H,
                metghalchiKeck(1.13, 26.3, -84.7)));     // [E] iso-octane values

            const FuelDefinition ethanol = makeFuel(
                "ethanol", 2, 6, 1,
                26.81e6, 924e3, 789.0,
                Poly_Ethanol, 2 * M_C + 6 * M_H + M_O,
                metghalchiKeck(1.11, 36.9, -140.5));     // [E] methanol values
            fuels.push_back(ethanol);

            fuels.push_back(makeFuel(
                "methanol", 1, 4, 1,
                19.92e6, 1185e3, 792.0,
                Poly_Methanol, M_C + 4 * M_H + M_O,
                metghalchiKeck(1.11, 36.9, -140.5)));

            fuels.push_back(blendFuels("e10", gasoline, ethanol, 0.10));
            fuels.push_back(blendFuels("e85", gasoline, ethanol, 0.85));

            fuels.push_back(makeFuel(
                "methane", 1, 4, 0,
                50.03e6, 0.0, 0.68,
                Poly_CH4, M_C + 4 * M_H,
                metghalchiKeck(1.08, 38.0, -140.0)));    // [E]

            fuels.push_back(makeFuel(
                "propane", 3, 8, 0,
                46.33e6, 0.0, 1.88,
                Poly_C3H8, 3 * M_C + 8 * M_H,
                metghalchiKeck(1.08, 34.2, -138.7)));

            FuelDefinition hydrogen = makeFuel(
                "hydrogen", 0, 2, 0,
                119.96e6, 0.0, 0.085,
                Poly_H2, 2 * M_H,
                metghalchiKeck(1.8, 290.0, -125.0));     // [E]
            hydrogen.leanLimit = 0.15;
            hydrogen.richLimit = 4.0;
            fuels.push_back(hydrogen);
        }
    };

    const FuelLibrary &fuelLibrary() {
        static const FuelLibrary library;
        return library;
    }

} /* namespace */

bool findFuel(const std::string &name_, FuelDefinition *fuel) {
    std::string name = name_;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return (c == '_' || c == ' ') ? '-' : static_cast<char>(std::tolower(c));
    });

    if (name == "isooctane") name = "iso-octane";
    else if (name == "heptane") name = "n-heptane";
    else if (name == "natural-gas" || name == "cng") name = "methane";
    else if (name == "lpg") name = "propane";
    else if (name == "petrol") name = "gasoline";

    for (const FuelDefinition &f : fuelLibrary().fuels) {
        if (f.name == name) {
            *fuel = f;
            return true;
        }
    }

    return false;
}

std::vector<std::string> fuelNames() {
    std::vector<std::string> names;
    for (const FuelDefinition &f : fuelLibrary().fuels) {
        names.push_back(f.name);
    }

    return names;
}

const FuelDefinition &defaultFuel() {
    return fuelLibrary().fuels[0];
}

double Composition::total() const {
    double sum = 0;
    for (int i = 0; i < SpeciesCount; ++i) sum += n[i];
    return sum;
}

Composition freshCharge(const FuelDefinition &fuel, double phi) {
    Composition c;
    c.n[Fuel] = phi / fuel.stoichiometricO2();
    c.n[O2] = 1.0;
    c.n[N2] = air::x_N2 / air::x_O2;
    c.n[Ar] = air::x_Ar / air::x_O2;
    c.n[CO2] = air::x_CO2 / air::x_O2;

    const double total = c.total();
    for (int i = 0; i < SpeciesCount; ++i) c.n[i] /= total;

    return c;
}

double equivalenceRatio(const FuelDefinition &fuel, const Composition &c) {
    if (c.n[O2] <= 0) return (c.n[Fuel] > 0) ? INFINITY : 0;
    return c.n[Fuel] * fuel.stoichiometricO2() / c.n[O2];
}

double burn(
    const FuelDefinition &fuel,
    double fuelMoles,
    double o2Moles,
    Composition *delta,
    double K)
{
    *delta = Composition();

    double F = std::fmax(fuelMoles, 0.0);
    const double A = std::fmax(o2Moles, 0.0);
    if (F <= 0) return 0;

    const double a = fuel.stoichiometricO2();
    if (A >= a * F) {
        // Lean or stoichiometric: complete combustion
        delta->n[Fuel] = -F;
        delta->n[O2] = -a * F;
        delta->n[CO2] = fuel.C * F;
        delta->n[H2O] = (fuel.H / 2) * F;
        return F;
    }

    // Rich: all oxygen is consumed. Burn only as much fuel as can be
    // oxidized to at least CO.
    if (fuel.C > fuel.O) {
        F = std::fmin(F, 2 * A / (fuel.C - fuel.O));
    }

    if (F <= 0) return 0;

    const double X = fuel.C * F;                // carbon atoms
    const double Y = (fuel.H / 2) * F;          // hydrogen molecules
    const double O_atoms = 2 * A + fuel.O * F;
    const double B = O_atoms - X;               // oxygen left after CO

    // Water-gas shift equilibrium CO2 + H2 <-> CO + H2O:
    //     K = (n_CO * n_H2O) / (n_CO2 * n_H2)
    // With c = n_CO2: (X - c)(B - c) = K c (Y - B + c)
    double c;
    const double b = X + B + K * (Y - B);
    if (std::abs(K - 1) < 1E-9) {
        c = (X * B) / b;
    }
    else {
        const double disc = std::fmax(b * b + 4 * (K - 1) * X * B, 0.0);
        c = (-b + std::sqrt(disc)) / (2 * (K - 1));
    }

    c = std::fmin(std::fmax(c, std::fmax(0.0, B - Y)), std::fmin(X, B));

    delta->n[Fuel] = -F;
    delta->n[O2] = -A;
    delta->n[CO2] = c;
    delta->n[CO] = X - c;
    delta->n[H2O] = B - c;
    delta->n[H2] = Y - (B - c);

    return F;
}

double heatRelease(const FuelDefinition &fuel, const Composition &delta) {
    double release = -delta.n[Fuel] * fuel.energy.chem;
    for (int i = 1; i < SpeciesCount; ++i) {
        release -= delta.n[i] * species(i).energy.chem;
    }

    return release;
}

} /* namespace thermo */
