#ifndef ATG_ENGINE_SIM_WANKEL_ENGINE_H
#define ATG_ENGINE_SIM_WANKEL_ENGINE_H

#include "engine.h"

#include "wankel_rotor.h"

// Wankel (rotary) engine.
//
// Each rotor is presented to the rest of the application as a cylinder bank
// with three "cylinders" (one per rotor face) so that per-cylinder displays
// and the exhaust/audio model work unchanged. The pistons and connecting
// rods of those cylinders only carry bookkeeping information and take no
// part in the simulation.
class WankelEngine : public Engine {
    public:
        WankelEngine();
        virtual ~WankelEngine() override;

        // cylinderBanks is the number of rotors; cylinderCount must be three
        // times the number of rotors
        void initialize(const Parameters &params);
        virtual void destroy() override;

        virtual void calculateDisplacement() override;

        // The piston engine friction models don't apply to rotary engines;
        // the eccentric shaft's friction torque is used
        virtual void initializeFriction() override;
        virtual Simulator *createSimulator(Vehicle *vehicle, Transmission *transmission) override;

        int getRotorCount() const { return m_rotorCount; }
        WankelRotor *getRotor(int i) const { return &m_rotors[i]; }

    protected:
        virtual CombustionChamber *createCombustionChamber(int index) override;

        WankelRotor *m_rotors;
        int m_rotorCount;
};

#endif /* ATG_ENGINE_SIM_WANKEL_ENGINE_H */
