/* netclose WINDOW: asks the window manager to close WINDOW
   (_NET_CLOSE_WINDOW, as its close button does): the window need not be
   active, as alt+F4 would need it. */
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

int main( int argc, char** argv )
{
    Display* display;
    XEvent e;
    if ( argc != 2 || !( display = XOpenDisplay( 0 ) ) )
        return 1;
    memset( &e, 0, sizeof( e ) );
    e.xclient.type = ClientMessage;
    e.xclient.window = strtoul( argv[1], 0, 0 );
    e.xclient.message_type = XInternAtom( display, "_NET_CLOSE_WINDOW", False );
    e.xclient.format = 32;
    e.xclient.data.l[0] = CurrentTime;
    e.xclient.data.l[1] = 2;   /* a pager, a tool: no timestamp check */
    XSendEvent( display, DefaultRootWindow( display ), False, SubstructureRedirectMask | SubstructureNotifyMask, &e );
    XCloseDisplay( display );
    return 0;
}
