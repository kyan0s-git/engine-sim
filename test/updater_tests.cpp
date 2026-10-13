#include <gtest/gtest.h>

#include "../include/updater.h"

TEST(UpdaterTests, CompareVersions) {
    EXPECT_EQ(Updater::compareVersions("0.3.0", "0.2.0"), 1);
    EXPECT_EQ(Updater::compareVersions("0.2.0", "0.10.0"), -1);
    EXPECT_EQ(Updater::compareVersions("v1.2.3", "1.2.3"), 0);
    EXPECT_EQ(Updater::compareVersions("1.2", "1.2.0"), 0);
    EXPECT_EQ(Updater::compareVersions("1.0.0", "1.0.0-beta"), 1);
    EXPECT_EQ(Updater::compareVersions("1.0.0-beta", "1.0.0"), -1);
    EXPECT_EQ(Updater::compareVersions("1.0.1-beta", "1.0.0"), 1);
}

TEST(UpdaterTests, ParseRelease) {
    // Abbreviated GitHub "latest release" response
    const std::string json = R"({
        "url": "https://api.github.com/repos/kyan0s-git/engine-sim/releases/1",
        "html_url": "https://github.com/kyan0s-git/engine-sim/releases/tag/v0.3.0",
        "tag_name": "v0.3.0",
        "name": "Engine Simulator v0.3.0 \"Engineering\"",
        "assets": [
            {
                "name": "engine-sim-0.3.0-windows-x64.zip",
                "browser_download_url": "https://github.com/kyan0s-git/engine-sim/releases/download/v0.3.0/engine-sim-0.3.0-windows-x64.zip"
            },
            {
                "name": "engine-sim-0.3.0-linux-x64.tar.gz",
                "browser_download_url": "https://github.com/kyan0s-git/engine-sim/releases/download/v0.3.0/engine-sim-0.3.0-linux-x64.tar.gz"
            },
            {
                "name": "engine-sim-0.3.0-macos-universal.zip",
                "browser_download_url": "https://github.com/kyan0s-git/engine-sim/releases/download/v0.3.0/engine-sim-0.3.0-macos-universal.zip"
            }
        ]
    })";

    Updater::Release release;
    ASSERT_TRUE(Updater::parseRelease(json, &release));
    EXPECT_EQ(release.version, "0.3.0");
    EXPECT_EQ(release.pageUrl, "https://github.com/kyan0s-git/engine-sim/releases/tag/v0.3.0");
    EXPECT_EQ(release.assetName, "engine-sim-0.3.0" + Updater::platformAssetSuffix());
    EXPECT_NE(release.assetUrl.find(release.assetName), std::string::npos);

    std::string name;
    Updater::findJsonString(json, "name", json.find("\"tag_name\""), &name);
    EXPECT_EQ(name, "Engine Simulator v0.3.0 \"Engineering\"");

    EXPECT_FALSE(Updater::parseRelease("{\"message\": \"Not Found\"}", &release));
}

TEST(UpdaterTests, DevelopmentBuildIsNotInstallable) {
    // The test executable doesn't live in an installed package
    Updater updater;
    Updater::Parameters params;
    params.currentVersion = "0.2.0";
    params.executablePath = ".";
    updater.initialize(params);
    EXPECT_FALSE(updater.canInstall());
    EXPECT_EQ(updater.getState(), Updater::State::Idle);

    updater.setEnabled(false);
    EXPECT_EQ(updater.getState(), Updater::State::Disabled);
}
