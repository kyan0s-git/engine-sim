#include "../include/yds_sdl_window_system.h"

#include "../include/yds_sdl_window.h"
#include "../include/yds_sdl_monitor.h"
#include "../include/yds_sdl_input_system.h"

#include <SDL.h>

ysSdlWindowSystem::ysSdlWindowSystem() : ysWindowSystem(Platform::Sdl) {
    m_quitRequested = false;

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    // SDL reference counts subsystems so this pairs with the call to
    // SDL_QuitSubSystem() in the destructor
    SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

ysSdlWindowSystem::~ysSdlWindowSystem() {
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

ysError ysSdlWindowSystem::NewWindow(ysWindow **newWindow) {
    YDS_ERROR_DECLARE("NewWindow");

    if (newWindow == nullptr) return YDS_ERROR_RETURN(ysError::InvalidParameter);

    ysSdlWindow *sdlWindow = m_windowArray.NewGeneric<ysSdlWindow>();
    *newWindow = static_cast<ysWindow *>(sdlWindow);

    return YDS_ERROR_RETURN(ysError::None);
}

ysMonitor *ysSdlWindowSystem::NewMonitor() {
    return m_monitorArray.NewGeneric<ysSdlMonitor>();
}

void ysSdlWindowSystem::SurveyMonitors() {
    ysWindowSystem::SurveyMonitors();

    const int displayCount = SDL_GetNumVideoDisplays();
    for (int i = 0; i < displayCount; ++i) {
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(i, &bounds) != 0) continue;

        ysMonitor *monitor = NewMonitor();
        monitor->SetOrigin(bounds.x, bounds.y);
        monitor->SetLogicalSize(bounds.w, bounds.h);

        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(i, &mode) == 0) {
            monitor->SetPhysicalSize(mode.w, mode.h);
        }
        else {
            monitor->SetPhysicalSize(bounds.w, bounds.h);
        }

        const char *name = SDL_GetDisplayName(i);
        monitor->SetDeviceName((name != nullptr) ? name : "");
        monitor->CalculateScaling();
    }

    // Headless/virtual setups may not report any display; always provide one
    // so that callers can safely index monitor 0
    if (GetNumMonitors() == 0) {
        ysMonitor *monitor = NewMonitor();
        monitor->SetOrigin(0, 0);
        monitor->SetLogicalSize(1920, 1080);
        monitor->SetPhysicalSize(1920, 1080);
        monitor->SetDeviceName("Default");
        monitor->CalculateScaling();
    }
}

ysSdlWindow *ysSdlWindowSystem::FindWindowFromId(uint32_t id) {
    const int windowCount = m_windowArray.GetNumObjects();
    for (int i = 0; i < windowCount; ++i) {
        ysSdlWindow *window = static_cast<ysSdlWindow *>(m_windowArray.Get(i));
        if (window->GetSdlWindowId() == id) return window;
    }

    return nullptr;
}

namespace {
    ysSdlWindowSystem::EventHook s_eventHook = nullptr;
    void *s_eventHookUser = nullptr;
}

void ysSdlWindowSystem::SetEventHook(EventHook hook, void *user) {
    s_eventHook = hook;
    s_eventHookUser = user;
}

void ysSdlWindowSystem::ProcessMessages() {
    ysSdlInputSystem *inputSystem = static_cast<ysSdlInputSystem *>(m_inputSystem);
    if (inputSystem != nullptr) {
        inputSystem->BeginFrame();
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        const bool captured = (s_eventHook != nullptr)
            ? s_eventHook(event, s_eventHookUser)
            : false;

        ysSdlWindow *target = nullptr;
        switch (event.type) {
            case SDL_QUIT:
                m_quitRequested = true;
                for (int i = 0; i < m_windowArray.GetNumObjects(); ++i) {
                    ysWindow *window = m_windowArray.Get(i);
                    if (window->IsOpen()) {
                        window->OnCloseWindow();
                        CloseWindow(window);
                    }
                }
                break;
            case SDL_WINDOWEVENT:
                target = FindWindowFromId(event.window.windowID);
                if (target == nullptr) break;

                switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
                        target->OnCloseWindow();
                        CloseWindow(target);
                        break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    case SDL_WINDOWEVENT_RESIZED:
                        target->OnResizeWindow(
                            target->GetScreenWidth(), target->GetScreenHeight());
                        break;
                    case SDL_WINDOWEVENT_MOVED:
                        target->OnMoveWindow(event.window.data1, event.window.data2);
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        target->OnActivate();
                        break;
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        if (inputSystem != nullptr) inputSystem->ReleaseAll();
                        target->OnDeactivate();
                        break;
                    default:
                        break;
                }
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                if (inputSystem == nullptr) break;
                if (captured
                    && event.type != SDL_KEYUP
                    && event.type != SDL_MOUSEBUTTONUP)
                {
                    break;
                }

                {
                    // Every one of these events carries the window ID in the
                    // same position within the union
                    target = FindWindowFromId(event.window.windowID);
                    if (target == nullptr && m_windowArray.GetNumObjects() > 0) {
                        target = static_cast<ysSdlWindow *>(m_windowArray.Get(0));
                    }

                    const bool accept = inputSystem->IsGlobalInputEnabled()
                        || (target != nullptr && target->IsActive());
                    if (accept) {
                        inputSystem->ProcessEvent(event, target);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void ysSdlWindowSystem::ConfineCursor(ysWindow *window) {
    if (IsCursorConfined()) return;
    ysWindowSystem::ConfineCursor(window);

    ysSdlWindow *sdlWindow = static_cast<ysSdlWindow *>(window);
    if (sdlWindow->GetSdlWindow() != nullptr) {
        SDL_SetWindowGrab(sdlWindow->GetSdlWindow(), SDL_TRUE);
    }
}

void ysSdlWindowSystem::ReleaseCursor() {
    if (!IsCursorConfined()) return;
    ysWindowSystem::ReleaseCursor();

    for (int i = 0; i < m_windowArray.GetNumObjects(); ++i) {
        ysSdlWindow *window = static_cast<ysSdlWindow *>(m_windowArray.Get(i));
        if (window->GetSdlWindow() != nullptr) {
            SDL_SetWindowGrab(window->GetSdlWindow(), SDL_FALSE);
        }
    }
}

void ysSdlWindowSystem::SetCursorPosition(int x, int y) {
    SDL_WarpMouseGlobal(x, y);
}

void ysSdlWindowSystem::SetCursorVisible(bool visible) {
    if (visible == IsMouseVisible()) return;
    ysWindowSystem::SetCursorVisible(visible);

    SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}
