#include "../include/yds_opengl_sdl_context.h"

#include "../include/yds_opengl_device.h"
#include "../include/yds_sdl_window.h"

#include <SDL.h>

ysOpenGLSdlContext::ysOpenGLSdlContext() : ysOpenGLVirtualContext(ysWindowSystemObject::Platform::Sdl) {
    m_context = nullptr;
    m_device = nullptr;
}

ysOpenGLSdlContext::~ysOpenGLSdlContext() {
    /* void */
}

ysError ysOpenGLSdlContext::CreateRenderingContext(ysOpenGLDevice *device, ysWindow *window, int major, int minor) {
    YDS_ERROR_DECLARE("CreateRenderingContext");

    if (window->GetPlatform() != ysWindowSystemObject::Platform::Sdl) {
        return YDS_ERROR_RETURN(ysError::IncompatiblePlatforms);
    }

    ysSdlWindow *sdlWindow = static_cast<ysSdlWindow *>(window);

    m_device = nullptr;
    m_targetWindow = nullptr;
    m_context = nullptr;
    m_isRealContext = false;

    // Only one real context is shared between all windows
    if (device->UpdateContext() == nullptr) {
        // Try the requested version first then progressively older core
        // profiles. macOS caps out at 4.1 (forward compatible core profile).
        const int versions[][2] = {
            { major, minor },
            { 4, 3 },
            { 4, 1 }
        };

        SDL_GLContext context = nullptr;
        for (const auto &version : versions) {
            if (version[0] > major || (version[0] == major && version[1] > minor)) continue;

            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, version[0]);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, version[1]);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

            context = SDL_GL_CreateContext(sdlWindow->GetSdlWindow());
            if (context != nullptr) break;
        }

        if (context == nullptr) {
            return YDS_ERROR_RETURN_MSG(ysError::CouldNotCreateContext, SDL_GetError());
        }

        if (SDL_GL_MakeCurrent(sdlWindow->GetSdlWindow(), context) != 0) {
            SDL_GL_DeleteContext(context);
            return YDS_ERROR_RETURN(ysError::CouldNotActivateContext);
        }

        m_context = context;
        m_isRealContext = true;
    }

    m_device = device;
    m_targetWindow = window;

    if (!LoadAllExtensions()) {
        return YDS_ERROR_RETURN_MSG(ysError::CouldNotCreateContext, "Required OpenGL functions are unavailable");
    }

    device->UpdateContext();

    // Vsync keeps the UI thread from spinning; the simulation runs on its own
    // timing so this does not affect audio. Adaptive vsync is preferred.
    if (SDL_GL_SetSwapInterval(-1) != 0) {
        SDL_GL_SetSwapInterval(1);
    }

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysOpenGLSdlContext::DestroyContext() {
    YDS_ERROR_DECLARE("DestroyContext");

    if (m_context != nullptr) {
        SDL_GL_MakeCurrent(nullptr, nullptr);
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(m_context));
        m_context = nullptr;
    }

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysOpenGLSdlContext::TransferContext(ysOpenGLVirtualContext *context) {
    YDS_ERROR_DECLARE("TransferContext");

    if (context->GetPlatform() != ysWindowSystemObject::Platform::Sdl) {
        return YDS_ERROR_RETURN(ysError::IncompatiblePlatforms);
    }

    ysOpenGLSdlContext *sdlContext = static_cast<ysOpenGLSdlContext *>(context);
    sdlContext->m_context = m_context;
    sdlContext->m_isRealContext = true;

    m_isRealContext = false;
    m_context = nullptr;

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysOpenGLSdlContext::SetContextMode(ContextMode mode) {
    YDS_ERROR_DECLARE("SetContextMode");

    ysWindow *window = GetWindow();
    if (window == nullptr) return YDS_ERROR_RETURN(ysError::NoContext);

    if (mode == ContextMode::Fullscreen) {
        window->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
    }
    else if (mode == ContextMode::Windowed) {
        window->SetWindowStyle(ysWindow::WindowStyle::Windowed);
    }

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysOpenGLSdlContext::SetContext(ysRenderingContext *realContext) {
    YDS_ERROR_DECLARE("SetContext");

    if (realContext != nullptr) {
        ysOpenGLSdlContext *real = static_cast<ysOpenGLSdlContext *>(realContext);
        ysSdlWindow *window = static_cast<ysSdlWindow *>(m_targetWindow);

        if (window == nullptr || window->GetSdlWindow() == nullptr) {
            return YDS_ERROR_RETURN_MSG(ysError::CouldNotActivateContext, "Context has no target window");
        }

        if (SDL_GL_MakeCurrent(window->GetSdlWindow(), static_cast<SDL_GLContext>(real->m_context)) != 0) {
            return YDS_ERROR_RETURN_MSG(ysError::CouldNotActivateContext, SDL_GetError());
        }
    }
    else {
        SDL_GL_MakeCurrent(nullptr, nullptr);
    }

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysOpenGLSdlContext::Present() {
    YDS_ERROR_DECLARE("Present");

    ysSdlWindow *window = static_cast<ysSdlWindow *>(m_targetWindow);
    if (window == nullptr || window->GetSdlWindow() == nullptr) {
        return YDS_ERROR_RETURN(ysError::NoContext);
    }

    SDL_GL_SwapWindow(window->GetSdlWindow());

    return YDS_ERROR_RETURN(ysError::None);
}

void ysOpenGLSdlContext::SetVsync(bool enabled) {
    if (enabled) {
        if (SDL_GL_SetSwapInterval(-1) != 0) SDL_GL_SetSwapInterval(1);
    }
    else {
        SDL_GL_SetSwapInterval(0);
    }
}

bool ysOpenGLSdlContext::LoadAllExtensions() {
    bool success = true;

#define YDS_LOAD_GL(name) \
    name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress(#name)); \
    success = success && (name != nullptr)

    YDS_LOAD_GL(glGenBuffers);
    YDS_LOAD_GL(glDeleteBuffers);
    YDS_LOAD_GL(glDeleteVertexArrays);
    YDS_LOAD_GL(glBindBuffer);
    YDS_LOAD_GL(glBindBufferRange);
    YDS_LOAD_GL(glBufferData);
    YDS_LOAD_GL(glGenVertexArrays);
    YDS_LOAD_GL(glBindVertexArray);
    YDS_LOAD_GL(glEnableVertexAttribArray);
    YDS_LOAD_GL(glVertexAttribPointer);
    YDS_LOAD_GL(glVertexAttribIPointer);

    YDS_LOAD_GL(glVertexAttrib3f);
    YDS_LOAD_GL(glVertexAttrib4f);

    YDS_LOAD_GL(glDeleteProgram);
    YDS_LOAD_GL(glDeleteShader);

    YDS_LOAD_GL(glCreateShader);
    YDS_LOAD_GL(glShaderSource);
    YDS_LOAD_GL(glCompileShader);
    YDS_LOAD_GL(glCreateProgram);
    YDS_LOAD_GL(glAttachShader);
    YDS_LOAD_GL(glDetachShader);
    YDS_LOAD_GL(glLinkProgram);
    YDS_LOAD_GL(glUseProgram);
    YDS_LOAD_GL(glBindAttribLocation);
    YDS_LOAD_GL(glBindFragDataLocation);
    YDS_LOAD_GL(glGetFragDataLocation);
    YDS_LOAD_GL(glGetUniformLocation);
    YDS_LOAD_GL(glGetShaderiv);
    YDS_LOAD_GL(glGetShaderInfoLog);
    YDS_LOAD_GL(glDrawBuffers);

    YDS_LOAD_GL(glUniform4f);
    YDS_LOAD_GL(glUniform4fv);
    YDS_LOAD_GL(glUniform3fv);
    YDS_LOAD_GL(glUniform2fv);
    YDS_LOAD_GL(glUniform3f);
    YDS_LOAD_GL(glUniform2f);
    YDS_LOAD_GL(glUniform1f);
    YDS_LOAD_GL(glUniform1i);

    YDS_LOAD_GL(glUniformMatrix4fv);
    YDS_LOAD_GL(glUniformMatrix3fv);

    YDS_LOAD_GL(glGetProgramiv);
    YDS_LOAD_GL(glGetActiveUniformName);
    YDS_LOAD_GL(glGetActiveUniformsiv);
    YDS_LOAD_GL(glGetActiveUniform);

    YDS_LOAD_GL(glDrawElementsBaseVertex);

    YDS_LOAD_GL(glTexImage2DMultisample);
    YDS_LOAD_GL(glActiveTexture);
    YDS_LOAD_GL(glGenerateMipmap);

    YDS_LOAD_GL(glMapBuffer);
    YDS_LOAD_GL(glMapBufferRange);
    YDS_LOAD_GL(glUnmapBuffer);

    YDS_LOAD_GL(glGenRenderbuffers);
    YDS_LOAD_GL(glDeleteRenderbuffers);
    YDS_LOAD_GL(glBindRenderbuffer);
    YDS_LOAD_GL(glRenderbufferStorage);
    YDS_LOAD_GL(glRenderbufferStorageMultisample);

    YDS_LOAD_GL(glCopyBufferSubData);
    YDS_LOAD_GL(glBufferSubData);

    YDS_LOAD_GL(glGenFramebuffers);
    YDS_LOAD_GL(glDeleteFramebuffers);
    YDS_LOAD_GL(glBindFramebuffer);
    YDS_LOAD_GL(glFramebufferTexture2D);
    YDS_LOAD_GL(glFramebufferRenderbuffer);
    YDS_LOAD_GL(glCheckFramebufferStatus);

    YDS_LOAD_GL(glBlitFramebuffer);
    YDS_LOAD_GL(glBlendEquation);

    YDS_LOAD_GL(glGetUniformBlockIndex);
    YDS_LOAD_GL(glUniformBlockBinding);
    YDS_LOAD_GL(glGetProgramInfoLog);

#undef YDS_LOAD_GL

    return success;
}
