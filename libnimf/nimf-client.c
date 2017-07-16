/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-client.c
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

#include "nimf-client.h"
#include "nimf-im.h"
#include "nimf-marshalers.h"
#include "nimf-enum-types.h"
#include <gio/gunixsocketaddress.h>
#include <string.h>
#include <unistd.h>
#include <libaudit.h>

static GSource    *nimf_client_socket_source  = NULL;
static GSource    *nimf_client_default_source = NULL;
static GHashTable *nimf_client_table          = NULL;
GMainContext      *nimf_client_socket_context = NULL;
NimfResult        *nimf_client_result         = NULL;
GSocketConnection *nimf_client_connection     = NULL;

G_DEFINE_ABSTRACT_TYPE (NimfClient, nimf_client, G_TYPE_OBJECT);

static gboolean
on_incoming_message (GSocket      *socket,
                     GIOCondition  condition,
                     gpointer      user_data)
{
  g_debug (G_STRLOC ": %s: socket fd:%d", G_STRFUNC, g_socket_get_fd (socket));

  nimf_message_unref (nimf_client_result->reply);
  nimf_client_result->is_dispatched = TRUE;

  if (condition & (G_IO_HUP | G_IO_ERR))
  {
    /* Because two GSource is created over one socket,
     * when G_IO_HUP | G_IO_ERR, callback can run two times.
     * the following code avoid that callback runs two times. */
    GSource *source = g_main_current_source ();

    if (source == nimf_client_default_source)
      g_source_destroy (nimf_client_socket_source);
    else if (source == nimf_client_socket_source)
      g_source_destroy (nimf_client_default_source);

    if (!g_socket_is_closed (socket))
      g_socket_close (socket, NULL);

    nimf_client_result->reply = NULL;

    g_critical (G_STRLOC ": %s: G_IO_HUP | G_IO_ERR", G_STRFUNC);

    return G_SOURCE_REMOVE;
  }

  NimfMessage *message;
  message = nimf_recv_message (socket);
  nimf_client_result->reply = message;
  gboolean retval;

  if (G_UNLIKELY (message == NULL))
  {
    g_critical (G_STRLOC ": NULL message");
    return G_SOURCE_CONTINUE;
  }

  NimfClient *client;
  client = g_hash_table_lookup (nimf_client_table,
                                GUINT_TO_POINTER (message->header->icid));

  switch (message->header->type)
  {
    /* signals */
    case NIMF_MESSAGE_PREEDIT_START:
      g_signal_emit_by_name (NIMF_IM (client), "preedit-start");
      nimf_send_message (socket, client->id, NIMF_MESSAGE_PREEDIT_START_REPLY,
                         NULL, 0, NULL);
      break;
    case NIMF_MESSAGE_PREEDIT_END:
      g_signal_emit_by_name (NIMF_IM (client), "preedit-end");
      nimf_send_message (socket, client->id, NIMF_MESSAGE_PREEDIT_END_REPLY,
                         NULL, 0, NULL);
      break;
    case NIMF_MESSAGE_PREEDIT_CHANGED:
      {
        NimfIM *im = NIMF_IM (client);
        gint    i;

        g_free (im->preedit_string);
        im->preedit_string = g_strndup (message->data,
                                        message->header->data_len - 1 - sizeof (gint));

        gint str_len = strlen (message->data);
        gint n_attr = (message->header->data_len - str_len - 1 - sizeof (gint)) /
                       sizeof (NimfPreeditAttr);

        nimf_preedit_attr_freev (im->preedit_attrs);
        im->preedit_attrs = g_malloc0_n (n_attr + 1, sizeof (NimfPreeditAttr *));

        for (i = 0; i < n_attr; i++)
          im->preedit_attrs[i] = g_memdup (message->data + str_len + 1 + i * sizeof (NimfPreeditAttr),
                                           sizeof (NimfPreeditAttr));

        im->preedit_attrs[n_attr] = NULL;

        im->cursor_pos = *(gint *) (message->data +
                                    message->header->data_len - sizeof (gint));
        g_signal_emit_by_name (im, "preedit-changed");
        nimf_send_message (socket, client->id,
                           NIMF_MESSAGE_PREEDIT_CHANGED_REPLY, NULL, 0, NULL);
      }
      break;
    case NIMF_MESSAGE_COMMIT:
      nimf_message_ref (message);
      g_signal_emit_by_name (NIMF_IM (client), "commit", (const gchar *) message->data);
      nimf_message_unref (message);
      nimf_send_message (socket, client->id, NIMF_MESSAGE_COMMIT_REPLY,
                         NULL, 0, NULL);
      break;
    case NIMF_MESSAGE_RETRIEVE_SURROUNDING:
      g_signal_emit_by_name (NIMF_IM (client), "retrieve-surrounding", &retval);
      nimf_send_message (socket, client->id,
                         NIMF_MESSAGE_RETRIEVE_SURROUNDING_REPLY,
                         &retval, sizeof (gboolean), NULL);
      break;
    case NIMF_MESSAGE_DELETE_SURROUNDING:
      nimf_message_ref (message);
      g_signal_emit_by_name (NIMF_IM (client), "delete-surrounding",
                             ((gint *) message->data)[0],
                             ((gint *) message->data)[1], &retval);
      nimf_message_unref (message);
      nimf_send_message (socket, client->id,
                         NIMF_MESSAGE_DELETE_SURROUNDING_REPLY,
                         &retval, sizeof (gboolean), NULL);
      break;
    case NIMF_MESSAGE_BEEP:
      g_signal_emit_by_name (NIMF_IM (client), "beep");
      nimf_send_message (socket, client->id, NIMF_MESSAGE_BEEP_REPLY,
                         NULL, 0, NULL);
      break;
    /* reply */
    case NIMF_MESSAGE_CREATE_CONTEXT_REPLY:
    case NIMF_MESSAGE_DESTROY_CONTEXT_REPLY:
    case NIMF_MESSAGE_FILTER_EVENT_REPLY:
    case NIMF_MESSAGE_RESET_REPLY:
    case NIMF_MESSAGE_FOCUS_IN_REPLY:
    case NIMF_MESSAGE_FOCUS_OUT_REPLY:
    case NIMF_MESSAGE_SET_SURROUNDING_REPLY:
    case NIMF_MESSAGE_GET_SURROUNDING_REPLY:
    case NIMF_MESSAGE_SET_CURSOR_LOCATION_REPLY:
    case NIMF_MESSAGE_SET_USE_PREEDIT_REPLY:
      break;
    default:
      g_warning (G_STRLOC ": %s: Unknown message type: %d", G_STRFUNC, message->header->type);
      break;
  }

  return G_SOURCE_CONTINUE;
}

gboolean
nimf_client_is_connected ()
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return nimf_client_connection != NULL &&
         g_socket_connection_is_connected (nimf_client_connection);
}

static void
nimf_client_init (NimfClient *client)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  static guint16 next_id = 0;
  guint16 id;

  if (nimf_client_table == NULL)
    nimf_client_table = g_hash_table_new_full (g_direct_hash,
                                               g_direct_equal,
                                               NULL,
                                               (GDestroyNotify) g_object_unref);
  do
    id = next_id++;
  while (id == 0 || g_hash_table_contains (nimf_client_table,
                                           GUINT_TO_POINTER (id)));
  client->id = id;

  g_hash_table_insert (nimf_client_table,
                       GUINT_TO_POINTER (client->id), client);
  g_hash_table_ref (nimf_client_table);
}

static void
nimf_client_constructed (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfClient *client = NIMF_CLIENT (object);
  GMutex mutex;

  g_mutex_init (&mutex);
  g_mutex_lock (&mutex);

  if (nimf_client_result == NULL)
    nimf_client_result = g_slice_new0 (NimfResult);

  if (nimf_client_connection == NULL)
  {
    GSocketClient  *socket_client;
    GSocketAddress *address;
    gint            retry_limit = 4;
    gint            retry_count = 0;
    GSocket        *socket;
    gchar          *addr;
    GError         *error = NULL;
    uid_t           uid;

    uid = audit_getloginuid ();
    if (uid == (uid_t) -1)
      uid = getuid ();

    addr = g_strdup_printf (NIMF_BASE_ADDRESS"%d", uid);
    address = g_unix_socket_address_new_with_type (addr, -1,
                                                   G_UNIX_SOCKET_ADDRESS_ABSTRACT);
    g_free (addr);

    socket_client = g_socket_client_new ();

    for (retry_count = 0; retry_count < retry_limit; retry_count++)
    {
      g_clear_error (&error);
      nimf_client_connection =
        g_socket_client_connect (socket_client, G_SOCKET_CONNECTABLE (address),
                                 NULL, &error);
      if (nimf_client_connection)
        break;
      else
        g_usleep (G_USEC_PER_SEC);;
    }

    g_object_unref (address);
    g_object_unref (socket_client);

    if (nimf_client_connection == NULL)
    {
      g_critical (G_STRLOC ": %s: %s", G_STRFUNC, error->message);
      g_clear_error (&error);
      return;
    }

    g_object_add_weak_pointer (G_OBJECT (nimf_client_connection),
                               (gpointer) &nimf_client_connection);
    socket = g_socket_connection_get_socket (nimf_client_connection);

    if (!socket)
    {
      g_critical (G_STRLOC ": %s: %s", G_STRFUNC, "Can't get socket");
      return;
    }

    if (G_UNLIKELY (nimf_client_socket_context == NULL))
    {
      nimf_client_socket_context = g_main_context_new ();

      /* when g_main_context_iteration(), iterate only socket */
      nimf_client_socket_source = g_socket_create_source (socket, G_IO_IN, NULL);
      g_source_set_can_recurse (nimf_client_socket_source, TRUE);
      g_source_set_callback (nimf_client_socket_source,
                             (GSourceFunc) on_incoming_message, NULL, NULL);
      g_source_attach (nimf_client_socket_source, nimf_client_socket_context);

      nimf_client_default_source = g_socket_create_source (socket, G_IO_IN, NULL);
      g_source_set_can_recurse (nimf_client_default_source, TRUE);
      g_source_set_callback (nimf_client_default_source,
                             (GSourceFunc) on_incoming_message, NULL, NULL);
      g_source_attach (nimf_client_default_source, NULL);
    }
  }
  else
    g_object_ref (nimf_client_connection);

  GSocket *socket = g_socket_connection_get_socket (nimf_client_connection);
  if (!socket || g_socket_is_closed (socket))
  {
    g_warning ("socket is closed");
    return;
  }

  nimf_send_message (socket, client->id, NIMF_MESSAGE_CREATE_CONTEXT,
                     NULL, 0, NULL);
  nimf_result_iteration_until (nimf_client_result, nimf_client_socket_context,
                               client->id, NIMF_MESSAGE_CREATE_CONTEXT_REPLY);

  g_mutex_unlock (&mutex);

  return;
}

static void
nimf_client_finalize (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfClient *client = NIMF_CLIENT (object);

  if (nimf_client_table)
    g_hash_table_steal (nimf_client_table, GUINT_TO_POINTER (client->id));

  if (nimf_client_connection)
  {
    GSocket *socket;
    socket = g_socket_connection_get_socket (nimf_client_connection);

    if (socket && !g_socket_is_closed (socket))
    {
      nimf_send_message (socket, client->id, NIMF_MESSAGE_DESTROY_CONTEXT,
                         NULL, 0, NULL);
      nimf_result_iteration_until (nimf_client_result, nimf_client_socket_context,
                                   client->id, NIMF_MESSAGE_DESTROY_CONTEXT_REPLY);
    }

    g_object_unref (nimf_client_connection);

    if (nimf_client_connection == NULL)
    {
      g_source_destroy (nimf_client_socket_source);
      g_source_destroy (nimf_client_default_source);
      g_source_unref (nimf_client_socket_source);
      g_source_unref (nimf_client_default_source);
      g_main_context_unref (nimf_client_socket_context);
      g_slice_free (NimfResult, nimf_client_result);
      g_hash_table_unref (nimf_client_table);
      nimf_client_socket_source  = NULL;
      nimf_client_default_source = NULL;
      nimf_client_socket_context = NULL;
      nimf_client_result = NULL;
      nimf_client_table  = NULL;
    }
  }

  G_OBJECT_CLASS (nimf_client_parent_class)->finalize (object);
}

static void
nimf_client_class_init (NimfClientClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = nimf_client_finalize;
  object_class->constructed  = nimf_client_constructed;
}
