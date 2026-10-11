#include <gtest/gtest.h>

#include "../include/function.h"

#include <stdlib.h>

TEST(FunctionTests, FunctionSanityCheck) {
    Function f;
    f.initialize(16, 1.0);
    f.destroy();
}

TEST(FunctionTests, FunctionTriangleFilterTest) {
    Function f;
    f.initialize(0, 1.0);
    for (int i = 0; i < 10; ++i) {
        f.addSample((double)i, (double)i * 2);
    }

    EXPECT_NEAR(f.sampleTriangle(-1.0), 0.0, 1E-6);
    EXPECT_NEAR(f.sampleTriangle(11.0), 18.0, 1E-6);

    for (int i = 0; i < 10; ++i) {
        EXPECT_NEAR(f.sampleTriangle((double)i), (double)i * 2, 1E-6);
    }

    f.destroy();
}

TEST(FunctionTests, FunctionClosestTest) {
    Function f;
    f.initialize(0, 1.0);
    f.addSample(0.0, 1.0);
    f.addSample(2.0, 1.0);
    f.addSample(3.0, 1.1);
    f.addSample(1.0, 1.0);
    f.addSample(5.0, 10.0);
    f.addSample(4.0, 9.0);

    EXPECT_EQ(f.closestSample(2.4), 2);
    EXPECT_EQ(f.closestSample(6.0), 5);

    f.destroy();
}

TEST(FunctionTests, FunctionRandomAddTest) {
    Function f;
    f.initialize(0, 1.0);

    for (int i = 0; i < 1000; ++i) {
        f.addSample(rand() % 1000, i);
    }

    EXPECT_TRUE(f.isOrdered());

    f.destroy();
}

TEST(FunctionTests, FunctionGaussianTest) {
    Function f;
    f.initialize(0, 1.0);
    f.addSample(0.0, 1.0);
    f.addSample(2.0, 1.0);
    f.addSample(3.0, 5.0);
    f.addSample(1.0, 1.0);
    f.addSample(5.0, 10.0);
    f.addSample(4.0, 9.0);

    // Outside of the sampled range the endpoints dominate
    EXPECT_NEAR(f.sampleGaussian(100.0), 10.0, 1E-3);
    EXPECT_NEAR(f.sampleGaussian(-100.0), 1.0, 1E-3);

    // Within a flat region the smoothed value stays close to the samples
    // (the kernel is wide enough to pick up a small contribution from x = 3)
    EXPECT_NEAR(f.sampleGaussian(0.5), 1.0, 0.01);

    // Smoothing never overshoots the data and preserves monotonicity
    double previous = f.sampleGaussian(0.0);
    for (double s = 0.0; s <= 5.0; s += 0.01) {
        const double v = f.sampleGaussian(s);
        EXPECT_GE(v, 1.0 - 1E-6);
        EXPECT_LE(v, 10.0 + 1E-6);
        EXPECT_GE(v, previous - 1E-6);
        previous = v;
    }

    f.destroy();
}
