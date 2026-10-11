#ifndef YDS_SDL_WINDOW_H
#define YDS_SDL_WINDOW_H

#include "yds_window.h"

struct SDL_Window;

class ysSdlWindowSystem;

class ysSdlWindow : public ysWindow {
    friend ysSdlWindowSystem;

public:
    ysSdlWindow();
    virtual ~ysSdlWindow();

    virtual ysError InitializeWindow(
        ysWindow *parent,
        const char *title,
        WindowStyle style,
        int x, int y,
        int width, int height,
        ysMonitor *monitor) override;
    virtual ysError InitializeWindow(
        ysWindow *parent,
        const char *title,
        WindowStyle style,
        ysMonitor *monitor) override;

    virtual bool SetWindowStyle(WindowStyle style) override;

    // SDL already reports positions in window coordinates. Delta expects a
    // bottom-left origin in physical pixels so we only need to flip/scale.
    virtual void ScreenToLocal(int &x, int &y) const override;

    virtual bool IsVisible() override;

    // Size of the drawable area in physical pixels (differs from the window
    // size in points on high-DPI displays such as Retina screens)
    virtual int GetScreenWidth() const override;
    virtual int GetScreenHeight() const override;

    // Ratio of physical pixels to window points
    float GetPixelScaleX() const;
    float GetPixelScaleY() const;

    SDL_Window *GetSdlWindow() const { return m_window; }
    unsigned int GetSdlWindowId() const;

protected:
    virtual void Close() override;
    virtual void SetTitle(const char *title) override;
    virtual void SetState(WindowState state = WindowState::Visible) override;

    virtual void AL_SetSize(int width, int height) override;
    virtual void AL_SetLocation(int x, int y) override;

protected:
    SDL_Window *m_window;
};

#endif /* YDS_SDL_WINDOW_H */
