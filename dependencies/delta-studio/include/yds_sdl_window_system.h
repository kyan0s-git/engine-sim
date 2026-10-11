#ifndef YDS_SDL_WINDOW_SYSTEM_H
#define YDS_SDL_WINDOW_SYSTEM_H

#include "yds_window_system.h"

#include <cstdint>

class ysSdlWindow;

// Cross-platform window system built on top of SDL2. Replaces the
// original Win32 implementation so that the engine can run on Windows,
// Linux and macOS with a single code path.
class ysSdlWindowSystem : public ysWindowSystem {
public:
    ysSdlWindowSystem();
    virtual ~ysSdlWindowSystem();

    virtual ysError NewWindow(ysWindow **newWindow) override;

    virtual ysMonitor *NewMonitor() override;
    virtual void SurveyMonitors() override;

    virtual void ProcessMessages() override;

    virtual void ConfineCursor(ysWindow *window) override;
    virtual void ReleaseCursor() override;

    virtual void SetCursorPosition(int x, int y) override;
    virtual void SetCursorVisible(bool visible) override;

    ysSdlWindow *FindWindowFromId(uint32_t id);

    // Set to true when the OS requests the application to quit
    // (e.g. Cmd+Q on macOS or the last window being closed)
    bool IsQuitRequested() const { return m_quitRequested; }

protected:
    bool m_quitRequested;
};

#endif /* YDS_SDL_WINDOW_SYSTEM_H */
