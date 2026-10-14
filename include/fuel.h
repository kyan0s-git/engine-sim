#ifndef ATG_ENGINE_FUEL_H
#define ATG_ENGINE_FUEL_H

#include "units.h"

#include "function.h"
#include "thermodynamics.h"

#include <string>

class Fuel {
    public:
        struct Parameters {
            std::string name = "Gasoline";
            double molecularMass =
                units::mass(100.0, units::g);
            double energyDensity =
                units::energy(48.1, units::kJ) / units::mass(1.0, units::g);
            double density =
                units::mass(0.755, units::kg) / units::volume(1.0, units::L);
            double molecularAfr = 25 / 2.0;
            double burningEfficiencyRandomness = 0.5;
            double lowEfficiencyAttenuation = 0.6;
            double maxBurningEfficiency = 0.8;
            double maxTurbulenceEffect = 2.0;
            double maxDilutionEffect = 50.0;
            Function *turbulenceToFlameSpeedRatio = nullptr;

            // Engineering physics model: fuel from the built-in library
            // (see thermo::findFuel) and the fraction of the fuel in the
            // burned charge that is oxidized (crevice and quench losses)
            std::string type = "gasoline";
            double combustionEfficiency = 0.97;
        };

        Fuel();
        ~Fuel();

        void initialize(const Parameters &params);

        inline double getMolecularMass() const { return m_molecularMass; }
        inline double getEnergyDensity() const { return m_energyDensity; }
        inline double getDensity() const { return m_density; }
        inline double getBurningEfficiencyRandomness() const { return m_burningEfficiencyRandomness; }
        inline double getLowEfficiencyAttenuation() const { return m_lowEfficiencyAttenuation;  }
        inline double getMaxBurningEfficiency() const { return m_maxBurningEfficiency; }
        inline double getMaxTurbulenceEffect() const { return m_maxTurbulenceEffect; }
        inline double getMaxDilutionEffect() const { return m_maxDilutionEffect; }

        double flameSpeed(
            double turbulence,
            double molecularAfr,
            double T,
            double P,
            double firingPressure,
            double motoringPressure) const;
        virtual double laminarBurningVelocity(double molecularAfr, double T, double P) const;

        // Turbulent flame speed for a turbulence intensity and laminar
        // burning velocity
        double turbulentFlameSpeed(double turbulence, double S_L) const;

        double getMolecularAfr() const { return m_molecularAfr; }

        // Engineering physics model
        const thermo::FuelDefinition &getDefinition() const { return m_definition; }
        double getCombustionEfficiency() const { return m_combustionEfficiency; }
        void setCombustionEfficiency(double efficiency) { m_combustionEfficiency = efficiency; }
        bool isKnownType() const { return m_knownType; }

        // Laminar burning velocity [m/s] for the equivalence ratio `phi`,
        // unburned gas temperature and pressure and burned gas (residual)
        // mole fraction
        double laminarBurningVelocityEngineering(
            double phi,
            double T_u,
            double P,
            double residualFraction) const;

    protected:
        std::string m_name;
        double m_molecularMass;
        double m_energyDensity;
        double m_density;
        double m_molecularAfr;
        double m_maxBurningEfficiency;
        double m_burningEfficiencyRandomness;
        double m_lowEfficiencyAttenuation;
        double m_maxTurbulenceEffect;
        double m_maxDilutionEffect;

        Function *m_turbulenceToFlameSpeedRatio;

        thermo::FuelDefinition m_definition;
        double m_combustionEfficiency;
        bool m_knownType;
};

#endif /* ATG_ENGINE_FUEL_H */
