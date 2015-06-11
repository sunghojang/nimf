/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*-  */
/*
 * dasom-jeongeum.c
 * This file is part of Dasom.
 *
 * Copyright (C) 2015 Hodong Kim <hodong@cogno.org>
 *
 * Dasom is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Dasom is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dasom.h"
#include "dasom-jeongeum.h"

G_DEFINE_DYNAMIC_TYPE (DasomJeongeum, dasom_jeongeum, DASOM_TYPE_ENGINE);

guint dasom_event_keycode_to_qwerty_keyval (const DasomEvent *event)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  guint keyval = 0;
  gboolean is_shift = event->key.state & DASOM_SHIFT_MASK;

  switch (event->key.hardware_keycode)
  {
    /* 24 q ~ 35 ] */
    case 24: keyval = is_shift ? 'Q' : 'q'; break;
    case 25: keyval = is_shift ? 'W' : 'w'; break;
    case 26: keyval = is_shift ? 'E' : 'e'; break;
    case 27: keyval = is_shift ? 'R' : 'r'; break;
    case 28: keyval = is_shift ? 'T' : 't'; break;
    case 29: keyval = is_shift ? 'Y' : 'y'; break;
    case 30: keyval = is_shift ? 'U' : 'u'; break;
    case 31: keyval = is_shift ? 'I' : 'i'; break;
    case 32: keyval = is_shift ? 'O' : 'o'; break;
    case 33: keyval = is_shift ? 'P' : 'p'; break;
    case 34: keyval = is_shift ? '{' : '['; break;
    case 35: keyval = is_shift ? '}' : ']'; break;
    /* 38 a ~ 48 ' */
    case 38: keyval = is_shift ? 'A' : 'a'; break;
    case 39: keyval = is_shift ? 'S' : 's'; break;
    case 40: keyval = is_shift ? 'D' : 'd'; break;
    case 41: keyval = is_shift ? 'F' : 'f'; break;
    case 42: keyval = is_shift ? 'G' : 'g'; break;
    case 43: keyval = is_shift ? 'H' : 'h'; break;
    case 44: keyval = is_shift ? 'J' : 'j'; break;
    case 45: keyval = is_shift ? 'K' : 'k'; break;
    case 46: keyval = is_shift ? 'L' : 'l'; break;
    case 47: keyval = is_shift ? ':' : ';'; break;
    case 48: keyval = is_shift ? '"' : ','; break;
    /* 52 z ~ 61 ? */
    case 52: keyval = is_shift ? 'Z' : 'z'; break;
    case 53: keyval = is_shift ? 'X' : 'x'; break;
    case 54: keyval = is_shift ? 'C' : 'c'; break;
    case 55: keyval = is_shift ? 'V' : 'v'; break;
    case 56: keyval = is_shift ? 'B' : 'b'; break;
    case 57: keyval = is_shift ? 'N' : 'n'; break;
    case 58: keyval = is_shift ? 'M' : 'm'; break;
    case 59: keyval = is_shift ? '<' : ','; break;
    case 60: keyval = is_shift ? '>' : '.'; break;
    case 61: keyval = is_shift ? '?' : '/'; break;
    default: keyval = event->key.keyval;    break;
  }

  return keyval;
}

void
dasom_jeongeum_reset (DasomEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_if_fail (DASOM_IS_ENGINE (engine));

  DasomJeongeum *jeongeum = DASOM_JEONGEUM (engine);

  const ucschar *flush;
  flush = hangul_ic_flush (jeongeum->context);

  if (flush[0] == 0)
    return;

  gchar *text = NULL;

  text = g_ucs4_to_utf8 (flush, -1, NULL, NULL, NULL);

  if (jeongeum->preedit_string != NULL)
  {
    g_free (jeongeum->preedit_string);
    jeongeum->preedit_string = NULL;
    dasom_engine_emit_preedit_changed (engine);
    dasom_engine_emit_preedit_end (engine);
  }

  dasom_engine_emit_commit (engine, text);

  g_free (text);
}

void
dasom_jeongeum_focus_in (DasomEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_if_fail (DASOM_IS_ENGINE (engine));
}

void
dasom_jeongeum_focus_out (DasomEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_if_fail (DASOM_IS_ENGINE (engine));

  dasom_jeongeum_reset (engine);
}

gboolean
dasom_jeongeum_filter_event (DasomEngine      *engine,
                             const DasomEvent *event)
{
  g_debug (G_STRLOC ": %s:keyval:%d\t hardware_keycode:%d",
           G_STRFUNC,
           event->key.keyval,
           event->key.hardware_keycode);

  guint keyval;
  gboolean retval;

  DasomJeongeum *jeongeum = DASOM_JEONGEUM (engine);

  if (event->key.type == DASOM_EVENT_KEY_RELEASE)
    return FALSE;

  if (event->key.keyval == DASOM_KEY_Shift_L ||
      event->key.keyval == DASOM_KEY_Shift_R)
    return FALSE;

  if (event->key.state & (DASOM_CONTROL_MASK | DASOM_MOD1_MASK))
  {
    dasom_jeongeum_reset (engine);
    return FALSE;
  }

  const ucschar *commit = hangul_ic_get_commit_string (jeongeum->context);
  const ucschar *preedit = hangul_ic_get_preedit_string (jeongeum->context);

  gchar *new_commit  = NULL;
  gchar *new_preedit = NULL;
  gchar *old_preedit = g_strdup (jeongeum->preedit_string);

  if (event->key.keyval == DASOM_KEY_BackSpace)
  {
    retval = hangul_ic_backspace (jeongeum->context);

    if (retval)
    {
      gchar *new_preedit = NULL;
      const ucschar *preedit = hangul_ic_get_preedit_string (jeongeum->context);

      if (preedit[0] != 0)
        new_preedit = g_ucs4_to_utf8 (preedit, -1, NULL, NULL, NULL);

      if (g_strcmp0 (old_preedit, new_preedit) != 0)
      {
        g_free (jeongeum->preedit_string);
        jeongeum->preedit_string = new_preedit;
        dasom_engine_emit_preedit_changed (engine);
      }

      if (old_preedit != NULL && preedit[0] == 0)
        dasom_engine_emit_preedit_end (engine);
    }
    return retval;
  }

  keyval = dasom_event_keycode_to_qwerty_keyval (event);
  retval = hangul_ic_process (jeongeum->context, keyval);

  if (retval)
    g_print (">>>>>>>>>>>>>>>> retval TRUE\n");
  else
    g_print (">>>>>>>>>>>>>>>> retval FALSE\n");

  if (commit[0] != 0)
    new_commit  = g_ucs4_to_utf8 (commit, -1, NULL, NULL, NULL);

  if (preedit[0] != 0)
    new_preedit = g_ucs4_to_utf8 (preedit, -1, NULL, NULL, NULL);

  if (commit[0] == 0)
  {
    if (old_preedit == NULL && preedit[0] != 0)
      dasom_engine_emit_preedit_start (engine);

    if (g_strcmp0 (old_preedit, new_preedit) != 0)
    {
      g_free (jeongeum->preedit_string);
      jeongeum->preedit_string = new_preedit;
      dasom_engine_emit_preedit_changed (engine);
    }

    if (old_preedit != NULL && preedit[0] == 0)
      dasom_engine_emit_preedit_end (engine);
  }

  if (commit[0] != 0)
  {
    if (old_preedit != NULL)
    {
      g_free (jeongeum->preedit_string);
      jeongeum->preedit_string = NULL;
      dasom_engine_emit_preedit_changed (engine);
      dasom_engine_emit_preedit_end (engine);
    }

    dasom_engine_emit_commit (engine, new_commit);

    if (preedit[0] != 0)
    {
      g_free (jeongeum->preedit_string);
      jeongeum->preedit_string = new_preedit;
      dasom_engine_emit_preedit_start (engine);
      dasom_engine_emit_preedit_changed (engine);
    }
  }

  if (keyval == DASOM_KEY_space)
  {
    dasom_engine_emit_commit (engine, " ");
    retval = TRUE;
  }

  g_free (new_commit);
  g_free (old_preedit);

  return retval;
}

static void
dasom_jeongeum_init (DasomJeongeum *jeongeum)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  jeongeum->context = hangul_ic_new ("2");
  jeongeum->name = g_strdup ("정");
}

static void
dasom_jeongeum_finalize (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  DasomJeongeum *jeongeum = DASOM_JEONGEUM (object);

  hangul_ic_delete   (jeongeum->context);
  g_free (jeongeum->preedit_string);

  g_free (jeongeum->name);

  G_OBJECT_CLASS (dasom_jeongeum_parent_class)->finalize (object);
}

void
dasom_jeongeum_get_preedit_string (DasomEngine  *engine,
                                   gchar       **str,
                                   gint         *cursor_pos)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_if_fail (DASOM_IS_ENGINE (engine));

  DasomJeongeum *jeongeum = DASOM_JEONGEUM (engine);

  if (str)
  {
    if (jeongeum->preedit_string)
      *str = g_strdup (jeongeum->preedit_string);
    else
      *str = g_strdup ("");

    g_print (G_STRLOC ": %s: PRE-EDIT:%s\n", G_STRFUNC, *str);
  }

  if (cursor_pos)
    *cursor_pos = g_utf8_strlen (jeongeum->preedit_string, -1);

  g_return_if_fail (str == NULL || g_utf8_validate (*str, -1, NULL));
}

const gchar *
dasom_jeongeum_get_name (DasomEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_if_fail (DASOM_IS_ENGINE (engine));

  DasomJeongeum *jeongeum = DASOM_JEONGEUM (engine);

  return jeongeum->name;
}

static void
dasom_jeongeum_class_init (DasomJeongeumClass *klass)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DasomEngineClass *engine_class = DASOM_ENGINE_CLASS (klass);

  engine_class->filter_event       = dasom_jeongeum_filter_event;
  engine_class->get_preedit_string = dasom_jeongeum_get_preedit_string;
  engine_class->reset              = dasom_jeongeum_reset;
  engine_class->focus_in           = dasom_jeongeum_focus_in;
  engine_class->focus_out          = dasom_jeongeum_focus_out;

  engine_class->get_name           = dasom_jeongeum_get_name;

  object_class->finalize = dasom_jeongeum_finalize;
}

static void
dasom_jeongeum_class_finalize (DasomJeongeumClass *klass)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}

void module_load (GTypeModule *type_module)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  dasom_jeongeum_register_type (type_module);
}

GType module_get_type ()
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return dasom_jeongeum_get_type ();
}

void module_unload ()
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}