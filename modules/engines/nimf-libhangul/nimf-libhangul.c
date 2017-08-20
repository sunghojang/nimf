/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-libhangul.c
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

#include <nimf.h>
#include <hangul.h>
#include <glib/gi18n.h>

#define NIMF_TYPE_LIBHANGUL             (nimf_libhangul_get_type ())
#define NIMF_LIBHANGUL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), NIMF_TYPE_LIBHANGUL, NimfLibhangul))
#define NIMF_LIBHANGUL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), NIMF_TYPE_LIBHANGUL, NimfLibhangulClass))
#define NIMF_IS_LIBHANGUL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NIMF_TYPE_LIBHANGUL))
#define NIMF_IS_LIBHANGUL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), NIMF_TYPE_LIBHANGUL))
#define NIMF_LIBHANGUL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), NIMF_TYPE_LIBHANGUL, NimfLibhangulClass))

typedef struct _NimfLibhangul      NimfLibhangul;
typedef struct _NimfLibhangulClass NimfLibhangulClass;

struct _NimfLibhangul
{
  NimfEngine parent_instance;

  NimfCandidate      *candidate;
  HangulInputContext *context;
  gchar              *preedit_string;
  NimfPreeditAttr   **preedit_attrs;
  NimfPreeditState    preedit_state;
  gchar              *id;

  NimfKey           **hanja_keys;
  GSettings          *settings;
  gboolean            is_double_consonant_rule;
  gboolean            is_auto_reordering;
  gchar              *layout;
  /* workaround: ignore reset called by commit callback in application */
  gboolean            ignore_reset_in_commit_cb;
  gboolean            is_committing;

  HanjaList          *hanja_list;
  gint                current_page;
  gint                n_pages;
};

struct _NimfLibhangulClass
{
  /*< private >*/
  NimfEngineClass parent_class;
};

static HanjaTable *nimf_libhangul_hanja_table  = NULL;
static HanjaTable *nimf_libhangul_symbol_table = NULL;
static gint        nimf_libhangul_hanja_table_ref_count = 0;

G_DEFINE_DYNAMIC_TYPE (NimfLibhangul, nimf_libhangul, NIMF_TYPE_ENGINE);

/* only for PC keyboards */
guint nimf_event_keycode_to_qwerty_keyval (const NimfEvent *event)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  guint keyval = 0;
  gboolean is_shift = event->key.state & NIMF_SHIFT_MASK;

  switch (event->key.hardware_keycode)
  {
    /* 20(-) ~ 21(=) */
    case 20: keyval = is_shift ? '_' : '-';  break;
    case 21: keyval = is_shift ? '+' : '=';  break;
    /* 24(q) ~ 35(]) */
    case 24: keyval = is_shift ? 'Q' : 'q';  break;
    case 25: keyval = is_shift ? 'W' : 'w';  break;
    case 26: keyval = is_shift ? 'E' : 'e';  break;
    case 27: keyval = is_shift ? 'R' : 'r';  break;
    case 28: keyval = is_shift ? 'T' : 't';  break;
    case 29: keyval = is_shift ? 'Y' : 'y';  break;
    case 30: keyval = is_shift ? 'U' : 'u';  break;
    case 31: keyval = is_shift ? 'I' : 'i';  break;
    case 32: keyval = is_shift ? 'O' : 'o';  break;
    case 33: keyval = is_shift ? 'P' : 'p';  break;
    case 34: keyval = is_shift ? '{' : '[';  break;
    case 35: keyval = is_shift ? '}' : ']';  break;
    /* 38(a) ~ 48(') */
    case 38: keyval = is_shift ? 'A' : 'a';  break;
    case 39: keyval = is_shift ? 'S' : 's';  break;
    case 40: keyval = is_shift ? 'D' : 'd';  break;
    case 41: keyval = is_shift ? 'F' : 'f';  break;
    case 42: keyval = is_shift ? 'G' : 'g';  break;
    case 43: keyval = is_shift ? 'H' : 'h';  break;
    case 44: keyval = is_shift ? 'J' : 'j';  break;
    case 45: keyval = is_shift ? 'K' : 'k';  break;
    case 46: keyval = is_shift ? 'L' : 'l';  break;
    case 47: keyval = is_shift ? ':' : ';';  break;
    case 48: keyval = is_shift ? '"' : '\''; break;
    /* 52(z) ~ 61(?) */
    case 52: keyval = is_shift ? 'Z' : 'z';  break;
    case 53: keyval = is_shift ? 'X' : 'x';  break;
    case 54: keyval = is_shift ? 'C' : 'c';  break;
    case 55: keyval = is_shift ? 'V' : 'v';  break;
    case 56: keyval = is_shift ? 'B' : 'b';  break;
    case 57: keyval = is_shift ? 'N' : 'n';  break;
    case 58: keyval = is_shift ? 'M' : 'm';  break;
    case 59: keyval = is_shift ? '<' : ',';  break;
    case 60: keyval = is_shift ? '>' : '.';  break;
    case 61: keyval = is_shift ? '?' : '/';  break;
    default: keyval = event->key.keyval;     break;
  }

  return keyval;
}

static void
nimf_libhangul_update_preedit (NimfEngine    *engine,
                               NimfServiceIM *target,
                               gchar         *new_preedit)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  /* preedit-start */
  if (hangul->preedit_state == NIMF_PREEDIT_STATE_END && new_preedit[0] != 0)
  {
    hangul->preedit_state = NIMF_PREEDIT_STATE_START;
    nimf_engine_emit_preedit_start (engine, target);
  }
  /* preedit-changed */
  if (hangul->preedit_string[0] != 0 || new_preedit[0] != 0)
  {
    g_free (hangul->preedit_string);
    hangul->preedit_string = new_preedit;
    hangul->preedit_attrs[0]->end_index = g_utf8_strlen (hangul->preedit_string, -1);
    nimf_engine_emit_preedit_changed (engine, target, hangul->preedit_string,
                                      hangul->preedit_attrs,
                                      g_utf8_strlen (hangul->preedit_string,
                                                     -1));
  }
  else
    g_free (new_preedit);
  /* preedit-end */
  if (hangul->preedit_state == NIMF_PREEDIT_STATE_START &&
      hangul->preedit_string[0] == 0)
  {
    hangul->preedit_state = NIMF_PREEDIT_STATE_END;
    nimf_engine_emit_preedit_end (engine, target);
  }
}

void
nimf_libhangul_emit_commit (NimfEngine    *engine,
                            NimfServiceIM *target,
                            const gchar   *text)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);
  hangul->is_committing = TRUE;
  nimf_engine_emit_commit (engine, target, text);
  hangul->is_committing = FALSE;
}

void
nimf_libhangul_reset (NimfEngine    *engine,
                      NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_if_fail (NIMF_IS_ENGINE (engine));

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  /* workaround: ignore reset called by commit callback in application */
  if (G_UNLIKELY (hangul->ignore_reset_in_commit_cb && hangul->is_committing))
    return;

  nimf_candidate_hide_window (hangul->candidate);

  const ucschar *flush;
  flush = hangul_ic_flush (hangul->context);

  if (flush[0] != 0)
  {
    gchar *text = g_ucs4_to_utf8 (flush, -1, NULL, NULL, NULL);
    nimf_libhangul_emit_commit (engine, target, text);
    g_free (text);
  }

  nimf_libhangul_update_preedit (engine, target, g_strdup (""));
}

void
nimf_libhangul_focus_in (NimfEngine    *engine,
                         NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_if_fail (NIMF_IS_ENGINE (engine));
}

void
nimf_libhangul_focus_out (NimfEngine    *engine,
                          NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_if_fail (NIMF_IS_ENGINE (engine));

  nimf_libhangul_reset (engine, target);
}

static void
on_candidate_clicked (NimfEngine    *engine,
                      NimfServiceIM *target,
                      gchar         *text,
                      gint           index)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  if (text)
  {
    /* hangul_ic 내부의 commit text가 사라집니다 */
    hangul_ic_reset (hangul->context);
    nimf_libhangul_emit_commit (engine, target, text);
    nimf_libhangul_update_preedit (engine, target, g_strdup (""));
  }

  nimf_candidate_hide_window (hangul->candidate);
}

static gint
nimf_libhangul_get_current_page (NimfEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return NIMF_LIBHANGUL (engine)->current_page;
}

static void
nimf_libhangul_update_page (NimfEngine    *engine,
                            NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  if (hangul->hanja_list == NULL)
    return;

  gint i;
  gint list_len = hanja_list_get_size (hangul->hanja_list);
  nimf_candidate_clear (hangul->candidate, target);

  for (i = (hangul->current_page - 1) * 10;
       i < MIN (hangul->current_page * 10, list_len); i++)
  {
    const Hanja *hanja = hanja_list_get_nth (hangul->hanja_list, i);
    const char  *item1 = hanja_get_value    (hanja);
    const char  *item2 = hanja_get_comment  (hanja);
    nimf_candidate_append (hangul->candidate, item1, item2);
  }

  nimf_candidate_set_page_values (hangul->candidate, target,
                                  hangul->current_page, hangul->n_pages, 10);
}

static gboolean
nimf_libhangul_page_up (NimfEngine *engine, NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  if (hangul->hanja_list == NULL)
    return FALSE;

  if (hangul->current_page <= 1)
  {
    nimf_candidate_select_first_item_in_page (hangul->candidate);
    return FALSE;
  }

  hangul->current_page--;
  nimf_libhangul_update_page (engine, target);
  nimf_candidate_select_last_item_in_page (hangul->candidate);

  return TRUE;
}

static gboolean
nimf_libhangul_page_down (NimfEngine *engine, NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  if (hangul->hanja_list == NULL)
    return FALSE;

  if (hangul->current_page == hangul->n_pages)
  {
    nimf_candidate_select_last_item_in_page (hangul->candidate);
    return FALSE;
  }

  hangul->current_page++;
  nimf_libhangul_update_page (engine, target);
  nimf_candidate_select_first_item_in_page (hangul->candidate);

  return TRUE;
}

static void
nimf_libhangul_page_home (NimfEngine *engine, NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  if (hangul->hanja_list == NULL)
    return;

  if (hangul->current_page <= 1)
  {
    nimf_candidate_select_first_item_in_page (hangul->candidate);
    return;
  }

  hangul->current_page = 1;
  nimf_libhangul_update_page (engine, target);
  nimf_candidate_select_first_item_in_page (hangul->candidate);
}

static void
nimf_libhangul_page_end (NimfEngine *engine, NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  if (hangul->hanja_list == NULL)
    return;

  if (hangul->current_page == hangul->n_pages)
  {
    nimf_candidate_select_last_item_in_page (hangul->candidate);
    return;
  }

  hangul->current_page = hangul->n_pages;
  nimf_libhangul_update_page (engine, target);
  nimf_candidate_select_last_item_in_page (hangul->candidate);
}

static void
on_candidate_scrolled (NimfEngine    *engine,
                       NimfServiceIM *target,
                       gdouble        value)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  if ((gint) value == nimf_libhangul_get_current_page (engine))
    return;

  while (hangul->n_pages > 1)
  {
    gint d = (gint) value - nimf_libhangul_get_current_page (engine);

    if (d > 0)
      nimf_libhangul_page_down (engine, target);
    else if (d < 0)
      nimf_libhangul_page_up (engine, target);
    else if (d == 0)
      break;
  }
}

static gboolean
nimf_libhangul_filter_leading_consonant (NimfEngine    *engine,
                                         NimfServiceIM *target,
                                         guint          keyval)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  const ucschar *ucs_preedit;
  ucs_preedit = hangul_ic_get_preedit_string (hangul->context);

  /* check ㄱ ㄷ ㅂ ㅅ ㅈ */
  if ((keyval == 'r' && ucs_preedit[0] == 0x3131 && ucs_preedit[1] == 0) ||
      (keyval == 'e' && ucs_preedit[0] == 0x3137 && ucs_preedit[1] == 0) ||
      (keyval == 'q' && ucs_preedit[0] == 0x3142 && ucs_preedit[1] == 0) ||
      (keyval == 't' && ucs_preedit[0] == 0x3145 && ucs_preedit[1] == 0) ||
      (keyval == 'w' && ucs_preedit[0] == 0x3148 && ucs_preedit[1] == 0))
  {
    gchar *preedit = g_ucs4_to_utf8 (ucs_preedit, -1, NULL, NULL, NULL);
    nimf_libhangul_emit_commit (engine, target, preedit);
    g_free (preedit);
    nimf_engine_emit_preedit_changed (engine, target, hangul->preedit_string,
                                      hangul->preedit_attrs,
                                      g_utf8_strlen (hangul->preedit_string,
                                                     -1));
    return TRUE;
  }

  return FALSE;
}

gboolean
nimf_libhangul_filter_event (NimfEngine    *engine,
                             NimfServiceIM *target,
                             NimfEvent     *event)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  guint    keyval;
  gboolean retval = FALSE;

  NimfLibhangul *hangul = NIMF_LIBHANGUL (engine);

  if (event->key.type   == NIMF_EVENT_KEY_RELEASE ||
      event->key.keyval == NIMF_KEY_Shift_L       ||
      event->key.keyval == NIMF_KEY_Shift_R)
    return FALSE;

  if (event->key.state & (NIMF_CONTROL_MASK | NIMF_MOD1_MASK))
  {
    nimf_libhangul_reset (engine, target);
    return FALSE;
  }

  if (G_UNLIKELY (nimf_event_matches (event,
                  (const NimfKey **) hangul->hanja_keys)))
  {
    if (nimf_candidate_is_window_visible (hangul->candidate) == FALSE)
    {
      hanja_list_delete (hangul->hanja_list);
      nimf_candidate_clear (hangul->candidate, target);
      hangul->hanja_list = hanja_table_match_exact (nimf_libhangul_hanja_table,
                                                    hangul->preedit_string);
      if (hangul->hanja_list == NULL)
        hangul->hanja_list = hanja_table_match_exact (nimf_libhangul_symbol_table,
                                                      hangul->preedit_string);
      hangul->n_pages = (hanja_list_get_size (hangul->hanja_list) + 9) / 10;
      hangul->current_page = 1;
      nimf_libhangul_update_page (engine, target);
      nimf_candidate_show_window (hangul->candidate, target, FALSE);
      nimf_candidate_select_first_item_in_page (hangul->candidate);
    }
    else
    {
      nimf_candidate_hide_window (hangul->candidate);
      nimf_candidate_clear (hangul->candidate, target);
      hanja_list_delete (hangul->hanja_list);
      hangul->hanja_list = NULL;
      hangul->current_page = 0;
      hangul->n_pages = 0;
    }

    return TRUE;
  }

  if (nimf_candidate_is_window_visible (hangul->candidate))
  {
    switch (event->key.keyval)
    {
      case NIMF_KEY_Return:
      case NIMF_KEY_KP_Enter:
        {
          gchar *text = nimf_candidate_get_selected_text (hangul->candidate);
          on_candidate_clicked (engine, target, text, -1);
          g_free (text);
        }
        break;
      case NIMF_KEY_Up:
      case NIMF_KEY_KP_Up:
        nimf_candidate_select_previous_item (hangul->candidate);
        break;
      case NIMF_KEY_Down:
      case NIMF_KEY_KP_Down:
        nimf_candidate_select_next_item (hangul->candidate);
        break;
      case NIMF_KEY_Page_Up:
      case NIMF_KEY_KP_Page_Up:
        nimf_libhangul_page_up (engine, target);
        break;
      case NIMF_KEY_Page_Down:
      case NIMF_KEY_KP_Page_Down:
        nimf_libhangul_page_down (engine, target);
        break;
      case NIMF_KEY_Home:
        nimf_libhangul_page_home (engine, target);
        break;
      case NIMF_KEY_End:
        nimf_libhangul_page_end (engine, target);
        break;
      case NIMF_KEY_Escape:
        nimf_candidate_hide_window (hangul->candidate);
        break;
      case NIMF_KEY_0:
      case NIMF_KEY_1:
      case NIMF_KEY_2:
      case NIMF_KEY_3:
      case NIMF_KEY_4:
      case NIMF_KEY_5:
      case NIMF_KEY_6:
      case NIMF_KEY_7:
      case NIMF_KEY_8:
      case NIMF_KEY_9:
      case NIMF_KEY_KP_0:
      case NIMF_KEY_KP_1:
      case NIMF_KEY_KP_2:
      case NIMF_KEY_KP_3:
      case NIMF_KEY_KP_4:
      case NIMF_KEY_KP_5:
      case NIMF_KEY_KP_6:
      case NIMF_KEY_KP_7:
      case NIMF_KEY_KP_8:
      case NIMF_KEY_KP_9:
        {
          if (hangul->hanja_list == NULL || hangul->current_page < 1)
            break;

          gint i, n;
          gint list_len = hanja_list_get_size (hangul->hanja_list);

          if (event->key.keyval >= NIMF_KEY_0 &&
              event->key.keyval <= NIMF_KEY_9)
            n = (event->key.keyval - NIMF_KEY_0 + 9) % 10;
          else if (event->key.keyval >= NIMF_KEY_KP_0 &&
                   event->key.keyval <= NIMF_KEY_KP_9)
            n = (event->key.keyval - NIMF_KEY_KP_0 + 9) % 10;
          else
            break;

          i = (hangul->current_page - 1) * 10 + n;

          if (i < MIN (hangul->current_page * 10, list_len))
          {
            const Hanja *hanja = hanja_list_get_nth (hangul->hanja_list, i);
            const char  *text = hanja_get_value (hanja);
            on_candidate_clicked (engine, target, (gchar *) text, -1);
          }
        }
        break;
      default:
        break;
    }

    return TRUE;
  }

  const ucschar *ucs_commit;
  const ucschar *ucs_preedit;

  if (G_UNLIKELY (event->key.keyval == NIMF_KEY_BackSpace))
  {
    retval = hangul_ic_backspace (hangul->context);

    if (retval)
    {
      ucs_preedit = hangul_ic_get_preedit_string (hangul->context);
      gchar *new_preedit = g_ucs4_to_utf8 (ucs_preedit, -1, NULL, NULL, NULL);
      nimf_libhangul_update_preedit (engine, target, new_preedit);
    }

    return retval;
  }

  if (G_UNLIKELY (g_strcmp0 (hangul->layout, "ro") == 0))
    keyval = event->key.keyval;
  else
    keyval = nimf_event_keycode_to_qwerty_keyval (event);

  if (!hangul->is_double_consonant_rule &&
      (g_strcmp0 (hangul->layout, "2") == 0) && /* 두벌식에만 적용 */
      nimf_libhangul_filter_leading_consonant (engine, target, keyval))
    return TRUE;

  retval = hangul_ic_process (hangul->context, keyval);

  ucs_commit  = hangul_ic_get_commit_string  (hangul->context);
  ucs_preedit = hangul_ic_get_preedit_string (hangul->context);

  gchar *new_commit  = g_ucs4_to_utf8 (ucs_commit,  -1, NULL, NULL, NULL);

  if (ucs_commit[0] != 0)
    nimf_libhangul_emit_commit (engine, target, new_commit);

  g_free (new_commit);

  gchar *new_preedit = g_ucs4_to_utf8 (ucs_preedit, -1, NULL, NULL, NULL);
  nimf_libhangul_update_preedit (engine, target, new_preedit);

  return retval;
}

static bool
on_libhangul_transition (HangulInputContext *ic,
                         ucschar             c,
                         const ucschar      *preedit,
                         void               *data)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  if ((hangul_is_choseong (c) && (hangul_ic_has_jungseong (ic) ||
                                  hangul_ic_has_jongseong (ic))) ||
      (hangul_is_jungseong (c) && hangul_ic_has_jongseong (ic)))
    return false;

  return true;
}

static void
nimf_libhangul_update_transition_cb (NimfLibhangul *hangul)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  if ((g_strcmp0 (hangul->layout, "2") == 0) && !hangul->is_auto_reordering)
    hangul_ic_connect_callback (hangul->context, "transition",
                                on_libhangul_transition, NULL);
  else
    hangul_ic_connect_callback (hangul->context, "transition", NULL, NULL);
}

static void
on_changed_layout (GSettings     *settings,
                   gchar         *key,
                   NimfLibhangul *hangul)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_free (hangul->layout);
  hangul->layout = g_settings_get_string (settings, key);
  hangul_ic_select_keyboard (hangul->context, hangul->layout);
  nimf_libhangul_update_transition_cb (hangul);
}

static void
on_changed_auto_reordering (GSettings     *settings,
                            gchar         *key,
                            NimfLibhangul *hangul)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  hangul->is_auto_reordering = g_settings_get_boolean (settings, key);
  nimf_libhangul_update_transition_cb (hangul);
}

static void
on_changed_keys (GSettings     *settings,
                 gchar         *key,
                 NimfLibhangul *hangul)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gchar **keys = g_settings_get_strv (settings, key);

  if (g_strcmp0 (key, "hanja-keys") == 0)
  {
    nimf_key_freev (hangul->hanja_keys);
    hangul->hanja_keys = nimf_key_newv ((const gchar **) keys);
  }

  g_strfreev (keys);
}

static void
on_changed_double_consonant_rule (GSettings     *settings,
                                  gchar         *key,
                                  NimfLibhangul *hangul)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  hangul->is_double_consonant_rule = g_settings_get_boolean (settings, key);
}

static void
on_changed_ignore_reset_in_commit_cb (GSettings     *settings,
                                      gchar         *key,
                                      NimfLibhangul *hangul)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  hangul->ignore_reset_in_commit_cb = g_settings_get_boolean (settings, key);
}

static void
nimf_libhangul_init (NimfLibhangul *hangul)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gchar **trigger_keys;
  gchar **hanja_keys;

  hangul->candidate = nimf_candidate_get_default ();
  hangul->settings = g_settings_new ("org.nimf.engines.nimf-libhangul");

  hangul->layout = g_settings_get_string (hangul->settings, "layout");
  hangul->is_double_consonant_rule =
    g_settings_get_boolean (hangul->settings, "double-consonant-rule");
  hangul->is_auto_reordering =
    g_settings_get_boolean (hangul->settings, "auto-reordering");
  hangul->ignore_reset_in_commit_cb =
    g_settings_get_boolean (hangul->settings, "ignore-reset-in-commit-cb");

  trigger_keys = g_settings_get_strv (hangul->settings, "trigger-keys");
  hanja_keys   = g_settings_get_strv (hangul->settings, "hanja-keys");

  hangul->hanja_keys = nimf_key_newv ((const gchar **) hanja_keys);
  hangul->context = hangul_ic_new (hangul->layout);

  hangul->id = g_strdup ("nimf-libhangul");
  hangul->preedit_string = g_strdup ("");
  hangul->preedit_attrs  = g_malloc0_n (2, sizeof (NimfPreeditAttr *));
  hangul->preedit_attrs[0] = nimf_preedit_attr_new (NIMF_PREEDIT_ATTR_UNDERLINE, 0, 0);
  hangul->preedit_attrs[1] = NULL;

  if (nimf_libhangul_hanja_table_ref_count == 0)
  {
    nimf_libhangul_hanja_table  = hanja_table_load (NULL);
    nimf_libhangul_symbol_table = hanja_table_load (MSSYMBOL_PATH);
  }

  nimf_libhangul_hanja_table_ref_count++;

  g_strfreev (trigger_keys);
  g_strfreev (hanja_keys);

  nimf_libhangul_update_transition_cb (hangul);

  g_signal_connect (hangul->settings, "changed::layout",
                    G_CALLBACK (on_changed_layout), hangul);
  g_signal_connect (hangul->settings, "changed::trigger-keys",
                    G_CALLBACK (on_changed_keys), hangul);
  g_signal_connect (hangul->settings, "changed::hanja-keys",
                    G_CALLBACK (on_changed_keys), hangul);
  g_signal_connect (hangul->settings, "changed::double-consonant-rule",
                    G_CALLBACK (on_changed_double_consonant_rule), hangul);
  g_signal_connect (hangul->settings, "changed::auto-reordering",
                    G_CALLBACK (on_changed_auto_reordering), hangul);
  g_signal_connect (hangul->settings, "changed::ignore-reset-in-commit-cb",
                    G_CALLBACK (on_changed_ignore_reset_in_commit_cb), hangul);
}

static void
nimf_libhangul_finalize (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfLibhangul *hangul = NIMF_LIBHANGUL (object);

  if (--nimf_libhangul_hanja_table_ref_count == 0)
  {
    hanja_table_delete (nimf_libhangul_hanja_table);
    hanja_table_delete (nimf_libhangul_symbol_table);
  }

  hanja_list_delete (hangul->hanja_list);
  hangul_ic_delete (hangul->context);
  g_free (hangul->preedit_string);
  nimf_preedit_attr_freev (hangul->preedit_attrs);
  g_free (hangul->id);
  g_free (hangul->layout);
  nimf_key_freev (hangul->hanja_keys);
  g_object_unref (hangul->settings);

  G_OBJECT_CLASS (nimf_libhangul_parent_class)->finalize (object);
}

const gchar *
nimf_libhangul_get_id (NimfEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_val_if_fail (NIMF_IS_ENGINE (engine), NULL);

  return NIMF_LIBHANGUL (engine)->id;
}

const gchar *
nimf_libhangul_get_icon_name (NimfEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_val_if_fail (NIMF_IS_ENGINE (engine), NULL);

  return NIMF_LIBHANGUL (engine)->id;
}

static void
nimf_libhangul_class_init (NimfLibhangulClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GObjectClass *object_class = G_OBJECT_CLASS (class);
  NimfEngineClass *engine_class = NIMF_ENGINE_CLASS (class);

  engine_class->filter_event       = nimf_libhangul_filter_event;
  engine_class->reset              = nimf_libhangul_reset;
  engine_class->focus_in           = nimf_libhangul_focus_in;
  engine_class->focus_out          = nimf_libhangul_focus_out;

  engine_class->candidate_page_up   = nimf_libhangul_page_up;
  engine_class->candidate_page_down = nimf_libhangul_page_down;
  engine_class->candidate_clicked   = on_candidate_clicked;
  engine_class->candidate_scrolled  = on_candidate_scrolled;

  engine_class->get_id             = nimf_libhangul_get_id;
  engine_class->get_icon_name      = nimf_libhangul_get_icon_name;

  object_class->finalize = nimf_libhangul_finalize;
}

static void
nimf_libhangul_class_finalize (NimfLibhangulClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}

void module_register_type (GTypeModule *type_module)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  nimf_libhangul_register_type (type_module);
}

GType module_get_type ()
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return nimf_libhangul_get_type ();
}
