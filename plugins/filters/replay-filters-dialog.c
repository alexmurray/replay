/* -*- mode: C; mode: C; c-file-style: "ellemtel"; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * Replay Visualisation System
 * Copyright (C) 2012 Commonwealth of Australia
 * Developed by Alex Murray <murray.alex@gmail.com>
 *
 * Replay is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 (GPLv3)
 * (with extensions as allowed under clause 7 of the GPLv3) as
 * published by the Free Software Foundation.
 *
 * Replay is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE
 * file distributed with Replay for more details.
 */

/**
 * SECTION:replay-filters-dialog
 * @brief: GtkDialog subclass to allow loading, saving and editing of filters
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "replay-filters-dialog.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <replay/replay-node-tree.h>
#include <replay/replay.h>
#include <replay/replay-log.h>

/* inherit from GtkWindow */
G_DEFINE_TYPE(ReplayFiltersDialog, replay_filters_dialog, GTK_TYPE_DIALOG);

enum
{
  PROP_0,
  PROP_FILTER_LIST,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];
struct _ReplayFiltersDialogPrivate
{
  ReplayFilterList *filter_list;

  GtkWidget *vbox;
  /* for operating on filters list */
  GtkWidget *filter_up_button;
  GtkWidget *filter_down_button;
  GtkWidget *add_filter_button;
  GtkWidget *edit_filter_button;
  GtkWidget *remove_filter_button;

  GtkTreeView *filter_tree_view;

  /* for creating new / editing existing filters */
  GtkWidget *filter_props_dialog;
  GtkWidget *pattern_text_entry;
  GtkWidget *nodes_view;
  GtkWidget *filter_action_combo_box;
  GtkWidget *group_name_combo_box_entry;
  GtkWidget *node_color_button;
  GtkWidget *group_color_button;
  GtkWidget *override_node_color_check_button;
  GtkListStore *nodes_list;
  GtkListStore *group_names_list;
  GtkListStore *filter_actions_list;
  ReplayNodeTree *node_tree;

  GtkWidget *info_bar;
  GtkWidget *info_bar_title;
  GtkWidget *info_bar_message;
};

#define REPLAY_FILTERS_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_FILTERS_DIALOG, ReplayFiltersDialogPrivate))

/* function prototypes */
static void replay_filters_dialog_dispose(GObject *object);

static void
replay_filters_dialog_get_property(GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ReplayFiltersDialog *self;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(object));

  self = REPLAY_FILTERS_DIALOG(object);

  switch (property_id) {
    case PROP_FILTER_LIST:
      g_value_set_object(value, replay_filters_dialog_get_filter_list(self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_filters_dialog_set_property(GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ReplayFiltersDialog *self;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(object));

  self = REPLAY_FILTERS_DIALOG(object);

  switch (property_id) {
    case PROP_FILTER_LIST:
      replay_filters_dialog_set_filter_list(self, g_value_dup_object(value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}


/*!
 * GObject related stuff
 */
static void replay_filters_dialog_class_init(ReplayFiltersDialogClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  obj_class->dispose = replay_filters_dialog_dispose;
  obj_class->set_property = replay_filters_dialog_set_property;
  obj_class->get_property = replay_filters_dialog_get_property;

  properties[PROP_FILTER_LIST] = g_param_spec_object("filter-list",
                                                      "filter-list",
                                                      "The current filter list which we are operating upon",
                                                      REPLAY_TYPE_FILTER_LIST,
                                                      G_PARAM_READWRITE);
  g_object_class_install_property(obj_class,
                                  PROP_FILTER_LIST,
                                  properties[PROP_FILTER_LIST]);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayFiltersDialogPrivate));

}

static void selection_changed_cb(GtkTreeSelection *selection,
                                 gpointer data)
{
  ReplayFiltersDialog *self;
  ReplayFiltersDialogPrivate *priv;
  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(data));
  self = REPLAY_FILTERS_DIALOG(data);
  priv = self->priv;

  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    gboolean sensitive;
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    /* if we have selected first row, then can't move up */
    sensitive = (gtk_tree_path_get_indices(path)[0] != 0);
    gtk_widget_set_sensitive(priv->filter_up_button, sensitive);

    sensitive =  (gtk_tree_path_get_indices(path)[0] !=
                  gtk_tree_model_iter_n_children(model, NULL) - 1);
    gtk_widget_set_sensitive(priv->filter_down_button, sensitive);

    gtk_widget_set_sensitive(priv->edit_filter_button, TRUE);
    gtk_widget_set_sensitive(priv->remove_filter_button, TRUE);
    gtk_tree_path_free(path);
  }
  else
  {
    gtk_widget_set_sensitive(priv->edit_filter_button, FALSE);
    gtk_widget_set_sensitive(priv->remove_filter_button, FALSE);
    gtk_widget_set_sensitive(priv->filter_up_button, FALSE);
    gtk_widget_set_sensitive(priv->filter_down_button, FALSE);
  }
}

static void nodes_enabled_renderer_toggled(GtkCellRendererToggle *renderer,
                                           gchar *path_str,
                                           gpointer data)
{
  ReplayFiltersDialog *self;
  ReplayFiltersDialogPrivate *priv;
  GtkTreeIter list_iter;
  gboolean set;
  GtkTreePath *list_path = gtk_tree_path_new_from_string(path_str);

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(data));
  self = REPLAY_FILTERS_DIALOG(data);
  priv = self->priv;

  gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->nodes_list),
                          &list_iter,
                          list_path);
  /* invert current value in model */
  gtk_tree_model_get(GTK_TREE_MODEL(priv->nodes_list),
                     &list_iter,
                     0, &set,
                     -1);
  gtk_list_store_set(priv->nodes_list,
                     &list_iter,
                     0, !set,
                     -1);
  gtk_tree_path_free(list_path);
}

static void
filter_action_changed(GtkComboBox *combo_box,
                      gpointer data)
{
  ReplayFiltersDialog *self;
  ReplayFiltersDialogPrivate *priv;
  ReplayFilterAction action;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(data));
  self = REPLAY_FILTERS_DIALOG(data);
  priv = self->priv;

  g_return_if_fail(combo_box == GTK_COMBO_BOX(priv->filter_action_combo_box));

  action = gtk_combo_box_get_active(combo_box);

  switch (action)
  {
    case REPLAY_FILTER_HIDE_NODES:
      gtk_widget_set_sensitive(priv->group_color_button, FALSE);
      gtk_widget_set_sensitive(priv->group_name_combo_box_entry, FALSE);
      gtk_widget_set_sensitive(priv->override_node_color_check_button, FALSE);
      gtk_widget_set_sensitive(priv->node_color_button, FALSE);
;
      break;

    case REPLAY_FILTER_GROUP_NODES:
      gtk_widget_set_sensitive(priv->group_color_button, TRUE);
      gtk_widget_set_sensitive(priv->group_name_combo_box_entry, TRUE);
      gtk_widget_set_sensitive(priv->override_node_color_check_button, FALSE);
      gtk_widget_set_sensitive(priv->node_color_button, FALSE);
;
      break;

    case REPLAY_FILTER_SET_NODE_PROPERTIES:
      gtk_widget_set_sensitive(priv->group_color_button, FALSE);
      gtk_widget_set_sensitive(priv->group_name_combo_box_entry, FALSE);
      gtk_widget_set_sensitive(priv->override_node_color_check_button, TRUE);
      gtk_widget_set_sensitive(priv->node_color_button, TRUE);
      break;

    default:
      /* we use action == -1 when resetting properties of filter dialog, so
       * ignore */
      if (action != (ReplayFilterAction)-1)
      {
        g_assert_not_reached();
      }
      break;
  }
}

static void enabled_toggled(GtkCellRendererToggle *renderer,
                            gchar *path_str,
                            gpointer data)
{
  ReplayFiltersDialog *self;
  ReplayFiltersDialogPrivate *priv;
  GtkTreeIter iter;
  gboolean enabled;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(data));
  self = REPLAY_FILTERS_DIALOG(data);
  priv = self->priv;

  enabled = !gtk_cell_renderer_toggle_get_active(renderer);
  gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(priv->filter_list),
                                      &iter,
                                      path_str);
  /* we update filter list when rows change */
  replay_filter_list_set_enabled(priv->filter_list,
                              &iter,
                              enabled);

}

static void show_filter_error(ReplayFiltersDialog *self,
                              const gchar *title,
                              const gchar *message)
{
  ReplayFiltersDialogPrivate *priv = self->priv;

  gtk_label_set_text(GTK_LABEL(priv->info_bar_title), title);
  gtk_label_set_text(GTK_LABEL(priv->info_bar_message), message);
  gtk_widget_show(priv->info_bar);
}

static void filter_props_dialog_response_cb(GtkDialog *dialog,
                                            gint response,
                                            gpointer data)
{
  ReplayFiltersDialog *self;
  ReplayFiltersDialogPrivate *priv;
  ReplayFilterAction action;
  const gchar *pattern, *group_name;
  gchar *group_color = NULL, *node_color = NULL;
  GdkRGBA rgba;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(data));

  self = REPLAY_FILTERS_DIALOG(data);
  priv = self->priv;

  selection = gtk_tree_view_get_selection(priv->filter_tree_view);

  switch (response)
  {
    case GTK_RESPONSE_OK:
      action = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->filter_action_combo_box));
      group_name = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(priv->group_name_combo_box_entry))));
      pattern = gtk_entry_get_text(GTK_ENTRY(priv->pattern_text_entry));

      /* if pattern or group_name are "" (empty string) just set as NULL */
      pattern = ((strcmp(pattern, "") == 0) ? NULL : pattern);
      group_name = ((strcmp(group_name, "") == 0) ? NULL : group_name);

      /* show a visible error message to the user if action is GROUP_NODES but
       * group name is empty */
      if (action == REPLAY_FILTER_GROUP_NODES && group_name == NULL)
      {
        show_filter_error(self, _("Error creating new filter"),
                          _("Invalid group name"));
        break;
      }

      gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(priv->group_color_button),
                                 &rgba);
      group_color = gdk_rgba_to_string(&rgba);
      gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(priv->node_color_button),
                                 &rgba);
      node_color = gdk_rgba_to_string(&rgba);

      /* if we already have a selected row, for each checked row in
       * nodes_list, see if it is already in the selected filter - if not,
       * add it - and for each unselected row, if it is already in the filter,
       * remove it */
      if (gtk_tree_selection_get_selected(selection, &model, &iter))
      {
        GtkTreeIter obj_iter;
        gboolean keep_going;
        for (keep_going = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->nodes_list),
                                                        &obj_iter);
             keep_going;
             keep_going = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->nodes_list),
                                                   &obj_iter))
        {
          gchar *name;
          gboolean enabled;
          gtk_tree_model_get(GTK_TREE_MODEL(priv->nodes_list),
                             &obj_iter,
                             0, &enabled,
                             1, &name,
                             -1);
          if (enabled)
          {
            replay_filter_list_add_to_names(REPLAY_FILTER_LIST(model), &iter, name);
          }
          else
          {
            if (replay_filter_list_has_name(REPLAY_FILTER_LIST(model), &iter, name))
            {
              /* if we remove last name, filter list will automatically remove
               * entry and hence iter will become invalid so we need to break
               * out so we don't try and use it again */
              replay_filter_list_remove_from_names(REPLAY_FILTER_LIST(model), &iter,
                                                name);
            }
          }
        }
        replay_filter_list_update_entry(REPLAY_FILTER_LIST(model), &iter,
                                     pattern,
                                     action,
                                     group_name,
                                     group_color,
                                     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->override_node_color_check_button)),
                                     node_color);
        gtk_tree_selection_unselect_iter(selection, &iter);
        /* now remove entry if it has no pattern or names to match on */
        if (pattern == NULL)
        {
          gchar **names;
          gtk_tree_model_get(model,
                             &iter,
                             REPLAY_FILTER_LIST_COL_NAMES, &names,
                             -1);
          if (g_strv_length(names) == 0)
          {
            replay_filter_list_remove_entry(REPLAY_FILTER_LIST(model), &iter);
          }
          g_strfreev(names);
        }
      }
      else
      {
        /* see if any selected names - if so create a gptrarray of copies of
         * them for filter_list and add new entry */
        GPtrArray *array = g_ptr_array_new();
        GtkTreeIter obj_iter;
        gboolean keep_going;
        for (keep_going = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->nodes_list),
                                                        &obj_iter);
             keep_going;
             keep_going = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->nodes_list),
                                                   &obj_iter))
        {
          gchar *name;
          gboolean enabled;
          gtk_tree_model_get(GTK_TREE_MODEL(priv->nodes_list),
                             &obj_iter,
                             0, &enabled,
                             1, &name,
                             -1);
          if (enabled)
          {
            g_ptr_array_add(array, name);
          }
        }

        /* ensure array is NULL terminated */
        g_ptr_array_add(array, NULL);

        /* add filter to filter list - get values from widgets in dialog */
        if (array->len > 0 || (pattern && strcmp(pattern, "") != 0))
        {
          replay_filter_list_insert_entry(REPLAY_FILTER_LIST(model), -1, TRUE, pattern,
                                       (gchar **)array->pdata,
                                       action,
                                       group_name,
                                       group_color,
                                       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->override_node_color_check_button)),
                                       node_color);
        }
        else
        {
          show_filter_error(self, _("Error creating new filter"),
                            _("No pattern or specific nodes specified"));
          g_free(group_color);
          g_free(node_color);
          g_ptr_array_free(array, TRUE);
          break;
        }
        g_ptr_array_free(array, TRUE);
      }
      g_free(group_color);
      g_free(node_color);
      gtk_widget_hide(priv->filter_props_dialog);
      break;

    default:
      gtk_widget_hide(priv->filter_props_dialog);
      break;
  }
}

static void replay_filters_dialog_update_nodes_list(ReplayFiltersDialog *self)
{
  ReplayFiltersDialogPrivate *priv;
  GtkTreeIter iter;
  GtkTreeModel *filter_list;
  GtkTreeSelection *selection;
  gboolean selected = FALSE;
  gchar *group_name = NULL;
  ReplayFilterAction action;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(self));
  priv = self->priv;

  selection = gtk_tree_view_get_selection(priv->filter_tree_view);

  /* clear any existing rows */
  gtk_list_store_clear(priv->nodes_list);

  /* add rows for name's in current selected row */
  selected = gtk_tree_selection_get_selected(selection, &filter_list, &iter);
  if (selected)
  {
    gchar **names;
    gtk_tree_model_get(filter_list, &iter,
                       REPLAY_FILTER_LIST_COL_NAMES, &names,
                       REPLAY_FILTER_LIST_COL_ACTION, &action,
                       REPLAY_FILTER_LIST_COL_GROUP_NAME, &group_name,
                       -1);
    if (names)
    {
      guint i;
      for (i = 0; i < g_strv_length(names); i++)
      {
        GtkTreeIter list_iter;

        gtk_list_store_append(priv->nodes_list, &list_iter);
        gtk_list_store_set(priv->nodes_list, &list_iter,
                           0, TRUE,
                           1, names[i],
                           -1);
      }
    }
    g_strfreev(names);
  }
  /* now append any remaining rows from object-tree which aren't already in a
   * group and which aren't the selected group */
  if (priv->node_tree)
  {
    GtkTreeIter obj_iter;
    gboolean keep_going;

    for (keep_going = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->node_tree), &obj_iter);
         keep_going;
         keep_going = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->node_tree), &obj_iter))
    {
      GtkTreeIter list_iter;
      gchar *name;
      gtk_tree_model_get(GTK_TREE_MODEL(priv->node_tree),
                         &obj_iter,
                         REPLAY_NODE_TREE_COL_ID, &name,
                         -1);
      /* make sure row we are adding is not the current group specified by the
       * filter - and is not in the group specified by the filter */
      if ((!selected ||
           (!replay_filter_list_has_name(REPLAY_FILTER_LIST(filter_list), &iter, name) &&
            (action != REPLAY_FILTER_GROUP_NODES ||
             g_strcmp0(group_name, name) != 0))))
      {
        gtk_list_store_append(priv->nodes_list, &list_iter);
        gtk_list_store_set(priv->nodes_list, &list_iter,
                           0, FALSE,
                           1, name,
                           -1);
      }
      g_free(name);
    }
  }
  g_free(group_name);
}

static void add_filter_clicked_cb(GtkButton *button,
                                  ReplayFiltersDialog *self)
{
  ReplayFiltersDialogPrivate *priv;
  GtkTreeSelection *selection;
  GdkRGBA rgba;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(self));

  priv = self->priv;

  /* unselect all and show props dialog to create a new entry */
  selection = gtk_tree_view_get_selection(priv->filter_tree_view);
  gtk_tree_selection_unselect_all(selection);
  replay_filters_dialog_update_nodes_list(self);
  gtk_entry_set_text(GTK_ENTRY(priv->pattern_text_entry), "");
  /* set nothing as active so we can set properties of all other widgets - will
   * become sensitive */
  gtk_combo_box_set_active(GTK_COMBO_BOX(priv->filter_action_combo_box), -1);
  gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(priv->group_name_combo_box_entry))), "");
  gdk_rgba_parse(&rgba, "#000");
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(priv->group_color_button),
                             &rgba);
  gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(priv->group_color_button),
                                 FALSE);
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(priv->node_color_button),
                             &rgba);
  gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(priv->node_color_button),
                                 FALSE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->override_node_color_check_button),
                               FALSE);
  /* create a hide filter by default */
  gtk_combo_box_set_active(GTK_COMBO_BOX(priv->filter_action_combo_box), 0);

  /* make sure info bar is hidden */
  gtk_widget_hide(priv->info_bar);
  gtk_dialog_run(GTK_DIALOG(priv->filter_props_dialog));
}

static void edit_filter_clicked_cb(GtkButton *button,
                                   ReplayFiltersDialog *self)
{
  ReplayFiltersDialogPrivate *priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(self));

  priv = self->priv;

  /* show props dialog with values from this entry */
  selection = gtk_tree_view_get_selection(priv->filter_tree_view);

  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    ReplayFilterAction action;
    gchar *pattern, *group_name, *group_color, *node_color;
    gboolean override_node_color;
    GdkRGBA rgba = {0};
    gtk_tree_model_get(model, &iter,
                       REPLAY_FILTER_LIST_COL_PATTERN, &pattern,
                       REPLAY_FILTER_LIST_COL_ACTION, &action,
                       REPLAY_FILTER_LIST_COL_GROUP_NAME, &group_name,
                       REPLAY_FILTER_LIST_COL_GROUP_COLOR, &group_color,
                       REPLAY_FILTER_LIST_COL_OVERRIDE_NODE_COLOR, &override_node_color,
                       REPLAY_FILTER_LIST_COL_NODE_COLOR, &node_color,
                       -1);

    replay_filters_dialog_update_nodes_list(self);

    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->filter_action_combo_box),
                             action);
    gtk_entry_set_text(GTK_ENTRY(priv->pattern_text_entry), pattern ? pattern : "");
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(priv->group_name_combo_box_entry))),
                       group_name ? group_name : "");
    if (group_color) {
      gdk_rgba_parse(&rgba, group_color);
      gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(priv->group_color_button),
                                 &rgba);
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->override_node_color_check_button),
                                 override_node_color);
    if (node_color) {
      gdk_rgba_parse(&rgba, node_color);
      gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(priv->node_color_button),
                                 &rgba);
    }
    g_free(pattern);
    g_free(group_name);
    g_free(group_color);
    g_free(node_color);
    gtk_dialog_run(GTK_DIALOG(priv->filter_props_dialog));
  }
}

static void remove_filter_clicked_cb(GtkButton *button,
                                     ReplayFiltersDialog *self)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection(self->priv->filter_tree_view);

  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    replay_filter_list_remove_entry(self->priv->filter_list, &iter);
  }
}


static void move_selected_up_cb(GtkButton *button,
                                ReplayFiltersDialog *self)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection(self->priv->filter_tree_view);

  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    replay_filter_list_move_entry_up(self->priv->filter_list, &iter);
    /* keep this one selected */
    gtk_tree_selection_select_iter(selection, &iter);
  }
}

static void move_selected_down_cb(GtkButton *button,
                                  ReplayFiltersDialog *self)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection(self->priv->filter_tree_view);

  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    replay_filter_list_move_entry_down(self->priv->filter_list, &iter);
    /* keep this one selected */
    gtk_tree_selection_select_iter(selection, &iter);
  }
}

static void replay_filters_dialog_init(ReplayFiltersDialog *self)
{
  GtkCellRenderer *enabled_renderer, *pattern_renderer, *group_name_renderer,
                  *action_renderer, *nodes_enabled_renderer,
                  *nodes_info_renderer;
  GtkTreeViewColumn *enabled_col, *action_col, *nodes_enabled_col, *pattern_col,
                    *nodes_info_col, *group_name_col;
  GtkTreeSelection *selection;
  ReplayFiltersDialogPrivate *priv;
  GtkTreeIter iter;
  GtkWidget *vbox;
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  GtkBuilder *builder;
  const gchar *root_objects[] = { "dialog-vbox", NULL };
  GError *error = NULL;
  gboolean ret;

  self->priv = priv = REPLAY_FILTERS_DIALOG_GET_PRIVATE(self);

  gtk_dialog_add_buttons(GTK_DIALOG(self),
                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                         GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                         NULL);
  gtk_window_set_title(GTK_WINDOW(self), "Replay Filters");
  gtk_window_set_destroy_with_parent(GTK_WINDOW(self), TRUE);

  builder = gtk_builder_new();
  /* TODO: use new resource framework rather than storing UI in filesystem */
  ret = gtk_builder_add_objects_from_file(builder,
                                          REPLAY_PLUGINS_DATADIR "/filters/replay-filters-dialog.ui",
                                          (gchar **)root_objects,
                                          &error);
  if (!ret) {
    g_warning("Failed to create filters dialog ui: %s", error->message);
    g_clear_error(&error);
  }

  priv->vbox = GTK_WIDGET(gtk_builder_get_object(builder, "dialog-vbox"));
  g_object_ref(priv->vbox);
  priv->filter_tree_view = GTK_TREE_VIEW(gtk_builder_get_object(builder, "filter_tree_view"));
  /* allow to select multiple rows, and set sensitivities of up and down
   * buttons when a row is selected */
  selection = gtk_tree_view_get_selection(priv->filter_tree_view);
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
  priv->add_filter_button = GTK_WIDGET(gtk_builder_get_object(builder, "add_filter_button"));
  g_signal_connect(priv->add_filter_button, "clicked",
                   G_CALLBACK(add_filter_clicked_cb), self);
  priv->remove_filter_button = GTK_WIDGET(gtk_builder_get_object(builder, "remove_filter_button"));
  g_signal_connect(priv->remove_filter_button, "clicked",
                   G_CALLBACK(remove_filter_clicked_cb), self);
  priv->edit_filter_button = GTK_WIDGET(gtk_builder_get_object(builder, "edit_filter_button"));
  g_signal_connect(priv->edit_filter_button, "clicked",
                   G_CALLBACK(edit_filter_clicked_cb), self);
  priv->filter_up_button = GTK_WIDGET(gtk_builder_get_object(builder, "filter_up_button"));
  g_signal_connect(priv->filter_up_button, "clicked",
                   G_CALLBACK(move_selected_up_cb), self);
  priv->filter_down_button = GTK_WIDGET(gtk_builder_get_object(builder, "filter_down_button"));
  g_signal_connect(priv->filter_down_button, "clicked",
                   G_CALLBACK(move_selected_down_cb), self);
  g_signal_connect(selection, "changed", G_CALLBACK(selection_changed_cb),
                   self);

  replay_filters_dialog_set_filter_list(self, replay_filter_list_new());
  enabled_renderer = gtk_cell_renderer_toggle_new();
  g_signal_connect(enabled_renderer,
                   "toggled",
                   G_CALLBACK(enabled_toggled),
                   self);
  pattern_renderer = gtk_cell_renderer_text_new();

  group_name_renderer = gtk_cell_renderer_text_new();

  action_renderer = gtk_cell_renderer_text_new();

  enabled_col = gtk_tree_view_column_new_with_attributes(_("Enabled"),
                                                         enabled_renderer,
                                                         "active", REPLAY_FILTER_LIST_COL_ENABLED,
                                                         NULL);
  pattern_col = gtk_tree_view_column_new_with_attributes(_("Pattern"),
                                                         pattern_renderer,
                                                         "text", REPLAY_FILTER_LIST_COL_PATTERN,
                                                         NULL);
  action_col = gtk_tree_view_column_new_with_attributes(_("Action"),
                                                         action_renderer,
                                                         "text", REPLAY_FILTER_LIST_COL_ACTION_STRING,
                                                         NULL);
  group_name_col = gtk_tree_view_column_new_with_attributes(_("Group Name"),
                                                            group_name_renderer,
                                                            "text", REPLAY_FILTER_LIST_COL_GROUP_NAME,
                                                            NULL);

  gtk_tree_view_insert_column(priv->filter_tree_view,
                              enabled_col,
                              0);
  gtk_tree_view_insert_column(priv->filter_tree_view,
                              pattern_col,
                              1);
  gtk_tree_view_insert_column(priv->filter_tree_view,
                              action_col,
                              2);
  gtk_tree_view_insert_column(priv->filter_tree_view,
                              group_name_col,
                              3);

  /* filter_props_dialog is run when user clicks add button, so set up some
   * static pointers to its widgets */
  /* TODO: refactor into own widget */
  /* TODO: use new resource framework rather than storing UI in filesystem */
  ret = gtk_builder_add_from_file(builder, REPLAY_PLUGINS_DATADIR "/filters/replay-filter-props-dialog.ui", &error);
  if (!ret) {
    g_warning("Failed to create filter props dialog ui: %s", error->message);
    g_clear_error(&error);
  }
  priv->filter_props_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "filter_props_dialog"));
  gtk_window_set_transient_for(GTK_WINDOW(priv->filter_props_dialog),
                               GTK_WINDOW(self));
  priv->pattern_text_entry = GTK_WIDGET(gtk_builder_get_object(builder, "pattern_text_entry"));
  priv->nodes_view = GTK_WIDGET(gtk_builder_get_object(builder, "nodes_view"));
  priv->nodes_list = gtk_list_store_new(2,
                                        G_TYPE_BOOLEAN, /* in filter */
                                        G_TYPE_STRING); /* name */

  nodes_enabled_renderer = gtk_cell_renderer_toggle_new();
  g_signal_connect(nodes_enabled_renderer,
                   "toggled",
                   G_CALLBACK(nodes_enabled_renderer_toggled),
                   self);
  nodes_enabled_col = gtk_tree_view_column_new_with_attributes(_("Select Node"),
                                                               nodes_enabled_renderer,
                                                               "active", 0,
                                                               NULL);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(priv->nodes_view),
                              nodes_enabled_col,
                              0);

  nodes_info_renderer = gtk_cell_renderer_text_new();
  nodes_info_col = gtk_tree_view_column_new_with_attributes(_("Node"),
                                                            nodes_info_renderer,
                                                            "text", 1,
                                                            NULL);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(priv->nodes_view),
                              nodes_info_col,
                              1);
  gtk_tree_view_set_model(GTK_TREE_VIEW(priv->nodes_view),
                          GTK_TREE_MODEL(priv->nodes_list));

  /* create actions list */
  priv->filter_actions_list = gtk_list_store_new(1, G_TYPE_STRING);
  gtk_list_store_insert(priv->filter_actions_list, &iter,
                        REPLAY_FILTER_HIDE_NODES);
  gtk_list_store_set(priv->filter_actions_list, &iter,
                     0, replay_filter_action_to_string(REPLAY_FILTER_HIDE_NODES), -1);
  gtk_list_store_insert(priv->filter_actions_list, &iter,
                        REPLAY_FILTER_GROUP_NODES);
  gtk_list_store_set(priv->filter_actions_list, &iter,
                     0, replay_filter_action_to_string(REPLAY_FILTER_GROUP_NODES), -1);
  gtk_list_store_insert(priv->filter_actions_list, &iter,
                        REPLAY_FILTER_SET_NODE_PROPERTIES);
  gtk_list_store_set(priv->filter_actions_list, &iter,
                     0, replay_filter_action_to_string(REPLAY_FILTER_SET_NODE_PROPERTIES), -1);

  priv->filter_action_combo_box = GTK_WIDGET(gtk_builder_get_object(builder, "filter_action_combo_box"));

  /* as the user selects a different action for the filter, make sensitive /
   * insensitive other widgets as appropriate */
  g_signal_connect(priv->filter_action_combo_box, "changed",
                   G_CALLBACK(filter_action_changed), self);

  gtk_combo_box_set_model(GTK_COMBO_BOX(priv->filter_action_combo_box),
                          GTK_TREE_MODEL(priv->filter_actions_list));
  priv->group_name_combo_box_entry = GTK_WIDGET(gtk_builder_get_object(builder, "group_name_combo_box_entry"));
  priv->group_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "group_color_button"));
  priv->override_node_color_check_button = GTK_WIDGET(gtk_builder_get_object(builder, "override_node_color_check_button"));
  priv->node_color_button = GTK_WIDGET(gtk_builder_get_object(builder, "node_color_button"));
  g_signal_connect(priv->filter_props_dialog,
                   "response",
                   G_CALLBACK(filter_props_dialog_response_cb),
                   self);
  priv->info_bar = gtk_info_bar_new_with_buttons(GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                 NULL);
  gtk_widget_set_no_show_all(priv->info_bar, TRUE);

  /* create a title label which is always bold and left aligned */
  priv->info_bar_title = gtk_label_new(NULL);
  gtk_misc_set_alignment(GTK_MISC(priv->info_bar_title), 0.0, 0.0);
  attr_list = pango_attr_list_new();
  attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  pango_attr_list_insert(attr_list, attr);
  gtk_label_set_attributes(GTK_LABEL(priv->info_bar_title), attr_list);
  pango_attr_list_unref(attr_list);

  /* create an error label which is always left aligned */
  priv->info_bar_message = gtk_label_new(NULL);
  gtk_misc_set_alignment(GTK_MISC(priv->info_bar_message), 0.0, 0.0);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(vbox), priv->info_bar_title);
  gtk_container_add(GTK_CONTAINER(vbox), priv->info_bar_message);
  gtk_container_add(GTK_CONTAINER(gtk_info_bar_get_content_area(GTK_INFO_BAR(priv->info_bar))),
                    vbox);
  gtk_widget_show_all(vbox);
  g_signal_connect(priv->info_bar, "response",
                   G_CALLBACK(gtk_widget_hide), NULL);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(priv->filter_props_dialog))),
                     priv->info_bar, FALSE, TRUE, 0);
  /* auto attach signal handlers */
  gtk_builder_connect_signals(builder, NULL);
  g_object_unref(builder);

  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(self))),
                     priv->vbox, FALSE, FALSE, 0);
  /* gtk container internals now hold this reference for us */
  g_object_unref(priv->vbox);
}


static void replay_filters_dialog_dispose(GObject *object)
{
  ReplayFiltersDialogPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(object));

  priv = REPLAY_FILTERS_DIALOG(object)->priv;

  if (priv->node_tree)
  {
    g_object_unref(priv->node_tree);
    priv->node_tree = NULL;
  }

  /* call parent dispose */
  G_OBJECT_CLASS(replay_filters_dialog_parent_class)->dispose(object);
}

void replay_filters_dialog_set_node_tree(ReplayFiltersDialog *self,
                                      ReplayNodeTree *tree)
{
  ReplayFiltersDialogPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(self));
  g_return_if_fail(REPLAY_IS_NODE_TREE(tree) || tree == NULL);

  priv = self->priv;

  if (priv->node_tree)
  {
    g_object_unref(priv->node_tree);
    priv->node_tree = NULL;
  }
  priv->node_tree = tree ? g_object_ref(tree) : NULL;
}

void replay_filters_dialog_set_filter_list(ReplayFiltersDialog *self,
                                        ReplayFilterList *list) {
  ReplayFiltersDialogPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILTERS_DIALOG(self));
  g_return_if_fail(REPLAY_IS_FILTER_LIST(list) || list == NULL);

  priv = self->priv;
  g_clear_object(&self->priv->filter_list);
  priv->filter_list = list ? g_object_ref(list) : NULL;
  gtk_tree_view_set_model(priv->filter_tree_view,
                          GTK_TREE_MODEL(priv->filter_list));

  g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_FILTER_LIST]);
}

/**
 * replay_filters_dialog_get_filter_list:
 * @self: A #ReplayFiltersDialog
 *
 * Return value: (transfer none): the current #ReplayFilterList
 *
 * Get the #ReplayFilterList for this dialog
 */
ReplayFilterList *
replay_filters_dialog_get_filter_list(ReplayFiltersDialog *self) {
  g_return_val_if_fail(REPLAY_IS_FILTERS_DIALOG(self), NULL);
  return self->priv->filter_list;
}

GtkWidget *
replay_filters_dialog_new(void) {
  return g_object_new(REPLAY_TYPE_FILTERS_DIALOG, NULL);
}
