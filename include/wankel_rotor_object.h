#ifndef ATG_ENGINE_SIM_WANKEL_ROTOR_OBJECT_H
#define ATG_ENGINE_SIM_WANKEL_ROTOR_OBJECT_H

#include "simulation_object.h"

#include "geometry_generator.h"
#include "wankel_engine.h"

// Draws a rotor of a Wankel engine along with its housing, ports, spark plug
// and the burning chamber(s)
class WankelRotorObject : public SimulationObject {
    public:
        WankelRotorObject();
        virtual ~WankelRotorObject() override;

        virtual void generateGeometry() override;
        virtual void render(const ViewParameters *view) override;
        virtual void process(float dt) override;
        virtual void destroy() override;

        WankelEngine *m_engine;
        WankelRotor *m_rotor;

    protected:
        void generateHousingBand(
            double startAngle,
            double endAngle,
            double offset,
            double width,
            GeometryGenerator::GeometryIndices *indices);

        GeometryGenerator::GeometryIndices m_housing;
        GeometryGenerator::GeometryIndices m_intakePort;
        GeometryGenerator::GeometryIndices m_exhaustPort;
        GeometryGenerator::GeometryIndices m_sparkPlug;
        GeometryGenerator::GeometryIndices m_rotorBody;
        GeometryGenerator::GeometryIndices m_flames[WankelRotor::FaceCount];
        bool m_lit[WankelRotor::FaceCount];
};

#endif /* ATG_ENGINE_SIM_WANKEL_ROTOR_OBJECT_H */
