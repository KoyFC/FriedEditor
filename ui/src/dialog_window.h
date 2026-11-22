#pragma once

#include <SDL.h>

namespace editor_ui
{
    // Asks the window manager to keep the window above its parent, as a dialog is, and only above it. Only the X11 hint for it is set, since SDL has no call that does this without also making the parent unusable. It is called until it says the window is done, and again when SDL shows the window, as SDL sets a hint of its own then. It does nothing on other systems, or when FRIED_EDITOR_NO_DIALOG_HINT is set.
    // False when the window is not there yet.
    bool keepDialogOf(SDL_Window *dialog, SDL_Window *parent);
}
