#include "dialog_window.h"

#include <SDL_syswm.h>

#include <cstdlib>

#ifdef FRIED_EDITOR_X11
#include <X11/Xlib.h>
#endif

namespace editor_ui
{
    bool keepDialogOf(SDL_Window *dialog, SDL_Window *parent)
    {
        static const bool s_isOff = std::getenv("FRIED_EDITOR_NO_DIALOG_HINT") != nullptr;
        if (s_isOff)
        {
            return true;
        }
#ifdef FRIED_EDITOR_X11
        SDL_SysWMinfo dialogInfo;
        SDL_SysWMinfo parentInfo;
        SDL_VERSION(&dialogInfo.version);
        SDL_VERSION(&parentInfo.version);
        if (!SDL_GetWindowWMInfo(dialog, &dialogInfo) || !SDL_GetWindowWMInfo(parent, &parentInfo) || dialogInfo.subsystem != SDL_SYSWM_X11
            || parentInfo.subsystem != SDL_SYSWM_X11 || dialogInfo.info.x11.window == 0 || parentInfo.info.x11.window == 0)
        {
            return false;
        }
        // The display SDL reports is not always usable from here, so the hint is set over a connection of the editor's own.
        static Display *s_display = XOpenDisplay(nullptr);
        if (!s_display)
        {
            return true;
        }
        Window current = 0;
        const Window wanted = (Window)parentInfo.info.x11.window;
        // SDL sets a hint of its own when it shows a window, so what is there is looked at and put right again.
        if (!XGetTransientForHint(s_display, (Window)dialogInfo.info.x11.window, &current) || current != wanted)
        {
            XSetTransientForHint(s_display, (Window)dialogInfo.info.x11.window, wanted);
            XFlush(s_display);
        }
#else
        (void)dialog;
        (void)parent;
#endif
        return true;
    }
}
