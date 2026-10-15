#include <gtest/gtest.h>

#include "../include/forced_induction.h"
#include "../include/crankshaft.h"
#include "../include/intake.h"
#include "../include/units.h"

#include <algorithm>
#include <cmath>

namespace {

    constexpr double Ambient = units::pressure(1.0, units::atm);

    // An intake whose plenum is drawn down by a stand-in for the engine
    class ForcedInductionTest : public ::testing::Test {
    protected:
        void SetUp() override {
            Crankshaft::Parameters crankParams;
            crankParams.mass = 10.0;
            crankParams.flywheelMass = 5.0;
            crankParams.momentOfInertia = 0.2;
            crankParams.crankThrow = 0.04;
            crankParams.rodJournals = 1;
            m_crankshaft.initialize(crankParams);
            m_crankshaft.m_body.v_theta = units::rpm(5000);

            Intake::Parameters intakeParams;
            intakeParams.volume = units::volume(2.0, units::L);
            intakeParams.CrossSectionArea = units::area(40.0, units::cm2);
            intakeParams.InputFlowK = GasSystem::k_carb(800.0);
            intakeParams.IdleFlowK = 0.0;
            intakeParams.RunnerFlowRate = GasSystem::k_carb(200.0);
            intakeParams.IdleThrottlePlatePosition = 1.0;
            m_intake.initialize(intakeParams);
            m_intake.m_throttle = 0.0;      // wide open
        }

        void TearDown() override {
            m_crankshaft.destroy();
        }

        void connect(ForcedInduction *fi) {
            fi->setCrankshaft(&m_crankshaft);
            fi->setExhaustSystemCount(1);
            fi->addIntake(&m_intake);
            m_intake.setForcedInduction(fi);
        }

        // One step: a 2.0 L four-stroke engine at 5000 rpm draws its
        // displacement from the plenum; the compressor and the throttle
        // refill it
        void step(ForcedInduction *fi, double dt) {
            GasSystem &plenum = m_intake.m_system;
            const double engineFlow = units::volume(2.0, units::L) * 5000.0 / 120.0;    // [m^3/s]
            const double drawn = plenum.n() * std::min(engineFlow * dt / plenum.volume(), 0.5);
            plenum.loseN(drawn, plenum.kineticEnergyPerMol());

            fi->process(dt);
            m_intake.process(dt);
        }

        Crankshaft m_crankshaft;
        Intake m_intake;
    };

} /* namespace */

TEST_F(ForcedInductionTest, RootsBlowerBoostsAndLoadsTheCrank) {
    ForcedInduction::Parameters params;
    params.type = ForcedInduction::Type::Roots;
    params.displacement = units::volume(1.2, units::L);
    params.driveRatio = 2.0;
    params.intercoolerEffectiveness = 0.0;

    ForcedInduction fi;
    fi.initialize(params);
    connect(&fi);

    const double dt = 1.0 / 20000;
    for (int i = 0; i < 20000; ++i) step(&fi, dt);

    EXPECT_GT(fi.getBoost(), units::pressure(0.2, units::bar));
    EXPECT_GT(m_intake.m_system.pressure(), Ambient);
    EXPECT_GT(fi.getCompressorOutletTemperature(), units::celcius(40.0));
    EXPECT_GT(fi.getDrivePower(), 1000.0);
    EXPECT_NEAR(fi.getShaftSpeed(), units::rpm(10000), units::rpm(1));
}

TEST_F(ForcedInductionTest, TurbochargerNeedsExhaustEnergy) {
    ForcedInduction::Parameters params;
    params.type = ForcedInduction::Type::Turbocharger;
    params.compressorFlow = 0.25;
    params.compressorDiameter = 0.055;
    params.boost = units::pressure(0.8, units::bar);

    ForcedInduction fi;
    fi.initialize(params);
    connect(&fi);

    const double dt = 1.0 / 20000;

    // Without exhaust flow the turbo doesn't spool and the compressor is a
    // restriction
    for (int i = 0; i < 4000; ++i) step(&fi, dt);
    EXPECT_LT(fi.getShaftSpeed(), units::rpm(1000));
    EXPECT_LT(fi.getBoost(), units::pressure(0.05, units::bar));

    // Hot exhaust at 2.5 atm spools it up; the wastegate holds the boost
    // near the set point
    const double exhaustFlow = 0.2 / units::AirMolecularMass;   // [mol/s]
    for (int i = 0; i < 60000; ++i) {
        fi.addExhaustFlow(exhaustFlow * dt, 1100.0, 2.5 * Ambient, 1.33, GasSystem::k_carb(2000.0));
        step(&fi, dt);
    }

    EXPECT_GT(fi.getShaftSpeed(), units::rpm(50000));
    EXPECT_NEAR(fi.getBoost(), params.boost, units::pressure(0.25, units::bar));
    EXPECT_GT(fi.getWastegateOpening(), 0.0);
    EXPECT_LE(fi.getShaftSpeed(), fi.getMaxSpeed() + 1.0);
}

TEST_F(ForcedInductionTest, BlowoffValveVentsWhenTheThrottleCloses) {
    ForcedInduction::Parameters params;
    params.type = ForcedInduction::Type::Centrifugal;
    params.driveRatio = 14.0;
    params.compressorDiameter = 0.08;
    params.compressorFlow = 0.4;

    ForcedInduction fi;
    fi.initialize(params);
    connect(&fi);

    const double dt = 1.0 / 20000;
    for (int i = 0; i < 20000; ++i) step(&fi, dt);
    EXPECT_GT(fi.getBoost(), units::pressure(0.1, units::bar));

    m_intake.m_throttle = 1.0;          // closed
    for (int i = 0; i < 2000; ++i) step(&fi, dt);
    EXPECT_GT(fi.getBlowoffOpening(), 0.5);
    EXPECT_GT(fi.getBlowoffLevel(), 0.0);
}
