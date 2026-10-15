#ifndef ATG_ENGINE_SIM_SHADERS_H
#define ATG_ENGINE_SIM_SHADERS_H

#include "delta.h"

#include "ui_math.h"

class Shaders : public dbasic::ShaderBase {
    public:
        Shaders();
        ~Shaders();

        ysError Initialize(
                dbasic::ShaderSet *shaderSet,
                ysRenderTarget *mainRenderTarget,
                ysRenderTarget *uiRenderTarget,
                ysShaderProgram *shaderProgram,
                ysInputLayout *inputLayout);
        virtual ysError UseMaterial(dbasic::Material *material);
		virtual void SetObjectTransform(const ysMatrix &mat);
		virtual void ConfigureModel(float scale, dbasic::ModelAsset *model);

        void SetBaseColor(const ysVector &color);
        void ResetBaseColor();

        // Shading of the 2D parts (see delta_engine_shader.frag)
        enum class Surface {
            Flat = 0,
            Cylinder = 2,       // across the model's local x, +-halfWidth
            Bar = 3,            // round bar across a generated line's width
            Journal = 4,        // dome over a generated circle
            Plate = 5,          // bevelled plate across a line's width
            Metal = 6,          // flat metal
            Turned = 7,         // machined disc (radius = halfWidth)
            Liner = 8           // honed cylinder bore
        };

        struct SurfaceParameters {
            float highlight = 0.35f;
            float shininess = 24.0f;
            float metallic = 0.6f;
            float grain = 0.04f;
            float grainDensity = 900.0f;    // cells per local unit
            float halfWidth = 1.0f;
        };

        void SetSurface(Surface surface, const SurfaceParameters &params);
        void SetSurface(Surface surface);
        void SetEmission(const ysVector &emission);

        dbasic::StageEnableFlags GetRegularFlags() const;
        dbasic::StageEnableFlags GetUiFlags() const;

        void CalculateCamera(
            float width,
            float height,
            const Bounds &cameraBounds,
            float screenWidth,
            float screenHeight,
            float angle = 0.0f);
        void CalculateUiCamera(float screenWidth, float screenHeight);

        void SetClearColor(const ysVector &col);

    public:
        dbasic::ShaderScreenVariables m_screenVariables;
        dbasic::ShaderScreenVariables m_uiScreenVariables;
        dbasic::ShaderObjectVariables m_objectVariables;

        ysVector m_cameraPosition;

    protected:
        dbasic::ShaderStage *m_mainStage;
        dbasic::ShaderStage *m_uiStage;

        dbasic::LightingControls m_lightingControls;
};

#endif /* ATG_ENGINE_SIM_SHADERS_H */
