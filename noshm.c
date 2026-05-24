// Intercept XShmQueryExtension to force Mesa to use XPutImage instead of XShmPutImage.
// XQuartz on macOS/Apple Silicon doesn't properly support MIT-SHM.
#include <stdbool.h>
typedef void Display;
typedef int Bool;
typedef int Status;

// Override all SHM functions to report SHM as unavailable
Bool XShmQueryExtension(Display *display) { return 0; }
Status XShmQueryVersion(Display *display, int *major, int *minor, Bool *pixmaps) { return 0; }
