#ifndef ATG_ENGINE_SIM_VEHICLE_NODE_H
#define ATG_ENGINE_SIM_VEHICLE_NODE_H

#include "object_reference_node.h"

#include "engine_sim.h"

namespace es_script {

    class VehicleNode : public ObjectReferenceNode<VehicleNode> {
    public:
        VehicleNode() { /* void */ }
        virtual ~VehicleNode() { /* void */ }

        void generate(Vehicle *vehicle) const
        {
            vehicle->initialize(m_parameters);
        }

    protected:
        virtual void registerInputs() {
            addInput("mass", &m_parameters.mass);
            addInput("drag_coefficient", &m_parameters.dragCoefficient);
            addInput("cross_sectional_area", &m_parameters.crossSectionArea);
            addInput("diff_ratio", &m_parameters.diffRatio);
            addInput("tire_radius", &m_parameters.tireRadius);
            addInput("rolling_resistance", &m_parameters.rollingResistance);
            addInput("stiffness", &m_parameters.stiffness);
            addInput("damping", &m_parameters.damping);
            addInput("max_flex", &m_parameters.maxFlex);
            addInput("limit_flex", &m_parameters.limitFlex);
            addInput("simulate_flex", &m_parameters.simulateFlex);
            addInput("max_brake_force", &m_parameters.maxBrakeForce);

            // Accepted for compatibility with scripts written for newer
            // versions of Engine Simulator; not used by this simulation
            addInput("coeff_static_friction", &m_unused.coeffStaticFriction);
            addInput("coeff_dynamic_friction", &m_unused.coeffDynamicFriction);
            addInput("wheel_base", &m_unused.wheelBase);
            addInput("track", &m_unused.track);
            addInput("max_brake_torque", &m_unused.maxBrakeTorque);
            addInput("center_of_mass", &m_unused.centerOfMass);
            addInput("weight_transfer_bias", &m_unused.weightTransferBias);
            addInput("vehicle_asset_file", &m_unused.vehicleAssetFile);

            ObjectReferenceNode<VehicleNode>::registerInputs();
        }

        virtual void _evaluate() {           
            setOutput(this);
            // Read inputs
            readAllInputs();

        }

        Vehicle::Parameters m_parameters;

        struct {
            double coeffStaticFriction = 0.7;
            double coeffDynamicFriction = 0.49;
            double wheelBase = 0.0;
            double track = 0.0;
            double maxBrakeTorque = 0.0;
            double centerOfMass = 0.0;
            double weightTransferBias = 1.0;
            std::string vehicleAssetFile;
        } m_unused;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_VEHICLE_NODE_H */
