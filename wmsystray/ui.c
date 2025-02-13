/*******************************************************************************
 ui.c

 General UI code; basically, anything that has visible effects is implemented
 here.

 This source code is copyright (C) 2004 Matthew Reppert. Use of this code is
 permitted under the code of the GNU General Public Liscense v2.
 ******************************************************************************/

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include <xembed.h>

#include "trace.h"
#include "systray.h"
#include "ui.h"


Display *main_disp;
Window main_wind, sel_wind, icon_wind, draw_wind;
char *display_string = NULL;
char *geometry_string = "64x64+0+0";
int width, height, pos_x, pos_y;
Pixmap bg_pixmap;
char *bg_data;

int wmaker = 1;
int loop_program = 1;
static GC main_gc;


XRectangle active_area = {
	.x = 8, .y = 4, .width = 48, .height = 48
};

static XRectangle left_button_area = {
	.x = 9, .y = 54, .width = 20, .height = 8
};

static XRectangle right_button_area = {
	.x = 35, .y = 54, .width = 20, .height = 8
};



/*
 * init_ui
 *
 * Create the main window and get it ready for use.
 */
int init_ui(char *app_name, int argc, char **argv) {
	XWMHints *wm_hints;
	XSizeHints *size_hints;
	XClassHint *class_hints;
	int border_width;
	int status;

	TRACE((stderr, "ENTERING: init_ui\n"));

	border_width = 0;

	/*
	 * Try to open a connection to an X server.
	 */
	main_disp = XOpenDisplay(display_string);
	if (main_disp == NULL) {
		TRACE((stderr, "Couldn't open display %s!\n",
				display_string ? display_string : ":0.0"));
		return -1;
	}

	size_hints = XAllocSizeHints();
	XWMGeometry (main_disp, DefaultScreen(main_disp), geometry_string,
		     NULL, border_width, size_hints,
		     &size_hints->x, &size_hints->y,
		     &size_hints->width, &size_hints->height, &status);


	/*
	 * Create our windows.
	 */
	main_wind = XCreateSimpleWindow (main_disp,
					 DefaultRootWindow (main_disp),	
					 size_hints->x, size_hints->y,
					 size_hints->width, size_hints->height,
					 0, 0, 0);

	icon_wind = XCreateSimpleWindow (main_disp,
					 DefaultRootWindow (main_disp),
					 size_hints->x, size_hints->y,
					 size_hints->width, size_hints->height,
					 0, 0, 0);

	draw_wind = icon_wind;
	if (!wmaker)
		draw_wind = main_wind;

	sel_wind = XCreateSimpleWindow (main_disp, DefaultRootWindow(main_disp),
					-1, -1, 1, 1, 0, 0, 0);


	/*
	 * Set window manager hints.
	 */
	class_hints = XAllocClassHint();
	class_hints->res_class = "WMSYSTRAY";
	class_hints->res_name = app_name;
	XSetClassHint (main_disp, main_wind, class_hints);
	XFree (class_hints);

	/*
	 * The icon window and SetCommand hints are necessary for icon
	 * window and dockapp stuff to work properly with Window Maker.
	 */
	wm_hints = XAllocWMHints();
	wm_hints->flags = WindowGroupHint | IconWindowHint | StateHint;
	wm_hints->window_group = main_wind;
	wm_hints->icon_window = icon_wind;
	wm_hints->initial_state = WithdrawnState;
	XSetWMHints (main_disp, main_wind, wm_hints);
	XFree (wm_hints);

	XSetCommand (main_disp, main_wind, argv, argc);


	/*
	 * Display windows.
	 */
	XMapRaised (main_disp, main_wind);
	XMapSubwindows (main_disp, icon_wind);
	XFlush (main_disp);

	/*
	 * Set the icon window background.
	 */
	status=XpmCreatePixmapFromData (main_disp, DefaultRootWindow(main_disp),
				 wmsystray_xpm, &bg_pixmap, NULL, NULL);
	if (status != XpmSuccess || status != 0) 
		TRACE((stderr, "XPM loading didn't work\n"));

	/* XSetWindowBackgroundPixmap (main_disp, icon_wind, bg_pixmap); */
	XSetWindowBackgroundPixmap (main_disp, draw_wind, ParentRelative);
	XClearWindow (main_disp, draw_wind);
	XFlush (main_disp);

	draw_ui_elements();

	/*
	 * Select input events we're interested in.
	 */
	XSelectInput(main_disp, main_wind, VisibilityChangeMask |
					   StructureNotifyMask |
					   ExposureMask |
					   PropertyChangeMask | 
					   /*
					   ButtonPressMask |
					   ButtonReleaseMask |
					   */
					   KeyPressMask |
					   KeyReleaseMask);

	XSelectInput(main_disp, icon_wind, VisibilityChangeMask |
					   StructureNotifyMask |
					   ExposureMask |
					   PropertyChangeMask | 
					   /*
					   ButtonPressMask |
					   ButtonReleaseMask |
					   */
					   KeyPressMask |
					   KeyReleaseMask);


	TRACE((stderr, "LEAVING: init_ui\n"));
	return 0;
}



/*
 * cleanup_ui
 *
 * Clean up the main window and free related resources.
 */
void cleanup_ui() {
	TRACE((stderr, "ENTERING: cleanup_ui\n"));

	XUnmapWindow (main_disp, main_wind);
	XDestroyWindow (main_disp, main_wind);
	XDestroyWindow (main_disp, icon_wind);
	XDestroyWindow (main_disp, sel_wind);
	XCloseDisplay (main_disp);

	TRACE((stderr, "LEAVING: cleanup_ui\n"));
}



/*
 * draw_ui_elements
 *
 * Draw the systray area and buttons.
 */
void draw_ui_elements() {
	XCopyArea (main_disp, bg_pixmap, draw_wind,
		   DefaultGC(main_disp, DefaultScreen(main_disp)),
		   active_area.x - 1, active_area.y - 1,
		   active_area.width + 2, active_area.height + 2,
		   active_area.x - 1, active_area.y - 1);
	XCopyArea (main_disp, bg_pixmap, draw_wind,
		   DefaultGC(main_disp, DefaultScreen(main_disp)),
		   left_button_area.x, left_button_area.y,
		   left_button_area.width, left_button_area.height,
		   left_button_area.x - 1, left_button_area.y - 1);
	XCopyArea (main_disp, bg_pixmap, draw_wind,
		   DefaultGC(main_disp, DefaultScreen(main_disp)),
		   right_button_area.x, right_button_area.y,
		   right_button_area.width, right_button_area.height,
		   right_button_area.x - 1, right_button_area.y - 1);
	XFlush (main_disp);
}



/*
 * point_is_in_rect
 *
 * Determines whether a point is in a rectangle.
 */
int point_is_in_rect (int x, int y, XRectangle *rect) {
	int ret = 0;
	int x2, y2;

	x2 = rect->x + rect->width;
	y2 = rect->y + rect->height;
	if (x >= rect->x && y >= rect->y && x <= x2 && y <= y2)
		ret = 1;

	return ret;
}



/*
 * wmsystray_handle_signal
 *
 * Handle UNIX signals.
 */
void wmsystray_handle_signal (int signum) {
	switch (signum) {
	case SIGINT:
	case SIGTERM:
		loop_program = 0;
		break;
	}
}



/*
 * wmsystray_event_loop
 *
 * Handle X events.
 */
void wmsystray_event_loop() {
	XEvent ev;
	XWindowAttributes attrib;
	Window cover;
	struct list_head *n;
	struct systray_item *item;

	while (loop_program) {
		while (XPending(main_disp)) {
			XNextEvent(main_disp, &ev);
			/* TRACE((stderr, "XEVENT %x\n", ev.type)); */

			/*
			 * There are several XEMBED events that clients may send
			 * to us.
			 */
			if (xembed_event_is_xembed_event(main_disp, &ev)) {
				switch (ev.xclient.data.l[1]) {
				case XEMBED_REQUEST_FOCUS:
					xembed_focus_in(main_disp,
							ev.xclient.window,
							XEMBED_FOCUS_CURRENT);
					break;

				case XEMBED_FOCUS_NEXT:
					break;

				case XEMBED_FOCUS_PREV:
					break;

				case XEMBED_REGISTER_ACCELERATOR:
					break;

				case XEMBED_UNREGISTER_ACCELERATOR:
					break;
				}

				continue;
			}

			if (event_is_systray_event(&ev)) {
				handle_systray_event (&ev);
				continue;
			}

			switch (ev.type) {
			case MapRequest:
				item = find_systray_item(ev.xmaprequest.window);
				if (item) {
					TRACE((stderr, "MRAISING: %X\n",
						item->window_id));
					XMapRaised (main_disp, item->window_id);
					xembed_window_activate (main_disp,
							item->window_id);
							/*
					xembed_focus_in (main_disp,
							item->window_id, 1);
							*/
				}
				break;
					
			case Expose:
				draw_ui_elements();
				break;

			case PropertyNotify:
				if (ev.xproperty.window == main_wind)
					break;

				TRACE((stderr, "Prop %x\n", ev.xproperty.window));
				item = find_systray_item (ev.xproperty.window);
				if (item && systray_property_update (item) == 0)
					break;

				break;

			case ConfigureNotify:
				if (ev.xany.window == draw_wind) 
					break;

				/*
				 * Don't let icons resize themselves to other
				 * than 24x24.
				 */
				XGetWindowAttributes (main_disp,
							ev.xproperty.window,
							&attrib);

				if (attrib.width != 24) 
					XResizeWindow (main_disp,
							ev.xproperty.window,
							24, 24);

				/*
				cover = XCreateSimpleWindow (main_disp,
								draw_wind,
								0, 0,
								64, 64,
								0, 0, 0);
				XMapWindow (main_disp, cover);
				XDestroyWindow (main_disp, cover);
				*/
				break;

			case ReparentNotify:
			case UnmapNotify:
			case DestroyNotify:
				if (ev.xreparent.parent == main_wind ||
				    ev.xreparent.parent == icon_wind)
					break;

				item = find_systray_item (ev.xreparent.window);
				if (item) {
					TRACE ((stderr, "\tRemove %x\n",
						item->window_id));
					systray_item_count--;
					list_del (&item->systray_list);
					repaint_systray();
				}

				break;

			case KeyPress:
			case KeyRelease:
				/* systray_forward_event(&ev); */
				break;

			case FocusIn:
				/* systray_focus_in(&ev); */
				break;

			case FocusOut:
				/* systray_focus_out(&ev); */
				break;
			}
		}

		usleep(100000L);
	}
}
