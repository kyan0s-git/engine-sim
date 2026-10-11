#include "../include/wankel_rotor_object.h"

#include "../include/constants.h"
#include "../include/engine_sim_application.h"
#include "../include/ui_utilities.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    constexpr int HousingSegments = 192;
    constexpr int FlankSegments = 48;
    constexpr double FaceAngle = 2 * constants::pi / 3;
} /* namespace */

WankelRotorObject::WankelRotorObject() {
    m_engine = nullptr;
    m_rotor = nullptr;

    for (int i = 0; i < WankelRotor::FaceCount; ++i) {
        m_lit[i] = false;
    }
}

WankelRotorObject::~WankelRotorObject() {
    /* void */
}

void WankelRotorObject::generateHousingBand(
    double startAngle,
    double endAngle,
    double offset,
    double width,
    GeometryGenerator::GeometryIndices *indices)
{
    GeometryGenerator *gen = m_app->getGeometryGenerator();
    const bool closed = (endAngle - startAngle) >= 2 * constants::pi - 1E-6;
    const int n = closed
        ? HousingSegments
        : std::max(2, static_cast<int>(HousingSegments * (endAngle - startAngle) / (2 * constants::pi)) + 1);

    const double cx = m_rotor->getCrankshaft()->getPosX();
    const double cy = m_rotor->getCrankshaft()->getPosY();

    std::vector<float> x0(n), y0(n), x1(n), y1(n);
    for (int i = 0; i < n; ++i) {
        const double alpha = closed
            ? startAngle + (2 * constants::pi * i) / n
            : startAngle + (endAngle - startAngle) * i / (n - 1.0);

        double x, y;
        m_rotor->getHousingPoint(alpha, &x, &y);

        // Offset away from the housing's center
        const double dx = x - cx, dy = y - cy;
        const double r = std::sqrt(dx * dx + dy * dy);
        const double s0 = (r + offset) / r;
        const double s1 = (r + offset + width) / r;

        x0[i] = static_cast<float>(cx + dx * s0);
        y0[i] = static_cast<float>(cy + dy * s0);
        x1[i] = static_cast<float>(cx + dx * s1);
        y1[i] = static_cast<float>(cy + dy * s1);
    }

    gen->startShape();
    gen->generateStrip2d(x0.data(), y0.data(), x1.data(), y1.data(), n, closed);
    gen->endShape(indices);
}

void WankelRotorObject::generateGeometry() {
    GeometryGenerator *gen = m_app->getGeometryGenerator();
    const double theta = m_rotor->getCrankshaft()->m_body.theta;
    const double phase = m_rotor->getPhase(theta);
    const double R = m_rotor->getGeneratingRadius();
    const double wall = R * 0.08;

    // Housing and ports (housing angles are measured from the minor axis)
    generateHousingBand(0.0, 2 * constants::pi, 0.0, wall, &m_housing);

    // Ports are drawn at least a few degrees wide so that narrow ports remain
    // visible
    auto portBand = [&](const WankelRotor::Port &port, GeometryGenerator::GeometryIndices *indices) {
        constexpr double MinDisplayWidth = 8 * constants::pi / 180;
        const double center = 0.5 * (port.startAngle + port.endAngle) + constants::pi / 2;
        const double width = std::max(port.endAngle - port.startAngle, MinDisplayWidth);
        generateHousingBand(center - width / 2, center + width / 2, wall, wall * 0.8, indices);
    };

    portBand(m_rotor->getIntakePort(), &m_intakePort);
    portBand(m_rotor->getExhaustPort(), &m_exhaustPort);

    {
        const double plugAngle = m_rotor->getSparkPlugAngle() + constants::pi / 2;
        const double plugWidth = 0.05;
        generateHousingBand(
            plugAngle - plugWidth, plugAngle + plugWidth, 0.0, wall * 1.8, &m_sparkPlug);
    }

    // Rotor
    {
        const int n = HousingSegments;
        std::vector<float> x(n), y(n);
        for (int i = 0; i < n; ++i) {
            const double localAngle = (2 * constants::pi * i) / n;
            double px, py;
            m_rotor->getRotorPoint(
                theta, localAngle, m_rotor->getFlankRadius(localAngle), &px, &py);
            x[i] = static_cast<float>(px);
            y[i] = static_cast<float>(py);
        }

        double cx, cy;
        m_rotor->getRotorCenter(theta, &cx, &cy);

        gen->startShape();
        gen->generateFilledPolygon2d(x.data(), y.data(), n, (float)cx, (float)cy);

        // Crown (internal) gear
        if (m_rotor->getCrownGearRadius() > 0) {
            GeometryGenerator::Ring2dParameters ring;
            ring.center_x = (float)cx;
            ring.center_y = (float)cy;
            ring.innerRadius = (float)m_rotor->getCrownGearRadius();
            ring.outerRadius = (float)(m_rotor->getCrownGearRadius() + R * 0.03);
            ring.maxEdgeLength = m_app->pixelsToUnits(5.0f);
            gen->generateRing2d(ring);
        }

        gen->endShape(&m_rotorBody);
    }

    // Burning chambers: band between the rotor flank and the housing
    const int firstChamber = m_rotor->getFirstChamber();
    for (int face = 0; face < WankelRotor::FaceCount; ++face) {
        m_lit[face] = m_engine->getChamber(firstChamber + face)->isLit();
        if (!m_lit[face]) continue;

        const int n = FlankSegments;
        std::vector<float> x0(n), y0(n), x1(n), y1(n);
        for (int i = 0; i < n; ++i) {
            const double localAngle = face * FaceAngle + FaceAngle * i / (n - 1.0);

            double px, py;
            m_rotor->getRotorPoint(
                theta, localAngle, m_rotor->getFlankRadius(localAngle), &px, &py);
            x0[i] = static_cast<float>(px);
            y0[i] = static_cast<float>(py);

            m_rotor->getHousingPoint(phase + localAngle, &px, &py);
            x1[i] = static_cast<float>(px);
            y1[i] = static_cast<float>(py);
        }

        gen->startShape();
        gen->generateStrip2d(x0.data(), y0.data(), x1.data(), y1.data(), n, false);
        gen->endShape(&m_flames[face]);
    }
}

void WankelRotorObject::render(const ViewParameters *view) {
    const int layer = m_rotor->getRodJournal();
    if (layer > view->Layer1 || layer < view->Layer0) return;

    const int depth = layer - view->Layer0;

    resetShader();

    if (view->Sublayer == 0) {
        m_app->getShaders()->SetBaseColor(tintByLayer(m_app->getPink(), depth));
        m_app->drawGenerated(m_housing, 0x0);

        m_app->getShaders()->SetBaseColor(tintByLayer(m_app->getBlue(), depth));
        m_app->drawGenerated(m_intakePort, 0x0);

        m_app->getShaders()->SetBaseColor(tintByLayer(m_app->getOrange(), depth));
        m_app->drawGenerated(m_exhaustPort, 0x0);

        m_app->getShaders()->SetBaseColor(tintByLayer(m_app->getYellow(), depth));
        m_app->drawGenerated(m_sparkPlug, 0x0);
    }
    else if (view->Sublayer == 1) {
        const ysVector rotorColor = tintByLayer(
            mix(m_app->getBackgroundColor(), m_app->getForegroundColor(), 0.4f),
            depth);
        m_app->getShaders()->SetBaseColor(rotorColor);
        m_app->drawGenerated(m_rotorBody, 0x10 + layer);

        // Only show the flame of the frontmost rotor
        if (depth == 0) {
            for (int face = 0; face < WankelRotor::FaceCount; ++face) {
                if (!m_lit[face]) continue;

                m_app->getShaders()->SetBaseColor(
                    ysMath::Mul(
                        m_app->getOrange(),
                        ysMath::LoadVector(1.0f, 1.0f, 1.0f, 0.6f)));
                m_app->drawGenerated(m_flames[face], 0x35);
            }
        }
    }
}

void WankelRotorObject::process(float dt) {
    /* void */
}

void WankelRotorObject::destroy() {
    /* void */
}
