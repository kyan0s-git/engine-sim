#include "../include/wankel_rotor.h"

#include "../include/constants.h"
#include "../include/utilities.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
    constexpr double FaceAngle = 2 * constants::pi / 3;

    // Rotation from the housing frame (in which the spark plug side minor
    // axis lies along +y) to the world frame
    double housingToWorld(const Crankshaft *crankshaft) {
        return crankshaft->getTdc() - constants::pi / 2;
    }
} /* namespace */

WankelRotor::WankelRotor() {
    m_crankshaft = nullptr;
    m_rodJournal = 0;
    m_index = 0;
    m_firstChamber = 0;

    m_e = 0.0;
    m_R = 0.0;
    m_depth = 0.0;
    m_depressionVolume = 0.0;
    m_mass = 0.0;
    m_momentOfInertia = 0.0;
    m_crownGearRadius = 0.0;
    m_sparkPlugAngle = 0.0;

    m_flankArea = 0.0;
    m_faceDisplacement = 0.0;
    m_minimumVolume = 0.0;
    m_maximumVolume = 0.0;
}

WankelRotor::~WankelRotor() {
    /* void */
}

void WankelRotor::initialize(const Parameters &params) {
    m_crankshaft = params.crankshaft;
    m_rodJournal = params.rodJournal;
    m_index = params.index;
    m_firstChamber = params.firstChamber;

    m_e = m_crankshaft->getThrow();
    m_R = params.generatingRadius;
    m_depth = params.depth;
    m_depressionVolume = params.depressionVolume;
    m_mass = params.mass;
    m_momentOfInertia = params.momentOfInertia;
    m_crownGearRadius = params.crownGearRadius;
    m_sparkPlugAngle = params.sparkPlugAngle;
    m_intakePort = params.intakePort;
    m_exhaustPort = params.exhaustPort;

    calculateFlankShape(
        std::max(params.calculationQuality, 16),
        std::max(params.housingResolution, 16));

    // Each face's chord area varies sinusoidally; sample one rotor
    // revolution to find the extremes
    constexpr int Samples = 720;
    double minArea = DBL_MAX, maxArea = -DBL_MAX;
    for (int i = 0; i < Samples; ++i) {
        const double phase = (2 * constants::pi * i) / Samples;
        const double area = faceChordArea(0, phase);
        minArea = std::min(minArea, area);
        maxArea = std::max(maxArea, area);
    }

    m_faceDisplacement = (maxArea - minArea) * m_depth;
    m_minimumVolume = std::max(
        (minArea - m_flankArea) * m_depth + m_depressionVolume,
        0.0);
    m_maximumVolume = (maxArea - m_flankArea) * m_depth + m_depressionVolume;
}

double WankelRotor::getPhase(double shaftAngle) const {
    const double lobeAngle =
        shaftAngle + m_crankshaft->getRodJournalAngle(m_rodJournal);
    return (lobeAngle - housingToWorld(m_crankshaft)) / 3.0;
}

double WankelRotor::faceChordArea(int face, double phase) const {
    const double a0 = phase + face * FaceAngle;
    const double a1 = a0 + FaceAngle;

    // Green's theorem along the housing between the two apexes...
    const double arc = 0.5 * (
        (3 * m_e * m_e + m_R * m_R) * FaceAngle
        + 2 * m_e * m_R * (std::sin(2 * a1) - std::sin(2 * a0)));

    // ...and back along the chord
    const double chord = 0.5 * (
        m_e * m_R * std::sin(a0 - 3 * phase)
        + m_e * m_R * std::sin(3 * phase - a1)
        - m_R * m_R * std::sin(FaceAngle));

    return arc + chord;
}

double WankelRotor::faceChordAreaDerivative(int face, double phase) const {
    const double a0 = phase + face * FaceAngle;
    const double a1 = a0 + FaceAngle;

    return
        2 * m_e * m_R * (std::cos(2 * a1) - std::cos(2 * a0))
        + m_e * m_R * (std::cos(3 * phase - a1) - std::cos(a0 - 3 * phase));
}

double WankelRotor::getChamberVolume(int face, double shaftAngle) const {
    const double phase = getPhase(shaftAngle);
    const double volume =
        (faceChordArea(face, phase) - m_flankArea) * m_depth + m_depressionVolume;

    // Guard against a degenerate geometry producing an empty chamber
    return std::max(volume, 0.01 * m_faceDisplacement);
}

double WankelRotor::getChamberVolumeDerivative(int face, double shaftAngle) const {
    // d(phase)/d(shaft angle) = 1/3
    return faceChordAreaDerivative(face, getPhase(shaftAngle)) * m_depth / 3.0;
}

double WankelRotor::getPortOpening(int face, const Port &port, double shaftAngle) const {
    const double width = port.endAngle - port.startAngle;
    if (width <= 0) return 0.0;

    // Face spans [0, FaceAngle] relative to its leading apex
    const double faceStart = getPhase(shaftAngle) + face * FaceAngle;
    const double portStart =
        positiveMod((port.startAngle + constants::pi / 2) - faceStart, 2 * constants::pi);
    const double portEnd = portStart + width;

    double open = 0.0;
    open += std::max(0.0, std::min(portEnd, FaceAngle) - std::max(portStart, 0.0));
    open += std::max(
        0.0,
        std::min(portEnd, 2 * constants::pi + FaceAngle) - std::max(portStart, 2 * constants::pi));

    return std::min(open / width, 1.0);
}

int WankelRotor::getFaceAtAngle(double housingAngle, double shaftAngle) const {
    const double d = positiveMod(
        (housingAngle + constants::pi / 2) - getPhase(shaftAngle),
        2 * constants::pi);
    return std::min(static_cast<int>(d / FaceAngle), FaceCount - 1);
}

double WankelRotor::getEffectiveInertia() const {
    // The rotor's center orbits at the eccentricity while the rotor spins at
    // a third of the shaft speed
    return m_mass * m_e * m_e + m_momentOfInertia / 9.0;
}

double WankelRotor::getChamberCrossSection() const {
    return m_depth * m_R * std::sqrt(3.0);
}

void WankelRotor::getHousingPoint(double alpha, double *x, double *y) const {
    const double hx = m_e * std::cos(3 * alpha) + m_R * std::cos(alpha);
    const double hy = m_e * std::sin(3 * alpha) + m_R * std::sin(alpha);

    const double rot = housingToWorld(m_crankshaft);
    const double c = std::cos(rot), s = std::sin(rot);
    *x = m_crankshaft->getPosX() + c * hx - s * hy;
    *y = m_crankshaft->getPosY() + s * hx + c * hy;
}

void WankelRotor::getRotorCenter(double shaftAngle, double *x, double *y) const {
    const double lobeAngle =
        shaftAngle + m_crankshaft->getRodJournalAngle(m_rodJournal);
    *x = m_crankshaft->getPosX() + m_e * std::cos(lobeAngle);
    *y = m_crankshaft->getPosY() + m_e * std::sin(lobeAngle);
}

void WankelRotor::getRotorPoint(
    double shaftAngle,
    double localAngle,
    double radius,
    double *x,
    double *y) const
{
    double cx, cy;
    getRotorCenter(shaftAngle, &cx, &cy);

    const double angle =
        getPhase(shaftAngle) + localAngle + housingToWorld(m_crankshaft);
    *x = cx + radius * std::cos(angle);
    *y = cy + radius * std::sin(angle);
}

double WankelRotor::getHousingAngleToWorld() const {
    return housingToWorld(m_crankshaft);
}

double WankelRotor::getFlankRadius(double localAngle) const {
    if (m_flank.empty()) return m_R;

    const int n = static_cast<int>(m_flank.size());
    const double s = positiveMod(localAngle, 2 * constants::pi) / (2 * constants::pi) * n;
    const int i0 = static_cast<int>(s) % n;
    const int i1 = (i0 + 1) % n;
    const double f = s - std::floor(s);

    return m_flank[i0] * (1 - f) + m_flank[i1] * f;
}

void WankelRotor::calculateFlankShape(int calculationQuality, int housingResolution) {
    // The largest rotor that fits the housing in every position is the inner
    // envelope of the housing as seen from the rotor. Sweep the rotor through
    // a full revolution and record, for every direction from the rotor
    // center, the closest point of the housing.
    const int bins = 3 * ((housingResolution + 2) / 3);
    const int housingSamples = 4 * bins;

    m_flank.assign(bins, DBL_MAX);

    for (int j = 0; j < calculationQuality; ++j) {
        const double phase = (2 * constants::pi * j) / calculationQuality;
        const double cx = m_e * std::cos(3 * phase);
        const double cy = m_e * std::sin(3 * phase);
        const double c = std::cos(-phase), s = std::sin(-phase);

        for (int i = 0; i < housingSamples; ++i) {
            const double alpha = (2 * constants::pi * i) / housingSamples;
            const double hx = m_e * std::cos(3 * alpha) + m_R * std::cos(alpha) - cx;
            const double hy = m_e * std::sin(3 * alpha) + m_R * std::sin(alpha) - cy;

            const double lx = c * hx - s * hy;
            const double ly = s * hx + c * hy;

            const double angle = positiveMod(std::atan2(ly, lx), 2 * constants::pi);
            const int bin = std::min(
                static_cast<int>(angle / (2 * constants::pi) * bins),
                bins - 1);

            m_flank[bin] = std::min(m_flank[bin], std::sqrt(lx * lx + ly * ly));
        }
    }

    // Fill any bins that weren't hit by interpolating their neighbors
    for (int i = 0; i < bins; ++i) {
        if (m_flank[i] != DBL_MAX) continue;

        int prev = i, next = i;
        while (m_flank[prev] == DBL_MAX) prev = (prev - 1 + bins) % bins;
        while (m_flank[next] == DBL_MAX) next = (next + 1) % bins;
        m_flank[i] = 0.5 * (m_flank[prev] + m_flank[next]);
    }

    // The apexes sit exactly on the housing
    for (int k = 0; k < FaceCount; ++k) {
        m_flank[k * bins / FaceCount] = m_R;
    }

    // Area between the flank and the chord, averaged over the three faces
    const double dAngle = 2 * constants::pi / bins;
    double sectorArea = 0.0;
    for (int i = 0; i < bins; ++i) {
        sectorArea += 0.5 * m_flank[i] * m_flank[i] * dAngle;
    }

    const double triangleArea = 0.5 * m_R * m_R * std::sin(FaceAngle);
    m_flankArea = std::max(sectorArea / FaceCount - triangleArea, 0.0);
}
