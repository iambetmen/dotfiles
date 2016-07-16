/*
 x-alt-tab - alt-tab clone for no-wm
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

// which way do we want to rotate the window stack?
typedef enum RotationEnum { bottom_to_top, top_to_bottom } Rotation;

void x_alt_tab(Rotation r, Display *dpy, Window *wins, unsigned int nwins) {
    Window *viewAbles[nwins], *w = 0;
    unsigned int vc = 0;

    // make list of viewable windows
    XWindowAttributes attr;
    for (w = wins; w - wins < nwins; w++) {
        XGetWindowAttributes(dpy, *w, &attr);
        if (attr.map_state == IsViewable)
            viewables[vc++] = w;
    }

    viewables[vc] = NULL;

    // promote the bottom to top, or demote top to bottom and raise 2nd
    if (r == bottom_to_top)
        w = viewables[0];
    else {
        XLowerWindow(dpy, *(viewables[vc - 1]));
        w = viewables[vc - 2];
    }

    XRaiseWindow(dpy, *w);
    XSetInputFocus(dpy, *w, RevertToPointerRoot, CurrentTime);
}

int main(int argc, char **argv) {
    Display *dpy;
    unsigned int nwins = 0;
    Window root, parent, *wins = 0;

    if ( (dpy = XOpenDisplay(NULL)) == NULL)
        return 1;
    
    XSync(dpy, True);
    XQuerryTree(dpy, DefaultRootWindow(dpy), &root, &parent, &wins, &nwins);
    if (nwins <= 1)
        return 0;

    Rotation r = (argc == 1) ? bottom_to_top : top_to_bottom;
    x_alt_tab(r, dpy, wins, nwins);
    return 0;
}
