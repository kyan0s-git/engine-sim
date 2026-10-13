#ifndef ATG_ENGINE_SIM_ENGINE_NODE_H
#define ATG_ENGINE_SIM_ENGINE_NODE_H

#include "object_reference_node.h"

#include "crankshaft_node.h"
#include "cylinder_bank_node.h"
#include "ignition_module_node.h"
#include "engine_context.h"
#include "fuel_node.h"
#include "throttle_nodes.h"

#include "engine_sim.h"

#include <map>
#include <set>

namespace es_script {

    class EngineNode : public ObjectReferenceNode<EngineNode> {
    public:
        EngineNode() { /* void */ }
        virtual ~EngineNode() { /* void */ }

        virtual Engine *createEngine() const {
            return new Engine;
        }

        virtual void buildEngine(Engine *engine) {
            int cylinderCount = 0;
            for (const CylinderBankNode *bank : m_cylinderBanks) {
                cylinderCount += bank->getCylinderCount();
            }

            std::set<ExhaustSystemNode *> exhaustSystems;
            std::set<IntakeNode *> intakes;
            for (const CylinderBankNode *bank : m_cylinderBanks) {
                const int n = bank->getCylinderCount();
                for (int i = 0; i < n; ++i) {
                    exhaustSystems.insert(bank->getCylinder(i).exhaust);
                    intakes.insert(bank->getCylinder(i).intake);
                }
            }

            EngineContext context;
            context.setEngine(engine);

            Engine::Parameters parameters = m_parameters;
            parameters.crankshaftCount = (int)m_crankshafts.size();
            parameters.cylinderBanks = (int)m_cylinderBanks.size();
            parameters.cylinderCount = cylinderCount;
            parameters.exhaustSystemCount = (int)exhaustSystems.size();
            parameters.intakeCount = (int)intakes.size();
            parameters.throttle = m_throttle->generate();

            std::vector<std::string> warnings;
            resolvePhysicsParameters(&parameters, &warnings);
            engine->initialize(parameters);
            for (const std::string &warning : warnings) {
                engine->addWarning(warning);
            }

            // The fuel is needed by the intakes and exhausts of the
            // engineering model
            Fuel *fuel = engine->getFuel();
            m_fuel->generate(fuel, &context);
            checkFuel(engine);

            {
                int i = 0;
                for (ExhaustSystemNode *exhaust : exhaustSystems) {
                    context.addExhaust(
                        exhaust, engine->getExhaustSystem(i++));
                }
            }

            {
                int i = 0;
                for (IntakeNode *intake : intakes) {
                    context.addIntake(
                        intake, engine->getIntake(i++));
                }
            }

            {
                int i = 0;
                for (const CylinderBankNode *bank : m_cylinderBanks) {
                    context.addHead(bank->getCylinderHead(), engine->getHead(i++));
                }
            }

            for (const CylinderBankNode *bank : m_cylinderBanks) {
                const int n = bank->getCylinderCount();
                for (int i = 0; i < n; ++i) {
                    exhaustSystems.insert(bank->getCylinder(i).exhaust);
                    intakes.insert(bank->getCylinder(i).intake);
                }
            }

            for (int i = 0; i < parameters.crankshaftCount; ++i) {
                m_crankshafts[i]->generate(engine->getCrankshaft(i), &context);
            }

            for (int i = 0; i < parameters.cylinderBanks; ++i) {
                m_cylinderBanks[i]->indexSlaveJournals(&context);
            }

            int cylinderIndex = 0;
            for (int i = 0; i < parameters.cylinderBanks; ++i) {
                m_cylinderBanks[i]->generate(
                    i,
                    cylinderIndex,
                    engine->getCylinderBank(i),
                    engine->getCrankshaft(0),
                    engine,
                    &context);
                cylinderIndex += m_cylinderBanks[i]->getCylinderCount();
            }

            for (int i = 0; i < parameters.cylinderBanks; ++i) {
                m_cylinderBanks[i]->connectRodAssemblies(&context);
            }

            m_ignitionModule->generate(engine, &context);
            
            Function *meanPistonSpeedToTurbulence = engine->addOwnedObject(new Function);
            meanPistonSpeedToTurbulence->initialize(30, 1);
            for (int i = 0; i < 30; ++i) {
                const double s = (double)i;
                meanPistonSpeedToTurbulence->addSample(s, s * 0.5);
            }

            CombustionChamber::Parameters ccParams;
            ccParams.CrankcasePressure = units::pressure(1.0, units::atm);
            ccParams.Fuel = fuel;
            ccParams.StartingPressure = units::pressure(1.0, units::atm);
            ccParams.StartingTemperature = units::celcius(25.0);
            ccParams.MeanPistonSpeedToTurbulence = meanPistonSpeedToTurbulence;
            ccParams.HeatTransferCoefficient = parameters.heatTransferCoefficient;
            ccParams.BlockTemperature = parameters.blockTemperature;
            ccParams.WallTemperature = parameters.wallTemperature;
            ccParams.HeatTransferMultiplier = parameters.heatTransferMultiplier;

            engine->initializeFriction();
            ccParams.PistonFriction = !engine->usesFrictionModel();

            for (int i = 0; i < engine->getCylinderCount(); ++i) {
                ccParams.Piston = engine->getPiston(i);
                ccParams.Head = engine->getHead(ccParams.Piston->getCylinderBank()->getIndex());
                engine->getChamber(i)->initialize(ccParams);
            }
        }

        // Converts the physics model names of the script into parameters
        void resolvePhysicsParameters(
            Engine::Parameters *parameters,
            std::vector<std::string> *warnings) const
        {
            if (m_physicsModel == "engineering") {
                parameters->physicsModel = GasSystem::Model::Engineering;
            }
            else {
                parameters->physicsModel = GasSystem::Model::Legacy;
                if (m_physicsModel != "legacy" && !m_physicsModel.empty()) {
                    warnings->push_back(
                        "Unknown physics_model \"" + m_physicsModel + "\"; using \"legacy\"");
                }
            }

            const bool engineering =
                parameters->physicsModel == GasSystem::Model::Engineering;
            if (!EngineFriction::parseModel(
                m_frictionModel, engineering, &parameters->friction.model))
            {
                warnings->push_back("Unknown friction_model \"" + m_frictionModel + "\"");
            }

            if (!EngineFriction::parseValvetrain(
                m_valvetrainType,
                &parameters->friction.valvetrain,
                &parameters->friction.rollerFollowers))
            {
                warnings->push_back("Unknown valvetrain_type \"" + m_valvetrainType + "\"");
            }
        }

        void checkFuel(Engine *engine) const {
            if (!engine->getFuel()->isKnownType()) {
                engine->addWarning("Unknown fuel type; using gasoline");
            }
        }

        void addCrankshaft(CrankshaftNode *crankshaft) {
            m_crankshafts.push_back(crankshaft);
        }

        void addCylinderBank(CylinderBankNode *bank) {
            m_cylinderBanks.push_back(bank);
        }

        int getIgnitionModuleCount() const {
            return m_ignitionModule == nullptr
                ? 0
                : 1;
        }

        void addIgnitionModule(IgnitionModuleNode *ignitionModule) {
            m_ignitionModule = ignitionModule;
        }

    protected:
        virtual void registerInputs() {
            addInput("name", &m_parameters.name);
            addInput("starter_torque", &m_parameters.starterTorque);
            addInput("starter_speed", &m_parameters.starterSpeed);
            addInput("dyno_min_speed", &m_parameters.dynoMinSpeed);
            addInput("dyno_max_speed", &m_parameters.dynoMaxSpeed);
            addInput("dyno_hold_step", &m_parameters.dynoHoldStep);
            addInput("redline", &m_parameters.redline);
            addInput("fuel", &m_fuel, InputTarget::Type::Object);
            addInput("throttle", &m_throttle, InputTarget::Type::Object);
            addInput("simulation_frequency", &m_parameters.initialSimulationFrequency);
            addInput("hf_gain", &m_parameters.initialHighFrequencyGain);
            addInput("jitter", &m_parameters.initialJitter);
            addInput("noise", &m_parameters.initialNoise);
            addInput("convolution", &m_parameters.initialConvolution);
            addInput("max_sle_solver_steps", &m_parameters.maxSleSolverSteps);
            addInput("fluid_simulation_steps", &m_parameters.fluidSimulationSteps);
            addInput("heat_transfer_coefficient", &m_parameters.heatTransferCoefficient);
            addInput("block_temperature", &m_parameters.blockTemperature);

            addInput("physics_model", &m_physicsModel);
            addInput("wall_temperature", &m_parameters.wallTemperature);
            addInput("heat_transfer_multiplier", &m_parameters.heatTransferMultiplier);
            addInput("friction_model", &m_frictionModel);
            addInput("friction_multiplier", &m_parameters.friction.multiplier);
            addInput("valvetrain_type", &m_valvetrainType);
            addInput("valves_per_cylinder", &m_parameters.friction.valvesPerCylinder);
            addInput("max_valve_lift", &m_parameters.friction.maxValveLift);
            addInput("main_bearing_diameter", &m_parameters.friction.mainBearingDiameter);
            addInput("main_bearing_length", &m_parameters.friction.mainBearingLength);
            addInput("main_bearing_count", &m_parameters.friction.mainBearingCount);
            addInput("chen_flynn_constant", &m_parameters.friction.chenFlynnConstant);
            addInput("chen_flynn_peak_pressure", &m_parameters.friction.chenFlynnPeakPressure);
            addInput("chen_flynn_piston_speed", &m_parameters.friction.chenFlynnPistonSpeed);
            addInput(
                "chen_flynn_piston_speed_squared",
                &m_parameters.friction.chenFlynnPistonSpeedSquared);

            ObjectReferenceNode<EngineNode>::registerInputs();
        }

        virtual void _evaluate() {
            setOutput(this);

            // Read inputs
            readAllInputs();
        }

        ThrottleNode *m_throttle = nullptr;
        IgnitionModuleNode *m_ignitionModule = nullptr;
        FuelNode *m_fuel = nullptr;

        Engine::Parameters m_parameters;
        std::string m_physicsModel = "legacy";
        std::string m_frictionModel = "auto";
        std::string m_valvetrainType = "dohc_bucket";
        std::vector<CrankshaftNode *> m_crankshafts;
        std::vector<CylinderBankNode *> m_cylinderBanks;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_ENGINE_NODE_H */
