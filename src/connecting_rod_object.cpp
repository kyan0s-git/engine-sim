#include "../include/connecting_rod_object.h"

#include "../include/engine_sim_application.h"
#include "../include/units.h"
#include "../include/ui_utilities.h"

ConnectingRodObject::ConnectingRodObject() {
    m_connectingRod = nullptr;
}

ConnectingRodObject::~ConnectingRodObject() {
    /* void */
}

void ConnectingRodObject::generateGeometry() {
    GeometryGenerator *gen = m_app->getGeometryGenerator();
    const int rodJournalCount = m_connectingRod->getRodJournalCount();

    GeometryGenerator::Line2dParameters params;
    params.x0 = params.x1 = 0;
    params.y0 = (float)(m_connectingRod->getBigEndLocal() + m_connectingRod->getCrankshaft()->getThrow() * 0.6);
    params.y1 = (float)m_connectingRod->getLittleEndLocal();
    params.lineWidth = (float)(m_connectingRod->getCrankshaft()->getThrow() * 0.5);

    gen->startShape();
    gen->generateLine2d(params);

    if (rodJournalCount > 0) {
        GeometryGenerator::Circle2dParameters circleParams;
        circleParams.radius = static_cast<float>(m_connectingRod->getSlaveThrow()) * 1.5f;
        circleParams.center_x = 0.0f;
        circleParams.center_y = static_cast<float>(m_connectingRod->getBigEndLocal());

        gen->generateCircle2d(circleParams);
    }

    gen->endShape(&m_connectingRodBody);

    if (rodJournalCount > 0) {
        gen->startShape();

        GeometryGenerator::Circle2dParameters circleParams;
        circleParams.radius = static_cast<float>(m_connectingRod->getCrankshaft()->getThrow()) * 0.2f;
        for (int i = 0; i < rodJournalCount; ++i) {
            double x, y;
            m_connectingRod->getRodJournalPositionLocal(i, &x, &y);

            circleParams.center_x = static_cast<float>(x);
            circleParams.center_y = static_cast<float>(y);

            gen->generateCircle2d(circleParams);
        }

        gen->endShape(&m_pins);
    }
}

void ConnectingRodObject::render(const ViewParameters *view) {
    if (m_connectingRod->getRodJournalCount() > 0 && view->Sublayer != 1) return;
    else if (m_connectingRod->getRodJournalCount() == 0 && view->Sublayer != 0) return;

    const int layer = m_connectingRod->getLayer();
    if (layer > view->Layer1 || layer < view->Layer0) return;

    const ysVector grey0 = mix(m_app->getBackgroundColor(), m_app->getForegroundColor(), 0.9333f);
    const ysVector grey1 = mix(m_app->getBackgroundColor(), m_app->getForegroundColor(), 0.8667f);
    const ysVector grey2 = mix(m_app->getBackgroundColor(), m_app->getForegroundColor(), 0.05f);

    using Material = EngineSimApplication::Material;
    ysVector color =
        (m_connectingRod->getPiston()->getCylinderBank()->getIndex() % 2 == 0)
        ? grey0
        : grey1;
    if (m_app->isRealisticView()) {
        color = m_app->materialColor(Material::Rod, color);
        if (m_connectingRod->getPiston()->getCylinderBank()->getIndex() % 2 != 0) {
            color = ysMath::Mul(color, ysMath::LoadVector(0.9f, 0.9f, 0.9f, 1.0f));
        }
    }
    color = tintByLayer(color, layer - view->Layer0);

    resetShader();
    setTransform(
        &m_connectingRod->m_body,
        (float)m_connectingRod->getCrankshaft()->getThrow(),
        0.0f,
        (float)m_connectingRod->getBigEndLocal());

    m_app->getShaders()->SetBaseColor(color);
    m_app->useMaterial(Material::Rod, Shaders::Surface::Metal);
    m_app->getEngine()->DrawModel(
        m_app->getShaders()->GetRegularFlags(),
        m_app->getAssetManager()->GetModelAsset("ConnectingRod"),
        0x32 - layer);

    m_app->getShaders()->SetBaseColor(color);
    m_app->useMaterial(Material::Rod, Shaders::Surface::Bar, 1.0f, true);
    setTransform(&m_connectingRod->m_body);
    m_app->drawGenerated(m_connectingRodBody, 0x32 - layer);

    if (m_connectingRod->getRodJournalCount() > 0) {
        const ysVector shadow =
            tintByLayer(m_app->materialColor(Material::Pin, grey2), layer - view->Layer0);

        m_app->getShaders()->SetBaseColor(shadow);
        m_app->useMaterial(Material::Pin, Shaders::Surface::Journal, 1.0f, true);
        m_app->drawGenerated(m_pins, 0x32 - layer);
    }
}

void ConnectingRodObject::process(float dt) {
    /* void */
}

void ConnectingRodObject::destroy() {
    /* void */
}
