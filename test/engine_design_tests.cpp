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

TEST(EngineDesignTests, LayoutsAndBanks) {
    EngineDesign w;
    w.layout = EngineDesign::Layout::W;
    w.wBanks = 4;
    w.cylinders = 11;
    w.validate();
    EXPECT_EQ(w.bankCount(), 4);
    EXPECT_EQ(w.cylinders % 4, 0);
    EXPECT_EQ(w.bankAngles().size(), 4u);

    EngineDesign arrow = w;
    arrow.wBanks = 3;
    arrow.cylinders = 12;
    arrow.validate();
    EXPECT_EQ(arrow.bankCount(), 3);
    EXPECT_DOUBLE_EQ(arrow.bankAngles()[1], 0.0);

    EngineDesign rotary;
    rotary.layout = EngineDesign::Layout::Rotary;
    rotary.rotors = 2;
    rotary.eccentricity = 0.015;
    rotary.generatingRadius = 0.105;
    rotary.rotorWidth = 0.080;
    rotary.validate();
    // 13B: 2 x 654 cc
    EXPECT_NEAR(rotary.displacement(), 1.309e-3, 2e-6);
}

TEST(EngineDesignTests, ExplicitFiringOrder) {
    std::vector<int> order;
    EXPECT_TRUE(EngineDesign::parseFiringOrder("1-8-4-5-7-3-6-2", 8, &order));
    EXPECT_EQ(order[1], 8);
    EXPECT_FALSE(EngineDesign::parseFiringOrder("1-8-4-5-7-3-6", 8, &order));
    EXPECT_FALSE(EngineDesign::parseFiringOrder("1-1-4-5-7-3-6-2", 8, &order));
    EXPECT_FALSE(EngineDesign::parseFiringOrder("1-x-4-5-7-3-6-2", 8, &order));

    EngineDesign d;
    d.layout = EngineDesign::Layout::V;
    d.cylinders = 8;
    d.firingOrderText = "1-8-4-5-7-3-6-2";
    EXPECT_TRUE(d.validate());
    EXPECT_EQ(d.firingOrder()[1], 8);

    // Invalid orders fall back to the automatic order
    d.firingOrderText = "1-2-3";
    EXPECT_FALSE(d.validate());
    EXPECT_TRUE(d.firingOrderText.empty());
}

TEST(EngineDesignTests, FiringOrderCoversAllCylindersOfEveryBank) {
    for (int banks : { 3, 4 }) {
        for (int perBank = 1; perBank <= 4; ++perBank) {
            EngineDesign d;
            d.layout = EngineDesign::Layout::W;
            d.wBanks = banks;
            d.cylinders = banks * perBank;
            d.validate();

            std::vector<int> order = d.firingOrder();
            ASSERT_EQ(static_cast<int>(order.size()), d.cylinders);
            std::sort(order.begin(), order.end());
            for (int i = 0; i < d.cylinders; ++i) EXPECT_EQ(order[i], i + 1);
        }
    }
}

TEST(EngineDesignTests, PresetsGenerateAndRoundTrip) {
    const std::vector<EngineDesign> &presets = builtinEnginePresets();
    ASSERT_GE(presets.size(), 9u);

    bool rotary = false, boosted = false, w = false;
    for (const EngineDesign &preset : presets) {
        EngineDesign d = preset;
        EXPECT_TRUE(d.validate()) << preset.name;

        const std::string script = generateEngineScript(d);
        EXPECT_NE(script.find("main()"), std::string::npos) << preset.name;
        EXPECT_EQ(script.find("e-0"), std::string::npos) << preset.name;    // no exponent notation

        EngineDesign read;
        ASSERT_TRUE(readEngineDesign(script, &read)) << preset.name;
        EXPECT_EQ(generateEngineScript(read), script) << preset.name;

        rotary |= d.isRotary();
        boosted |= d.isBoosted();
        w |= (d.layout == EngineDesign::Layout::W);
    }

    EXPECT_TRUE(rotary);
    EXPECT_TRUE(boosted);
    EXPECT_TRUE(w);
}

TEST(EngineDesignTests, ForcedInductionScript) {
    EngineDesign d;
    d.induction = EngineDesign::Induction::Turbo;
    EXPECT_NE(generateEngineScript(d).find("turbocharger("), std::string::npos);

    d.induction = EngineDesign::Induction::Roots;
    EXPECT_NE(generateEngineScript(d).find("\"roots\""), std::string::npos);

    d.induction = EngineDesign::Induction::Natural;
    EXPECT_EQ(generateEngineScript(d).find("add_forced_induction"), std::string::npos);
}
