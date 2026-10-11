#include <gtest/gtest.h>

#include "../include/wankel_rotor.h"
#include "../include/constants.h"
#include "../include/units.h"

#include <cmath>

namespace {

    // Geometry similar to a Mazda 13B: e = 15 mm, R = 105 mm, b = 80 mm
    class WankelRotorTest : public ::testing::Test {
    protected:
        void SetUp() override {
            Crankshaft::Parameters crankParams;
            crankParams.mass = 1.0;
            crankParams.flywheelMass = 1.0;
            crankParams.momentOfInertia = 1.0;
            crankParams.crankThrow = units::distance(15, units::mm);
            crankParams.rodJournals = 1;
            crankParams.tdc = constants::pi / 2;
            m_crankshaft.initialize(crankParams);
            m_crankshaft.setRodJournalAngle(0, 0.0);

            WankelRotor::Parameters params;
            params.crankshaft = &m_crankshaft;
            params.rodJournal = 0;
            params.generatingRadius = units::distance(105, units::mm);
            params.depth = units::distance(80, units::mm);
            params.depressionVolume = units::volume(50, units::cc);
            params.sparkPlugAngle = 0.0;
            params.intakePort.startAngle = units::angle(130, units::deg);
            params.intakePort.endAngle = units::angle(150, units::deg);
            params.exhaustPort.startAngle = units::angle(220, units::deg);
            params.exhaustPort.endAngle = units::angle(240, units::deg);
            m_rotor.initialize(params);
        }

        void TearDown() override {
            m_crankshaft.destroy();
        }

        // Crankshaft body angle for a given cycle angle (the engine turns
        // clockwise so the body angle decreases)
        double shaftAngle(double cycleAngle) const {
            return m_crankshaft.getTdc() - cycleAngle;
        }

        Crankshaft m_crankshaft;
        WankelRotor m_rotor;
    };

} /* namespace */

TEST_F(WankelRotorTest, DisplacementMatchesClosedForm) {
    const double e = units::distance(15, units::mm);
    const double R = units::distance(105, units::mm);
    const double b = units::distance(80, units::mm);
    const double expected = 3 * std::sqrt(3.0) * e * R * b;

    EXPECT_NEAR(m_rotor.getFaceDisplacement(), expected, expected * 1E-6);
    EXPECT_NEAR(units::convert(m_rotor.getFaceDisplacement(), units::cc), 654.6, 0.5);
}

TEST_F(WankelRotorTest, TotalVolumeIsConstant) {
    double reference = 0.0;
    for (int i = 0; i < 100; ++i) {
        const double theta = i * 0.137;
        double total = 0.0;
        for (int face = 0; face < 3; ++face) {
            total += m_rotor.getChamberVolume(face, theta);
        }

        if (i == 0) reference = total;
        EXPECT_NEAR(total, reference, reference * 1E-9);
    }
}

TEST_F(WankelRotorTest, DerivativeMatchesFiniteDifference) {
    const double h = 1E-6;
    for (int i = 0; i < 50; ++i) {
        const double theta = i * 0.377;
        for (int face = 0; face < 3; ++face) {
            const double numeric =
                (m_rotor.getChamberVolume(face, theta + h)
                    - m_rotor.getChamberVolume(face, theta - h)) / (2 * h);
            EXPECT_NEAR(
                m_rotor.getChamberVolumeDerivative(face, theta),
                numeric,
                1E-9);
        }
    }
}

TEST_F(WankelRotorTest, CompressionTdcAtCycleStartUnderSparkPlug) {
    const double theta = shaftAngle(0.0);
    const int face = m_rotor.getFaceAtAngle(m_rotor.getSparkPlugAngle(), theta);

    // The face under the spark plug is at its minimum volume
    EXPECT_NEAR(
        m_rotor.getChamberVolume(face, theta),
        m_rotor.getMinimumVolume(),
        m_rotor.getFaceDisplacement() * 1E-6);
    EXPECT_NEAR(m_rotor.getChamberVolumeDerivative(face, theta), 0.0, 1E-9);

    // Realistic compression ratio for this geometry
    const double cr = m_rotor.getMaximumVolume() / m_rotor.getMinimumVolume();
    EXPECT_GT(cr, 6.0);
    EXPECT_LT(cr, 14.0);

    // A face reaches the spark plug at TDC every shaft revolution
    for (int rev = 1; rev < 3; ++rev) {
        const double t = shaftAngle(rev * 2 * constants::pi);
        const int f = m_rotor.getFaceAtAngle(m_rotor.getSparkPlugAngle(), t);
        EXPECT_NE(f, face);
        EXPECT_NEAR(m_rotor.getChamberVolume(f, t), m_rotor.getMinimumVolume(), 1E-9);
    }
}

TEST_F(WankelRotorTest, FourStrokeSequence) {
    const double theta0 = shaftAngle(0.0);
    const int face = m_rotor.getFaceAtAngle(m_rotor.getSparkPlugAngle(), theta0);

    // Follow one face through the cycle: expansion, exhaust, intake,
    // compression
    bool exhaustOpened = false, intakeOpened = false;
    double exhaustOpenAt = -1, intakeOpenAt = -1;
    for (int i = 0; i <= 1080; ++i) {
        const double cycleAngle = units::angle(i, units::deg);
        const double theta = shaftAngle(cycleAngle);

        const double exhaust =
            m_rotor.getPortOpening(face, m_rotor.getExhaustPort(), theta);
        const double intake =
            m_rotor.getPortOpening(face, m_rotor.getIntakePort(), theta);

        EXPECT_GE(exhaust, 0.0);
        EXPECT_LE(exhaust, 1.0);

        if (exhaust > 0 && !exhaustOpened) {
            exhaustOpened = true;
            exhaustOpenAt = i;
        }

        if (intake > 0 && !intakeOpened) {
            intakeOpened = true;
            intakeOpenAt = i;
        }
    }

    ASSERT_TRUE(exhaustOpened);
    ASSERT_TRUE(intakeOpened);

    // The face reaches BDC of its expansion stroke at 270 shaft degrees; the
    // exhaust port opens towards the end of the expansion stroke and the
    // intake port opens around the overlap TDC (540 degrees)
    EXPECT_GT(exhaustOpenAt, 90);
    EXPECT_LT(exhaustOpenAt, 270);
    EXPECT_GT(intakeOpenAt, 360);
    EXPECT_LT(intakeOpenAt, 540);
}

TEST_F(WankelRotorTest, FlankStaysInsideHousing) {
    // Every point of the rotor flank must stay inside the housing
    for (int i = 0; i < 64; ++i) {
        const double theta = i * 0.0981;
        for (int j = 0; j < 90; ++j) {
            const double localAngle = 2 * constants::pi * j / 90.0;
            double x, y;
            m_rotor.getRotorPoint(
                theta, localAngle, m_rotor.getFlankRadius(localAngle), &x, &y);

            // Distance from the shaft center compared with the housing
            // radius in the same direction (the housing is star-shaped)
            const double angle = std::atan2(y, x);
            double best = 1E9, hx = 0, hy = 0;
            for (int k = 0; k < 2048; ++k) {
                const double alpha = 2 * constants::pi * k / 2048.0;
                double px, py;
                m_rotor.getHousingPoint(alpha, &px, &py);
                const double d = std::abs(std::remainder(std::atan2(py, px) - angle, 2 * constants::pi));
                if (d < best) {
                    best = d;
                    hx = px;
                    hy = py;
                }
            }

            EXPECT_LE(
                std::sqrt(x * x + y * y),
                std::sqrt(hx * hx + hy * hy) + units::distance(0.5, units::mm));
        }
    }
}
