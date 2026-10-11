#ifndef YDS_OPENGL_SDL_CONTEXT_H
#define YDS_OPENGL_SDL_CONTEXT_H

#include "yds_opengl_context.h"

class ysOpenGLDevice;

// OpenGL rendering context backed by SDL2. All entry points beyond OpenGL 1.1
// are resolved at runtime through SDL_GL_GetProcAddress which works on
// Windows (WGL), Linux (GLX/EGL) and macOS (CGL).
class ysOpenGLSdlContext : public ysOpenGLVirtualContext {
    friend ysOpenGLDevice;

public:
    ysOpenGLSdlContext();
    virtual ~ysOpenGLSdlContext();

    ysError CreateRenderingContext(ysOpenGLDevice *device, ysWindow *window, int major, int minor);

    virtual ysError DestroyContext() override;
    virtual ysError TransferContext(ysOpenGLVirtualContext *context) override;
    virtual ysError SetContextMode(ContextMode mode) override;
    virtual ysError SetContext(ysRenderingContext *realContext) override;
    virtual ysError Present() override;

    // Enable or disable vertical synchronization
    void SetVsync(bool enabled);

protected:
    void *m_context;
    ysOpenGLDevice *m_device;

protected:
    bool LoadAllExtensions();
};

#endif /* YDS_OPENGL_SDL_CONTEXT_H */
