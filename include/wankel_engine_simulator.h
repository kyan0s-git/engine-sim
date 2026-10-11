#ifndef ATG_ENGINE_SIM_WANKEL_ENGINE_SIMULATOR_H
#define ATG_ENGINE_SIM_WANKEL_ENGINE_SIMULATOR_H

#include "piston_engine_simulator.h"

class WankelEngine;

// Simulator for Wankel engines. Reuses the crankshaft, drivetrain, gas
// exchange and audio handling of the piston engine simulator; the rotors
// act directly on the eccentric shaft through their chambers' pressure.
class WankelEngineSimulator : public PistonEngineSimulator {
    public:
        WankelEngineSimulator();
        virtual ~WankelEngineSimulator() override;

    protected:
        virtual void addEngineParts() override;
        virtual void placeParts() override;
        virtual void processIgnitionEvents() override;

        WankelEngine *getWankelEngine() const;
};

#endif /* ATG_ENGINE_SIM_WANKEL_ENGINE_SIMULATOR_H */
