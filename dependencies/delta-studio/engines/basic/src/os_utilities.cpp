#include "../include/os_utilities.h"

#include "../include/path.h"

#include <SDL.h>

#include <string>

dbasic::Path dbasic::GetModulePath() {
    // SDL resolves the executable's directory on every platform (including
    // inside a macOS .app bundle where it points at Contents/Resources)
    char *basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        return Path(".");
    }

    std::string path(basePath);
    SDL_free(basePath);

    // SDL returns the directory with a trailing separator; strip it so that
    // parent path queries behave as expected
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }

    return Path(path);
}
