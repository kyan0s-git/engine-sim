#ifndef ATG_ENGINE_SIM_WANKEL_ROTOR_H
#define ATG_ENGINE_SIM_WANKEL_ROTOR_H

#include "crankshaft.h"
#include "function.h"

#include <vector>

// Geometry and kinematics of one rotor of a Wankel (rotary) engine.
//
// The rotor housing is the epitrochoid traced by the rotor's apexes:
//
//     H(a) = e * exp(3ia) + R * exp(ia)
//
// where e is the eccentricity (the throw of the eccentric shaft's journal)
// and R is the generating radius (rotor center to apex). The rotor turns at a
// third of the eccentric shaft's speed so every face completes a full
// four-stroke cycle over three shaft revolutions.
//
// Housing angles (ports, spark plug) are measured counter-clockwise from the
// housing's minor axis on the spark plug side, which points towards the
// crankshaft's TDC direction. A face is at top dead center of its
// compression stroke when the crankshaft's cycle angle is 0.
class WankelRotor {
    public:
        static constexpr int FaceCount = 3;

        struct Port {
            double startAngle = 0.0;
            double endAngle = 0.0;

            // Flow coefficient as a function of the fraction of the port
            // that is open to the chamber [0, 1]
            Function *flowRate = nullptr;
        };

        struct Parameters {
            Crankshaft *crankshaft = nullptr;
            int rodJournal = 0;

            double generatingRadius = 0.0;
            double depth = 0.0;
            double depressionVolume = 0.0;
            double mass = 0.0;
            double momentOfInertia = 0.0;
            double crownGearRadius = 0.0;

            double sparkPlugAngle = 0.0;
            Port intakePort;
            Port exhaustPort;

            // Resolution of the numerically computed rotor flank shape
            int calculationQuality = 1024;
            int housingResolution = 256;

            int index = 0;
            int firstChamber = 0;
        };

    public:
        WankelRotor();
        ~WankelRotor();

        void initialize(const Parameters &params);

        // Rotor phase (angle of apex 0 in the housing frame) for the given
        // crankshaft body angle
        double getPhase(double shaftAngle) const;

        double getChamberVolume(int face, double shaftAngle) const;

        // Derivative of the chamber volume with respect to the crankshaft
        // body angle [m^3/rad]
        double getChamberVolumeDerivative(int face, double shaftAngle) const;

        // Fraction [0, 1] of the port that is uncovered by the given face
        double getPortOpening(int face, const Port &port, double shaftAngle) const;

        // Face that covers the given housing angle
        int getFaceAtAngle(double housingAngle, double shaftAngle) const;

        // Moment of inertia of the rotor as seen by the eccentric shaft
        double getEffectiveInertia() const;

        // Swept volume of a single face (the conventional rotary engine
        // displacement is this value times the number of rotors)
        double getFaceDisplacement() const { return m_faceDisplacement; }
        double getMinimumVolume() const { return m_minimumVolume; }
        double getMaximumVolume() const { return m_maximumVolume; }

        // Cross section of a chamber (housing depth * chord length) used by
        // the gas and flame models
        double getChamberCrossSection() const;

        // Display helpers (world coordinates)
        void getHousingPoint(double alpha, double *x, double *y) const;
        void getRotorCenter(double shaftAngle, double *x, double *y) const;
        void getRotorPoint(double shaftAngle, double localAngle, double radius, double *x, double *y) const;
        double getHousingAngleToWorld() const;
        double getFlankRadius(double localAngle) const;

        Crankshaft *getCrankshaft() const { return m_crankshaft; }
        int getRodJournal() const { return m_rodJournal; }
        int getIndex() const { return m_index; }
        int getFirstChamber() const { return m_firstChamber; }

        double getEccentricity() const { return m_e; }
        double getGeneratingRadius() const { return m_R; }
        double getDepth() const { return m_depth; }
        double getCrownGearRadius() const { return m_crownGearRadius; }
        double getSparkPlugAngle() const { return m_sparkPlugAngle; }

        const Port &getIntakePort() const { return m_intakePort; }
        const Port &getExhaustPort() const { return m_exhaustPort; }

        // Area between the housing and the chord connecting the face's apexes
        double faceChordArea(int face, double phase) const;
        double faceChordAreaDerivative(int face, double phase) const;

    protected:
        void calculateFlankShape(int calculationQuality, int housingResolution);

        Crankshaft *m_crankshaft;
        int m_rodJournal;
        int m_index;
        int m_firstChamber;

        double m_e;
        double m_R;
        double m_depth;
        double m_depressionVolume;
        double m_mass;
        double m_momentOfInertia;
        double m_crownGearRadius;
        double m_sparkPlugAngle;

        Port m_intakePort;
        Port m_exhaustPort;

        // Rotor flank shape in polar coordinates around the rotor center
        // (inner envelope of the housing)
        std::vector<double> m_flank;

        // Area between the chord connecting two apexes and the flank
        double m_flankArea;

        double m_faceDisplacement;
        double m_minimumVolume;
        double m_maximumVolume;
};

#endif /* ATG_ENGINE_SIM_WANKEL_ROTOR_H */
