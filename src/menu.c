/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                     2003 Biju Chacko (botsie@users.sourceforge.net)
 *                     2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                     2004 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <errno.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <gmodule.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/xgtkicontheme.h>

#include "main.h"
#include "menu.h"
#ifdef USE_DESKTOP_MENU
#include "desktop-menu-stub.h"
#endif

#define WLIST_MAXLEN 20

static NetkScreen *netk_screen = NULL;
static GtkWidget *windowlist = NULL;
#ifdef USE_DESKTOP_MENU
static XfceDesktopMenu *desktop_menu = NULL;
static GModule *module_desktop_menu = NULL;
#endif

/*******************************************************************************  
 *  Window list menu
 *******************************************************************************
 */

static void
activate_window (GtkWidget * item, NetkWindow * win)
{
    TRACE ("dummy");
    netk_workspace_activate(netk_window_get_workspace(win));
    netk_window_activate(win);
}

static void
set_num_screens (gpointer num)
{
    static Atom xa_NET_NUMBER_OF_DESKTOPS = 0;
    XClientMessageEvent sev;
    int n;

    TRACE ("dummy");
    if (!xa_NET_NUMBER_OF_DESKTOPS)
    {
	xa_NET_NUMBER_OF_DESKTOPS =
	    XInternAtom (GDK_DISPLAY (), "_NET_NUMBER_OF_DESKTOPS", False);
    }

    n = GPOINTER_TO_INT (num);

    sev.type = ClientMessage;
    sev.display = GDK_DISPLAY ();
    sev.format = 32;
    sev.window = GDK_ROOT_WINDOW ();
    sev.message_type = xa_NET_NUMBER_OF_DESKTOPS;
    sev.data.l[0] = n;

    gdk_error_trap_push ();

    XSendEvent (GDK_DISPLAY (), GDK_ROOT_WINDOW (), False,
		SubstructureNotifyMask | SubstructureRedirectMask,
		(XEvent *) & sev);

    gdk_flush ();
    gdk_error_trap_pop ();
}

static GtkWidget *
create_window_list_item (NetkWindow * win, GList **pix_unref_needed)
{
    const char *name = NULL;
    GString *label;
    GtkWidget *mi;
	GdkPixbuf *icon = NULL, *tmp;

    TRACE ("dummy");
    if (netk_window_is_skip_pager (win) || netk_window_is_skip_tasklist (win))
	return NULL;

    if (!name)
	name = netk_window_get_name (win);

    label = g_string_new (name);

    if (label->len >= WLIST_MAXLEN)
    {
	g_string_truncate (label, WLIST_MAXLEN);
	g_string_append (label, " ...");
    }

    if (netk_window_is_minimized (win))
    {
	g_string_prepend (label, "[");
	g_string_append (label, "]");
    }
	
	tmp = netk_window_get_icon(win);
	if(tmp) {
		gint w, h;
		w = gdk_pixbuf_get_width(tmp);
		h = gdk_pixbuf_get_height(tmp);
		if(w != 22 || h != 22) {
			icon = gdk_pixbuf_scale_simple(tmp, 24, 24, GDK_INTERP_BILINEAR);
			/* the GdkPixbuf returned by netk_window_get_icon() should never be
			 * freed, but if we scale the image, we need to free it */
			*pix_unref_needed = g_list_prepend(*pix_unref_needed, icon);
		} else
			icon = tmp;
	}

	if(icon) {
		GtkWidget *img = gtk_image_new_from_pixbuf(icon);
		gtk_widget_show(img);
		mi = gtk_image_menu_item_new_with_label(label->str);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
	} else
		mi = gtk_menu_item_new_with_label (label->str);

    g_string_free (label, TRUE);

    return mi;
}

static GtkWidget *
create_windowlist_menu (GList **pix_unref_needed)
{
    int i, n;
    GList *windows, *li;
    GtkWidget *menu3, *mi, *label;
    NetkWindow *win;
    NetkWorkspace *ws, *aws;
    GtkStyle *style;

    TRACE ("dummy");
    menu3 = gtk_menu_new ();
    style = gtk_widget_get_style (menu3);

    mi = gtk_menu_item_new_with_label(_("Window list"));
    gtk_widget_set_sensitive(mi, FALSE);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu3), mi);

    windows = netk_screen_get_windows_stacked (netk_screen);
    n = netk_screen_get_workspace_count (netk_screen);
    aws = netk_screen_get_active_workspace (netk_screen);

    for (i = 0; i < n; i++)
    {
	char *ws_name;
	const char *realname;
	gboolean active;

	ws = netk_screen_get_workspace (netk_screen, i);
	realname = netk_workspace_get_name (ws);

	active = (ws == aws);

	if (realname)
	{
	    ws_name = g_strdup_printf ("<i>%s</i>", realname);
	}
	else
	{
	    ws_name = g_strdup_printf ("<i>%d</i>", i + 1);
	}

	mi = gtk_menu_item_new_with_label (ws_name);
	g_free (ws_name);

	label = gtk_bin_get_child (GTK_BIN (mi));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);

	if (active)
	    gtk_widget_set_sensitive (mi, FALSE);

	g_signal_connect_swapped (mi, "activate",
				  G_CALLBACK (netk_workspace_activate), ws);

	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);

	for (li = windows; li; li = li->next)
	{
	    win = li->data;

	    /* sticky windows don;t match the workspace
	     * only show them on the active workspace */
	    if (netk_window_get_workspace (win) != ws &&
		!(active && netk_window_is_sticky (win)))
	    {
		continue;
	    }

	    mi = create_window_list_item (win, pix_unref_needed);

	    if (!mi)
		continue;

	    gtk_widget_show (mi);
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);

	    if (!active)
	    {
		gtk_widget_modify_fg (gtk_bin_get_child (GTK_BIN (mi)),
				      GTK_STATE_NORMAL,
				      &(style->fg[GTK_STATE_INSENSITIVE]));
	    }

	    g_signal_connect (mi, "activate", G_CALLBACK (activate_window),
			      win);
	}

	mi = gtk_separator_menu_item_new ();
	gtk_widget_show (mi);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);
    }

    mi = gtk_menu_item_new_with_label (_("Add workspace"));
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);
    g_signal_connect_swapped (mi, "activate",
			      G_CALLBACK (set_num_screens),
			      GINT_TO_POINTER (n + 1));

    mi = gtk_menu_item_new_with_label (_("Delete workspace"));
    gtk_widget_show (mi);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu3), mi);
    g_signal_connect_swapped (mi, "activate",
			      G_CALLBACK (set_num_screens),
			      GINT_TO_POINTER (n - 1));

    return menu3;
}

/* Popup menu / windowlist
 * -----------------------
*/
void
popup_menu (int button, guint32 time)
{
#ifdef BUILD_DESKTOP_MENU
	GtkWidget *menu_widget;
	
	if(!module_desktop_menu)
		return;
	
	if(!desktop_menu)
		desktop_menu = xfce_desktop_menu_new(NULL, FALSE);
	else if(xfce_desktop_menu_need_update(desktop_menu))
		xfce_desktop_menu_force_regen(desktop_menu);

	if(desktop_menu) {
		menu_widget = xfce_desktop_menu_get_widget(desktop_menu);
		gtk_menu_popup (GTK_MENU (menu_widget), NULL, NULL, NULL, NULL,
				button, time);
	}
#endif
}

void
popup_windowlist (int button, guint32 time)
{
    static GtkWidget *menu = NULL;
	static GList *pix_unref_needed = NULL, *l;

    if (menu)
    {
	gtk_widget_destroy (menu);
    }
	
	if(pix_unref_needed) {
		for(l=pix_unref_needed; l; l=l->next)
			g_object_unref(G_OBJECT(l->data));
		g_list_free(pix_unref_needed);
		pix_unref_needed = NULL;
	}

    windowlist = menu = create_windowlist_menu (&pix_unref_needed);

    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
}

static gboolean
button_press (GtkWidget * w, GdkEventButton * bevent)
{
    int button = bevent->button;
    int state = bevent->state;
    gboolean handled = FALSE;

    DBG ("button press (0x%x)", button);

    if (button == 2 || (button == 1 && state & GDK_SHIFT_MASK &&
			state & GDK_CONTROL_MASK))
    {
	popup_windowlist (button, bevent->time);
	handled = TRUE;
    }
#ifdef BUILD_DESKTOP_MENU
    else if (button == 3 || (button == 1 && state & GDK_SHIFT_MASK))
    {
	popup_menu (button, bevent->time);
	handled = TRUE;
    }
#endif

    return handled;
}

static gboolean
button_scroll (GtkWidget * w, GdkEventScroll * sevent)
{
    GdkScrollDirection direction = sevent->direction;
    NetkWorkspace *ws = NULL;
    gint n, active;

    DBG ("scroll");

    n = netk_screen_get_workspace_count (netk_screen);

    if (n <= 1)
	return FALSE;

    ws = netk_screen_get_active_workspace (netk_screen);
    active = netk_workspace_get_number (ws);

    if (direction == GDK_SCROLL_UP || direction == GDK_SCROLL_LEFT)
    {
	ws = (active > 0) ?
	    netk_screen_get_workspace (netk_screen, active - 1) :
	    netk_screen_get_workspace (netk_screen, n - 1);
    }
    else
    {
	ws = (active < n - 1) ?
	    netk_screen_get_workspace (netk_screen, active + 1) :
	    netk_screen_get_workspace (netk_screen, 0);
    }

    netk_workspace_activate (ws);
    return TRUE;
}

void
menu_force_regen()
{
#ifdef USE_DESKTOP_MENU	
	if(!module_desktop_menu)
		return;

	if(!desktop_menu)
		desktop_menu = xfce_desktop_menu_new(NULL, FALSE);
	else
		xfce_desktop_menu_force_regen(desktop_menu);
#endif
}

/*  Initialization 
 *  --------------
*/
void
menu_init (XfceDesktop * xfdesktop)
{	
    TRACE ("dummy");
    netk_screen = xfdesktop->netk_screen;

    DBG ("connecting callbacks");

#ifdef HAVE_SIGNAL_H
	signal(SIGCHLD, SIG_IGN);
#endif
	
    g_signal_connect (xfdesktop->fullscreen, "button-press-event",
		      G_CALLBACK (button_press), NULL);

    g_signal_connect (xfdesktop->fullscreen, "scroll-event",
		      G_CALLBACK (button_scroll), NULL);
#if USE_DESKTOP_MENU
	if((module_desktop_menu=xfce_desktop_menu_stub_init())) {
		desktop_menu = xfce_desktop_menu_new(NULL, TRUE);
		xfce_desktop_menu_start_autoregen(desktop_menu, 10);
	} else
		g_warning("%s: Unable to initialise menu module. Right-click menu will be unavailable.\n", PACKAGE);
#endif
}

void
menu_load_settings (XfceDesktop * xfdesktop)
{
    TRACE ("dummy");
}

void
menu_cleanup(XfceDesktop *xfdesktop)
{
#ifdef USE_DESKTOP_MENU
	if(module_desktop_menu) {
		if(desktop_menu) {
			xfce_desktop_menu_stop_autoregen(desktop_menu);
			xfce_desktop_menu_destroy(desktop_menu);
		}
		xfce_desktop_menu_stub_cleanup_all(module_desktop_menu);
	}
#endif
}
