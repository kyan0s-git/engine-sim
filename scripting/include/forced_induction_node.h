#ifndef ATG_ENGINE_SIM_FORCED_INDUCTION_NODE_H
#define ATG_ENGINE_SIM_FORCED_INDUCTION_NODE_H

#include "object_reference_node.h"

#include "engine_sim.h"

#include "../../include/forced_induction.h"

#include <string>

namespace es_script {

    class ForcedInductionNode : public ObjectReferenceNode<ForcedInductionNode> {
    public:
        ForcedInductionNode() { /* void */ }
        virtual ~ForcedInductionNode() { /* void */ }

        // Creates the turbocharger or supercharger and connects it to the
        // engine's intakes and exhausts
        void generate(Engine *engine) const {
            ForcedInduction::Parameters params;

            if (m_type == "roots") params.type = ForcedInduction::Type::Roots;
            else if (m_type == "twin_screw") params.type = ForcedInduction::Type::TwinScrew;
            else if (m_type == "centrifugal") params.type = ForcedInduction::Type::Centrifugal;
            else {
                params.type = ForcedInduction::Type::Turbocharger;
                if (m_type != "turbo" && m_type != "turbocharger") {
                    engine->addWarning("Unknown forced induction type \"" + m_type + "\"; using \"turbo\"");
                }
            }

            if (m_arrangement == "twin") params.arrangement = ForcedInduction::Arrangement::Twin;
            else if (m_arrangement == "sequential") params.arrangement = ForcedInduction::Arrangement::Sequential;
            else {
                params.arrangement = ForcedInduction::Arrangement::Single;
                if (m_arrangement != "single") {
                    engine->addWarning("Unknown turbocharger arrangement \"" + m_arrangement + "\"; using \"single\"");
                }
            }

            params.compressorDiameter = m_compressorDiameter;
            params.compressorFlow = m_compressorFlow;
            params.compressorEfficiency = m_compressorEfficiency;
            params.maxSpeed = m_maxSpeed;
            params.turbineAr = m_turbineAr;
            params.turbineEfficiency = m_turbineEfficiency;
            params.inertia = m_inertia;
            params.ballBearings = (m_bearings == "ball");
            params.boost = m_boost;
            params.blowoffValve = m_blowoffValve;
            params.driveRatio = m_driveRatio;
            params.displacement = m_displacement;
            params.bypassValve = m_bypassValve;
            params.intercoolerEffectiveness = m_intercoolerEffectiveness;
            params.chargeVolume = m_chargeVolume;
            params.sequentialSpeed = m_sequentialSpeed;
            params.model = engine->getPhysicsModel();
            params.fuel = &engine->getFuel()->getDefinition();

            ForcedInduction *forcedInduction = engine->addOwnedObject(new ForcedInduction);
            forcedInduction->initialize(params);
            engine->setForcedInduction(forcedInduction);
        }

    protected:
        virtual void registerInputs() {
            addInput("type", &m_type);
            addInput("arrangement", &m_arrangement);
            addInput("compressor_diameter", &m_compressorDiameter);
            addInput("compressor_flow", &m_compressorFlow);
            addInput("compressor_efficiency", &m_compressorEfficiency);
            addInput("max_speed", &m_maxSpeed);
            addInput("turbine_ar", &m_turbineAr);
            addInput("turbine_efficiency", &m_turbineEfficiency);
            addInput("inertia", &m_inertia);
            addInput("bearings", &m_bearings);
            addInput("boost", &m_boost);
            addInput("blowoff_valve", &m_blowoffValve);
            addInput("drive_ratio", &m_driveRatio);
            addInput("displacement", &m_displacement);
            addInput("bypass_valve", &m_bypassValve);
            addInput("intercooler_effectiveness", &m_intercoolerEffectiveness);
            addInput("charge_volume", &m_chargeVolume);
            addInput("sequential_speed", &m_sequentialSpeed);

            ObjectReferenceNode<ForcedInductionNode>::registerInputs();
        }

        virtual void _evaluate() {
            setOutput(this);
            readAllInputs();
        }

        std::string m_type = "turbo";
        std::string m_arrangement = "single";
        double m_compressorDiameter = 0.06;
        double m_compressorFlow = 0.3;
        double m_compressorEfficiency = 0.75;
        double m_maxSpeed = 0.0;
        double m_turbineAr = 0.64;
        double m_turbineEfficiency = 0.7;
        double m_inertia = 3e-5;
        std::string m_bearings = "journal";
        double m_boost = 1e5;
        bool m_blowoffValve = true;
        double m_driveRatio = 1.0;
        double m_displacement = 1.9e-3;
        bool m_bypassValve = true;
        double m_intercoolerEffectiveness = 0.7;
        double m_chargeVolume = 6e-3;
        double m_sequentialSpeed = 400.0;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_FORCED_INDUCTION_NODE_H */
