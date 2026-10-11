#ifndef YDS_SDL_INPUT_SYSTEM_H
#define YDS_SDL_INPUT_SYSTEM_H

#include "yds_input_system.h"

#include <vector>

union SDL_Event;

class ysSdlWindow;

// Input system driven by SDL events. SDL merges all physical keyboards and
// mice into a single logical device each, which is all the engine needs.
class ysSdlInputSystem : public ysInputSystem {
public:
    ysSdlInputSystem();
    virtual ~ysSdlInputSystem();

    // Returns true if the event was consumed
    bool ProcessEvent(const SDL_Event &event, ysSdlWindow *window);

    // Releases every key/button; used when the window loses focus so that
    // keys are not left "stuck" down
    void ReleaseAll();

    // Called once per frame before events are processed. Applies releases
    // that were deferred because the matching press had not been observed
    // by the application yet (e.g. a quick tap at a low framerate).
    void BeginFrame();

protected:
    virtual ysError CreateDevices() override;
    virtual ysInputDevice *CreateDevice(ysInputDevice::InputDeviceType type, int id) override;

    ysInputDevice *m_keyboardDevice;
    ysInputDevice *m_mouseDevice;

    // Accumulates fractional wheel motion from high-resolution trackpads
    float m_wheelRemainder;

    std::vector<ysKey::Code> m_deferredKeyReleases;
    std::vector<ysMouse::Button> m_deferredButtonReleases;
};

#endif /* YDS_SDL_INPUT_SYSTEM_H */
