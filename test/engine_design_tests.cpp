#include <gtest/gtest.h>

#include "../include/engine_design.h"
#include "../include/constants.h"

#include <algorithm>
#include <cmath>

TEST(EngineDesignTests, Displacement) {
    EngineDesign design;
    design.cylinders = 4;
    design.bore = 0.086;
    design.stroke = 0.086;

    const double expected = 4 * constants::pi * 0.086 * 0.086 / 4 * 0.086;
    EXPECT_NEAR(design.displacement(), expected, 1E-9);
    EXPECT_NEAR(design.displacement(), 0.001998, 1E-5);
}

TEST(EngineDesignTests, ValidateClampsValues) {
    EngineDesign design;
    EXPECT_TRUE(design.validate());

    design.layout = EngineDesign::Layout::V;
    design.cylinders = 7;
    design.compressionRatio = 40;
    design.rodLength = 0.01;
    EXPECT_FALSE(design.validate());

    EXPECT_EQ(design.cylinders, 8);
    EXPECT_DOUBLE_EQ(design.compressionRatio, 18.0);
    EXPECT_GE(design.rodLength, 1.2 * design.stroke);
}

TEST(EngineDesignTests, FiringOrderCoversAllCylinders) {
    const EngineDesign::Layout layouts[] = {
        EngineDesign::Layout::Inline, EngineDesign::Layout::V, EngineDesign::Layout::Flat };

    for (EngineDesign::Layout layout : layouts) {
        for (int n = 1; n <= 12; ++n) {
            EngineDesign design;
            design.layout = layout;
            design.cylinders = n;
            design.validate();

            std::vector<int> order = design.firingOrder();
            ASSERT_EQ(static_cast<int>(order.size()), design.cylinders);

            std::sort(order.begin(), order.end());
            for (int i = 0; i < design.cylinders; ++i) {
                EXPECT_EQ(order[i], i + 1);
            }
        }
    }
}

TEST(EngineDesignTests, ScriptRoundTrip) {
    EngineDesign design;
    design.name = "Test V8";
    design.layout = EngineDesign::Layout::V;
    design.cylinders = 8;
    design.bore = 0.1016;
    design.stroke = 0.0889;
    design.compressionRatio = 9.5;
    design.redline = 6200;
    design.fuel = "e85";
    design.lambda = 0.9;
    design.engineering = false;

    const std::string script = generateEngineScript(design);
    EXPECT_NE(script.find("main()"), std::string::npos);

    EngineDesign read;
    ASSERT_TRUE(readEngineDesign(script, &read));
    EXPECT_EQ(read.name, design.name);
    EXPECT_EQ(read.layout, design.layout);
    EXPECT_EQ(read.cylinders, design.cylinders);
    EXPECT_NEAR(read.bore, design.bore, 1E-6);
    EXPECT_NEAR(read.stroke, design.stroke, 1E-6);
    EXPECT_NEAR(read.compressionRatio, design.compressionRatio, 1E-6);
    EXPECT_NEAR(read.redline, design.redline, 1E-3);
    EXPECT_EQ(read.fuel, design.fuel);
    EXPECT_NEAR(read.lambda, design.lambda, 1E-6);
    EXPECT_EQ(read.engineering, design.engineering);

    EngineDesign none;
    EXPECT_FALSE(readEngineDesign("private node main {}\n", &none));
}

TEST(EngineDesignTests, FileName) {
    EXPECT_EQ(designFileName("My V8 (race)"), "my_v8_race");
    EXPECT_EQ(designFileName("  "), "custom_engine");
}
