#ifndef ATG_ENGINE_SIM_UPDATER_H
#define ATG_ENGINE_SIM_UPDATER_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

// Checks GitHub for a newer release and installs it.
//
// Network access and archive extraction use the command line tools that come
// with the operating system (curl, tar and ditto), so the application doesn't
// need an HTTP or archive library. The check runs on a background thread.
//
// Installing downloads the package for this platform into the user data
// folder and extracts it there. The new files are copied over the
// installation by a small helper script once the application has exited,
// after which the application is started again. Installing is only offered
// for packaged installations in a writable location; otherwise the release
// page is opened instead.
class Updater {
    public:
        enum class State {
            Disabled,
            Idle,
            Checking,
            UpToDate,
            Available,
            Downloading,
            ReadyToRestart,
            Failed
        };

        struct Release {
            std::string version;
            std::string pageUrl;
            std::string assetName;
            std::string assetUrl;
        };

        struct Parameters {
            std::string currentVersion;
            std::string repository = "kyan0s-git/engine-sim";

            // Release information URL; empty = GitHub's latest release of
            // the repository (other URLs are used by the tests)
            std::string releaseUrl;
            std::string executablePath;
            std::string userDataPath;
        };

    public:
        Updater();
        ~Updater();

        void initialize(const Parameters &params);
        void setEnabled(bool enabled);

        void checkAsync();
        void installAsync();

        // Starts the helper that replaces the installation and relaunches the
        // application; the caller should exit right after this returns true
        bool startRestart();

        State getState() const { return m_state; }
        Release getRelease() const;
        std::string getError() const;

        // Short status for the user interface (upper case)
        std::string getStatusText() const;

        // True if this is a packaged installation that can be replaced
        bool canInstall() const { return !m_installRoot.empty(); }
        const std::string &getInstallRoot() const { return m_installRoot; }

        // Version comparison: 1 if a is newer than b, -1 if older, 0 if equal
        static int compareVersions(const std::string &a, const std::string &b);

        // Finds the value of a string field ("key": "value") in JSON text
        // starting at `from`; returns the position after the value or
        // std::string::npos
        static size_t findJsonString(
            const std::string &json,
            const std::string &key,
            size_t from,
            std::string *value);

        // Package name suffix of this platform, e.g. "-linux-x64.tar.gz"
        static std::string platformAssetSuffix();

        // Parses the GitHub "latest release" JSON
        static bool parseRelease(const std::string &json, Release *release);

    protected:
        void check();
        void install();
        void join();
        void fail(const std::string &error);
        std::string findInstallRoot() const;

        Parameters m_params;
        std::string m_installRoot;
        std::string m_stagedPath;

        std::atomic<State> m_state;
        mutable std::mutex m_lock;
        Release m_release;
        std::string m_error;

        std::thread m_thread;
};

#endif /* ATG_ENGINE_SIM_UPDATER_H */
