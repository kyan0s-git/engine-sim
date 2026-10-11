#ifndef ATG_ENGINE_SIM_WANKEL_ENGINE_NODE_H
#define ATG_ENGINE_SIM_WANKEL_ENGINE_NODE_H

#include "engine_node.h"

#include "exhaust_system_node.h"
#include "function_node.h"
#include "ignition_wire_node.h"
#include "intake_node.h"
#include "rod_journal_node.h"

#include "../../include/wankel_combustion_chamber.h"
#include "../../include/wankel_engine.h"

#include <cmath>
#include <memory>
#include <set>
#include <vector>

namespace es_script {

    class WankelRotorNode : public ObjectReferenceNode<WankelRotorNode> {
    public:
        WankelRotorNode() { /* void */ }
        virtual ~WankelRotorNode() { /* void */ }

        double getMass() const { return m_mass; }
        double getMomentOfInertia() const { return m_momentOfInertia; }
        double getRadius() const { return m_radius; }
        double getCrownGearRadius() const { return m_crownGearRadius; }
        double getDepth() const { return m_depth; }
        double getDepressionVolume() const { return m_depressionVolume; }

    protected:
        virtual void registerInputs() {
            addInput("mass", &m_mass);
            addInput("moment_of_inertia", &m_momentOfInertia);
            addInput("radius", &m_radius);
            addInput("crown_gear_radius", &m_crownGearRadius);
            addInput("depth", &m_depth);
            addInput("depression_volume", &m_depressionVolume);

            // Accepted for compatibility with other script libraries. The
            // rotor's phase is fully determined by the eccentric shaft.
            addInput("angle", &m_angle);

            ObjectReferenceNode<WankelRotorNode>::registerInputs();
        }

        virtual void _evaluate() {
            setOutput(this);
            readAllInputs();
        }

        double m_mass = 0.0;
        double m_momentOfInertia = 0.0;
        double m_radius = 0.0;
        double m_crownGearRadius = 0.0;
        double m_angle = 0.0;
        double m_depth = 0.0;
        double m_depressionVolume = 0.0;
    };

    class WankelPeripheralPortNode : public ObjectReferenceNode<WankelPeripheralPortNode> {
    public:
        WankelPeripheralPortNode() { /* void */ }
        virtual ~WankelPeripheralPortNode() { /* void */ }

        WankelRotor::Port generate(EngineContext *context) const {
            WankelRotor::Port port;
            port.startAngle = std::fmin(m_startAngle, m_endAngle);
            port.endAngle = std::fmax(m_startAngle, m_endAngle);
            port.flowRate = (m_flowRate != nullptr)
                ? m_flowRate->generate(context)
                : nullptr;

            return port;
        }

    protected:
        virtual void registerInputs() {
            addInput("start_angle", &m_startAngle);
            addInput("end_angle", &m_endAngle);
            addInput("flow_rate", &m_flowRate, InputTarget::Type::Object);

            ObjectReferenceNode<WankelPeripheralPortNode>::registerInputs();
        }

        virtual void _evaluate() {
            setOutput(this);
            readAllInputs();
        }

        double m_startAngle = 0.0;
        double m_endAngle = 0.0;
        FunctionNode *m_flowRate = nullptr;
    };

    class WankelEngineNode : public EngineNode {
    public:
        struct Rotor {
            WankelRotorNode *rotor;
            RodJournalNode *rodJournal;
            IntakeNode *intake;
            ExhaustSystemNode *exhaust;
            WankelPeripheralPortNode *intakePort;
            WankelPeripheralPortNode *exhaustPort;
            double soundAttenuation;
            double primaryLength;
            double intakeRunnerCrossSectionArea;
            double exhaustCrossSectionArea;
            double sparkPlugAngle;
        };

    public:
        WankelEngineNode() { /* void */ }
        virtual ~WankelEngineNode() { /* void */ }

        void addRotor(const Rotor &rotor, IgnitionWireNode *wire) {
            m_rotors.push_back(std::make_unique<Rotor>(rotor));

            // A rotor has a single spark plug which is wired to its first
            // chamber
            if (wire != nullptr) {
                wire->connect(m_rotors.back().get(), 0);
            }
        }

        int getRotorCount() const { return static_cast<int>(m_rotors.size()); }

        virtual Engine *createEngine() const override {
            return new WankelEngine;
        }

        virtual void buildEngine(Engine *engine_) override {
            WankelEngine *engine = static_cast<WankelEngine *>(engine_);
            const int rotorCount = getRotorCount();

            std::set<ExhaustSystemNode *> exhaustSystems;
            std::set<IntakeNode *> intakes;
            for (const auto &rotor : m_rotors) {
                exhaustSystems.insert(rotor->exhaust);
                intakes.insert(rotor->intake);
            }

            EngineContext context;
            context.setEngine(engine);

            Engine::Parameters parameters = m_parameters;
            parameters.crankshaftCount = static_cast<int>(m_crankshafts.size());
            parameters.cylinderBanks = rotorCount;
            parameters.cylinderCount = rotorCount * WankelRotor::FaceCount;
            parameters.exhaustSystemCount = static_cast<int>(exhaustSystems.size());
            parameters.intakeCount = static_cast<int>(intakes.size());
            parameters.throttle = m_throttle->generate();
            engine->initialize(parameters);

            {
                int i = 0;
                for (ExhaustSystemNode *exhaust : exhaustSystems) {
                    context.addExhaust(exhaust, engine->getExhaustSystem(i++));
                    exhaust->generate(&context);
                }
            }

            {
                int i = 0;
                for (IntakeNode *intake : intakes) {
                    context.addIntake(intake, engine->getIntake(i++));
                    intake->generate(&context);
                }
            }

            for (int i = 0; i < parameters.crankshaftCount; ++i) {
                m_crankshafts[i]->generate(engine->getCrankshaft(i), &context);

                // Every face goes through a full cycle in three revolutions
                engine->getCrankshaft(i)->setCycleLength(6 * constants::pi);
            }

            for (int r = 0; r < rotorCount; ++r) {
                generateRotor(r, *m_rotors[r], engine, &context);
            }

            m_ignitionModule->generate(engine, &context);

            Fuel *fuel = engine->getFuel();
            m_fuel->generate(fuel, &context);

            Function *meanPistonSpeedToTurbulence = engine->addOwnedObject(new Function);
            meanPistonSpeedToTurbulence->initialize(30, 1);
            for (int i = 0; i < 30; ++i) {
                const double s = static_cast<double>(i);
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

            for (int i = 0; i < engine->getCylinderCount(); ++i) {
                ccParams.Piston = engine->getPiston(i);
                ccParams.Head = engine->getHead(ccParams.Piston->getCylinderBank()->getIndex());
                engine->getChamber(i)->initialize(ccParams);
            }
        }

    protected:
        void generateRotor(
            int index,
            const Rotor &node,
            WankelEngine *engine,
            EngineContext *context) const
        {
            const int firstChamber = index * WankelRotor::FaceCount;
            Crankshaft *crankshaft =
                context->getCrankshaft(node.rodJournal->getCrankshaft());

            WankelRotor::Parameters params;
            params.crankshaft = crankshaft;
            params.rodJournal = context->getRodJournalIndex(node.rodJournal);
            params.generatingRadius = node.rotor->getRadius();
            params.depth = node.rotor->getDepth();
            params.depressionVolume = node.rotor->getDepressionVolume();
            params.mass = node.rotor->getMass();
            params.momentOfInertia = node.rotor->getMomentOfInertia();
            params.crownGearRadius = node.rotor->getCrownGearRadius();
            params.sparkPlugAngle = node.sparkPlugAngle;
            params.intakePort = node.intakePort->generate(context);
            params.exhaustPort = node.exhaustPort->generate(context);
            params.calculationQuality = m_rotorCalculationQuality;
            params.housingResolution = m_rotorHousingResolution;
            params.index = index;
            params.firstChamber = firstChamber;

            WankelRotor *rotor = engine->getRotor(index);
            rotor->initialize(params);

            // Present the rotor as a bank of three cylinders pointing towards
            // the spark plug side of the housing
            CylinderBank::Parameters bankParams;
            bankParams.crankshaft = crankshaft;
            bankParams.positionX = crankshaft->getPosX();
            bankParams.positionY = crankshaft->getPosY();
            bankParams.angle = crankshaft->getTdc() - constants::pi / 2;
            bankParams.bore = std::sqrt(4 * rotor->getChamberCrossSection() / constants::pi);
            bankParams.deckHeight = rotor->getGeneratingRadius() + rotor->getEccentricity();
            bankParams.displayDepth = 0.5;
            bankParams.cylinderCount = WankelRotor::FaceCount;
            bankParams.index = index;

            CylinderBank *bank = engine->getCylinderBank(index);
            bank->initialize(bankParams);

            // Short passages between the ports and the runners
            const double portPassageLength = units::distance(2.0, units::inch);
            CylinderHead::Parameters headParams;
            headParams.Bank = bank;
            headParams.ExhaustPortFlow = nullptr;
            headParams.IntakePortFlow = nullptr;
            headParams.Valvetrain = nullptr;
            headParams.CombustionChamberVolume = node.rotor->getDepressionVolume();
            headParams.IntakeRunnerCrossSectionArea = node.intakeRunnerCrossSectionArea;
            headParams.IntakeRunnerVolume = node.intakeRunnerCrossSectionArea * portPassageLength;
            headParams.ExhaustRunnerCrossSectionArea = node.exhaustCrossSectionArea;
            headParams.ExhaustRunnerVolume = node.exhaustCrossSectionArea * portPassageLength;

            CylinderHead *head = engine->getHead(index);
            head->initialize(headParams);

            Intake *intake = context->getIntake(node.intake);
            ExhaustSystem *exhaust = context->getExhaust(node.exhaust);

            for (int face = 0; face < WankelRotor::FaceCount; ++face) {
                const int chamberIndex = firstChamber + face;

                Piston::Parameters pistonParams;
                pistonParams.Rod = engine->getConnectingRod(chamberIndex);
                pistonParams.Bank = bank;
                pistonParams.CylinderIndex = face;
                pistonParams.BlowbyFlowCoefficient = 0.0;
                pistonParams.CompressionHeight = 0.0;
                pistonParams.WristPinPosition = 0.0;
                pistonParams.Displacement = 0.0;
                pistonParams.mass = 0.0;
                engine->getPiston(chamberIndex)->initialize(pistonParams);

                head->setIntake(face, intake);
                head->setExhaustSystem(face, exhaust);
                head->setSoundAttenuation(face, node.soundAttenuation);
                head->setHeaderPrimaryLength(face, node.primaryLength);

                static_cast<WankelCombustionChamber *>(engine->getChamber(chamberIndex))
                    ->setRotor(rotor, face);
            }

            context->setCylinderIndex(&node, 0, firstChamber);
        }

        virtual void registerInputs() override {
            addInput("rotor_calculation_quality", &m_rotorCalculationQuality);
            addInput("rotor_housing_resolution", &m_rotorHousingResolution);

            EngineNode::registerInputs();
        }

        int m_rotorCalculationQuality = 1024;
        int m_rotorHousingResolution = 256;

        std::vector<std::unique_ptr<Rotor>> m_rotors;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_WANKEL_ENGINE_NODE_H */
