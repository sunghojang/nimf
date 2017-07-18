/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-xim.c
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

#include "nimf-xim.h"
#include "IMdkit/i18nMethod.h"
#include "IMdkit/i18nX.h"

G_DEFINE_DYNAMIC_TYPE (NimfXim, nimf_xim, NIMF_TYPE_SERVICE);

static void nimf_xim_set_engine_by_id (NimfService *service,
                                       const gchar *engine_id)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GHashTableIter iter;
  gpointer       im;

  g_hash_table_iter_init (&iter, NIMF_XIM (service)->ims);

  while (g_hash_table_iter_next (&iter, NULL, &im))
    nimf_service_im_set_engine_by_id (im, engine_id);
}

static guint16
nimf_xim_add_im (NimfXim   *xim,
                 NimfXimIM *xim_im)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  guint16 icid;

  do
    icid = xim->next_icid++;
  while (icid == 0 || g_hash_table_contains (xim->ims,
                                             GUINT_TO_POINTER (icid)));
  NIMF_SERVICE_IM (xim_im)->icid = icid;
  g_hash_table_insert (xim->ims, GUINT_TO_POINTER (icid), xim_im);

  return icid;
}

static void nimf_xim_set_cursor_location (NimfXim          *xim,
                                          IMChangeICStruct *data)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfServiceIM *im;
  NimfXimIM     *xim_im;
  int            x = 0, y = 0, height = 0;

  im = g_hash_table_lookup (xim->ims, GUINT_TO_POINTER (data->icid));
  xim_im = NIMF_XIM_IM (im);

  Window window;

  if (xim_im->focus_window)
    window = xim_im->focus_window;
  else
    window = xim_im->client_window;

  if (window)
  {
    Window child;
    XWindowAttributes attr;
    XGetWindowAttributes (xim->xims->core.display, window, &attr);
    XTranslateCoordinates(xim->xims->core.display, window, attr.root,
                          0, attr.height, &x, &y, &child);
  }

  if (gtk_widget_is_visible (xim->window))
  {
    gtk_window_get_size (GTK_WINDOW (xim->window), NULL, &height);

    if (window)
      gtk_window_move (GTK_WINDOW (xim->window), x, y - height);
    else
      gtk_window_move (GTK_WINDOW (xim->window), 0, 0);
  }
}

static int nimf_xim_set_ic_values (NimfXim          *xim,
                                   IMChangeICStruct *data)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfServiceIM *im;
  NimfXimIM     *xim_im;
  CARD16 i;

  im = g_hash_table_lookup (xim->ims, GUINT_TO_POINTER (data->icid));
  xim_im = NIMF_XIM_IM (im);

  for (i = 0; i < data->ic_attr_num; i++)
  {
    if (g_strcmp0 (XNInputStyle, data->ic_attr[i].name) == 0)
      xim_im->input_style = *(CARD32*) data->ic_attr[i].value;
    else if (g_strcmp0 (XNClientWindow, data->ic_attr[i].name) == 0)
      xim_im->client_window = *(Window *) data->ic_attr[i].value;
    else if (g_strcmp0 (XNFocusWindow, data->ic_attr[i].name) == 0)
      xim_im->focus_window = *(Window *) data->ic_attr[i].value;
    else
      g_warning (G_STRLOC ": %s %s", G_STRFUNC, data->ic_attr[i].name);
  }

  for (i = 0; i < data->preedit_attr_num; i++)
  {
    if (g_strcmp0 (XNPreeditState, data->preedit_attr[i].name) == 0)
    {
      XIMPreeditState state = *(XIMPreeditState *) data->preedit_attr[i].value;
      switch (state)
      {
        case XIMPreeditEnable:
          nimf_service_im_set_use_preedit (im, TRUE);
          break;
        case XIMPreeditDisable:
          nimf_service_im_set_use_preedit (im, FALSE);
          break;
        case XIMPreeditUnKnown:
          break;
        default:
          g_warning (G_STRLOC ": %s: XIMPreeditState: %ld is ignored",
                     G_STRFUNC, state);
          break;
      }
    }
    else
      g_critical (G_STRLOC ": %s: %s is ignored",
                  G_STRFUNC, data->preedit_attr[i].name);
  }

  for (i = 0; i < data->status_attr_num; i++)
  {
    g_critical (G_STRLOC ": %s: %s is ignored",
                G_STRFUNC, data->status_attr[i].name);
  }

  return 1;
}

static int nimf_xim_create_ic (NimfXim          *xim,
                               IMChangeICStruct *data)
{
  g_debug (G_STRLOC ": %s, data->connect_id: %d", G_STRFUNC, data->connect_id);

  NimfXimIM *xim_im;
  xim_im = g_hash_table_lookup (xim->ims, GUINT_TO_POINTER (data->icid));

  if (!xim_im)
  {
    xim_im = nimf_xim_im_new (NIMF_SERVICE (xim)->server, xim);
    xim_im->connect_id = data->connect_id;
    data->icid = nimf_xim_add_im (xim, xim_im);
    g_debug (G_STRLOC ": icid = %d", data->icid);
  }

  nimf_xim_set_ic_values (xim, data);

  return 1;
}

static int nimf_xim_destroy_ic (NimfXim           *xim,
                                IMDestroyICStruct *data)
{
  g_debug (G_STRLOC ": %s, data->icid = %d", G_STRFUNC, data->icid);

  return g_hash_table_remove (xim->ims, GUINT_TO_POINTER (data->icid));
}

static int nimf_xim_get_ic_values (NimfXim          *xim,
                                   IMChangeICStruct *data)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfServiceIM *im;
  im = g_hash_table_lookup (xim->ims, GUINT_TO_POINTER (data->icid));
  CARD16 i;

  for (i = 0; i < data->ic_attr_num; i++)
  {
    if (g_strcmp0 (XNFilterEvents, data->ic_attr[i].name) == 0)
    {
      data->ic_attr[i].value_length = sizeof (CARD32);
      data->ic_attr[i].value = g_malloc (sizeof (CARD32));
      *(CARD32 *) data->ic_attr[i].value = KeyPressMask | KeyReleaseMask;
    }
    else if (g_strcmp0 (XNSeparatorofNestedList, data->ic_attr[i].name) == 0)
    {
      data->ic_attr[i].value_length = sizeof (CARD16);
      data->ic_attr[i].value = g_malloc (sizeof (CARD16));
      *(CARD16 *) data->ic_attr[i].value = 0;
    }
    else
      g_critical (G_STRLOC ": %s: %s is ignored",
                  G_STRFUNC, data->ic_attr[i].name);
  }

  for (i = 0; i < data->preedit_attr_num; i++)
  {
    if (g_strcmp0 (XNPreeditState, data->preedit_attr[i].name) == 0)
    {
      data->preedit_attr[i].value_length = sizeof (XIMPreeditState);
      data->preedit_attr[i].value = g_malloc (sizeof (XIMPreeditState));

      if (im->use_preedit)
        *(XIMPreeditState *) data->preedit_attr[i].value = XIMPreeditEnable;
      else
        *(XIMPreeditState *) data->preedit_attr[i].value = XIMPreeditDisable;
    }
    else
      g_critical (G_STRLOC ": %s: %s is ignored",
                  G_STRFUNC, data->preedit_attr[i].name);
  }

  for (i = 0; i < data->status_attr_num; i++)
    g_critical (G_STRLOC ": %s: %s is ignored",
                G_STRFUNC, data->status_attr[i].name);

  return 1;
}

static int nimf_xim_forward_event (NimfXim              *xim,
                                   IMForwardEventStruct *data)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  XKeyEvent        *xevent;
  NimfEvent        *event;
  gboolean          retval;
  KeySym            keysym;
  unsigned int      consumed;
  NimfModifierType  state;

  xevent = (XKeyEvent*) &(data->event);

  event = nimf_event_new (NIMF_EVENT_NOTHING);

  if (xevent->type == KeyPress)
    event->key.type = NIMF_EVENT_KEY_PRESS;
  else
    event->key.type = NIMF_EVENT_KEY_RELEASE;

  event->key.state = (NimfModifierType) xevent->state;
  event->key.keyval = NIMF_KEY_VoidSymbol;
  event->key.hardware_keycode = xevent->keycode;

  XkbLookupKeySym (xim->xims->core.display,
                   event->key.hardware_keycode,
                   event->key.state,
                   &consumed, &keysym);
  event->key.keyval = (guint) keysym;

  state = event->key.state & ~consumed;
  event->key.state |= (NimfModifierType) state;

  NimfServiceIM *im;
  im = g_hash_table_lookup (xim->ims, GUINT_TO_POINTER (data->icid));
  retval  = nimf_service_im_filter_event (im, event);
  nimf_event_free (event);

  if (G_UNLIKELY (!retval))
    IMForwardEvent (xim->xims, (XPointer) data);
  else
    nimf_xim_set_cursor_location (xim, (IMChangeICStruct *) data);

  return 1;
}

static int nimf_xim_set_ic_focus (NimfXim             *xim,
                                  IMChangeFocusStruct *data)
{
  NimfServiceIM *im;
  im = g_hash_table_lookup (xim->ims, GUINT_TO_POINTER (data->icid));

  g_debug (G_STRLOC ": %s, icid = %d, connection id = %d",
           G_STRFUNC, data->icid, im->icid);

  nimf_service_im_focus_in (im);

  return 1;
}

static int nimf_xim_unset_ic_focus (NimfXim             *xim,
                                    IMChangeFocusStruct *data)
{
  NimfServiceIM *im;
  im = g_hash_table_lookup (xim->ims, GUINT_TO_POINTER (data->icid));

  g_debug (G_STRLOC ": %s, icid = %d", G_STRFUNC, data->icid);

  nimf_service_im_focus_out (im);

  return 1;
}

static int nimf_xim_reset_ic (NimfXim         *xim,
                              IMResetICStruct *data)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfServiceIM *im;
  im = g_hash_table_lookup (xim->ims, GUINT_TO_POINTER (data->icid));
  nimf_service_im_reset (im);

  return 1;
}

static int
on_incoming_message (XIMS        xims,
                     IMProtocol *data,
                     NimfXim    *xim)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_val_if_fail (xims != NULL, True);
  g_return_val_if_fail (data != NULL, True);

  if (!NIMF_IS_XIM (xim))
    g_error ("Invalid IMUserData");

  int retval;

  switch (data->major_code)
  {
    case XIM_OPEN:
      g_debug (G_STRLOC ": XIM_OPEN: connect_id: %u", data->imopen.connect_id);
      retval = 1;
      break;
    case XIM_CLOSE:
      g_debug (G_STRLOC ": XIM_CLOSE: connect_id: %u",
               data->imclose.connect_id);
      retval = 1;
      break;
    case XIM_PREEDIT_START_REPLY:
      g_debug (G_STRLOC ": XIM_PREEDIT_START_REPLY");
      retval = 1;
      break;
    case XIM_CREATE_IC:
      retval = nimf_xim_create_ic (xim, &data->changeic);
      break;
    case XIM_DESTROY_IC:
      retval = nimf_xim_destroy_ic (xim, &data->destroyic);
      break;
    case XIM_SET_IC_VALUES:
      retval = nimf_xim_set_ic_values (xim, &data->changeic);
      break;
    case XIM_GET_IC_VALUES:
      retval = nimf_xim_get_ic_values (xim, &data->changeic);
      break;
    case XIM_FORWARD_EVENT:
      retval = nimf_xim_forward_event (xim, &data->forwardevent);
      break;
    case XIM_SET_IC_FOCUS:
      retval = nimf_xim_set_ic_focus (xim, &data->changefocus);
      break;
    case XIM_UNSET_IC_FOCUS:
      retval = nimf_xim_unset_ic_focus (xim, &data->changefocus);
      break;
    case XIM_RESET_IC:
      retval = nimf_xim_reset_ic (xim, &data->resetic);
      break;
    default:
      g_warning (G_STRLOC ": %s: major op code %d not handled", G_STRFUNC,
                 data->major_code);
      retval = 0;
      break;
  }

  return retval;
}

static int
on_xerror (Display *display, XErrorEvent *error)
{
  gchar err_msg[64];

  XGetErrorText (display, error->error_code, err_msg, 63);
  g_warning (G_STRLOC ": %s: XError: %s "
    "serial: %lu, error_code: %d request_code: %d minor_code: %d resourceid=%lu",
    G_STRFUNC, err_msg, error->serial, error->error_code, error->request_code,
    error->minor_code, error->resourceid);

  return 1;
}

typedef struct
{
  GSource source;
  XIMS    xims;
  GPollFD poll_fd;
} NimfXEventSource;

static gboolean nimf_xevent_source_prepare (GSource *source,
                                            gint    *timeout)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  Display *display = ((NimfXEventSource *) source)->xims->core.display;
  *timeout = -1;
  return XPending (display) > 0;
}

static gboolean nimf_xevent_source_check (GSource *source)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfXEventSource *display_source = (NimfXEventSource *) source;

  if (display_source->poll_fd.revents & G_IO_IN)
    return XPending (display_source->xims->core.display) > 0;
  else
    return FALSE;
}

static gboolean nimf_xevent_source_dispatch (GSource     *source,
                                             GSourceFunc  callback,
                                             gpointer     user_data)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  XIMS     xims    = ((NimfXEventSource*) source)->xims;
  Display *display = xims->core.display;
  XEvent   event;

  while (XPending (display) > 0)
  {
    XNextEvent (display, &event);
    if (!XFilterEvent (&event, None))
    {
      switch (event.type)
      {
        case SelectionRequest:
          WaitXSelectionRequest (display, &event, (XPointer) xims);
          break;
        case ClientMessage:
          {
            XClientMessageEvent cme = *(XClientMessageEvent *) &event;

            if (cme.message_type == XInternAtom (display, "_XIM_XCONNECT", False))
              WaitXConnectMessage (display, &event, (XPointer) xims);
            else if (cme.message_type == XInternAtom (display, "_XIM_PROTOCOL", False))
              WaitXIMProtocol (display, &event, (XPointer) xims);
          }
          break;
        case MappingNotify:
          g_message (G_STRLOC ": %s: MappingNotify", G_STRFUNC);
          break;
        default:
          g_warning (G_STRLOC ": %s: event type: %d not filtered",
                     G_STRFUNC, event.type);
          break;
      }
    }
  }

  return TRUE;
}

static void nimf_xevent_source_finalize (GSource *source)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}

static GSourceFuncs event_funcs = {
  nimf_xevent_source_prepare,
  nimf_xevent_source_check,
  nimf_xevent_source_dispatch,
  nimf_xevent_source_finalize
};

static GSource *nimf_xevent_source_new (XIMS xims)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GSource *source;
  NimfXEventSource *xevent_source;

  source = g_source_new (&event_funcs, sizeof (NimfXEventSource));
  xevent_source = (NimfXEventSource *) source;
  xevent_source->xims = xims;

  xevent_source->poll_fd.fd = ConnectionNumber (xevent_source->xims->core.display);
  xevent_source->poll_fd.events = G_IO_IN;
  g_source_add_poll (source, &xevent_source->poll_fd);

  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_can_recurse (source, TRUE);

  return source;
}

static gboolean nimf_xim_start (NimfService *service)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfXim *xim = NIMF_XIM (service);
  Display *display;
  Window   window;
  XIMS     xims;

  display = XOpenDisplay (NULL);

  if (display == NULL)
  {
    g_warning (G_STRLOC ": %s: Can't open display", G_STRFUNC);
    return FALSE;
  }

  XIMStyle im_styles [] = {
    XIMPreeditPosition  | XIMStatusNothing,
    XIMPreeditCallbacks | XIMStatusNothing,
    XIMPreeditNothing   | XIMStatusNothing,
    XIMPreeditPosition  | XIMStatusCallbacks,
    XIMPreeditCallbacks | XIMStatusCallbacks,
    XIMPreeditNothing   | XIMStatusCallbacks,
    XIMPreeditCallbacks | XIMStatusNone, /* on-the-spot */
    XIMPreeditNothing   | XIMStatusNone, /* on-root-window */
    XIMPreeditNone      | XIMStatusNone, /* do not anyhing */
    0
  };

  XIMEncoding ims_encodings[] = {
    "COMPOUND_TEXT",
    NULL
  };

  XIMStyles    styles;
  XIMEncodings encodings;

  styles.count_styles = sizeof (im_styles) / sizeof (XIMStyle) - 1;
  styles.supported_styles = im_styles;

  encodings.count_encodings = sizeof (ims_encodings) / sizeof (XIMEncoding) - 1;
  encodings.supported_encodings = ims_encodings;

  XSetWindowAttributes attrs;

  attrs.event_mask = KeyPressMask | KeyReleaseMask;
  attrs.override_redirect = True;

  window = XCreateWindow (display,      /* Display *display */
                          DefaultRootWindow (display),  /* Window parent */
                          0, 0,         /* int x, y */
                          1, 1,         /* unsigned int width, height */
                          0,            /* unsigned int border_width */
                          0,            /* int depth */
                          InputOutput,  /* unsigned int class */
                          CopyFromParent, /* Visual *visual */
                          CWOverrideRedirect | CWEventMask, /* unsigned long valuemask */
                          &attrs);      /* XSetWindowAttributes *attributes */

  xims = IMOpenIM (display,
                   IMModifiers,        "Xi18n",
                   IMServerWindow,     window,
                   IMServerName,       PACKAGE,
                   IMLocale,           "C,en,ja,ko,zh", /* FIXME: Make get_supported_locales() */
                   IMServerTransport,  "X/",
                   IMInputStyles,      &styles,
                   IMEncodingList,     &encodings,
                   IMProtocolHandler,  on_incoming_message,
                   IMUserData,         xim,
                   IMFilterEventMask,  KeyPressMask | KeyReleaseMask,
                   NULL);

  if (!xims)
  {
    g_warning (G_STRLOC ": %s: XIM is not started.", G_STRFUNC);
    return FALSE;
  }

  xim->xims = xims;
  xim->xevent_source = nimf_xevent_source_new (xims);
  g_source_attach (xim->xevent_source, NULL);
  XSetErrorHandler (on_xerror);

  return TRUE;
}

static void nimf_xim_stop (NimfService *service)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  /* TODO FIXME */
  /* source */
  /* imcloseim */
  /* xclosedisplay */
}

static const gchar *
nimf_xim_get_id (NimfService *service)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_val_if_fail (NIMF_IS_SERVICE (service), NULL);

  return NIMF_XIM (service)->id;
}

static void
on_changed_ignore_xim_preedit_callbacks (GSettings *settings,
                                         gchar     *key,
                                         NimfXim   *xim)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  xim->ignore_xim_preedit_callbacks =
    g_settings_get_boolean (xim->settings, key);
}

static void
nimf_xim_init (NimfXim *xim)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  xim->id = g_strdup ("nimf-xim");
  xim->ims = g_hash_table_new_full (g_direct_hash,
                                         g_direct_equal,
                                         NULL,
                                         (GDestroyNotify) g_object_unref);

  xim->settings = g_settings_new ("org.nimf.services.xim");
  xim->ignore_xim_preedit_callbacks =
    g_settings_get_boolean (xim->settings, "ignore-xim-preedit-callbacks");

  g_signal_connect (xim->settings, "changed::ignore-xim-preedit-callbacks",
                    G_CALLBACK (on_changed_ignore_xim_preedit_callbacks), xim);

  gtk_init (NULL, NULL);
  /* gtk entry */
  xim->entry = gtk_entry_new ();
  gtk_editable_set_editable (GTK_EDITABLE (xim->entry), FALSE);
  /* gtk window */
  xim->window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_type_hint (GTK_WINDOW (xim->window),
                            GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  gtk_container_set_border_width (GTK_CONTAINER (xim->window), 1);
  gtk_container_add (GTK_CONTAINER (xim->window), xim->entry);
  gtk_window_move (GTK_WINDOW (xim->window), 0, 0);
}

static void nimf_xim_finalize (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfXim *xim = NIMF_XIM (object);

  g_hash_table_unref (xim->ims);
  g_free (xim->id);
  gtk_widget_destroy (xim->window);
  g_object_unref (xim->settings);

  if (xim->xevent_source)
  {
    g_source_destroy (xim->xevent_source);
    g_source_unref   (xim->xevent_source);
  }

  G_OBJECT_CLASS (nimf_xim_parent_class)->finalize (object);
}

static void
nimf_xim_class_init (NimfXimClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GObjectClass     *object_class  = G_OBJECT_CLASS (class);
  NimfServiceClass *service_class = NIMF_SERVICE_CLASS (class);

  service_class->get_id           = nimf_xim_get_id;
  service_class->start            = nimf_xim_start;
  service_class->stop             = nimf_xim_stop;
  service_class->set_engine_by_id = nimf_xim_set_engine_by_id;

  object_class->finalize = nimf_xim_finalize;
}

static void
nimf_xim_class_finalize (NimfXimClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}

void module_register_type (GTypeModule *type_module)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  nimf_xim_register_type (type_module);
}

GType module_get_type ()
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return nimf_xim_get_type ();
}
