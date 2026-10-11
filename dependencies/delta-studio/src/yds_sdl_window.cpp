#include "../include/yds_sdl_window.h"

#include "../include/yds_sdl_window_system.h"

#include <SDL.h>

#include <cstring>

ysSdlWindow::ysSdlWindow() : ysWindow(Platform::Sdl) {
    m_window = nullptr;
}

ysSdlWindow::~ysSdlWindow() {
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

static Uint32 sdlWindowFlags(ysWindow::WindowStyle style) {
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    switch (style) {
        case ysWindow::WindowStyle::Fullscreen:
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            break;
        case ysWindow::WindowStyle::Popup:
            flags |= SDL_WINDOW_BORDERLESS;
            break;
        case ysWindow::WindowStyle::Windowed:
        default:
            flags |= SDL_WINDOW_RESIZABLE;
            break;
    }

    return flags;
}

ysError ysSdlWindow::InitializeWindow(
    ysWindow *parent,
    const char *title,
    WindowStyle style,
    int x, int y,
    int width, int height,
    ysMonitor *monitor)
{
    YDS_ERROR_DECLARE("InitializeWindow");

    if (!CheckCompatibility(parent)) return YDS_ERROR_RETURN(ysError::IncompatiblePlatforms);

    YDS_NESTED_ERROR_CALL(ysWindow::InitializeWindow(parent, title, style, x, y, width, height, monitor));

    // These attributes affect the pixel format of the window so they must be
    // configured before the window is created
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    // Keep the window on screen; (0, 0) is often under a menu bar or panel
    const int posX = (x == 0 && y == 0) ? SDL_WINDOWPOS_CENTERED : x;
    const int posY = (x == 0 && y == 0) ? SDL_WINDOWPOS_CENTERED : y;

    m_window = SDL_CreateWindow(title, posX, posY, width, height, sdlWindowFlags(style));
    if (m_window == nullptr) {
        // Retry without a stencil buffer which some software renderers lack
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
        m_window = SDL_CreateWindow(title, posX, posY, width, height, sdlWindowFlags(style));
    }

    if (m_window == nullptr) {
        return YDS_ERROR_RETURN_MSG(ysError::ApiError, SDL_GetError());
    }

    int wx = 0, wy = 0;
    SDL_GetWindowPosition(m_window, &wx, &wy);
    m_locationx = wx;
    m_locationy = wy;

    m_width = GetScreenWidth();
    m_height = GetScreenHeight();

    m_windowState = WindowState::Visible;
    // Some window managers never send an initial focus event, so assume the
    // freshly created window is active (matches the Win32 behaviour)
    m_active = true;

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlWindow::InitializeWindow(ysWindow *parent, const char *title, WindowStyle style, ysMonitor *monitor) {
    YDS_ERROR_DECLARE("InitializeWindow");

    YDS_NESTED_ERROR_CALL(
        InitializeWindow(
            parent, title, style,
            monitor->GetOriginX(), monitor->GetOriginY(),
            monitor->GetLogicalWidth(), monitor->GetLogicalHeight(),
            monitor));

    return YDS_ERROR_RETURN(ysError::None);
}

bool ysSdlWindow::SetWindowStyle(WindowStyle style) {
    if (m_window == nullptr) return false;
    if (!ysWindow::SetWindowStyle(style)) return false;

    switch (style) {
        case WindowStyle::Fullscreen:
            SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            break;
        case WindowStyle::Popup:
            SDL_SetWindowFullscreen(m_window, 0);
            SDL_SetWindowBordered(m_window, SDL_FALSE);
            break;
        case WindowStyle::Windowed:
        default:
            SDL_SetWindowFullscreen(m_window, 0);
            SDL_SetWindowBordered(m_window, SDL_TRUE);
            SDL_SetWindowResizable(m_window, SDL_TRUE);
            break;
    }

    SDL_ShowWindow(m_window);
    SDL_RaiseWindow(m_window);

    OnResizeWindow(GetScreenWidth(), GetScreenHeight());

    return true;
}

void ysSdlWindow::ScreenToLocal(int &x, int &y) const {
    if (m_window == nullptr) {
        x = y = 0;
        return;
    }

    // Positions stored by the input system are already relative to the
    // window and expressed in physical pixels; flip to a bottom-left origin
    y = GetScreenHeight() - y;
}

bool ysSdlWindow::IsVisible() {
    if (m_window == nullptr) return false;

    const Uint32 flags = SDL_GetWindowFlags(m_window);
    return (flags & SDL_WINDOW_SHOWN) != 0 && (flags & SDL_WINDOW_MINIMIZED) == 0;
}

int ysSdlWindow::GetScreenWidth() const {
    if (m_window == nullptr) return 0;

    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(m_window, &w, &h);
    return w;
}

int ysSdlWindow::GetScreenHeight() const {
    if (m_window == nullptr) return 0;

    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(m_window, &w, &h);
    return h;
}

float ysSdlWindow::GetPixelScaleX() const {
    if (m_window == nullptr) return 1.0f;

    int w = 0, h = 0, pw = 0, ph = 0;
    SDL_GetWindowSize(m_window, &w, &h);
    SDL_GL_GetDrawableSize(m_window, &pw, &ph);
    return (w > 0) ? static_cast<float>(pw) / w : 1.0f;
}

float ysSdlWindow::GetPixelScaleY() const {
    if (m_window == nullptr) return 1.0f;

    int w = 0, h = 0, pw = 0, ph = 0;
    SDL_GetWindowSize(m_window, &w, &h);
    SDL_GL_GetDrawableSize(m_window, &pw, &ph);
    return (h > 0) ? static_cast<float>(ph) / h : 1.0f;
}

unsigned int ysSdlWindow::GetSdlWindowId() const {
    return (m_window != nullptr) ? SDL_GetWindowID(m_window) : 0;
}

void ysSdlWindow::Close() {
    ysWindow::Close();

    if (m_window != nullptr) {
        SDL_HideWindow(m_window);
    }
}

void ysSdlWindow::SetTitle(const char *title) {
    ysWindow::SetTitle(title);

    if (m_window != nullptr) {
        SDL_SetWindowTitle(m_window, title);
    }
}

void ysSdlWindow::SetState(WindowState state) {
    if (m_window == nullptr) return;

    ysWindow::SetState(state);

    switch (state) {
        case WindowState::Visible:
            SDL_ShowWindow(m_window);
            break;
        case WindowState::Hidden:
            SDL_HideWindow(m_window);
            break;
        case WindowState::Maximized:
            SDL_MaximizeWindow(m_window);
            break;
        case WindowState::Minimized:
            SDL_MinimizeWindow(m_window);
            break;
        default:
            break;
    }
}

void ysSdlWindow::AL_SetSize(int width, int height) {
    if (m_window == nullptr) return;

    ysWindow::AL_SetSize(width, height);

    // Sizes passed in are in physical pixels; SDL wants window points
    const float sx = GetPixelScaleX();
    const float sy = GetPixelScaleY();
    SDL_SetWindowSize(
        m_window,
        static_cast<int>(width / sx + 0.5f),
        static_cast<int>(height / sy + 0.5f));
}

void ysSdlWindow::AL_SetLocation(int x, int y) {
    if (m_window == nullptr) return;

    ysWindow::AL_SetLocation(x, y);

    SDL_SetWindowPosition(m_window, x, y);
}
