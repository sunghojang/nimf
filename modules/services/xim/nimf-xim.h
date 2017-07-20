/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-xim.h
 * This file is part of Nimf.
 *
 * Copyright (C) 2015-2017 Hodong Kim <cogniti@gmail.com>
 *
 * Nimf is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nimf is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NIMF_XIM_H__
#define __NIMF_XIM_H__

#include "config.h"
#include <nimf.h>
#include <glib/gi18n.h>
#include <X11/XKBlib.h>
#include "IMdkit/Xi18n.h"
#include "nimf-xim-im.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NIMF_TYPE_XIM               (nimf_xim_get_type ())
#define NIMF_XIM(object)            (G_TYPE_CHECK_INSTANCE_CAST ((object), NIMF_TYPE_XIM, NimfXim))
#define NIMF_XIM_CLASS(class)       (G_TYPE_CHECK_CLASS_CAST ((class), NIMF_TYPE_XIM, NimfXimClass))
#define NIMF_IS_XIM(object)         (G_TYPE_CHECK_INSTANCE_TYPE ((object), NIMF_TYPE_XIM))
#define NIMF_IS_XIM_CLASS(class)    (G_TYPE_CHECK_CLASS_TYPE ((class), NIMF_TYPE_XIM))
#define NIMF_XIM_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), NIMF_TYPE_XIM, NimfXimClass))

typedef struct _NimfXim      NimfXim;
typedef struct _NimfXimClass NimfXimClass;

struct _NimfXim
{
  NimfService parent_instance;

  GSource    *xevent_source;
  gchar      *id;
  GHashTable *ims;
  guint16     next_icid;
  XIMS        xims;
  GtkWidget  *window;
  GtkWidget  *entry;
  gboolean    ignore_xim_preedit_callbacks;
  GSettings  *settings;
  gboolean    active;

  Display *display;
  Window   im_window;
  Atom     atom_xconnect;
  Atom     atom_protocol;
};

struct _NimfXimClass
{
  NimfServiceClass parent_class;
};

GType nimf_xim_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __NIMF_XIM_H__ */
