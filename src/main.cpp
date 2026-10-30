#include "../include/engine_sim_application.h"
#include "../include/updater.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

    void printUsage(const char *program) {
        std::printf(
            "Usage: %s [options] [engine.mr]\n"
            "\n"
            "Options:\n"
            "  --engine <file>     Engine script to load (same as passing it positionally)\n"
            "  --theme <file>      Theme script to load\n"
            "  --data <dir>        Directory containing the assets/ and es/ folders\n"
            "  --fullscreen        Start in fullscreen mode\n"
            "  --windowed          Start in windowed mode\n"
            "  --no-update-check   Don't check for new releases\n"
            "  --version           Print the version and exit\n"
            "  --help              Show this message\n",
            program);
    }

    // Returns false if the application should exit immediately
    bool parseArguments(int argc, char *argv[], EngineSimApplication::StartupOptions *options) {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            const bool hasValue = (i + 1) < argc;

            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage(argv[0]);
                return false;
            }
            else if (arg == "--version" || arg == "-v") {
                std::printf(
                    "Engine Simulator %s\n"
                    "Based on Engine Simulator by Ange Yaghi (MIT license)\n",
                    EngineSimApplication::getBuildVersion().c_str());
                return false;
            }
            else if ((arg == "--engine" || arg == "-e") && hasValue) {
                options->engineScript = argv[++i];
            }
            else if ((arg == "--theme" || arg == "-t") && hasValue) {
                options->themeScript = argv[++i];
            }
            else if ((arg == "--data" || arg == "-d") && hasValue) {
                options->dataRoot = argv[++i];
            }
            else if (arg == "--fullscreen") {
                options->fullscreen = EngineSimApplication::StartupOptions::Toggle::On;
            }
            else if (arg == "--windowed") {
                options->fullscreen = EngineSimApplication::StartupOptions::Toggle::Off;
            }
            else if (arg == "--no-update-check") {
                options->checkForUpdates = false;
            }
            else if (arg.rfind("-psn_", 0) == 0) {
                // macOS Finder passes a process serial number; ignore it
            }
            else if (!arg.empty() && arg[0] != '-') {
                options->engineScript = arg;
            }
            else {
                std::fprintf(stderr, "Unknown option: %s\n\n", arg.c_str());
                printUsage(argv[0]);
                return false;
            }
        }

        return true;
    }

} /* namespace */

// --update-test [release API URL]: checks for the latest release, installs it
// (even if it isn't newer) and restarts into it with --version, writing the
// output to <user data>/updates/relaunch.log. Used by CI to exercise the
// in-app updater on every platform without a window.
int runUpdateTest(int argc, char *argv[]) {
    Updater::Parameters params;
    params.currentVersion = EngineSimApplication::getBuildVersion();
    params.force = true;
    params.relaunchArguments = "--version";
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--update-test") == 0 && argv[i + 1][0] != '-') {
            params.releaseUrl = argv[i + 1];
        }
    }

    char *basePath = SDL_GetBasePath();
    if (basePath != nullptr) {
        params.executablePath = basePath;
        SDL_free(basePath);
    }

    char *prefPath = SDL_GetPrefPath("EngineSimulator", "engine-sim");
    if (prefPath != nullptr) {
        params.userDataPath = prefPath;
        SDL_free(prefPath);
    }

    const char *appImage = std::getenv("APPIMAGE");
    if (appImage != nullptr) params.appImagePath = appImage;

    params.relaunchLog = params.userDataPath + "updates/relaunch.log";

    Updater updater;
    updater.initialize(params);
    std::printf("Install root: %s\n", updater.getInstallRoot().empty() ? "(none)" : updater.getInstallRoot().c_str());

    std::string message;
    const bool ok = updater.runSynchronously(&message);
    std::printf("%s\n", message.c_str());
    if (ok) std::printf("Relaunch log: %s\n", params.relaunchLog.c_str());
    std::fflush(stdout);
    return ok ? 0 : 1;
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--update-test") == 0) return runUpdateTest(argc, argv);
    }

    EngineSimApplication::StartupOptions options;
    if (!parseArguments(argc, argv, &options)) {
        return 0;
    }

    EngineSimApplication application;
    application.setStartupOptions(options);
    if (!application.initialize(nullptr, ysContextObject::DeviceAPI::OpenGL4_0)) {
        const std::string message =
            "Engine Simulator could not start.\n\n" + application.getStartupError();
        std::fprintf(stderr, "%s\n", message.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Simulator", message.c_str(), nullptr);

        application.destroy();
        return 1;
    }

    application.run();
    application.destroy();

    return 0;
}
