#include "../include/os_utilities.h"

#include "../include/path.h"

#include <SDL.h>

dbasic::Path dbasic::GetModulePath() {
    // SDL resolves the executable's directory on every platform (including
    // inside a macOS .app bundle where it points at Contents/Resources)
    char *basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        return Path(".");
    }

    Path result(basePath);
    SDL_free(basePath);

    return result;
}
