/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#include "xfdesktop-file-icon.h"

struct _XfdesktopFileIconPrivate
{
    GList *active_jobs;
};

static void xfdesktop_file_icon_class_init(XfdesktopFileIconClass *klass);
static void xfdesktop_file_icon_init(XfdesktopFileIcon *icon);
static void xfdesktop_file_icon_finalize(GObject *obj);


G_DEFINE_ABSTRACT_TYPE(XfdesktopFileIcon, xfdesktop_file_icon,
                       XFDESKTOP_TYPE_ICON)


static void
xfdesktop_file_icon_class_init(XfdesktopFileIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopFileIconPrivate));
    
    gobject_class->finalize = xfdesktop_file_icon_finalize;
}

static void
xfdesktop_file_icon_init(XfdesktopFileIcon *icon)
{
    icon->priv = G_TYPE_INSTANCE_GET_PRIVATE(icon, XFDESKTOP_TYPE_FILE_ICON,
                                             XfdesktopFileIconPrivate);
}

static void
xfdesktop_file_icon_finalize(GObject *obj)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(obj);
    
    if(icon->priv->active_jobs) {
        GList *l;
        ThunarVfsJob *job;
        GCallback cb;
        
        for(l = icon->priv->active_jobs; l; l = l->next) {
            job = THUNAR_VFS_JOB(l->data);
            cb = g_object_get_data(G_OBJECT(job),
                                             "--xfdesktop-file-icon-callback");
            if(cb) {
                gpointer data = g_object_get_data(obj,
                                                  "--xfdesktop-file-icon-data");
                g_signal_handlers_disconnect_by_func(G_OBJECT(job),
                                                     G_CALLBACK(cb),
                                                     data);
                g_object_set_data(G_OBJECT(job),
                                  "--xfdesktop-file-icon-callback", NULL);
            }
            thunar_vfs_job_cancel(job);
            g_object_unref(G_OBJECT(job));
        }
        g_list_free(icon->priv->active_jobs);
    }
    
    G_OBJECT_CLASS(xfdesktop_file_icon_parent_class)->finalize(obj);
}


G_CONST_RETURN ThunarVfsInfo *
xfdesktop_file_icon_peek_info(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), NULL);
    
    if(klass->peek_info)
       return klass->peek_info(icon);
    else
        return NULL;
}

void
xfdesktop_file_icon_update_info(XfdesktopFileIcon *icon,
                                ThunarVfsInfo *info)
{
    XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));
    
    if(klass->update_info)
       klass->update_info(icon, info);
}

gboolean
xfdesktop_file_icon_can_rename_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    
    if(klass->can_rename_file)
       return klass->can_rename_file(icon);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_rename_file(XfdesktopFileIcon *icon,
                                const gchar *new_name)
{
    XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    g_return_val_if_fail(new_name && *new_name, FALSE);
    
    if(klass->rename_file)
       return klass->rename_file(icon, new_name);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_can_delete_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    
    if(klass->can_delete_file)
       return klass->can_delete_file(icon);
    else
        return FALSE;
}

gboolean
xfdesktop_file_icon_delete_file(XfdesktopFileIcon *icon)
{
    XfdesktopFileIconClass *klass = XFDESKTOP_FILE_ICON_GET_CLASS(icon);
    
    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON(icon), FALSE);
    
    if(klass->delete_file)
       return klass->delete_file(icon);
    else
        return FALSE;
}


void
xfdesktop_file_icon_add_active_job(XfdesktopFileIcon *icon,
                                   ThunarVfsJob *job)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon) && job);
    
    icon->priv->active_jobs = g_list_prepend(icon->priv->active_jobs,
                                             g_object_ref(G_OBJECT(job)));
}

gboolean
xfdesktop_file_icon_remove_active_job(XfdesktopFileIcon *icon,
                                      ThunarVfsJob *job)
{
    if(g_list_find(icon->priv->active_jobs, job)) {
        icon->priv->active_jobs = g_list_remove(icon->priv->active_jobs, job);
        g_object_unref(G_OBJECT(job));
        return TRUE;
    } else
        return FALSE;
}
