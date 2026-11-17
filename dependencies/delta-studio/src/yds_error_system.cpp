#include "../include/yds_error_system.h"

#include "../include/yds_error_handler.h"

#include <assert.h>

ysErrorSystem *ysErrorSystem::g_instance = nullptr;
thread_local int ysErrorSystem::s_stackLevel = 0;
thread_local const char *ysErrorSystem::s_callStack[ysErrorSystem::MAX_STACK_LEVEL] = {};

ysErrorSystem::ysErrorSystem() {
    if (g_instance != nullptr) {
        YDS_ERROR_RAISE(ysError::MultipleErrorSystems);
    }

    g_instance = this;
}

ysErrorSystem::~ysErrorSystem() {
    /* void */
}

ysErrorSystem *ysErrorSystem::GetInstance() {
    if (g_instance == nullptr) g_instance = new ysErrorSystem;
    return g_instance;
}

void ysErrorSystem::Destroy() {
    delete g_instance;      // was delete[] on a single object
    g_instance = nullptr;
}

ysError ysErrorSystem::RaiseError(ysError error, unsigned int line, ysObject *object, const char *file, const char *msg, bool affectStack) {
    if (error != ysError::None) {
        for (int i = 0; i < m_errorHandlers.GetNumObjects(); i++) {
            m_errorHandlers.Get(i)->OnErrorMessage(error, line, object, file, msg);
        }
    }

    if (affectStack) {
        StackDescend();
    }

    return error;
}

void ysErrorSystem::StackRaise(const char *callName) {
    if (s_stackLevel >= 0 && s_stackLevel < MAX_STACK_LEVEL) {
        s_callStack[s_stackLevel] = callName;
    }
    s_stackLevel++;
}

void ysErrorSystem::StackDescend() {
    if (s_stackLevel > 0) --s_stackLevel;
}
