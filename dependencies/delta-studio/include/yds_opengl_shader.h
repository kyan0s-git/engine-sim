#ifndef YDS_OPENGL_SHADER_H
#define YDS_OPENGL_SHADER_H

#include "yds_shader.h"

#include <string>
#include <vector>

class ysOpenGLShader : public ysShader {
    friend class ysOpenGLDevice;

public:
    ysOpenGLShader();
    virtual ~ysOpenGLShader();

    struct Binding {
        std::string Name;
        int Slot = 0;
        bool IsSampler = false;
    };

protected:
    unsigned int m_handle;

    // Explicit binding points declared in the shader source
    std::vector<Binding> m_bindings;
};

#endif /* YDS_OPENGL_SHADER_H */
