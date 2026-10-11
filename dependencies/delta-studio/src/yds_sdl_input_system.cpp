#include "../include/yds_sdl_input_system.h"

#include "../include/yds_sdl_window.h"

#include <SDL.h>

#include <cmath>
#include <cstdio>

namespace {

    // Maps SDL key codes onto the engine's key codes. Key codes (rather than
    // scan codes) are used so that letters follow the active keyboard layout,
    // which matches the behaviour of Win32 virtual key codes.
    ysKey::Code translateKey(SDL_Keycode key) {
        if (key >= SDLK_a && key <= SDLK_z) {
            return static_cast<ysKey::Code>(static_cast<int>(ysKey::Code::A) + (key - SDLK_a));
        }
        else if (key >= SDLK_0 && key <= SDLK_9) {
            return static_cast<ysKey::Code>(static_cast<int>(ysKey::Code::N0) + (key - SDLK_0));
        }
        else if (key >= SDLK_F1 && key <= SDLK_F12) {
            return static_cast<ysKey::Code>(static_cast<int>(ysKey::Code::F1) + (key - SDLK_F1));
        }
        else if (key >= SDLK_F13 && key <= SDLK_F24) {
            return static_cast<ysKey::Code>(static_cast<int>(ysKey::Code::F13) + (key - SDLK_F13));
        }
        else if (key >= SDLK_KP_1 && key <= SDLK_KP_9) {
            return static_cast<ysKey::Code>(static_cast<int>(ysKey::Code::Numpad1) + (key - SDLK_KP_1));
        }

        switch (key) {
            case SDLK_BACKSPACE: return ysKey::Code::Back;
            case SDLK_TAB: return ysKey::Code::Tab;
            case SDLK_CLEAR: return ysKey::Code::Clear;
            case SDLK_RETURN: return ysKey::Code::Return;
            case SDLK_RETURN2: return ysKey::Code::Return;
            case SDLK_KP_ENTER: return ysKey::Code::Return;
            case SDLK_PAUSE: return ysKey::Code::Pause;
            case SDLK_CAPSLOCK: return ysKey::Code::CapsLock;
            case SDLK_ESCAPE: return ysKey::Code::Escape;
            case SDLK_SPACE: return ysKey::Code::Space;
            case SDLK_PAGEUP: return ysKey::Code::PageUp;
            case SDLK_PAGEDOWN: return ysKey::Code::PageDown;
            case SDLK_END: return ysKey::Code::End;
            case SDLK_HOME: return ysKey::Code::Home;
            case SDLK_LEFT: return ysKey::Code::Left;
            case SDLK_UP: return ysKey::Code::Up;
            case SDLK_RIGHT: return ysKey::Code::Right;
            case SDLK_DOWN: return ysKey::Code::Down;
            case SDLK_SELECT: return ysKey::Code::Select;
            case SDLK_PRINTSCREEN: return ysKey::Code::PrintScreen;
            case SDLK_INSERT: return ysKey::Code::Insert;
            case SDLK_DELETE: return ysKey::Code::Delete;
            case SDLK_HELP: return ysKey::Code::Help;
            case SDLK_LGUI: return ysKey::Code::LeftWin;
            case SDLK_RGUI: return ysKey::Code::RightWin;
            case SDLK_APPLICATION: return ysKey::Code::Apps;
            case SDLK_SLEEP: return ysKey::Code::Sleep;
            case SDLK_KP_0: return ysKey::Code::Numpad0;
            case SDLK_KP_MULTIPLY: return ysKey::Code::Multiply;
            case SDLK_KP_PLUS: return ysKey::Code::Add;
            case SDLK_SEPARATOR: return ysKey::Code::Separator;
            case SDLK_KP_MINUS: return ysKey::Code::Subtract;
            case SDLK_KP_PERIOD: return ysKey::Code::Decimal;
            case SDLK_KP_DECIMAL: return ysKey::Code::Decimal;
            case SDLK_KP_DIVIDE: return ysKey::Code::Divide;
            case SDLK_NUMLOCKCLEAR: return ysKey::Code::NumLock;
            case SDLK_SCROLLLOCK: return ysKey::Code::ScrollLock;
            case SDLK_LSHIFT: return ysKey::Code::LeftShift;
            case SDLK_RSHIFT: return ysKey::Code::RightShift;
            case SDLK_LCTRL: return ysKey::Code::LeftControl;
            case SDLK_RCTRL: return ysKey::Code::RightControl;
            case SDLK_LALT: return ysKey::Code::LeftMenu;
            case SDLK_RALT: return ysKey::Code::RightMenu;
            case SDLK_AC_BACK: return ysKey::Code::BrowserBack;
            case SDLK_AC_FORWARD: return ysKey::Code::BrowserForward;
            case SDLK_AC_REFRESH: return ysKey::Code::BrowserRefresh;
            case SDLK_AC_STOP: return ysKey::Code::BrowserStop;
            case SDLK_AC_SEARCH: return ysKey::Code::BrowserSearch;
            case SDLK_AC_BOOKMARKS: return ysKey::Code::BrowserFavorites;
            case SDLK_AC_HOME: return ysKey::Code::BrowserHome;
            case SDLK_MUTE: return ysKey::Code::VolumeMute;
            case SDLK_VOLUMEDOWN: return ysKey::Code::VolumeDown;
            case SDLK_VOLUMEUP: return ysKey::Code::VolumeUp;
            case SDLK_AUDIONEXT: return ysKey::Code::MediaNextTrack;
            case SDLK_AUDIOPREV: return ysKey::Code::MediaPrevTrack;
            case SDLK_AUDIOSTOP: return ysKey::Code::MediaStop;
            case SDLK_AUDIOPLAY: return ysKey::Code::MediaPlay;
            case SDLK_MAIL: return ysKey::Code::LaunchMail;
            case SDLK_MEDIASELECT: return ysKey::Code::LaunchMediaSelect;
            case SDLK_EQUALS: return ysKey::Code::OEM_Plus;
            case SDLK_PLUS: return ysKey::Code::OEM_Plus;
            case SDLK_COMMA: return ysKey::Code::OEM_Comma;
            case SDLK_MINUS: return ysKey::Code::OEM_Minus;
            case SDLK_PERIOD: return ysKey::Code::OEM_Period;
            case SDLK_SEMICOLON: return ysKey::Code::OEM_1;
            case SDLK_SLASH: return ysKey::Code::OEM_2;
            case SDLK_BACKQUOTE: return ysKey::Code::OEM_3;
            case SDLK_LEFTBRACKET: return ysKey::Code::OEM_4;
            case SDLK_BACKSLASH: return ysKey::Code::OEM_5;
            case SDLK_RIGHTBRACKET: return ysKey::Code::OEM_6;
            case SDLK_QUOTE: return ysKey::Code::OEM_7;
            default: return ysKey::Code::Undefined;
        }
    }

    // Win32 reports a generic virtual key for modifiers in addition to the
    // left/right specific one; the engine's code relies on the generic one.
    ysKey::Code genericModifier(ysKey::Code code) {
        switch (code) {
            case ysKey::Code::LeftShift:
            case ysKey::Code::RightShift:
                return ysKey::Code::Shift;
            case ysKey::Code::LeftControl:
            case ysKey::Code::RightControl:
                return ysKey::Code::Control;
            case ysKey::Code::LeftMenu:
            case ysKey::Code::RightMenu:
                return ysKey::Code::Menu;
            default:
                return ysKey::Code::Undefined;
        }
    }

    ysKey::Variation keyVariation(ysKey::Code code) {
        switch (code) {
            case ysKey::Code::LeftShift:
            case ysKey::Code::LeftControl:
            case ysKey::Code::LeftMenu:
                return ysKey::Variation::Left;
            case ysKey::Code::RightShift:
            case ysKey::Code::RightControl:
            case ysKey::Code::RightMenu:
                return ysKey::Variation::Right;
            default:
                return ysKey::Variation::Undefined;
        }
    }

    bool translateButton(Uint8 button, ysMouse::Button *out) {
        switch (button) {
            case SDL_BUTTON_LEFT: *out = ysMouse::Button::Left; return true;
            case SDL_BUTTON_RIGHT: *out = ysMouse::Button::Right; return true;
            case SDL_BUTTON_MIDDLE: *out = ysMouse::Button::Middle; return true;
            case SDL_BUTTON_X1: *out = ysMouse::Button::Aux_1; return true;
            case SDL_BUTTON_X2: *out = ysMouse::Button::Aux_2; return true;
            default: return false;
        }
    }

    // Matches the Win32 WHEEL_DELTA constant that the engine was tuned for
    constexpr float WheelDelta = 120.0f;

} /* namespace */

ysSdlInputSystem::ysSdlInputSystem() : ysInputSystem(Platform::Sdl) {
    m_keyboardDevice = nullptr;
    m_mouseDevice = nullptr;
    m_wheelRemainder = 0.0f;
}

ysSdlInputSystem::~ysSdlInputSystem() {
    /* void */
}

ysError ysSdlInputSystem::CreateDevices() {
    YDS_ERROR_DECLARE("CreateDevices");

    m_keyboardDevice = CreateDevice(ysInputDevice::InputDeviceType::KEYBOARD, 0);
    m_keyboardDevice->SetGeneric(false);
    m_keyboardDevice->SetConnected(true);

    m_mouseDevice = CreateDevice(ysInputDevice::InputDeviceType::MOUSE, 0);
    m_mouseDevice->SetGeneric(false);
    m_mouseDevice->SetConnected(true);

    return YDS_ERROR_RETURN(ysError::None);
}

ysInputDevice *ysSdlInputSystem::CreateDevice(ysInputDevice::InputDeviceType type, int id) {
    const char *baseName = "";
    switch (type) {
        case ysInputDevice::InputDeviceType::CUSTOM: baseName = "HID"; break;
        case ysInputDevice::InputDeviceType::KEYBOARD: baseName = "KEYBOARD"; break;
        case ysInputDevice::InputDeviceType::MOUSE: baseName = "MOUSE"; break;
        default: break;
    }

    const int deviceId = (id >= 0) ? id : GetNextDeviceID(type);

    char deviceName[ysInputDevice::MAX_NAME_LENGTH];
    std::snprintf(deviceName, sizeof(deviceName), "%s%03d", baseName, deviceId);

    ysInputDevice *newDevice = m_inputDeviceArray.NewGeneric<ysInputDevice>();
    newDevice->SetName(deviceName);
    newDevice->SetType(type);
    newDevice->SetDeviceID(deviceId);
    newDevice->SetGeneric(true);

    RegisterDevice(newDevice);

    return newDevice;
}

bool ysSdlInputSystem::ProcessEvent(const SDL_Event &event, ysSdlWindow *window) {
    ysKeyboard *keyboard = (m_keyboardDevice != nullptr)
        ? m_keyboardDevice->GetAsKeyboard()
        : nullptr;
    ysMouse *mouse = (m_mouseDevice != nullptr)
        ? m_mouseDevice->GetAsMouse()
        : nullptr;

    const float scaleX = (window != nullptr) ? window->GetPixelScaleX() : 1.0f;
    const float scaleY = (window != nullptr) ? window->GetPixelScaleY() : 1.0f;

    switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            if (keyboard == nullptr) return false;

            // Auto-repeat would otherwise re-trigger "pressed" transitions
            if (event.key.repeat != 0) return true;

            const ysKey::Code code = translateKey(event.key.keysym.sym);
            if (code == ysKey::Code::Undefined) return false;

            const ysKey::State state = (event.type == SDL_KEYDOWN)
                ? ysKey::State::DownTransition
                : ysKey::State::UpTransition;
            const ysKey::Variation variation = keyVariation(code);

            // If the press hasn't been seen by the application yet, hold the
            // release until the next frame so that short taps aren't lost
            if (state == ysKey::State::UpTransition
                && keyboard->GetKey(code)->m_state == ysKey::State::DownTransition)
            {
                m_deferredKeyReleases.push_back(code);
            }
            else {
                keyboard->SetKeyState(code, state, variation);
            }

            const ysKey::Code generic = genericModifier(code);
            if (generic != ysKey::Code::Undefined) {
                const SDL_Keymod mods = SDL_GetModState();
                bool stillDown = false;
                if (generic == ysKey::Code::Shift) stillDown = (mods & KMOD_SHIFT) != 0;
                else if (generic == ysKey::Code::Control) stillDown = (mods & KMOD_CTRL) != 0;
                else if (generic == ysKey::Code::Menu) stillDown = (mods & KMOD_ALT) != 0;

                if (event.type == SDL_KEYDOWN) {
                    if (!keyboard->IsKeyDown(generic)) {
                        keyboard->SetKeyState(generic, state, variation);
                    }
                }
                else if (!stillDown) {
                    keyboard->SetKeyState(generic, state, variation);
                }
            }

            return true;
        }
        case SDL_MOUSEMOTION:
            if (mouse == nullptr) return false;

            mouse->SetOsPosition(
                static_cast<int>(event.motion.x * scaleX),
                static_cast<int>(event.motion.y * scaleY));
            mouse->UpdatePosition(
                static_cast<int>(event.motion.xrel * scaleX),
                static_cast<int>(event.motion.yrel * scaleY),
                true);
            return true;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            if (mouse == nullptr) return false;

            ysMouse::Button button;
            if (!translateButton(event.button.button, &button)) return false;

            mouse->SetOsPosition(
                static_cast<int>(event.button.x * scaleX),
                static_cast<int>(event.button.y * scaleY));
            if (event.type == SDL_MOUSEBUTTONUP && mouse->IsDown(button) && !mouse->ProcessMouseButton(button, ysMouse::ButtonState::Down)) {
                // Still in the down transition; release on the next frame
                m_deferredButtonReleases.push_back(button);
            }
            else {
                mouse->UpdateButton(
                    button,
                    (event.type == SDL_MOUSEBUTTONDOWN)
                        ? ysMouse::ButtonState::DownTransition
                        : ysMouse::ButtonState::UpTransition);
            }
            return true;
        }
        case SDL_MOUSEWHEEL:
        {
            if (mouse == nullptr) return false;

#if SDL_VERSION_ATLEAST(2, 0, 18)
            float amount = event.wheel.preciseY;
#else
            float amount = static_cast<float>(event.wheel.y);
#endif
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) amount = -amount;

            // Trackpads report many small fractional deltas; accumulate them
            // so that slow scrolling is not lost to integer truncation
            m_wheelRemainder += amount * WheelDelta;
            const float whole = std::trunc(m_wheelRemainder);
            m_wheelRemainder -= whole;

            mouse->UpdateWheel(static_cast<int>(whole));
            return true;
        }
        default:
            return false;
    }
}

void ysSdlInputSystem::BeginFrame() {
    if (m_keyboardDevice != nullptr) {
        ysKeyboard *keyboard = m_keyboardDevice->GetAsKeyboard();
        for (const ysKey::Code code : m_deferredKeyReleases) {
            keyboard->SetKeyState(code, ysKey::State::UpTransition, ysKey::Variation::Undefined);
        }
    }

    if (m_mouseDevice != nullptr) {
        ysMouse *mouse = m_mouseDevice->GetAsMouse();
        for (const ysMouse::Button button : m_deferredButtonReleases) {
            mouse->UpdateButton(button, ysMouse::ButtonState::UpTransition);
        }
    }

    m_deferredKeyReleases.clear();
    m_deferredButtonReleases.clear();
}

void ysSdlInputSystem::ReleaseAll() {
    m_deferredKeyReleases.clear();
    m_deferredButtonReleases.clear();

    if (m_keyboardDevice != nullptr) {
        ysKeyboard *keyboard = m_keyboardDevice->GetAsKeyboard();
        for (int i = 0; i < static_cast<int>(ysKey::Code::Count); ++i) {
            const ysKey::Code code = static_cast<ysKey::Code>(i);
            if (keyboard->IsKeyDown(code)) {
                keyboard->SetKeyState(code, ysKey::State::UpTransition, ysKey::Variation::Undefined);
            }
        }
    }

    if (m_mouseDevice != nullptr) {
        ysMouse *mouse = m_mouseDevice->GetAsMouse();
        for (int i = 0; i < static_cast<int>(ysMouse::Button::Count); ++i) {
            const ysMouse::Button button = static_cast<ysMouse::Button>(i);
            if (mouse->IsDown(button)) {
                mouse->UpdateButton(button, ysMouse::ButtonState::UpTransition);
            }
        }
    }
}
