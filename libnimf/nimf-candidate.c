/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-candidate.c
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

#include "nimf-candidate.h"
#include <gtk/gtk.h>

static NimfCandidate *nimf_candidate_default = NULL;

struct _NimfCandidate
{
  GObject parent_instance;

  NimfServiceIM *target;
  GtkWidget     *window;
  GtkWidget     *entry;
  GtkWidget     *treeview;
  GtkWidget     *scrollbar;
  gint           cell_height;
};

struct _NimfCandidateClass
{
  GObjectClass parent_class;
};

enum
{
  INDEX_COLUMN,
  MAIN_COLUMN,
  EXTRA_COLUMN,
  N_COLUMNS
};

G_DEFINE_TYPE (NimfCandidate, nimf_candidate, G_TYPE_OBJECT);

static void
on_tree_view_row_activated (GtkTreeView       *tree_view,
                            GtkTreePath       *path,
                            GtkTreeViewColumn *column,
                            NimfCandidate     *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfEngineClass *engine_class;

  g_return_if_fail (candidate->target &&
                    NIMF_IS_ENGINE (candidate->target->engine));

  engine_class = NIMF_ENGINE_GET_CLASS (candidate->target->engine);

  gchar *text = nimf_candidate_get_selected_text (candidate);
  gint *indices = gtk_tree_path_get_indices (path);

  if (engine_class->candidate_clicked)
    engine_class->candidate_clicked (candidate->target->engine,
                                     candidate->target, text, indices[0]);
  g_free (text);
}

gboolean
on_range_change_value (GtkRange      *range,
                       GtkScrollType  scroll,
                       gdouble        value,
                       NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_return_val_if_fail (candidate->target &&
                        NIMF_IS_ENGINE (candidate->target->engine), FALSE);

  NimfEngineClass *engine_class;
  GtkAdjustment   *adjustment;
  gdouble          lower, upper;

  adjustment = gtk_range_get_adjustment (range);
  lower = gtk_adjustment_get_lower (adjustment);
  upper = gtk_adjustment_get_upper (adjustment);

  if (value < lower)
    value = lower;
  if (value > upper - 1)
    value = upper - 1;

  engine_class = NIMF_ENGINE_GET_CLASS (candidate->target->engine);

  if (engine_class->candidate_scrolled)
    engine_class->candidate_scrolled (candidate->target->engine,
                                      candidate->target, value);
  return FALSE;
}


static gboolean
on_entry_draw (GtkWidget *widget,
               cairo_t   *cr,
               gpointer   user_data)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkStyleContext *style_context;
  PangoContext    *pango_context;
  PangoLayout     *layout;
  const char      *text;
  gint             cursor_index;
  gint             x, y;

  style_context = gtk_widget_get_style_context (widget);
  pango_context = gtk_widget_get_pango_context (widget);
  layout = gtk_entry_get_layout (GTK_ENTRY (widget));
  text = pango_layout_get_text (layout);
  gtk_entry_get_layout_offsets (GTK_ENTRY (widget), &x, &y);
  cursor_index = g_utf8_offset_to_pointer (text, gtk_editable_get_position (GTK_EDITABLE (widget))) - text;
  gtk_render_insertion_cursor (style_context, cr, x, y, layout, cursor_index,
                               pango_context_get_base_dir (pango_context));
  return FALSE;
}

static void
nimf_candidate_init (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column[N_COLUMNS];
  GtkListStore      *store;
  gint               fixed_height = 32;
  gint               horizontal_space;

  gtk_init (NULL, NULL);
  nimf_candidate_default = candidate;

  /* gtk entry */
  candidate->entry = gtk_entry_new ();
  gtk_editable_set_editable (GTK_EDITABLE (candidate->entry), FALSE);
  gtk_widget_set_no_show_all (candidate->entry, TRUE);
  g_signal_connect_after (candidate->entry, "draw",
                          G_CALLBACK (on_entry_draw), NULL);
  /* gtk tree view */
  store = gtk_list_store_new (N_COLUMNS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
  candidate->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  g_object_unref (store);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (candidate->treeview), FALSE);
  gtk_widget_style_get (candidate->treeview, "horizontal-separator",
                        &horizontal_space, NULL);
  candidate->cell_height = fixed_height + horizontal_space / 2;
  gtk_widget_set_size_request (candidate->treeview,
                               (gint) (candidate->cell_height * 10 / 1.6),
                               candidate->cell_height * 10);
  g_signal_connect (candidate->treeview, "row-activated",
                    (GCallback) on_tree_view_row_activated, candidate);
  /* column */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "height", fixed_height, "font", "Sans 14", NULL);

  column[INDEX_COLUMN] = gtk_tree_view_column_new_with_attributes ("Index",
                                        renderer, "text", INDEX_COLUMN, NULL);
  column[MAIN_COLUMN]  = gtk_tree_view_column_new_with_attributes ("Main",
                                        renderer, "text", MAIN_COLUMN, NULL);
  column[EXTRA_COLUMN] = gtk_tree_view_column_new_with_attributes ("Extra",
                                        renderer, "text", EXTRA_COLUMN, NULL);
  gtk_tree_view_column_set_sizing (column[INDEX_COLUMN], GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_column_set_sizing (column[MAIN_COLUMN],  GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_column_set_sizing (column[EXTRA_COLUMN], GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (candidate->treeview),
                               column[INDEX_COLUMN]);
  gtk_tree_view_append_column (GTK_TREE_VIEW (candidate->treeview),
                               column[MAIN_COLUMN]);
  gtk_tree_view_append_column (GTK_TREE_VIEW (candidate->treeview),
                               column[EXTRA_COLUMN]);
  /* scrollbar */
  GtkAdjustment *adjustment = gtk_adjustment_new (1.0, 1.0, 2.0, 1.0, 1.0, 1.0);
  candidate->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gtk_range_set_slider_size_fixed (GTK_RANGE (candidate->scrollbar), FALSE);
  g_signal_connect (candidate->scrollbar, "change-value",
                    G_CALLBACK (on_range_change_value), candidate);
  GtkCssProvider  *provider;
  GtkStyleContext *style_context;
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                       ".scrollbar {"
                       "  -GtkScrollbar-has-backward-stepper: true;"
                       "  -GtkScrollbar-has-forward-stepper:  true;"
                       "  -GtkScrollbar-has-secondary-forward-stepper:  true;"
                       "}" , -1, NULL);
  style_context = gtk_widget_get_style_context (candidate->scrollbar);
  gtk_style_context_add_provider (style_context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  /* gtk box */
  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_box_pack_start (GTK_BOX (vbox), candidate->entry, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), candidate->treeview,  TRUE,  TRUE, 0);
  gtk_box_pack_end   (GTK_BOX (hbox), candidate->scrollbar, FALSE, TRUE, 0);

  /* gtk window */
  candidate->window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_type_hint (GTK_WINDOW (candidate->window),
                            GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  gtk_container_set_border_width (GTK_CONTAINER (candidate->window), 1);
  gtk_container_add (GTK_CONTAINER (candidate->window), vbox);
  gtk_widget_realize (candidate->window);
}

static void
nimf_candidate_finalize (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gtk_widget_destroy (NIMF_CANDIDATE (object)->window);
  G_OBJECT_CLASS (nimf_candidate_parent_class)->finalize (object);
}

static void
nimf_candidate_class_init (NimfCandidateClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = nimf_candidate_finalize;
}

void nimf_candidate_clear (NimfCandidate *candidate,
                           NimfServiceIM *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeModel *model;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (candidate->treeview));
  gtk_list_store_clear (GTK_LIST_STORE (model));
  nimf_candidate_set_page_values (candidate, target, 1, 1, 5);
}

void nimf_candidate_append (NimfCandidate *candidate,
                            const gchar   *item1,
                            const gchar   *item2)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeModel  *model;
  GtkTreeIter    iter;
  gint           n_row;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (candidate->treeview));
  n_row = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL);

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set    (GTK_LIST_STORE (model), &iter,
                         INDEX_COLUMN, (n_row + 1) % 10,
                         MAIN_COLUMN, item1, -1);

  if (item2)
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        EXTRA_COLUMN, item2, -1);
}

void nimf_candidate_set_auxiliary_text (NimfCandidate *candidate,
                                        const gchar   *text,
                                        gint           cursor_pos)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gtk_entry_set_text (GTK_ENTRY (candidate->entry), text);
  gtk_editable_set_position (GTK_EDITABLE (candidate->entry), cursor_pos);
}

void nimf_candidate_set_page_values (NimfCandidate *candidate,
                                     NimfServiceIM *target,
                                     gint           page_index,
                                     gint           n_pages,
                                     gint           page_size)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkRange *range = GTK_RANGE (candidate->scrollbar);

  candidate->target = target;
  gtk_range_set_range (range, 1.0, (gdouble) n_pages + 1.0);

  if (page_index != (gint) gtk_range_get_value (range))
    gtk_range_set_value (range, (gdouble) page_index);

  gtk_widget_set_size_request (candidate->treeview,
                               (gint) (candidate->cell_height *  10 / 1.6),
                               candidate->cell_height * page_size);
}

void nimf_candidate_show_window (NimfCandidate *candidate,
                                 NimfServiceIM *target,
                                 gboolean       show_entry)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkRequisition  natural_size;
  int             x, y, w, h;
  int             screen_width, screen_height;

  #if GTK_CHECK_VERSION (3, 22, 0)
    GdkRectangle  geometry;
    GdkDisplay   *display = gtk_widget_get_display (candidate->window);
    GdkWindow    *window  = gtk_widget_get_window  (candidate->window);
    GdkMonitor   *monitor = gdk_display_get_monitor_at_window (display, window);
    gdk_monitor_get_geometry (monitor, &geometry);
    screen_width  = geometry.width;
    screen_height = geometry.height;
  #else
    screen_width  = gdk_screen_width ();
    screen_height = gdk_screen_height ();
  #endif

  candidate->target = target;

  if (show_entry)
    gtk_widget_show (candidate->entry);
  else
    gtk_widget_hide (candidate->entry);

  gtk_widget_show_all (candidate->window);
  gtk_widget_get_preferred_size (candidate->window, NULL, &natural_size);
  gtk_window_resize (GTK_WINDOW (candidate->window),
                     natural_size.width, natural_size.height);
  gtk_window_get_size (GTK_WINDOW (candidate->window), &w, &h);

  x = target->cursor_area.x - target->cursor_area.width;
  y = target->cursor_area.y + target->cursor_area.height;

  if (x + w > screen_width)
    x = screen_width - w;

  if (y + h > screen_height)
    y = target->cursor_area.y - h;

  gtk_window_move (GTK_WINDOW (candidate->window), x, y);
}

void nimf_candidate_hide_window (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gtk_widget_hide (candidate->window);
}

gboolean nimf_candidate_is_window_visible (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return gtk_widget_is_visible (candidate->window);
}

void
nimf_candidate_select_last_item_in_page (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeModel     *model;
  GtkTreeSelection *selection;
  GtkTreeIter       iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (candidate->treeview));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (candidate->treeview));
  gint n_row = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL);

  if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (model),
                                     &iter, NULL, MAX (0, n_row - 1)))
    gtk_tree_selection_select_iter (selection, &iter);
}

void
nimf_candidate_select_item_by_index_in_page (NimfCandidate *candidate,
                                             gint           index)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeModel     *model;
  GtkTreeSelection *selection;
  GtkTreeIter       iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (candidate->treeview));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (candidate->treeview));

  if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (model), &iter, NULL, index))
    gtk_tree_selection_select_iter (selection, &iter);
}

void
nimf_candidate_select_previous_item (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeModel     *model;
  GtkTreeSelection *selection;
  GtkTreeIter       iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (candidate->treeview));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (candidate->treeview));

  if (gtk_tree_selection_get_selected (selection, &model, &iter) == FALSE)
  {
    nimf_candidate_select_last_item_in_page (candidate);
    return;
  }

  if (gtk_tree_model_iter_previous (model, &iter))
  {
    gtk_tree_selection_select_iter (selection, &iter);
  }
  else
  {
    NimfEngineClass *engine_class;
    engine_class = NIMF_ENGINE_GET_CLASS (candidate->target->engine);

    if (engine_class->candidate_page_up)
    {
      if (engine_class->candidate_page_up (candidate->target->engine,
                                           candidate->target))
        nimf_candidate_select_last_item_in_page (candidate);
    }
  }
}

void
nimf_candidate_select_first_item_in_page (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeModel     *model;
  GtkTreeSelection *selection;
  GtkTreeIter       iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (candidate->treeview));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (candidate->treeview));

  if (gtk_tree_model_get_iter_first (model, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
}

void
nimf_candidate_select_next_item (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeModel     *model;
  GtkTreeSelection *selection;
  GtkTreeIter       iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (candidate->treeview));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (candidate->treeview));

  if (gtk_tree_selection_get_selected (selection, &model, &iter) == FALSE)
  {
    nimf_candidate_select_first_item_in_page (candidate);
    return;
  }

  if (gtk_tree_model_iter_next (model, &iter))
  {
    gtk_tree_selection_select_iter (selection, &iter);
  }
  else
  {
    NimfEngineClass *engine_class;
    engine_class = NIMF_ENGINE_GET_CLASS (candidate->target->engine);

    if (engine_class->candidate_page_down)
    {
      if (engine_class->candidate_page_down (candidate->target->engine,
                                             candidate->target))
        nimf_candidate_select_first_item_in_page (candidate);
    }
  }
}

NimfCandidate *nimf_candidate_get_default ()
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return nimf_candidate_default;
}

NimfCandidate *nimf_candidate_new ()
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return g_object_new (NIMF_TYPE_CANDIDATE, NULL);
}

gchar *nimf_candidate_get_selected_text (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeIter   iter;
  GtkTreeModel *model;
  gchar        *text = NULL;

  GtkTreeSelection *selection;
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (candidate->treeview));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, MAIN_COLUMN, &text, -1);

  return text;
}

gint nimf_candidate_get_selected_index (NimfCandidate *candidate)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GtkTreeIter       iter;
  GtkTreeModel     *model;
  GtkTreeSelection *selection;
  gint              index = -1;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (candidate->treeview));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
    gint *indices = gtk_tree_path_get_indices (path);

    if (indices)
      index = indices[0];

    gtk_tree_path_free (path);
  }

  return index;
}
