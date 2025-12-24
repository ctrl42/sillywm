// sillywm
// by stx4

#include <time.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>

#include <fontconfig/fontconfig.h>

#include "ipc.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct {
	int keycode;
	char* app;
} silly_bind;

typedef struct {
	int x, y, w, h;
} silly_button;

typedef struct {
	int head, max;
	void* data; // reusable
} silly_vec;

typedef struct {
	Window wnd;
	XftDraw* xftdraw;
	GC gc;
} silly_bar;

typedef struct silly_window {
	Window client;
	Window border;
	GC border_gc;

	bool rolled;
	int border_x, border_y;
	int border_width, border_height;
	int client_width, client_height;

	silly_button close, minimize;
	silly_button titlebar;

	struct silly_window* next;
} silly_window;

static char* close_button_xpm[] = {
	"18 18 2 1",
	"X c #EBDBB2",
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
	"X c #EBDBB2",
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

float BAR_REFRESH =  2.0f;

int TITLE_HEIGHT  = 18;
int BAR_HEIGHT    = 22;
int BUTTON_SIZE   = 18;
int BORDER_SIZE   =  4;

int EDGE_PADDING  = 10;
int DEFAULT_X     = 10;
int DEFAULT_Y     = 32;
int MIN_SIZE      = 64;

int TITLE_BG      = 0x282828;
int BAR_TEXT      = 0xEBDBB2;
int BAR_BG        = 0x282828;
int BORDER_BG     = 0x282828;
int BORDER_EDGE   = 0x3C3836;
int WINDOW_BG     = 0x282828;

int MOD_MASK      = Mod4Mask;
int CURSOR        = XC_X_cursor;
#define BORDER_INNER (BORDER_SIZE - 1)

#define SOCKET_BASE "/tmp/sillywm-%s.sock"
#define CONF_NAME   ".sillyrc"
char* font_name = "Fixedsys Excelsior:pixelsize=16:antialias=false";
/*
char* font_name = "Liberation Mono:pixelsize=12:antialias=true:autohint=true"; // default
*/

#define BATTERY_LOC  "/sys/class/power_supply/BAT1/capacity"

// mess (not sorry)
extern char** environ;

int sockfd;
Display* dpy;
Window root;
Window focus = None;
XftFont* font;
XftColor ren_fg;
XRenderColor text_fg;
bool to_quit = false;
int scr, scr_w, scr_h;
silly_window* current = NULL;
Pixmap close_pixmap,    close_mask;
Pixmap minimize_pixmap, minimize_mask;

static int error_pit(Display* dpy, XErrorEvent* e) { return 0; }

void silly_run(char* app) {
	posix_spawnp(NULL, "/bin/sh", NULL, NULL, (char* []){ "sh", "-c", app, NULL }, environ);
}

silly_bar* silly_init_bar(void) {
	silly_bar* bar = calloc(1, sizeof(silly_bar));
	
	bar->wnd = XCreateSimpleWindow(
		dpy, root,
		0, 0, scr_w, BAR_HEIGHT,
		0, 0, BAR_BG
	);
	bar->xftdraw = XftDrawCreate(
		dpy, bar->wnd,
		DefaultVisual(dpy, scr),
		DefaultColormap(dpy, scr)
	);
	XSelectInput(dpy, bar->wnd, ExposureMask);
	XMapWindow(dpy, bar->wnd);
	bar->gc = XCreateGC(dpy, bar->wnd, 0, NULL);

	return bar;
}

void silly_draw_bar(silly_bar* bar, char* title, char* status) {
	XSetForeground(dpy, bar->gc, BAR_BG);
	XFillRectangle(dpy, bar->wnd, bar->gc, 0, 0, scr_w, BAR_HEIGHT);

	XSetForeground(dpy, bar->gc, BORDER_BG);
	XFillRectangle(dpy, bar->wnd, bar->gc, 0, BAR_HEIGHT - BORDER_SIZE, scr_w, BAR_HEIGHT - BORDER_SIZE);

	XGlyphInfo extents;
	XftTextExtentsUtf8(dpy, font, (FcChar8*)status, strlen(status), &extents);
	XftDrawStringUtf8(bar->xftdraw, &ren_fg, font, 5, (BAR_HEIGHT + 4) / 2, (FcChar8*)title, strlen(title));
	XftDrawStringUtf8(bar->xftdraw, &ren_fg, font, scr_w - extents.width - 5, (BAR_HEIGHT + 4) / 2, (FcChar8*)status, strlen(status));

	XSetForeground(dpy, bar->gc, BORDER_EDGE);
	XDrawLine(dpy, bar->wnd, bar->gc, 0, BAR_HEIGHT - BORDER_SIZE, scr_w, BAR_HEIGHT - BORDER_SIZE);
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
		"%02d:%02d %02d/%02d/%02d",
		tm->tm_hour, tm->tm_min,
		tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900
	);
	silly_draw_bar(bar, title ? title : "(desk)", status);
	if (title) XFree(title);
}

void silly_destroy_bar(silly_bar* bar) {
	XFreeGC(dpy, bar->gc);
	XDestroyWindow(dpy, bar->wnd);
	free(bar);
}

// DOES NOT CHECK IF BUTTON IS DOWN
bool silly_button_inside(int x, int y, silly_button* but) {
    return x >= but->x && x < but->x + but->w && y >= but->y && y < but->y + but->h;
}

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

// not yet defined til later
void silly_unregister_window(silly_window* swnd);
void silly_move_window(silly_window* swnd, int x, int y);

void silly_close_window(silly_window* swnd) {
	close_window(swnd->client);
	if (swnd->rolled) silly_unregister_window(swnd);
	focus = None;
}

silly_window* silly_register_window(Window client) {
	silly_window* swnd = calloc(1, sizeof(silly_window));
	swnd->client = client;

	// get window content information
	Window r;
	int x, y, client_width, client_height, borderw, depth;
	XGetGeometry(dpy, swnd->client, &r, &x, &y, &client_width, &client_height, &borderw, &depth);

	x = x != 0 ? x : DEFAULT_X;
	y = y != 0 ? y : DEFAULT_Y;

	int border_width  = client_width  + (BORDER_SIZE * 2);
	int border_height = client_height + (BORDER_SIZE * 2) + TITLE_HEIGHT + 1;

	swnd->rolled = false;
	
	swnd->border_x = x;
	swnd->border_y = y;

	swnd->client_width  = client_width;
	swnd->client_height = client_height;
	
	swnd->border_width  = border_width;
	swnd->border_height = border_height;

	swnd->close    = (silly_button){ BORDER_SIZE, BORDER_SIZE, BUTTON_SIZE, TITLE_HEIGHT };
	swnd->minimize = (silly_button){ swnd->border_width - BORDER_SIZE - BUTTON_SIZE, BORDER_SIZE, BUTTON_SIZE, TITLE_HEIGHT };
	swnd->titlebar = (silly_button){ BORDER_SIZE + BUTTON_SIZE, BORDER_SIZE, swnd->client_width - (BUTTON_SIZE * 2) - 2, TITLE_HEIGHT };

	swnd->border = XCreateSimpleWindow(dpy, root, x, y, border_width, border_height, 0, 0, BORDER_BG);
	XReparentWindow(dpy, swnd->client, swnd->border, BORDER_SIZE, BORDER_SIZE + TITLE_HEIGHT + 1);

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
	silly_move_window(swnd, x, y);

	swnd->next = current;
	current = swnd;

	return current;
}

void silly_roll_window(silly_window* swnd) {
	swnd->rolled = !swnd->rolled;
	if (swnd->rolled) XUnmapWindow(dpy, swnd->client);
	else XMapWindow(dpy, swnd->client);
	int height = swnd->rolled ? BORDER_SIZE + TITLE_HEIGHT + 1 : swnd->border_height;
	XResizeWindow(dpy, swnd->border, swnd->border_width, height);

	Window set = swnd->rolled ? None : swnd->client;
	XSetInputFocus(dpy, root, set, CurrentTime);
	focus = set;
}

void silly_unregister_window(silly_window* swnd) {
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
		XReparentWindow(dpy, swnd->client, DefaultRootWindow(dpy), BORDER_SIZE, BAR_HEIGHT + BORDER_SIZE);
	XDestroyWindow(dpy, swnd->border);
	XSetErrorHandler(h);

	if (swnd->border_gc) XFreeGC(dpy, swnd->border_gc);
	free(swnd);
}

void silly_move_window(silly_window* swnd, int x, int y) {
	if (swnd->rolled) {
		x = MAX(EDGE_PADDING, MIN(x, scr_w - EDGE_PADDING - swnd->border_width));
		y = MAX(BAR_HEIGHT + EDGE_PADDING, MIN(y, scr_h - EDGE_PADDING - (BORDER_SIZE + TITLE_HEIGHT + 1)));
	} else {
		x = MAX(EDGE_PADDING, MIN(x, scr_w - EDGE_PADDING - swnd->border_width)); 
		y = MAX(BAR_HEIGHT + EDGE_PADDING, MIN(y, scr_h - EDGE_PADDING - swnd->border_height));
	}
	XMoveWindow(dpy, swnd->border, x, y);

	swnd->border_x = x;
	swnd->border_y = y;
}

void silly_size_window(silly_window* swnd, int width, int height) {
	swnd->client_width  = MAX(MIN_SIZE, width);
	swnd->client_height = MAX(MIN_SIZE, height);

	swnd->border_width  = swnd->client_width  + (BORDER_SIZE * 2);
	swnd->border_height = swnd->client_height + (BORDER_SIZE * 2)
		+ TITLE_HEIGHT + 1;

	swnd->close    = (silly_button){ BORDER_SIZE, BORDER_SIZE, BUTTON_SIZE, TITLE_HEIGHT };
	swnd->minimize = (silly_button){ swnd->border_width - BORDER_SIZE - BUTTON_SIZE, BORDER_SIZE, BUTTON_SIZE, TITLE_HEIGHT };
	swnd->titlebar = (silly_button){ BORDER_SIZE + BUTTON_SIZE, BORDER_SIZE, swnd->client_width - (BUTTON_SIZE * 2) - 2, TITLE_HEIGHT };

	XResizeWindow(dpy, swnd->client,
		swnd->client_width, swnd->client_height);
	XResizeWindow(dpy, swnd->border,
		swnd->border_width, swnd->border_height);
}

void silly_redraw_borders(silly_window* swnd) {
	// title block
	XSetForeground(dpy, swnd->border_gc, TITLE_BG);
	XFillRectangle(dpy, swnd->border, swnd->border_gc, BORDER_SIZE, BORDER_SIZE, swnd->client_width, TITLE_HEIGHT);

	// borders
	XSetForeground(dpy, swnd->border_gc, BORDER_EDGE); 
	XDrawRectangle(dpy, swnd->border, swnd->border_gc, 0, 0, swnd->border_width - 1, swnd->border_height - 1); // border rectangle 
	XDrawRectangle(dpy, swnd->border, swnd->border_gc, BORDER_INNER, BORDER_INNER,
		swnd->border_width - (BORDER_INNER * 2) - 1, swnd->border_height - (BORDER_INNER * 2) - 1); // client inner rectangle	

	// border lines
	XDrawLine(dpy, swnd->border, swnd->border_gc, 0, BORDER_SIZE + TITLE_HEIGHT, swnd->border_width, BORDER_SIZE + TITLE_HEIGHT);
	XDrawLine(dpy, swnd->border, swnd->border_gc, 0, swnd->border_height - BORDER_SIZE - BUTTON_SIZE, swnd->border_width, swnd->border_height - BORDER_SIZE - BUTTON_SIZE);
	XDrawLine(dpy, swnd->border, swnd->border_gc, BORDER_SIZE + BUTTON_SIZE, 0, BORDER_SIZE + BUTTON_SIZE, swnd->border_height);
	XDrawLine(dpy, swnd->border, swnd->border_gc, swnd->border_width - BORDER_SIZE - 1 - BUTTON_SIZE, 0, swnd->border_width - BORDER_SIZE - BUTTON_SIZE, swnd->border_height);

	// button pixmaps
	XCopyArea(dpy, close_pixmap,    swnd->border, swnd->border_gc, 0, 0, BUTTON_SIZE, BUTTON_SIZE, BORDER_SIZE, BORDER_SIZE);
	XCopyArea(dpy, minimize_pixmap, swnd->border, swnd->border_gc, 0, 0, BUTTON_SIZE, BUTTON_SIZE, swnd->border_width - BORDER_SIZE - BUTTON_SIZE, BORDER_SIZE);

	// clear background
	XSetForeground(dpy, swnd->border_gc, WINDOW_BG);
	XFillRectangle(dpy, swnd->border, swnd->border_gc, BORDER_SIZE, BORDER_SIZE + TITLE_HEIGHT + 1, swnd->client_width, swnd->client_height);
}

void silly_handle_ctl(int fd) {
	char c;
	char buf[MAX_IPC_SIZE];

	read(fd, &buf, sizeof(silly_ctrl_command));
	silly_ctrl_command* ctrl = (silly_ctrl_command*)buf;
	char* data = buf + sizeof(silly_ctrl_command);

	for (int i = 0; i < MAX_IPC_SIZE - sizeof(silly_ctrl_command); i++) {
		read(fd, &c, 1);
		buf[sizeof(silly_ctrl_command) + i] = c;
		if (i >= ctrl->len) {
			buf[sizeof(silly_ctrl_command) + i + 1] = 0;
			break;
		}
	}
		
	switch (ctrl->cmd) {
	case EXEC:
		silly_run(data);
		break;
	case KILL:
		if (focus == None) break;
		close_window(focus);
		focus = None;
		break;
	case MOVE: {
		silly_window* swnd = silly_find_window(focus);
		if (!swnd) break;

		int rel_x = (int)*data & (1 << 0);
		int rel_y = (int)*data & (1 << 1);
		int x     = ctrl->param1 + (rel_x ? swnd->border_x : 0);
		int y     = ctrl->param2 + (rel_y ? swnd->border_y : 0);
		silly_move_window(swnd, x, y);
		break;
	}
	case SIZE: {
		silly_window* swnd = silly_find_window(focus);
		if (!swnd) break;

		int rel_w = (int)*data & (1 << 0);
		int rel_h = (int)*data & (1 << 1);
		int w     = ctrl->param1 + (rel_w ? swnd->client_width  : 0);
		int h     = ctrl->param2 + (rel_h ? swnd->client_height : 0);
		silly_size_window(swnd, w, h);
		break;
	}
	case QUIT:
		to_quit = true;
		break;
	}
}

void* silly_ctl_loop(void* arg) {
	while (1) {
		int datafd = accept(sockfd, NULL, NULL);
		if (datafd > 0) {
			silly_handle_ctl(datafd);
			close(datafd);
		}
		if (to_quit) break;
	}
	return NULL;
}

int main(void) {
	dpy = XOpenDisplay(0);
	if (!dpy) return 1;

	setenv("XDG_CURRENT_DESKTOP", "sillywm", true);
	setenv("XDG_SESSION_TYPE", "x11", true);

	root = DefaultRootWindow(dpy);
	scr = DefaultScreen(dpy);
	scr_w = DisplayWidth(dpy, scr);
	scr_h = DisplayHeight(dpy, scr);

	// initialize sillyc (ideally) socket
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;

	char sock_name[128];
	sprintf(sock_name, SOCKET_BASE, getenv("DISPLAY"));
	strcpy(addr.sun_path, sock_name);
	
	unlink(sock_name);
	bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
	listen(sockfd, 32);

	int sock_flags = fcntl(sockfd, F_GETFL, 0);

	// sillyc handler doesn't poke Xlib, safely throw off handler
	// pro: this also starts handling while we parse ~/.sillyrc
	pthread_t ctl_handler;
	if (pthread_create(&ctl_handler, NULL, silly_ctl_loop, NULL)) {
		perror("pthread");
		fprintf(stderr, "pthread is fucking gay\n");
		exit(1);
	}

	char* home = getenv("HOME");
	if (home) {
		char* combine = malloc(strlen(home) + strlen(CONF_NAME) + 2);
		sprintf(combine, "%s/%s", home, CONF_NAME);
		fprintf(stderr, "%s\n", combine);

		pid_t config_pid;
		posix_spawnp(&config_pid, combine, NULL, NULL, (char* []){ combine, NULL }, environ);
		waitpid(config_pid, NULL, 0); // wait for all sillyc init cmds
	
		free(combine);
	}

	// return to regular wm activities
	text_fg = (XRenderColor){
		.red   = ((BAR_TEXT    >> 16) & 0xFF) * 257,
		.green = ((BAR_TEXT    >>  8) & 0xFF) * 257,
		.blue  = ((BAR_TEXT    >>  0) & 0xFF) * 257,
		.alpha = 0xFFFF 
	};

	font = XftFontOpenName(dpy, scr, font_name);
	if (!font) return 1; // install fixedsys brother
	
	Visual*  vis  = DefaultVisual(dpy, scr);
	Colormap cmap = DefaultColormap(dpy, scr);
	XftColorAllocValue(dpy, vis, cmap, &text_fg, &ren_fg);

	XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
	Cursor cur = XCreateFontCursor(dpy, CURSOR);
	XDefineCursor(dpy, root, cur);

	XpmAttributes xpm_attr;
	xpm_attr.valuemask = XpmSize;
	XpmCreatePixmapFromData(dpy, root, close_button_xpm,    &close_pixmap,    &close_mask,    &xpm_attr);
	XpmCreatePixmapFromData(dpy, root, minimize_button_xpm, &minimize_pixmap, &minimize_mask, &xpm_attr);

	silly_bar* bar = silly_init_bar();
	silly_refresh_bar(bar, None);

	XEvent ev;
	XButtonEvent start;
	XWindowAttributes attr;

	silly_window* swnd;

	time_t last_time = time(NULL), current_time;
	while (1) {
		// sillybar
		current_time = time(NULL);
		if (difftime(current_time, last_time) >= BAR_REFRESH) {
			silly_refresh_bar(bar, focus);
			last_time = current_time;
		}

        if (!XPending(dpy)) { usleep(500); continue; }; // reduce load
		XNextEvent(dpy, &ev);
		if (ev.type == MapRequest) {
			swnd = silly_find_window(ev.xmaprequest.window);
			if (!swnd) {
				XWindowAttributes map_attr;
				XGetWindowAttributes(dpy, ev.xmaprequest.window, &map_attr);
				if (!map_attr.override_redirect) {
					swnd = silly_register_window(ev.xmaprequest.window);
					silly_redraw_borders(swnd); // initial draw
				}
			}
			focus = swnd->client;
			XSetInputFocus(dpy, focus, RevertToPointerRoot, CurrentTime);
			silly_refresh_bar(bar, focus);
		} else if (ev.type == UnmapNotify || ev.type == DestroyNotify) {
			swnd = silly_find_window(ev.xunmap.window);
			if (swnd && !swnd->rolled) silly_unregister_window(swnd);
			silly_refresh_bar(bar, None);
		} else if (ev.type == Expose) {
			swnd = silly_find_window(ev.xexpose.window);
			if (swnd && ev.xexpose.window == swnd->border) silly_redraw_borders(swnd);
			if (ev.xexpose.window == bar->wnd) silly_refresh_bar(bar, focus);
		} else if (ev.type == ButtonPress) {
			int x = ev.xbutton.x, y = ev.xbutton.y;
			swnd = silly_find_window(ev.xbutton.window);
			
			if      (swnd) {
				XRaiseWindow(dpy, swnd->border);
				XWindowAttributes client_attr;
				XGetWindowAttributes(dpy, swnd->client, &client_attr);
				if (client_attr.map_state == IsViewable) {
					focus = swnd->client;
					XSetInputFocus(dpy, focus, RevertToPointerRoot, CurrentTime);
					silly_refresh_bar(bar, focus);
				}
			}

			if (swnd && silly_button_inside(x, y, &swnd->close)) {
				silly_close_window(swnd);
			} else if (swnd && silly_button_inside(x, y, &swnd->minimize)) {
				silly_roll_window(swnd);
				silly_refresh_bar(bar, swnd->rolled ? None : swnd->client);
			} else if (swnd && silly_button_inside(x, y, &swnd->titlebar)) {
				start = ev.xbutton;
				XGetWindowAttributes(dpy, ev.xbutton.window, &attr);
				XGrabPointer(dpy, swnd->border, True, PointerMotionMask | ButtonReleaseMask,
					GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
			}
        } else if (ev.type == MotionNotify) {
			int x = attr.x + ev.xbutton.x_root - start.x_root;
			int y = attr.y + ev.xbutton.y_root - start.y_root;
			swnd = silly_find_window(start.window);
			if (swnd) silly_move_window(swnd, x, y);
		} else if (ev.type == ButtonRelease) {
			XUngrabPointer(dpy, CurrentTime);
			start.window = None;
		}

		if (to_quit) break;
		swnd = NULL;
    }	

	// shutdown server
	close(sockfd);
	unlink(sock_name);

	XFreeCursor(dpy, cur);
	XUngrabKeyboard(dpy, CurrentTime);
	silly_destroy_bar(bar);
	return 0;
}
