#ifndef ATG_ENGINE_SIM_WANKEL_COMBUSTION_CHAMBER_H
#define ATG_ENGINE_SIM_WANKEL_COMBUSTION_CHAMBER_H

#include "combustion_chamber.h"

class WankelRotor;

// Working chamber between one face of a Wankel rotor and the housing. The
// gas, flame and port flow models are shared with piston engines; only the
// geometry and the way the gas pressure drives the engine differ.
class WankelCombustionChamber : public CombustionChamber {
    public:
        WankelCombustionChamber();
        virtual ~WankelCombustionChamber() override;

        void setRotor(WankelRotor *rotor, int face);
        WankelRotor *getRotor() const { return m_rotor; }
        int getFace() const { return m_face; }

        virtual void apply(atg_scs::SystemState *system) override;

        virtual double getFrictionForce() const override;
        virtual double getVolume() const override;
        virtual double getSweptVolume() const override;
        virtual double pistonSpeed() const override;

        virtual double calculateIntakeFlowRate() const override;
        virtual double calculateExhaustFlowRate() const override;
        virtual double getIntakeOpening() const override;
        virtual double getExhaustOpening() const override;

    protected:
        double shaftAngle() const;

        WankelRotor *m_rotor;
        int m_face;
};

#endif /* ATG_ENGINE_SIM_WANKEL_COMBUSTION_CHAMBER_H */
