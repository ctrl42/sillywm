// sillywm
// by stx4

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

Display* dpy;
Window root;
XftFont* font;
int scr, scr_w, scr_h;
bool to_quit = false;
XftColor ren_fg;

static int error_pit(Display* dpy, XErrorEvent* e) { return 0; }

#define TITLE_HEIGHT       18
#define BUTTON_WIDTH       18
#define BORDER_EXT          4

#define BAR_HEIGHT TITLE_HEIGHT + BORDER_EXT
#define BORDER_INNER (BORDER_EXT - 1)

static char* font_name = "Fixedsys Excelsior:pixelsize=15:antialias=false";

#define TITLE_COLOR  0x3C3836
#define BAR_TEXT     0xFBF1C7
#define BORDER_COLOR 0x282828
#define BORDER_DARK  0x504945
#define WINDOW_BACK  0x3C3836

#define MOD_MASK     Mod1Mask

XRenderColor text_fg = {
	.red   = ((BAR_TEXT    >> 16) & 0xFF) * 257,
	.green = ((BAR_TEXT    >>  8) & 0xFF) * 257,
	.blue  = ((BAR_TEXT    >>  0) & 0xFF) * 257,
	.alpha = 0xFFFF 
};

typedef struct {
	char* name;
	char** argv;
} silly_exec;

static silly_exec launch_apps[] = {
	{"feh",(char*[]){"feh","--bg-scale","/home/stx/.wm/bg.jpg",NULL}}
};

void silly_init_apps(void) {
	for (int i = 0; i < (sizeof(launch_apps) / sizeof(launch_apps[0])); i++)
		if (fork() == 0) execvp(launch_apps[i].name, launch_apps[i].argv);
}

typedef struct {
	bool quit;
	int keycode;
	silly_exec app;
} silly_bind;

silly_bind binds[] = {
	{false,XK_Return,{"st",(char*[]){"st",NULL}}},
	{true, XK_q,{NULL,NULL}}
};

void silly_init_binds(void) {
	for (int i = 0; i < sizeof(binds) / sizeof(binds[0]); i++)
		XGrabKey(dpy, XKeysymToKeycode(dpy, binds[i].keycode), MOD_MASK, root, True, GrabModeAsync, GrabModeAsync);
}

void silly_handle_bind(XKeyEvent* ev) {
	KeySym sym = XLookupKeysym(ev, 0);
	for (int i = 0; i < sizeof(binds) / sizeof(binds[0]); i++)
		if (binds[i].keycode == sym) {
			to_quit = binds[i].quit;
			if (to_quit) return;
			if (fork() == 0) execvp(binds[i].app.name, binds[i].app.argv);
			return;
		}
}

typedef struct {
	Window wnd;
	XftDraw* xftdraw;
	GC gc;
} silly_bar;

silly_bar* silly_init_bar(void) {
	silly_bar* bar = calloc(1, sizeof(silly_bar));
	
	bar->wnd = XCreateSimpleWindow(
		dpy, root,
		0, 0, scr_w, BAR_HEIGHT,
		0, 0, TITLE_COLOR
	);
	bar->xftdraw = XftDrawCreate(
		dpy, bar->wnd,
		DefaultVisual(dpy, scr),
		DefaultColormap(dpy, scr)
	);
	XMapWindow(dpy, bar->wnd);
	bar->gc = XCreateGC(dpy, bar->wnd, 0, NULL);

	return bar;
}

void silly_draw_bar(silly_bar* bar, char* string) {
	XSetForeground(dpy, bar->gc, TITLE_COLOR);
	XFillRectangle(dpy, bar->wnd, bar->gc, 0, 0, scr_w, BAR_HEIGHT);

	XSetForeground(dpy, bar->gc, BORDER_COLOR);
	XFillRectangle(dpy, bar->wnd, bar->gc, 0, BAR_HEIGHT - BORDER_EXT, scr_w, BAR_HEIGHT - BORDER_EXT);
	
	XftDrawStringUtf8(bar->xftdraw, &ren_fg, font, 5, (BAR_HEIGHT + 4) / 2, (FcChar8*)string, strlen(string));

	XSetForeground(dpy, bar->gc, BORDER_DARK);
	XDrawLine(dpy, bar->wnd, bar->gc, 0, BAR_HEIGHT - BORDER_EXT, scr_w, BAR_HEIGHT - BORDER_EXT);
	XDrawLine(dpy, bar->wnd, bar->gc, 0, BAR_HEIGHT - 1, scr_w, BAR_HEIGHT - 1);
}

void silly_refresh_bar(silly_bar* bar, Window focus) {
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);

	Atom utf8        = XInternAtom(dpy, "UTF8_STRING",  False);
	Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);

	Atom type;
	int format;
	unsigned long nitems, after;
	char* title = NULL; // unsigned char* prop

	// focus can be None, so instead of handling:
	XErrorHandler h = XSetErrorHandler(error_pit);
	XGetWindowProperty(dpy, focus, net_wm_name, 0, (~0L), False, utf8, &type, &format, &nitems, &after, (unsigned char**)&title);
	if (!title) XFetchName(dpy, focus, &title);
	XSetErrorHandler(h);

	char status[128];
	sprintf(status,
		"%s || %02d:%02d | %02d/%02d/%02d | %d",
		title ? title : "(desk)",
		tm->tm_hour, tm->tm_min,
		tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900,
		tm->tm_yday
	);
	silly_draw_bar(bar, status);
	if (title) XFree(title);
}

void silly_destroy_bar(silly_bar* bar) {
	XFreeGC(dpy, bar->gc);
	XDestroyWindow(dpy, bar->wnd);
	free(bar);
}

static char* close_button_xpm[] = {
	"18 18 2 1",
	"X c #FBF1C7",
	". c #3C3836",
	"..................",
	"..................",
	"..................",
	"..................",
	"....XX......XX....",
	"....XXX....XXX....",
	".....XXX..XXX.....",
	"......XXXXXX......",
	".......XXXX.......",
	".......XXXX.......",
	"......XXXXXX......",
	".....XXX..XXX.....",
	"....XXX....XXX....",
	"....XX......XX....",
	"..................",
	"..................",
	"..................",
	".................."
};

static char* minimize_button_xpm[] = {
	"18 18 2 1",
	"X c #FBF1C7",
	". c #3C3836",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	".....XXXXXXXX.....",
	"......XXXXXX......",
	".......XXXX.......",
	"........XX........",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	".................."
};

typedef struct {
	int x, y, w, h;
} silly_button;

bool silly_button_inside(int x, int y, silly_button* but) {
    return x >= but->x && x < but->x + but->w && y >= but->y && y < but->y + but->h;
}

typedef struct silly_window {
	Window client;
	Window border;
	GC border_gc;

	bool rolled;
	int border_x, border_y;
	int border_width, border_height;
	int client_width, client_height;

	silly_button close, minimize;
	silly_button titlebar; // i know like wtf or smth

	struct silly_window* next;
} silly_window;
silly_window* current = NULL;

Pixmap close_pixmap,    close_mask;
Pixmap minimize_pixmap, minimize_mask;

void close_window(Window wnd) {
	XEvent event;
	memset(&event, 0, sizeof(event));
	event.xclient.type = ClientMessage;
	event.xclient.window = wnd;
	event.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", true);
	event.xclient.format = 32;
	event.xclient.data.l[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", false);
	event.xclient.data.l[1] = CurrentTime;
	XSendEvent(dpy, wnd, False, NoEventMask, &event);
	XFlush(dpy);
}

silly_window* silly_find_window(Window wnd) {
	for (silly_window* swnd = current; swnd; swnd = swnd->next)
		if (swnd->client == wnd || swnd->border == wnd) return swnd;
	return NULL;
}

silly_window* silly_register_window(Window client) {
	silly_window* swnd = calloc(1, sizeof(silly_window));
	swnd->client = client;

	// get window content information
	Window r;
	int x, y, client_width, client_height, borderw, depth;
	XGetGeometry(dpy, swnd->client, &r, &x, &y, &client_width, &client_height, &borderw, &depth);

	y = MAX(BAR_HEIGHT, y);

	int border_width  = client_width  + (BORDER_EXT * 2);
	int border_height = client_height + (BORDER_EXT * 2) + TITLE_HEIGHT + 1;

	swnd->rolled = false;
	
	swnd->border_x = x;
	swnd->border_y = y;

	swnd->client_width  = client_width;
	swnd->client_height = client_height;
	
	swnd->border_width  = border_width;
	swnd->border_height = border_height;

	swnd->close    = (silly_button){ BORDER_EXT, BORDER_EXT, BUTTON_WIDTH, TITLE_HEIGHT };
	swnd->minimize = (silly_button){ swnd->border_width - BORDER_EXT - BUTTON_WIDTH, BORDER_EXT, BUTTON_WIDTH, TITLE_HEIGHT };
	swnd->titlebar = (silly_button){ BORDER_EXT + BUTTON_WIDTH, BORDER_EXT, swnd->client_width - (BUTTON_WIDTH * 2) - 2, TITLE_HEIGHT };

	swnd->border = XCreateSimpleWindow(dpy, root, x, y, border_width, border_height, 0, 0, BORDER_COLOR);
	XReparentWindow(dpy, swnd->client, swnd->border, BORDER_EXT, BORDER_EXT + TITLE_HEIGHT + 1);

	// spawn em
	XMapWindow(dpy, swnd->client);
	XMapWindow(dpy, swnd->border);

	// stuff
	XSelectInput(dpy, swnd->client, StructureNotifyMask); // unmap
	XSelectInput(dpy, swnd->border, SubstructureRedirectMask | ButtonPressMask | ButtonReleaseMask | ExposureMask);

	// grab title bar window stuff
	XGrabButton(dpy, AnyButton, AnyModifier, swnd->border, False, ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);

	// permit client destruction later
	Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, swnd->client, &wm_delete, 1);
	// this too
	XSetWindowBorderWidth(dpy, swnd->client, 0);

	// border essentials 
	swnd->border_gc = XCreateGC(dpy, swnd->border, 0, NULL);

	swnd->next = current;
	current = swnd;

	return current;
}

void silly_roll_window(silly_window* swnd) {
	swnd->rolled = !swnd->rolled;
	if (swnd->rolled) XUnmapWindow(dpy, swnd->client);
	else XMapWindow(dpy, swnd->client);
	int height = swnd->rolled ? BORDER_EXT + TITLE_HEIGHT + 1 : swnd->border_height;
	XResizeWindow(dpy, swnd->border, swnd->border_width, height);

	if (swnd->rolled) XSetInputFocus(dpy, root, None, CurrentTime);
	else XSetInputFocus(dpy, swnd->client, RevertToPointerRoot, CurrentTime);
}

void silly_unregister_window(Window wnd) {
	silly_window* swnd = silly_find_window(wnd);
	if (!swnd) return;

	if (current == swnd) {
		current = swnd->next;
	} else {
		silly_window* search = current;
		while (search && search->next != swnd)
			search = search->next;
		if (!search) goto unmap;
		search->next = swnd->next;
	}

unmap:
	XErrorHandler h = XSetErrorHandler(error_pit);
	XWindowAttributes attr;
	if (XGetWindowAttributes(dpy, swnd->client, &attr) && attr.map_state == IsViewable)
		XReparentWindow(dpy, swnd->client, DefaultRootWindow(dpy), BORDER_EXT, BAR_HEIGHT + BORDER_EXT);
	XDestroyWindow(dpy, swnd->border);
	XSetErrorHandler(h);

	if (swnd->border_gc) XFreeGC(dpy, swnd->border_gc);
	free(swnd);
}

void silly_move_window(Window wnd, int x, int y) {
	silly_window* swnd = silly_find_window(wnd);
	if (!swnd) return;

	if (swnd->rolled) {
		x = MAX(0,          MIN(x, scr_w - swnd->border_width));
		y = MAX(BAR_HEIGHT, MIN(y, scr_h - (BORDER_EXT + TITLE_HEIGHT + 1)));
	} else {
		x = MAX(0,          MIN(x, scr_w - swnd->border_width)); 
		y = MAX(BAR_HEIGHT, MIN(y, scr_h - swnd->border_height));
	}
	XMoveWindow(dpy, swnd->border, x, y);

	swnd->border_x = x;
	swnd->border_y = y;
}

void silly_redraw_borders(silly_window* swnd) {
	// title block
	XSetForeground(dpy, swnd->border_gc, TITLE_COLOR);
	XFillRectangle(dpy, swnd->border, swnd->border_gc, BORDER_EXT, BORDER_EXT, swnd->client_width, TITLE_HEIGHT);

	// borders
	XSetForeground(dpy, swnd->border_gc, BORDER_DARK); 
	XDrawRectangle(dpy, swnd->border, swnd->border_gc, 0, 0, swnd->border_width - 1, swnd->border_height - 1); // border rectangle 
	XDrawRectangle(dpy, swnd->border, swnd->border_gc, BORDER_INNER, BORDER_INNER,
		swnd->border_width - (BORDER_INNER * 2) - 1, swnd->border_height - (BORDER_INNER * 2) - 1); // client inner rectangle	

	// border lines
	XDrawLine(dpy, swnd->border, swnd->border_gc, 0, BORDER_EXT + TITLE_HEIGHT, swnd->border_width, BORDER_EXT + TITLE_HEIGHT);
	XDrawLine(dpy, swnd->border, swnd->border_gc, 0, swnd->border_height - BORDER_EXT - BUTTON_WIDTH, swnd->border_width, swnd->border_height - BORDER_EXT - BUTTON_WIDTH);
	XDrawLine(dpy, swnd->border, swnd->border_gc, BORDER_EXT + BUTTON_WIDTH, 0, BORDER_EXT + BUTTON_WIDTH, swnd->border_height);
	XDrawLine(dpy, swnd->border, swnd->border_gc, swnd->border_width - BORDER_EXT - 1 - BUTTON_WIDTH, 0, swnd->border_width - BORDER_EXT - BUTTON_WIDTH, swnd->border_height);

	// button pixmaps
	XCopyArea(dpy, close_pixmap,    swnd->border, swnd->border_gc, 0, 0, 18, 18, BORDER_EXT, BORDER_EXT);
	XCopyArea(dpy, minimize_pixmap, swnd->border, swnd->border_gc, 0, 0, 18, 18, swnd->border_width - BORDER_EXT - 18, BORDER_EXT);

	// clear background
	XSetForeground(dpy, swnd->border_gc, WINDOW_BACK);
	XFillRectangle(dpy, swnd->border, swnd->border_gc, BORDER_EXT, BORDER_EXT + TITLE_HEIGHT + 1, swnd->client_width, swnd->client_height);
}

int main(void) {
	dpy = XOpenDisplay(0);
	if (!dpy) return 1;

	root = DefaultRootWindow(dpy);
	scr = DefaultScreen(dpy);
	scr_w = DisplayWidth(dpy, scr);
	scr_h = DisplayHeight(dpy, scr);

	font = XftFontOpenName(dpy, scr, font_name);
	if (!font) return 1; // install fixedsys brother
	
	Visual*  vis  = DefaultVisual(dpy, scr);
	Colormap cmap = DefaultColormap(dpy, scr);
	XftColorAllocValue(dpy, vis, cmap, &text_fg, &ren_fg);

	XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
	Cursor cur = XCreateFontCursor(dpy, XC_X_cursor);
	XDefineCursor(dpy, root, cur);

	XpmAttributes xpm_attr;
	xpm_attr.valuemask = XpmSize;
	XpmCreatePixmapFromData(dpy, root, close_button_xpm,    &close_pixmap,    &close_mask,    &xpm_attr);
	XpmCreatePixmapFromData(dpy, root, minimize_button_xpm, &minimize_pixmap, &minimize_mask, &xpm_attr);

	silly_init_binds();
	silly_init_apps();
	silly_bar* bar = silly_init_bar();
	silly_refresh_bar(bar, None);

	XEvent ev;
	XButtonEvent start;
	XWindowAttributes attr;

	silly_window* swnd;

	time_t last_time = time(NULL), current_time;
	while (1) {
		current_time = time(NULL);
		if (difftime(current_time, last_time) >= 1.0) {
			Window focus;
			int revert;
			XGetInputFocus(dpy, &focus, &revert);
			silly_refresh_bar(bar, focus);
			last_time = current_time;
		}

        if (!XPending(dpy)) { usleep(1000); continue; };
		XNextEvent(dpy, &ev);
		if (ev.type == MapRequest) {
			swnd = NULL;
			swnd = silly_find_window(ev.xmaprequest.window);

			if (!swnd) {
				XWindowAttributes map_attr;
				XGetWindowAttributes(dpy, ev.xmaprequest.window, &map_attr);
				if (!map_attr.override_redirect) {
					swnd = silly_register_window(ev.xmaprequest.window);
					silly_redraw_borders(swnd); // initial draw
				}
			}
			XSetInputFocus(dpy, swnd->client, RevertToPointerRoot, CurrentTime);
		} else if (ev.type == UnmapNotify || ev.type == DestroyNotify) {
			swnd = silly_find_window(ev.xunmap.window);
			if (swnd && !swnd->rolled)
				silly_unregister_window(ev.xunmap.window);
		} else if (ev.type == Expose) {
			swnd = silly_find_window(ev.xexpose.window);
			if (swnd && ev.xexpose.window == swnd->border) silly_redraw_borders(swnd);
		} else if (ev.type == ButtonPress) {
			int x = ev.xbutton.x, y = ev.xbutton.y;
			swnd = silly_find_window(ev.xbutton.window);
			
			if      (swnd) {
				XRaiseWindow(dpy, swnd->border);
				XWindowAttributes client_attr;
				XGetWindowAttributes(dpy, swnd->client, &client_attr);
				if (client_attr.map_state == IsViewable)
					XSetInputFocus(dpy, swnd->client, RevertToPointerRoot, CurrentTime);
			}

			if (swnd && silly_button_inside(x, y, &swnd->close))
				close_window(swnd->client);
			else if (swnd && silly_button_inside(x, y, &swnd->minimize))
				silly_roll_window(swnd);
			else if (swnd && silly_button_inside(x, y, &swnd->titlebar)) {
				start = ev.xbutton;
				XGetWindowAttributes(dpy, ev.xbutton.window, &attr);
				XGrabPointer(dpy, swnd->border, True, PointerMotionMask | ButtonReleaseMask,
					GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
			}
        } else if (ev.type == MotionNotify) {
			int x = attr.x + ev.xbutton.x_root - start.x_root;
			int y = attr.y + ev.xbutton.y_root - start.y_root;
			silly_move_window(start.window, x, y);
		} else if (ev.type == ButtonRelease) {
			XUngrabPointer(dpy, CurrentTime);
			start.window = None;
		} else if (ev.type == KeyPress)
			silly_handle_bind(&ev.xkey);
		
		if (to_quit) break;
		swnd = NULL;
    }

	XFreeCursor(dpy, cur);
	XUngrabKeyboard(dpy, CurrentTime);
	silly_destroy_bar(bar);
	return 0;
}
