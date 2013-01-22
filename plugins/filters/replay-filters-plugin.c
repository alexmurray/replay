/* -*- mode: C; c-file-style: "ellemtel"; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include "replay-filters-plugin.h"
#include "replay-filters-dialog.h"
#include <replay/replay-window-activatable.h>
#include <replay/replay-option-activatable.h>
#include <replay/replay-window.h>

#define REPLAY_FILTERS_PLUGIN_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), REPLAY_TYPE_FILTERS_PLUGIN, ReplayFiltersPluginPrivate))

struct _ReplayFiltersPluginPrivate
{
  ReplayWindow *window;
  ReplayNodeTree *node_tree;
  gulong nt_ri_id;
  ReplayFiltersDialog *filters_dialog;
  /* list of filters */
  ReplayFilterList *filter_list;
  gboolean refiltering;
  gboolean rerun;
  GtkActionGroup *global_actions;
  GtkActionGroup *doc_actions;
  guint           ui_id;
  GOptionContext *context;
};

static void filter_list_row_inserted_cb(GtkTreeModel *model,
                                        GtkTreePath *path,
                                        GtkTreeIter *iter,
                                        gpointer data);
static void filter_list_row_changed_cb(GtkTreeModel *model,
                                       GtkTreePath *path,
                                       GtkTreeIter *iter,
                                       gpointer data);
static void filter_list_row_deleted_cb(GtkTreeModel *model,
                                       GtkTreePath *path,
                                       gpointer data);
static void filters_action_cb(GtkAction *action,
                              gpointer data);
static void import_action_cb(GtkAction *action,
                             gpointer data);
static void export_action_cb(GtkAction *action,
                             gpointer data);
static void hide_action_cb(GtkAction *action,
                           gpointer data);
static void group_action_cb(GtkAction *action,
                            gpointer data);
static void replay_window_activatable_iface_init(ReplayWindowActivatableInterface *iface);
static void replay_option_activatable_iface_init(ReplayOptionActivatableInterface *iface);

static GtkActionEntry global_entries[] =
{
  { "FiltersAction", GTK_STOCK_EDIT, N_("Edit F_ilters"), NULL,
    N_("Edit the current list of filters"), G_CALLBACK(filters_action_cb) },
  { "ImportAction", NULL, N_("Import Filters"), NULL,
    N_("Import previously exported filters"), G_CALLBACK(import_action_cb) },
  { "ExportAction", NULL, N_("Export Filters"), NULL,
    N_("Export the current list of filters"), G_CALLBACK(export_action_cb) },
};
static GtkActionEntry doc_entries[] =
{
  { "HideAction", NULL, N_("_Hide nodes"), "<Control>h",
    N_("Hide the currently selected nodes"), G_CALLBACK(hide_action_cb) },
  { "GroupAction", NULL, N_("_Group nodes"), "<Control>g",
    N_("Group the currently selected nodes"), G_CALLBACK(group_action_cb) },
};

static gchar *filters_file_name;
static GOptionEntry entries[] =
{
  /* filter list */
  {
    "filters", 'f', 0, G_OPTION_ARG_FILENAME,
    &filters_file_name,
    N_("Filters to load from file"),
    N_("FILTER")
  },
  { NULL }
};

#define MENU_PATH "/MenuBar/EditMenu/EditOps1"
#define TOOLBAR_PATH "/ToolBar/ToolItemOps1"

G_DEFINE_DYNAMIC_TYPE_EXTENDED(ReplayFiltersPlugin,
                               replay_filters_plugin,
                               PEAS_TYPE_EXTENSION_BASE,
                               0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(REPLAY_TYPE_WINDOW_ACTIVATABLE,
                                                             replay_window_activatable_iface_init)
                               G_IMPLEMENT_INTERFACE_DYNAMIC(REPLAY_TYPE_OPTION_ACTIVATABLE,
                                                             replay_option_activatable_iface_init));

enum {
  PROP_WINDOW = 1,
  PROP_CONTEXT = 2
};

static void replay_filters_plugin_dispose(GObject *object);

static void
notify_window_node_tree_cb(ReplayWindow *window,
                           GParamSpec *pspec,
                           gpointer data)
{
  ReplayFiltersPlugin *self = REPLAY_FILTERS_PLUGIN(data);
  ReplayFiltersPluginPrivate *priv = self->priv;

  if (priv->filters_dialog)
  {
    replay_filters_dialog_set_node_tree(priv->filters_dialog,
                                        replay_window_get_node_tree(window));
  }
}

static void
replay_filters_plugin_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  ReplayFiltersPlugin *self = REPLAY_FILTERS_PLUGIN(object);
  ReplayWindow *window;

  switch (prop_id) {
    case PROP_WINDOW:
      window = g_value_dup_object(value);
      /* window can sometimes be NULL?? */
      if (window)
      {
        self->priv->window = REPLAY_WINDOW(window);
        /* watch window's node-tree so we can update it on our filters dialog */
        g_signal_connect(self->priv->window, "notify::node-tree",
                         G_CALLBACK(notify_window_node_tree_cb), self);
      }
      break;

    case PROP_CONTEXT:
      self->priv->context = g_value_get_pointer(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
replay_filters_plugin_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  ReplayFiltersPlugin *self = REPLAY_FILTERS_PLUGIN(object);

  switch (prop_id) {
    case PROP_WINDOW:
      g_value_set_object(value, self->priv->window);
      break;

    case PROP_CONTEXT:
      g_value_set_pointer(value, self->priv->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}
static void
replay_filters_plugin_init (ReplayFiltersPlugin *self)
{
  self->priv = REPLAY_FILTERS_PLUGIN_GET_PRIVATE (self);
}

static void
replay_filters_plugin_dispose (GObject *object)
{
  ReplayFiltersPlugin *self = REPLAY_FILTERS_PLUGIN(object);
  g_clear_object(&self->priv->window);
  G_OBJECT_CLASS (replay_filters_plugin_parent_class)->dispose (object);
}

static void filters_action_cb(GtkAction *action,
                              gpointer data)
{
  ReplayFiltersPluginPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(data));

  priv = REPLAY_FILTERS_PLUGIN(data)->priv;

  gtk_widget_show_all(GTK_WIDGET(priv->filters_dialog));
  gtk_window_present(GTK_WINDOW(priv->filters_dialog));
}

static void import_action_cb(GtkAction *action,
                             gpointer data)
{
  ReplayFiltersPluginPrivate *priv;
  gint status;
  GtkWidget *dialog;

  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(data));

  priv = REPLAY_FILTERS_PLUGIN(data)->priv;
  dialog = gtk_file_chooser_dialog_new("Import Filters",
                                       GTK_WINDOW(priv->window),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                       NULL);
  status = gtk_dialog_run(GTK_DIALOG(dialog));
  if (status == GTK_RESPONSE_ACCEPT) {
    gchar *filename;
    ReplayFilterList *filter_list;
    GError *error = NULL;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    filter_list = replay_filter_list_new_from_file(filename, &error);
    if (filter_list) {
      /* set on filters dialog and wait for notify signal to actually set it
       * locally */
      replay_filters_dialog_set_filter_list(priv->filters_dialog, filter_list);
    } else {
      gchar *message = g_strdup_printf("Failed to load filters from file: %s",
                                       filename);
      replay_log_error(replay_window_get_log(priv->window), "filters-dialog", message, error ? error->message : NULL);
      g_clear_error(&error);
    }
    g_free(filename);
  }
  gtk_widget_destroy(dialog);
}

static void export_action_cb(GtkAction *action,
                             gpointer data)
{
  ReplayFiltersPluginPrivate *priv;
  gint status;
  GtkWidget *dialog;

  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(data));

  priv = REPLAY_FILTERS_PLUGIN(data)->priv;
  dialog = gtk_file_chooser_dialog_new("Export Filters",
                                                  GTK_WINDOW(priv->window),
                                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                  NULL);
  status = gtk_dialog_run(GTK_DIALOG(dialog));
  if (status == GTK_RESPONSE_ACCEPT) {
    gchar *filename;
    GError *error = NULL;
    gboolean ret;

    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    ret = replay_filter_list_to_file(priv->filter_list, filename, &error);
    if (!ret) {
      gchar *message = g_strdup_printf("Failed to export filters to file: %s",
                                       filename);
      replay_log_error(replay_window_get_log(priv->window), "filters-dialog", message, error ? error->message : NULL);
      g_clear_error(&error);
    }
    g_free(filename);
  }
  gtk_widget_destroy(dialog);
}

static void hide_action_cb(GtkAction *action,
                           gpointer data)
{
  ReplayFiltersPluginPrivate *priv;
  ReplayGraphView *graph_view;
  gchar **ids;

  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(data));

  priv = REPLAY_FILTERS_PLUGIN(data)->priv;

  graph_view = replay_window_get_graph_view(priv->window);
  ids = replay_graph_view_get_selected_node_ids(graph_view);
  replay_graph_view_set_selected_node_ids(graph_view, NULL);

  replay_filter_list_insert_entry(priv->filter_list,
                               -1, TRUE, NULL,
                               ids,
                               REPLAY_FILTER_HIDE_NODES,
                               NULL, NULL, FALSE, NULL);
  g_strfreev(ids);
}

static gboolean row_is_group(GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer data)
{
  gchar *group_name;

  gtk_tree_model_get(model, iter,
                     REPLAY_FILTER_LIST_COL_GROUP_NAME, &group_name,
                     -1);
  if (group_name)
  {
    g_free(group_name);
    return TRUE;
  }
  return FALSE;
}

static void combo_box_changed_cb(GtkComboBox *combo_box,
                                 GtkColorButton *color_button)
{
  gboolean ret;
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *color = NULL;
  GdkRGBA rgba;

  ret = gtk_combo_box_get_active_iter(combo_box, &iter);
  if (!ret)
  {
    // an existing group wasn't selected, this is a new one so ignore */
    return;
  }

  model = gtk_combo_box_get_model(combo_box);
  gtk_tree_model_get(model, &iter,
                     REPLAY_FILTER_LIST_COL_GROUP_COLOR, &color,
                     -1);
  gdk_rgba_parse(&rgba, color);
  g_free(color);
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_button),
                             &rgba);
}

gboolean prompt_for_group_details(ReplayFiltersPlugin *self,
                                  gchar **name,
                                  gchar **color)
{
  GtkWidget *dialog;
  GtkWidget *combo_box;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *color_button;
  GtkTreeModel *filtered_model;
  GtkTreeIter filter_iter;
  gint status = GTK_RESPONSE_REJECT;
  gboolean ret = FALSE;

  dialog = gtk_dialog_new_with_buttons("Select / create group",
                                       NULL,
                                       GTK_DIALOG_MODAL,
                                       GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                       NULL);
  grid = gtk_grid_new();
  label = gtk_label_new(_("Name"));
  gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

  /* filter filter list so we only show existing groups */
  filtered_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(self->priv->filter_list),
                                             NULL);
  /* show only those rows with children */
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filtered_model),
                                         row_is_group,
                                         NULL,
                                         NULL);
  combo_box = gtk_combo_box_new_with_entry();
  gtk_combo_box_set_model(GTK_COMBO_BOX(combo_box),
                          filtered_model);
  gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo_box),
                                      REPLAY_FILTER_LIST_COL_GROUP_NAME);
  gtk_grid_attach(GTK_GRID(grid), combo_box, 1, 0, 1, 1);

  label = gtk_label_new(_("Color"));
  gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);

  color_button = gtk_color_button_new();
  gtk_grid_attach(GTK_GRID(grid), color_button, 1, 1, 1, 1);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                     grid, TRUE, TRUE, 0);
  g_signal_connect(combo_box, "changed", G_CALLBACK(combo_box_changed_cb),
                   color_button);
  gtk_widget_show_all(dialog);

  status = gtk_dialog_run(GTK_DIALOG(dialog));
  ret = (status == GTK_RESPONSE_ACCEPT);

  if (ret)
  {
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo_box), &filter_iter))
    {
      gtk_tree_model_get(filtered_model, &filter_iter,
                         REPLAY_FILTER_LIST_COL_GROUP_NAME, name,
                         REPLAY_FILTER_LIST_COL_GROUP_COLOR, color,
                         -1);
    }
    else
    {
      GdkRGBA rgba;
      *name = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo_box)))));
      gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_button), &rgba);
      *color = gdk_rgba_to_string(&rgba);
    }
  }

  g_object_unref(filtered_model);
  gtk_widget_destroy(dialog);
  return ret;
}

static void group_action_cb(GtkAction *action,
                            gpointer data)
{
  ReplayFiltersPlugin *self;
  ReplayFiltersPluginPrivate *priv;
  gboolean ret;
  gchar *group_name, *group_color;

  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(data));

  self = REPLAY_FILTERS_PLUGIN(data);
  priv = self->priv;

  /* get details of group (name + color) from user */
  ret = prompt_for_group_details(self, &group_name, &group_color);
  if (ret) {
    ReplayGraphView *graph_view;
    gchar **ids;

    graph_view = replay_window_get_graph_view(priv->window);
    ids = replay_graph_view_get_selected_node_ids(graph_view);
    replay_graph_view_set_selected_node_ids(graph_view, NULL);

    replay_filter_list_add_names_to_group(priv->filter_list,
                                          ids, group_name, group_color);
    g_strfreev(ids);
  }
}

static void filters_dialog_response_cb(GtkDialog *dialog,
                                       gint response_id,
                                       gpointer user_data)
{
  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(user_data));

  switch (response_id)
  {
    case GTK_RESPONSE_HELP:
      gtk_show_uri(NULL, "help:replay-filters", gtk_get_current_event_time(), NULL);
      break;

    default:
      gtk_widget_hide(GTK_WIDGET(dialog));
      break;
  }
}

static void
filters_dialog_notify_filter_list_cb(ReplayFiltersDialog *dialog,
                                     GParamSpec *pspec,
                                     ReplayFiltersPlugin *self) {
  ReplayFiltersPluginPrivate *priv = self->priv;
  ReplayFilterList *list =  replay_filters_dialog_get_filter_list(dialog);

  g_clear_object(&priv->filter_list);

  if (list) {
    priv->filter_list = g_object_ref(list);

    /* connect to filter list */
    g_signal_connect(priv->filter_list,
                     "row-deleted",
                     G_CALLBACK(filter_list_row_deleted_cb),
                     self);
    g_signal_connect(priv->filter_list,
                     "row-inserted",
                     G_CALLBACK(filter_list_row_inserted_cb),
                     self);
    g_signal_connect(priv->filter_list,
                     "row-changed",
                     G_CALLBACK(filter_list_row_changed_cb),
                     self);
    /* make it look like the filter list had a row inserted so we connect to
     * signals etc correctly */
    filter_list_row_inserted_cb(GTK_TREE_MODEL(priv->filter_list), NULL, NULL, self);
  }
}

typedef struct _FilterProperties
{
  gchar **names;
  gchar *pattern;
  GPatternSpec *spec;
  ReplayFilterAction action;
  gchar *group_name;
  gchar *group_color;
  gboolean override_color;
  gchar *node_color;
  /* after moving rows in object-tree, gtk_tree_model_foreach() returns, so we
   * need to tell it to be re-run if we have moved rows during
   * set_filter_properties */
  gboolean rerun;
} FilterProperties;

static gboolean set_filter_properties(GtkTreeModel *model,
                                      GtkTreePath *tree_path,
                                      GtkTreeIter *iter,
                                      gpointer user_data)
{
  gchar *name;
  ReplayNodeTree *node_tree = REPLAY_NODE_TREE(model);
  FilterProperties *props = (FilterProperties *)user_data;
  GtkTreeIter group_iter;
  GtkTreePath *group_path;
  gboolean match = FALSE;

  gtk_tree_model_get(model,
                     iter,
                     REPLAY_NODE_TREE_COL_ID, &name,
                     -1);

  /* match name against pattern or against names */
  match = (props->spec && g_pattern_match_string(props->spec, name));
  if (!match && props->names)
  {
    guint i;
    for (i = 0;
         i < g_strv_length(props->names);
         i++)
    {
      match = !g_ascii_strcasecmp(name, props->names[i]);
      if (match)
      {
        break;
      }
    }
  }
  if (match)
  {
    if (props->action == REPLAY_FILTER_HIDE_NODES)
    {
      replay_node_tree_request_hidden(node_tree, iter, TRUE);
    }
    else
    {
      replay_node_tree_request_hidden(node_tree, iter, FALSE);
    }

    /* if we need to be in a group, add to group if not already there - and
     * this is not the group itself */
    if (props->action == REPLAY_FILTER_GROUP_NODES)
    {
      /* tree paths will potentially change if we create a new group -
       * otherwise this will return an existing group */
      GtkTreePath *old_tree_path = tree_path;
      /* will either create it or return existing one for us */
      if (!replay_node_tree_find_group(node_tree, props->group_name, &group_iter))
      {
        gboolean ret = replay_node_tree_create_group(node_tree,
                                                     props->group_name,
                                                     props->group_color,
                                                     &group_iter, NULL);
        g_assert(ret);
        ret = replay_node_tree_find_group(node_tree, props->group_name, &group_iter);
        g_assert(ret);
      }
      /* need to recalculate tree_path as will prob have changed after
       * inserting new group */
      tree_path = gtk_tree_model_get_path(model, iter);
      group_path = gtk_tree_model_get_path(model, &group_iter);
      if (!gtk_tree_path_is_ancestor(group_path, tree_path) &&
          gtk_tree_path_compare(group_path, tree_path) != 0)
      {
        props->rerun |= replay_node_tree_move_to_group(node_tree, &group_iter, iter);
      }
      gtk_tree_path_free(tree_path);
      tree_path = old_tree_path;
      gtk_tree_path_free(group_path);
    }
    else
    {
      /* object should not be in a group - if we are already a descendant of
       * any group, move us to root */
      if (gtk_tree_path_get_depth(tree_path) > 1)
      {
        /* move from group to root */
        props->rerun |= replay_node_tree_move_to_group(node_tree, NULL, iter);
      }
    }

    if (props->action == REPLAY_FILTER_SET_NODE_PROPERTIES)
    {
      if (props->override_color)
      {
        GVariant *node_props = g_variant_new_parsed("{'color': %v}",
                                                    g_variant_new_string(props->node_color));
        replay_node_tree_push_override_props(node_tree, iter, node_props);
        /* no need to unref as initial floating reference is taken by node
         * tree */
      }
      else
      {
        replay_node_tree_clear_override_props(node_tree, iter);
      }
    }
    else
    {
      replay_node_tree_clear_override_props(node_tree, iter);
    }
  }
  g_free(name);
  /* keep going only if we don't need to rerun */
  return props->rerun;
}

static void apply_filter_properties(ReplayFiltersPlugin *self,
                                    FilterProperties *props)
{
  ReplayFiltersPluginPrivate *priv = self->priv;

  /* unset filter properties from this pattern */
  /* gtk_tree_model_foreach stops walking the tree if we add and remove rows
   * during the walk - which is what happens if we move a row to / from a group
   * - so we need to re-run the foreach if this occurs */
  do
  {
    props->rerun = FALSE;
    gtk_tree_model_foreach(GTK_TREE_MODEL(priv->node_tree),
                           set_filter_properties,
                           props);
  } while (props->rerun);
}

static gboolean apply_filter_list_row(GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      gpointer data)
{
  ReplayFiltersPlugin *self;
  FilterProperties props;
  gboolean enabled;

  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(model), TRUE);
  g_return_val_if_fail(REPLAY_IS_FILTERS_PLUGIN(data), TRUE);

  self = REPLAY_FILTERS_PLUGIN(data);

  gtk_tree_model_get(model,
                     iter,
                     REPLAY_FILTER_LIST_COL_ENABLED, &enabled,
                     REPLAY_FILTER_LIST_COL_PATTERN, &props.pattern,
                     REPLAY_FILTER_LIST_COL_NAMES, &props.names,
                     REPLAY_FILTER_LIST_COL_ACTION, &props.action,
                     REPLAY_FILTER_LIST_COL_GROUP_NAME, &props.group_name,
                     REPLAY_FILTER_LIST_COL_GROUP_COLOR, &props.group_color,
                     REPLAY_FILTER_LIST_COL_OVERRIDE_NODE_COLOR, &props.override_color,
                     REPLAY_FILTER_LIST_COL_NODE_COLOR, &props.node_color,
                     -1);

  if (enabled)
  {
    props.spec = props.pattern ? g_pattern_spec_new(props.pattern) : NULL;
    /* apply new filter properties */
    apply_filter_properties(self, &props);
    if (props.spec)
    {
      g_pattern_spec_free(props.spec);
    }
  }
  g_strfreev(props.names);
  g_free(props.pattern);
  g_free(props.group_name);
  g_free(props.group_color);
  g_free(props.node_color);
  /* keep going */
  return FALSE;
}


/*
 * make sure we don't refilter while already refiltering - returns FALSE so can
 * be used as an idle callback
 */
static gboolean replay_filters_plugin_refilter_node_tree(ReplayFiltersPlugin *self)
{
  ReplayFiltersPluginPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_FILTERS_PLUGIN(self), FALSE);

  priv = self->priv;
  if (!priv->refiltering && priv->node_tree)
  {
    priv->refiltering = TRUE;

    /* remove all existing groups in node tree */
    replay_node_tree_remove_all_groups(priv->node_tree);
    replay_node_tree_unhide_all(priv->node_tree);
    replay_node_tree_clear_all_override_props(priv->node_tree);
    gtk_tree_model_foreach(GTK_TREE_MODEL(priv->filter_list),
                           apply_filter_list_row,
                           self);
    priv->refiltering = FALSE;
  }
  return FALSE;
}

/* when the FilterList has a row removed refilter entire node tree -
 * unfortunately the row still exists though at this time, so instead schedule
 * refilter to happen as an idle callback later
 */
static void filter_list_row_deleted_cb(GtkTreeModel *model,
                                       GtkTreePath *path,
                                       gpointer data)
{
  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(data));
  /* if there is now 0 rows in model don't need to refilter for each new entry
   * in node tree */
  if (gtk_tree_model_iter_n_children(model, NULL) == 0)
  {
    ReplayFiltersPluginPrivate *priv = REPLAY_FILTERS_PLUGIN(data)->priv;
    g_assert(priv->nt_ri_id > 0);
    g_signal_handler_disconnect(priv->node_tree, priv->nt_ri_id);
    priv->nt_ri_id = 0;
  }
  gdk_threads_add_idle((GSourceFunc)replay_filters_plugin_refilter_node_tree,
                       data);
}


/* when the FilterList changes, we need to refilter the ReplayNodeTree */
static void filter_list_row_changed_cb(GtkTreeModel *model,
                                       GtkTreePath *path,
                                       GtkTreeIter *iter,
                                       gpointer data)
{
  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(data));
  replay_filters_plugin_refilter_node_tree(REPLAY_FILTERS_PLUGIN(data));
}

/* when the new rows is inserted in FilterList */
static void filter_list_row_inserted_cb(GtkTreeModel *model,
                                        GtkTreePath *path,
                                        GtkTreeIter *iter,
                                        gpointer data)
{
  ReplayFiltersPlugin *self;
  ReplayFiltersPluginPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILTERS_PLUGIN(data));

  self = REPLAY_FILTERS_PLUGIN(data);
  priv = self->priv;

  /* if there is only 1 row in model now refilter every time a row is added to
   * node tree */
  if (gtk_tree_model_iter_n_children(model, NULL) == 1 &&
      priv->node_tree)
  {
    /* when rows get added to node tree, refilter it */
    g_assert(priv->nt_ri_id == 0);
    priv->nt_ri_id = g_signal_connect_swapped(priv->node_tree,
                                              "row-inserted",
                                              G_CALLBACK(replay_filters_plugin_refilter_node_tree),
                                              self);
  }
  replay_filters_plugin_refilter_node_tree(REPLAY_FILTERS_PLUGIN(data));
}

static void
window_notify_node_tree_cb(ReplayWindow *window,
                           GParamSpec *pspec,
                           ReplayFiltersPlugin *self) {
  ReplayFiltersPluginPrivate *priv;
  ReplayNodeTree *node_tree;

  priv = self->priv;

  if (priv->nt_ri_id) {
    g_signal_handler_disconnect(priv->node_tree, priv->nt_ri_id);
    priv->nt_ri_id = 0;
  }
  g_clear_object(&priv->node_tree);

  node_tree = replay_window_get_node_tree(window);
  if (node_tree) {
    priv->node_tree = g_object_ref(node_tree);
    if (priv->filter_list) {
      filter_list_row_inserted_cb(GTK_TREE_MODEL(priv->filter_list), NULL, NULL, self);
    }
  }
}

static void
doc_actions_notify_sensitive_cb(GtkActionGroup *doc_actions,
                                GParamSpec *pspec,
                                ReplayFiltersPlugin *self) {
  gtk_action_group_set_sensitive(self->priv->doc_actions,
                                 gtk_action_group_get_sensitive(doc_actions));
}

static void
replay_filters_plugin_activate(ReplayWindowActivatable *plugin)
{
  ReplayFiltersPlugin *self = REPLAY_FILTERS_PLUGIN(plugin);
  ReplayFiltersPluginPrivate *priv = self->priv;
  GtkActionGroup *doc_actions;
  GtkUIManager *manager;

  priv->filters_dialog = REPLAY_FILTERS_DIALOG(replay_filters_dialog_new());
  gtk_window_set_transient_for(GTK_WINDOW(priv->filters_dialog),
                               GTK_WINDOW(priv->window));
  g_signal_connect(priv->filters_dialog, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  g_signal_connect(priv->filters_dialog, "response",
                   G_CALLBACK(filters_dialog_response_cb), self);
  g_signal_connect(priv->filters_dialog, "notify::filter-list",
                   G_CALLBACK(filters_dialog_notify_filter_list_cb), self);
  replay_filters_dialog_set_filter_list(priv->filters_dialog,
                                     replay_filter_list_new());

  /* set up to do filtering - watch window for change in the node-tree property
   * and add menu options for our ui */
  manager = replay_window_get_ui_manager(priv->window);

  priv->global_actions = gtk_action_group_new("ReplayFiltersPluginGlobalActions");
  gtk_action_group_set_translation_domain(priv->global_actions,
                                          GETTEXT_PACKAGE);
  gtk_action_group_add_actions(priv->global_actions,
                               global_entries,
                               G_N_ELEMENTS(global_entries),
                               self);
  gtk_ui_manager_insert_action_group(manager, priv->global_actions, -1);

  priv->doc_actions = gtk_action_group_new("ReplayFiltersPluginDocActions");
  gtk_action_group_set_translation_domain(priv->doc_actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions(priv->doc_actions,
                               doc_entries,
                               G_N_ELEMENTS(doc_entries),
                               self);
  gtk_ui_manager_insert_action_group(manager, priv->doc_actions, -1);

  priv->ui_id = gtk_ui_manager_new_merge_id(manager);

  /* add actions to menu and toolbar */
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "EditFilters",
                        "FiltersAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  /* add a separator */
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "Separator1",
                        NULL,
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "ImportFilters",
                        "ImportAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "ExportFilters",
                        "ExportAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  /* add a separator */
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "Separator2",
                        NULL,
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "HideNodes",
                        "HideAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "GroupNodes",
                        "GroupAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        TOOLBAR_PATH,
                        "Filters",
                        "FiltersAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_action_set_is_important(gtk_action_group_get_action(priv->global_actions,
                                                          "FiltersAction"),
                              TRUE);

  /* when sensitivity of the global doc actions changes, make sure we respect that */
  doc_actions = replay_window_get_doc_action_group(priv->window);
  gtk_action_group_set_sensitive(priv->doc_actions,
                                 gtk_action_group_get_sensitive(doc_actions));
  g_signal_connect(doc_actions, "notify::sensitive",
                   G_CALLBACK(doc_actions_notify_sensitive_cb), self);
  gtk_ui_manager_ensure_update(manager);

  /* watch node-tree of window */
  g_signal_connect(priv->window, "notify::node-tree",
                   G_CALLBACK(window_notify_node_tree_cb), self);

  if (filters_file_name) {
    ReplayFilterList *filter_list = replay_filter_list_new_from_file(filters_file_name, NULL);
    if (filter_list) {
      /* set on filters dialog and wait for notify signal to actually set it
       * locally */
      replay_filters_dialog_set_filter_list(priv->filters_dialog, filter_list);
    }
  }
}

static void
replay_filters_plugin_deactivate(ReplayWindowActivatable *plugin)
{
  ReplayFiltersPlugin *self = REPLAY_FILTERS_PLUGIN(plugin);
  ReplayFiltersPluginPrivate *priv = self->priv;
  GtkUIManager *manager;
  GtkActionGroup *doc_actions;

  /* disconnect signals and remove menu options */
  g_signal_handlers_disconnect_by_func(priv->window,
                                       G_CALLBACK(window_notify_node_tree_cb),
                                       self);

  doc_actions = replay_window_get_doc_action_group(priv->window);
  g_signal_handlers_disconnect_by_func(doc_actions,
                                       G_CALLBACK(doc_actions_notify_sensitive_cb), self);
  manager = replay_window_get_ui_manager(priv->window);

  gtk_ui_manager_remove_ui(manager, priv->ui_id);
  gtk_ui_manager_remove_action_group(manager, priv->global_actions);
  g_clear_object(&priv->global_actions);
  gtk_ui_manager_remove_action_group(manager, priv->doc_actions);
  g_clear_object(&priv->doc_actions);
}

static void
replay_filters_plugin_option_activate(ReplayOptionActivatable *activatable)
{
  ReplayFiltersPlugin *self = REPLAY_FILTERS_PLUGIN(activatable);
  GOptionGroup *group = g_option_group_new("filters-plugin",
                                           "Automatic filtering for Replay",
                                           "Show filters plugin options",
                                           NULL,
                                           NULL);
  g_option_group_add_entries(group, entries);
  g_option_context_add_group(self->priv->context, group);
}

static void
replay_filters_plugin_class_init (ReplayFiltersPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (object_class, sizeof (ReplayFiltersPluginPrivate));

  object_class->get_property = replay_filters_plugin_get_property;
  object_class->set_property = replay_filters_plugin_set_property;
  object_class->dispose = replay_filters_plugin_dispose;

  g_object_class_override_property(object_class, PROP_WINDOW, "window");
  g_object_class_override_property(object_class, PROP_CONTEXT, "context");
}

static void
replay_window_activatable_iface_init(ReplayWindowActivatableInterface *iface)
{
  iface->activate = replay_filters_plugin_activate;
  iface->deactivate = replay_filters_plugin_deactivate;
}

static void
replay_option_activatable_iface_init(ReplayOptionActivatableInterface *iface)
{
  iface->activate = replay_filters_plugin_option_activate;
}

static void
replay_filters_plugin_class_finalize(ReplayFiltersPluginClass *klass)
{
  /* nothing to do */
}

G_MODULE_EXPORT void
peas_register_types(PeasObjectModule *module)
{
  replay_filters_plugin_register_type(G_TYPE_MODULE(module));

  peas_object_module_register_extension_type(module,
                                             REPLAY_TYPE_WINDOW_ACTIVATABLE,
                                             REPLAY_TYPE_FILTERS_PLUGIN);
  peas_object_module_register_extension_type(module,
                                             REPLAY_TYPE_OPTION_ACTIVATABLE,
                                             REPLAY_TYPE_FILTERS_PLUGIN);
}
