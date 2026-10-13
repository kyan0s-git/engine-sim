#include "../include/updater.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
#endif

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
    std::string quote(const std::string &s) {
        return "\"" + s + "\"";
    }

    // Runs a command without showing a console window and returns its exit
    // code; standard output is captured in `output`
    int runCommand(const std::string &command, std::string *output) {
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr, writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return -1;
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = writePipe;
        si.hStdError = writePipe;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi = {};
        std::string commandLine = "cmd.exe /c \"" + command + "\"";
        std::vector<char> buffer(commandLine.begin(), commandLine.end());
        buffer.push_back('\0');

        const BOOL created = CreateProcessA(
            nullptr, buffer.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        CloseHandle(writePipe);

        if (!created) {
            CloseHandle(readPipe);
            return -1;
        }

        char chunk[4096];
        DWORD read = 0;
        while (ReadFile(readPipe, chunk, sizeof(chunk), &read, nullptr) && read > 0) {
            if (output != nullptr) output->append(chunk, read);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(readPipe);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return static_cast<int>(exitCode);
    }
#else
    std::string quote(const std::string &s) {
        std::string result = "'";
        for (const char c : s) {
            if (c == '\'') result += "'\\''";
            else result += c;
        }

        return result + "'";
    }

    int runCommand(const std::string &command, std::string *output) {
        FILE *pipe = popen((command + " 2>&1").c_str(), "r");
        if (pipe == nullptr) return -1;

        char chunk[4096];
        size_t read;
        while ((read = std::fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
            if (output != nullptr) output->append(chunk, read);
        }

        const int status = pclose(pipe);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
#endif

    std::vector<int> versionNumbers(const std::string &version, bool *prerelease) {
        std::string v = version;
        if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v = v.substr(1);

        const size_t suffix = v.find_first_of("-+");
        *prerelease = (suffix != std::string::npos && v[suffix] == '-');
        v = v.substr(0, suffix);

        std::vector<int> numbers;
        std::stringstream ss(v);
        std::string part;
        while (std::getline(ss, part, '.')) {
            numbers.push_back(std::atoi(part.c_str()));
        }

        while (numbers.size() < 3) numbers.push_back(0);
        return numbers;
    }

    bool isWritableDirectory(const fs::path &path) {
        std::error_code ec;
        const fs::path probe = path / ".engine-sim-write-test";
        {
            std::ofstream file(probe);
            if (!file.is_open()) return false;
        }

        fs::remove(probe, ec);
        return true;
    }

} /* namespace */

Updater::Updater() : m_state(State::Idle) {
    /* void */
}

Updater::~Updater() {
    join();
}

void Updater::initialize(const Parameters &params) {
    m_params = params;
    m_installRoot = findInstallRoot();
}

void Updater::setEnabled(bool enabled) {
    if (!enabled) m_state = State::Disabled;
    else if (m_state == State::Disabled) m_state = State::Idle;
}

void Updater::join() {
    if (m_thread.joinable()) m_thread.join();
}

void Updater::checkAsync() {
    const State state = m_state;
    if (state == State::Disabled || state == State::Checking || state == State::Downloading) return;

    join();
    m_state = State::Checking;
    m_thread = std::thread(&Updater::check, this);
}

void Updater::installAsync() {
    if (m_state != State::Available || !canInstall()) return;

    join();
    m_state = State::Downloading;
    m_thread = std::thread(&Updater::install, this);
}

Updater::Release Updater::getRelease() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_release;
}

std::string Updater::getError() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_error;
}

void Updater::fail(const std::string &error) {
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_error = error;
    }

    m_state = State::Failed;
}

std::string Updater::getStatusText() const {
    const Release release = getRelease();
    switch (m_state.load()) {
    case State::Checking:
        return "CHECKING FOR UPDATES...";
    case State::UpToDate:
        return "UP TO DATE";
    case State::Available:
        return canInstall()
            ? "UPDATE " + release.version + " AVAILABLE -- [F8] INSTALL"
            : "UPDATE " + release.version + " AVAILABLE -- [F8] DOWNLOAD PAGE";
    case State::Downloading:
        return "DOWNLOADING UPDATE " + release.version + "...";
    case State::ReadyToRestart:
        return "UPDATE " + release.version + " READY -- [F8] RESTART";
    case State::Failed:
        return "UPDATE FAILED (SEE CONSOLE)";
    case State::Disabled:
        return "UPDATE CHECK DISABLED";
    case State::Idle:
    default:
        return "";
    }
}

int Updater::compareVersions(const std::string &a, const std::string &b) {
    bool preA, preB;
    const std::vector<int> va = versionNumbers(a, &preA);
    const std::vector<int> vb = versionNumbers(b, &preB);

    const size_t n = std::max(va.size(), vb.size());
    for (size_t i = 0; i < n; ++i) {
        const int x = (i < va.size()) ? va[i] : 0;
        const int y = (i < vb.size()) ? vb[i] : 0;
        if (x != y) return (x > y) ? 1 : -1;
    }

    // 1.0.0-beta is older than 1.0.0
    if (preA != preB) return preA ? -1 : 1;
    return 0;
}

size_t Updater::findJsonString(
    const std::string &json,
    const std::string &key,
    size_t from,
    std::string *value)
{
    const std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern, from);
    while (pos != std::string::npos) {
        size_t i = pos + pattern.size();
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        if (i < json.size() && json[i] == ':') {
            ++i;
            while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
            if (i < json.size() && json[i] == '"') {
                std::string result;
                for (++i; i < json.size() && json[i] != '"'; ++i) {
                    if (json[i] == '\\' && i + 1 < json.size()) {
                        ++i;
                        switch (json[i]) {
                        case 'n': result += '\n'; break;
                        case 't': result += '\t'; break;
                        case '/': result += '/'; break;
                        default: result += json[i]; break;
                        }
                    }
                    else {
                        result += json[i];
                    }
                }

                *value = result;
                return i + 1;
            }
        }

        pos = json.find(pattern, pos + 1);
    }

    return std::string::npos;
}

std::string Updater::platformAssetSuffix() {
#if defined(_WIN32)
    return "-windows-x64.zip";
#elif defined(__APPLE__)
    return "-macos-universal.zip";
#else
    return "-linux-x64.tar.gz";
#endif
}

bool Updater::parseRelease(const std::string &json, Release *release) {
    std::string tag;
    if (findJsonString(json, "tag_name", 0, &tag) == std::string::npos) {
        return false;
    }

    *release = Release();
    release->version = (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
    findJsonString(json, "html_url", 0, &release->pageUrl);

    const std::string suffix = platformAssetSuffix();
    size_t pos = 0;
    std::string url;
    while ((pos = findJsonString(json, "browser_download_url", pos, &url)) != std::string::npos) {
        if (url.size() >= suffix.size()
            && url.compare(url.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
            release->assetUrl = url;
            release->assetName = url.substr(url.find_last_of('/') + 1);
            break;
        }
    }

    return true;
}

void Updater::check() {
    const std::string url = !m_params.releaseUrl.empty()
        ? m_params.releaseUrl
        : "https://api.github.com/repos/" + m_params.repository + "/releases/latest";

    // The HTTP status is appended on a line of its own
    const std::string command =
        "curl -sSL --max-time 20"
        " -H " + quote("Accept: application/vnd.github+json") +
        " -H " + quote("User-Agent: engine-sim-updater") +
        " -w " + quote("\n%{http_code}") +
        " " + quote(url);

    std::string output;
    if (runCommand(command, &output) != 0) {
        fail("Could not reach the update server: " + output.substr(0, 200));
        return;
    }

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    const size_t statusLine = output.find_last_of('\n');
    const std::string status =
        (statusLine == std::string::npos) ? output : output.substr(statusLine + 1);
    output = (statusLine == std::string::npos) ? "" : output.substr(0, statusLine);

    if (status == "404") {
        // Nothing has been released yet
        m_state = State::UpToDate;
        return;
    }
    else if (status != "200" && status != "000") {     // 000: file:// URLs
        fail("The update server returned HTTP " + status);
        return;
    }

    Release release;
    if (!parseRelease(output, &release)) {
        fail("Unexpected response from GitHub");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_release = release;
    }

    m_state = (compareVersions(release.version, m_params.currentVersion) > 0)
        ? State::Available
        : State::UpToDate;
}

void Updater::install() {
    const Release release = getRelease();
    if (release.assetUrl.empty()) {
        fail("The release has no package for this platform");
        return;
    }

    std::error_code ec;
    const fs::path updates = fs::path(m_params.userDataPath) / "updates";
    const fs::path staging = updates / "staging";
    const fs::path archive = updates / release.assetName;
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    if (ec) {
        fail("Could not create " + staging.string());
        return;
    }

    std::string output;
    const std::string download =
        "curl -fsSL --max-time 900 -H " + quote("User-Agent: engine-sim-updater")
        + " -o " + quote(archive.string()) + " " + quote(release.assetUrl);
    if (runCommand(download, &output) != 0) {
        fail("Download failed: " + output.substr(0, 200));
        return;
    }

    // tar reads .zip files on Windows 10 and newer (bsdtar); macOS uses
    // ditto, which keeps the app bundle's attributes and signature
#if defined(__APPLE__)
    const std::string extract =
        "ditto -x -k " + quote(archive.string()) + " " + quote(staging.string());
#elif defined(_WIN32)
    const std::string extract =
        "tar -xf " + quote(archive.string()) + " -C " + quote(staging.string());
#else
    const std::string extract =
        "tar -xzf " + quote(archive.string()) + " -C " + quote(staging.string());
#endif

    output.clear();
    if (runCommand(extract, &output) != 0) {
        fail("Could not extract the update: " + output.substr(0, 200));
        return;
    }

    fs::remove(archive, ec);

    // Packages contain a single top level folder
    fs::path packageRoot = staging;
    for (const fs::directory_entry &entry : fs::directory_iterator(staging, ec)) {
        if (entry.is_directory()) {
            packageRoot = entry.path();
            break;
        }
    }

#if defined(__APPLE__)
    const fs::path expected = packageRoot / "Engine Simulator.app";
#elif defined(_WIN32)
    const fs::path expected = packageRoot / "bin" / "engine-sim-app.exe";
#else
    const fs::path expected = packageRoot / "bin" / "engine-sim-app";
#endif

    if (!fs::exists(expected, ec)) {
        fail("The downloaded package is incomplete");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_stagedPath = packageRoot.string();
    }

    m_state = State::ReadyToRestart;
}

bool Updater::startRestart() {
    if (m_state != State::ReadyToRestart) return false;

    join();

    std::string staged;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        staged = m_stagedPath;
    }

    const fs::path updates = fs::path(m_params.userDataPath) / "updates";

#ifdef _WIN32
    const fs::path script = updates / "apply_update.cmd";
    const DWORD pid = GetCurrentProcessId();
    {
        std::ofstream file(script, std::ios::out | std::ios::trunc);
        if (!file.is_open()) return false;

        // Wait for the application to exit, copy the new files over the
        // installation and start it again
        file << "@echo off\r\n"
            << ":wait\r\n"
            << "tasklist /FI \"PID eq " << pid << "\" 2>NUL | find \"" << pid << "\" >NUL\r\n"
            << "if not errorlevel 1 (timeout /t 1 /nobreak >NUL & goto wait)\r\n"
            << "robocopy " << quote(staged) << " " << quote(m_installRoot)
            << " /E /NFL /NDL /NJH /NJS /NP /R:5 /W:1 >NUL\r\n"
            << "start \"\" " << quote((fs::path(m_installRoot) / "bin" / "engine-sim-app.exe").string()) << "\r\n";
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::string commandLine = "cmd.exe /c " + quote(script.string());
    std::vector<char> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back('\0');

    if (!CreateProcessA(
        nullptr, buffer.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
#else
    const fs::path script = updates / "apply_update.sh";
    {
        std::ofstream file(script, std::ios::out | std::ios::trunc);
        if (!file.is_open()) return false;

        file << "#!/bin/sh\n"
            << "while kill -0 " << getpid() << " 2>/dev/null; do sleep 0.2; done\n";
#ifdef __APPLE__
        // m_installRoot is the app bundle; replace it as a whole
        const std::string app = m_installRoot;
        file << "rm -rf " << quote(app + ".new") << "\n"
            << "ditto " << quote(staged + "/Engine Simulator.app") << " " << quote(app + ".new")
            << " && rm -rf " << quote(app) << " && mv " << quote(app + ".new") << " " << quote(app) << "\n"
            << "xattr -dr com.apple.quarantine " << quote(app) << " 2>/dev/null\n"
            << "open " << quote(app) << "\n";
#else
        file << "cp -Rf " << quote(staged + "/.") << " " << quote(m_installRoot + "/") << "\n"
            << "exec " << quote(m_installRoot + "/bin/engine-sim-app") << "\n";
#endif
    }

    const std::string scriptPath = script.string();
    char sh[] = "/bin/sh";
    std::vector<char> arg(scriptPath.begin(), scriptPath.end());
    arg.push_back('\0');
    char *argv[] = { sh, arg.data(), nullptr };

    pid_t child;
    return posix_spawn(&child, "/bin/sh", nullptr, nullptr, argv, environ) == 0;
#endif
}

std::string Updater::findInstallRoot() const {
    std::error_code ec;
    if (m_params.executablePath.empty()) return "";

    fs::path dir = fs::path(m_params.executablePath);
    if (!fs::is_directory(dir, ec)) dir = dir.parent_path();
    dir = fs::weakly_canonical(dir, ec);

#ifdef __APPLE__
    // .../Engine Simulator.app/Contents/{MacOS,Resources}
    for (fs::path p = dir; !p.empty() && p != p.root_path(); p = p.parent_path()) {
        if (p.extension() == ".app") {
            const bool packaged = fs::exists(p / "Contents" / "Resources" / "assets" / "main.mr", ec);
            return (packaged && isWritableDirectory(p.parent_path()) && isWritableDirectory(p))
                ? p.string()
                : "";
        }
    }

    return "";
#else
    // <root>/bin/engine-sim-app with <root>/{assets,es,delta}
    if (dir.filename() != "bin") return "";

    const fs::path root = dir.parent_path();
    const bool packaged =
        fs::exists(root / "assets" / "main.mr", ec)
        && fs::exists(root / "es" / "engine_sim.mr", ec)
        && fs::is_directory(root / "delta", ec);

    return (packaged && isWritableDirectory(root) && isWritableDirectory(dir))
        ? root.string()
        : "";
#endif
}
