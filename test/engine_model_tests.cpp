#include <gtest/gtest.h>

#include "../include/engine_friction.h"
#include "../include/engine_metrics.h"
#include "../include/units.h"

#include <cmath>

namespace {

    // 2.0 L inline four, 86 x 86 mm, DOHC with bucket tappets
    EngineFriction makeFourCylinder(EngineFriction::Model model) {
        EngineFriction::Parameters params;
        params.model = model;
        params.valvetrain = EngineFriction::Valvetrain::DohcBucket;
        params.valvesPerCylinder = 4;
        params.maxValveLift = units::distance(9.0, units::mm);

        EngineFriction::Geometry geometry;
        geometry.bore = units::distance(86.0, units::mm);
        geometry.stroke = units::distance(86.0, units::mm);
        geometry.cylinders = 4;
        geometry.banks = 1;
        geometry.crankJournals = 4;
        geometry.compressionRatio = 10.0;
        geometry.displacement =
            4 * constants::pi * geometry.bore * geometry.bore / 4 * geometry.stroke;

        EngineFriction friction;
        friction.initialize(params, geometry);
        return friction;
    }

    // Motored friction of 0.85-2.0 L four-cylinder SI engines at WOT
    // (Barnes-Moss/Bishop correlation, includes pumping) [bar]
    double bishop(double rpm) {
        const double n = rpm / 1000.0;
        return 0.97 + 0.15 * n + 0.05 * n * n;
    }

} /* namespace */

TEST(EngineModelTests, PattonNitschkeHeywoodMagnitude) {
    const EngineFriction friction =
        makeFourCylinder(EngineFriction::Model::PattonNitschkeHeywood);

    // Derived bearing dimensions
    EXPECT_EQ(friction.getMainBearingCount(), 5);
    EXPECT_NEAR(friction.getMainBearingDiameter(), 0.65 * 0.086, 1E-9);

    const double bar = units::pressure(1.0, units::bar);
    const double fmep2000 = friction.fmep(units::rpm(2000), 1.0, 0.0) / bar;
    const double fmep6000 = friction.fmep(units::rpm(6000), 1.0, 0.0) / bar;

    // About 1 bar at 2000 rpm and 2-2.5 bar at 6000 rpm for this engine
    EXPECT_NEAR(fmep2000, 1.0, 0.15);
    EXPECT_GT(fmep6000, 1.8);
    EXPECT_LT(fmep6000, 2.8);

    // Mechanical friction is below the Bishop total, which also includes
    // motored pumping losses (growing with speed) and comes from older
    // engines, but of the same magnitude
    for (double rpm = 1000; rpm <= 6000; rpm += 1000) {
        const double fmep = friction.fmep(units::rpm(rpm), 1.0, 0.0) / bar;
        EXPECT_LT(fmep, 1.05 * bishop(rpm)) << rpm;
        EXPECT_GT(fmep, 0.5 * bishop(rpm)) << rpm;
    }

    // Friction rises with speed and slightly with load
    EXPECT_GT(fmep6000, fmep2000);
    EXPECT_GT(
        friction.fmep(units::rpm(3000), 1.0, 0.0),
        friction.fmep(units::rpm(3000), 0.3, 0.0));

    // Torque from FMEP: T = FMEP V_d / (4 pi)
    EXPECT_NEAR(
        friction.torque(1E5),
        1E5 * friction.getGeometry().displacement / (4 * constants::pi),
        1E-9);
}

TEST(EngineModelTests, ChenFlynn) {
    const EngineFriction friction = makeFourCylinder(EngineFriction::Model::ChenFlynn);
    const double rpm = 3000;
    const double Sp = 2 * 0.086 * rpm / 60;
    const double p_max = units::pressure(60.0, units::bar);
    const EngineFriction::Parameters &p = friction.getParameters();

    EXPECT_NEAR(
        friction.fmep(units::rpm(rpm), 1.0, p_max),
        p.chenFlynnConstant + p.chenFlynnPeakPressure * p_max
            + p.chenFlynnPistonSpeed * Sp + p.chenFlynnPistonSpeedSquared * Sp * Sp,
        1E-6);
}

TEST(EngineModelTests, ParseNames) {
    EngineFriction::Model model;
    EXPECT_TRUE(EngineFriction::parseModel("auto", true, &model));
    EXPECT_EQ(model, EngineFriction::Model::PattonNitschkeHeywood);
    EXPECT_TRUE(EngineFriction::parseModel("auto", false, &model));
    EXPECT_EQ(model, EngineFriction::Model::Legacy);
    EXPECT_TRUE(EngineFriction::parseModel("chen_flynn", true, &model));
    EXPECT_EQ(model, EngineFriction::Model::ChenFlynn);
    EXPECT_FALSE(EngineFriction::parseModel("magic", true, &model));

    EngineFriction::Valvetrain valvetrain;
    bool roller;
    EXPECT_TRUE(EngineFriction::parseValvetrain("pushrod_roller", &valvetrain, &roller));
    EXPECT_EQ(valvetrain, EngineFriction::Valvetrain::Pushrod);
    EXPECT_TRUE(roller);
    EXPECT_TRUE(EngineFriction::parseValvetrain("sohc_finger", &valvetrain, &roller));
    EXPECT_EQ(valvetrain, EngineFriction::Valvetrain::SohcFinger);
    EXPECT_FALSE(roller);
}

TEST(EngineModelTests, SaeJ1349) {
    // 1.0 at the standard conditions (99 kPa dry air, 25 C)
    EXPECT_NEAR(saeJ1349CorrectionFactor(99000.0, 298.0), 1.0, 1E-12);

    // Lower pressure and hotter air correct power upwards
    EXPECT_GT(saeJ1349CorrectionFactor(90000.0, 308.0), 1.0);
    EXPECT_LT(
        saeJ1349CorrectionFactor(SimulationAmbientPressure, SimulationAmbientTemperature),
        1.0);
}
