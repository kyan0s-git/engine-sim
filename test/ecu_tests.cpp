#include <gtest/gtest.h>

#include "../include/ecu.h"

TEST(EcuTableTests, BilinearSamplingAndClamping) {
    EcuTable t;
    t.resize({ 0, 1000, 2000 }, { 50, 100 }, 0.0);
    t.at(0, 0) = 10; t.at(0, 1) = 20; t.at(0, 2) = 30;
    t.at(1, 0) = 0;  t.at(1, 1) = 10; t.at(1, 2) = 20;

    EXPECT_DOUBLE_EQ(t.sample(500, 50), 15.0);
    EXPECT_DOUBLE_EQ(t.sample(1000, 75), 15.0);
    EXPECT_DOUBLE_EQ(t.sample(-100, 50), 10.0);       // clamped
    EXPECT_DOUBLE_EQ(t.sample(5000, 200), 20.0);
    EXPECT_DOUBLE_EQ(EcuTable::position(t.x, 1500), 1.5);
}

TEST(EcuTableTests, OneDimensional) {
    EcuTable t;
    t.resize({ 0, 50, 100 }, {}, 0.0);
    t.at(0, 0) = 2000; t.at(0, 1) = 4000; t.at(0, 2) = 6000;
    EXPECT_EQ(t.rows(), 1);
    EXPECT_DOUBLE_EQ(t.sample(25), 3000.0);
}

TEST(EcuTests, TuneRoundTrip) {
    Ecu ecu;
    Ecu::Settings &s = ecu.settings();
    s.ignition.resize({ 0, 3000, 6000 }, { 30, 100 }, 20.0);
    s.ignition.at(1, 2) = 33.5;
    s.lambda.resize({ 0, 6000 }, { 30, 100 }, 1.0);
    s.boost.resize({ 0, 6000 }, {}, 80.0);
    s.upshift.resize({ 0, 100 }, {}, 5000.0);
    s.downshift.resize({ 0, 100 }, {}, 2000.0);
    s.revLimit = 7123;
    s.automatic = true;
    s.launchControl = true;
    s.launchRpm = 4200;
    s.limiter = Ecu::Limiter::Soft;

    const std::string text = ecu.serialize();

    Ecu other;
    other.settings() = Ecu::Settings();
    std::string error;
    ASSERT_TRUE(other.deserialize(text, &error)) << error;
    EXPECT_DOUBLE_EQ(other.settings().ignition.at(1, 2), 33.5);
    EXPECT_EQ(other.settings().ignition.rows(), 2);
    EXPECT_DOUBLE_EQ(other.settings().revLimit, 7123);
    EXPECT_TRUE(other.settings().automatic);
    EXPECT_TRUE(other.settings().launchControl);
    EXPECT_DOUBLE_EQ(other.settings().launchRpm, 4200);
    EXPECT_EQ(other.settings().limiter, Ecu::Limiter::Soft);
    EXPECT_DOUBLE_EQ(other.settings().boost.sample(3000), 80.0);

    // A malformed row is rejected without changing the tune
    std::string broken = text;
    broken.replace(broken.find("ignition.row1="), 14, "ignition.row1=1,");
    EXPECT_FALSE(other.deserialize(broken + "\nignition.row1=1,2\n", &error));
    EXPECT_DOUBLE_EQ(other.settings().ignition.at(1, 2), 33.5);
}
