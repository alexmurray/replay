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
 * SECTION:replay-filter-list
 * @short_description: A list all the filters
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include "replay-filter-list.h"
#include <replay/replay-event.h>

#define REPLAY_FILTER_LIST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_FILTER_LIST, ReplayFilterListPrivate))

/* store filter prefs in a GKeyFile - index of each is used as group_name in
   key file */
#define REPLAY_FILTER_LIST_KEY_ENABLED "enabled"
#define REPLAY_FILTER_LIST_KEY_PATTERN "pattern"
#define REPLAY_FILTER_LIST_KEY_NAMES "names"
#define REPLAY_FILTER_LIST_KEY_ACTION "action"
#define REPLAY_FILTER_LIST_KEY_GROUP_NAME "group_name"
#define REPLAY_FILTER_LIST_KEY_GROUP_COLOR "group_color"
#define REPLAY_FILTER_LIST_KEY_OVERRIDE_NODE_COLOR "override_node_color"
#define REPLAY_FILTER_LIST_KEY_NODE_COLOR "node_color"

/* function definitions */
static void replay_filter_list_class_init(ReplayFilterListClass *klass);

static void replay_filter_list_tree_model_init(GtkTreeModelIface *iface);

static void replay_filter_list_init(ReplayFilterList *self);

static void replay_filter_list_finalize(GObject *object);

static GtkTreeModelFlags replay_filter_list_get_flags(GtkTreeModel *tree_model);

static gint replay_filter_list_get_n_columns(GtkTreeModel *tree_model);

static GType replay_filter_list_get_column_type(GtkTreeModel *tree_model,
                                             gint index);

static gboolean replay_filter_list_get_iter(GtkTreeModel *tree_model,
                                         GtkTreeIter *iter,
                                         GtkTreePath *path);
static GtkTreePath *replay_filter_list_get_path(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter);
static void replay_filter_list_get_value(GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      gint column,
                                      GValue *value);
static gboolean replay_filter_list_iter_next(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter);
static gboolean replay_filter_list_iter_children(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter,
                                              GtkTreeIter *parent);
static gboolean replay_filter_list_iter_has_child(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter);
static gint replay_filter_list_iter_n_children(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter);
static gboolean replay_filter_list_iter_nth_child(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent,
                                               gint n);
static gboolean replay_filter_list_iter_parent(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            GtkTreeIter *child);

typedef struct _ReplayFilterListEntry ReplayFilterListEntry;

struct _ReplayFilterListEntry
{
  gboolean enabled;
  gchar *pattern;
  gchar **names;
  ReplayFilterAction action;
  gchar *group_name;
  gchar *group_color;
  gboolean override_node_color;
  gchar *node_color;

  ReplayFilterListEntry *next;
  ReplayFilterListEntry *prev;
  gint index;
};

struct _ReplayFilterListPrivate
{
  gint num_entries;
  ReplayFilterListEntry *first;
  ReplayFilterListEntry *last;

  GHashTable *hash_table; /* map group names to entries */
  gint stamp; /* Random integer to check if iter belongs to model */
};

/* define ReplayFilterList as inheriting from GObject and implementing the
   GtkTreeModel interface */
G_DEFINE_TYPE_EXTENDED(ReplayFilterList,
                       replay_filter_list,
                       G_TYPE_OBJECT,
                       0,
                       G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                                             replay_filter_list_tree_model_init));

static void replay_filter_list_class_init(ReplayFilterListClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  /* override parent methods */
  obj_class->finalize = replay_filter_list_finalize;

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayFilterListPrivate));
}

static void replay_filter_list_tree_model_init(GtkTreeModelIface *iface)
{
  iface->get_flags = replay_filter_list_get_flags;
  iface->get_n_columns = replay_filter_list_get_n_columns;
  iface->get_column_type = replay_filter_list_get_column_type;
  iface->get_iter = replay_filter_list_get_iter;
  iface->get_path = replay_filter_list_get_path;
  iface->get_value = replay_filter_list_get_value;
  iface->iter_next = replay_filter_list_iter_next;
  iface->iter_children = replay_filter_list_iter_children;
  iface->iter_has_child = replay_filter_list_iter_has_child;
  iface->iter_n_children = replay_filter_list_iter_n_children;
  iface->iter_nth_child = replay_filter_list_iter_nth_child;
  iface->iter_parent = replay_filter_list_iter_parent;
}

static void replay_filter_list_init(ReplayFilterList *self)
{
  ReplayFilterListPrivate *priv;

  self->priv = priv = REPLAY_FILTER_LIST_GET_PRIVATE(self);

  priv->num_entries = 0;
  priv->first = priv->last = NULL;
  priv->stamp = g_random_int(); /* random int to check whether an iter belongs
                                 * to our model */
}

/* removes the given entry */
static void _remove_entry(ReplayFilterList *self,
                          ReplayFilterListEntry *entry)
{
  ReplayFilterListPrivate *priv;

  priv = self->priv;

  if (priv->first == entry)
  {
    priv->first = entry->next;
  }
  if (priv->last == entry)
  {
    priv->last = entry->prev;
  }
  if (entry->prev)
  {
    entry->prev->next = entry->next;
  }
  if (entry->next)
  {
    ReplayFilterListEntry *next = entry->next;
    entry->next->prev = entry->prev;
    while (next)
    {
      next->index--;
      next = next->next;
    }
  }

  priv->num_entries--;
}

static void free_entry(ReplayFilterListEntry *entry)
{
  /* free data */
  g_free(entry->pattern);
  g_strfreev(entry->names);
  g_free(entry->group_name);
  g_free(entry->group_color);
  g_free(entry->node_color);
  g_free(entry);
}

static void replay_filter_list_finalize(GObject *object)
{
  ReplayFilterList *self;
  ReplayFilterListPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILTER_LIST(object));

  self = REPLAY_FILTER_LIST(object);
  priv = self->priv;

  while (priv->first != NULL)
  {
    ReplayFilterListEntry *entry = priv->first;
    _remove_entry(self, entry);
    free_entry(entry);
  }
  g_assert(priv->num_entries == 0);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_filter_list_parent_class)->finalize(object);
}

static GtkTreeModelFlags replay_filter_list_get_flags(GtkTreeModel *tree_model)
{
  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), (GtkTreeModelFlags)0);

  return (GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST);
}

static gint replay_filter_list_get_n_columns(GtkTreeModel *tree_model)
{
  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), 0);

  return REPLAY_FILTER_LIST_N_COLUMNS;
}

static GType column_types[REPLAY_FILTER_LIST_N_COLUMNS] = {
  G_TYPE_BOOLEAN, /* REPLAY_FILTER_LIST_COL_ENABLED */
  G_TYPE_STRING, /* REPLAY_FILTER_LIST_COL_PATTERN */
  G_TYPE_POINTER, /* REPLAY_FILTER_LIST_COL_NAMES */
  G_TYPE_INT, /* REPLAY_FILTER_LIST_COL_ACTION */
  G_TYPE_STRING, /* REPLAY_FILTER_LIST_COL_ACTION_STRING */
  G_TYPE_STRING, /* REPLAY_FILTER_LIST_COL_GROUP_NAME */
  G_TYPE_STRING, /* REPLAY_FILTER_LIST_COL_GROUP_COLOR */
  G_TYPE_BOOLEAN, /* REPLAY_FILTER_LIST_COL_OVERRIDE_NODE_COLOR */
  G_TYPE_STRING, /* REPLAY_FILTER_LIST_COL_NODE_COLOR */
};

static GType replay_filter_list_get_column_type(GtkTreeModel *tree_model,
                                             gint col)
{
  GType type = G_TYPE_INVALID;

  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), G_TYPE_INVALID);
  g_return_val_if_fail(((col < REPLAY_FILTER_LIST_N_COLUMNS) &&
                        (col >= 0)), G_TYPE_INVALID);

  /* override COL_NAMES to be G_TYPE_STRV */
  if (col == REPLAY_FILTER_LIST_COL_NAMES) {
    type = G_TYPE_STRV;
  } else {
    type = column_types[col];
  }
  return type;
}

/* Create an iter from the given path */
static gboolean replay_filter_list_get_iter(GtkTreeModel *tree_model,
                                         GtkTreeIter *iter,
                                         GtkTreePath *path)
{
  ReplayFilterList *self;
  ReplayFilterListPrivate *priv;
  ReplayFilterListEntry *entry;
  gint *indices, n, depth;

  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), FALSE);
  g_return_val_if_fail(path != NULL, FALSE);

  self = REPLAY_FILTER_LIST(tree_model);
  priv = self->priv;

  indices = gtk_tree_path_get_indices(path);
  depth = gtk_tree_path_get_depth(path);

  /* since we are a list, should always be depth 1 */
  g_assert(depth == 1);

  n = indices[0];
  if (n >= priv->num_entries || n < 0)
  {
    return FALSE;
  }

  entry = priv->first;
  while (entry->index < n)
  {
    entry = entry->next;
  }

  g_assert(entry->index == n);
  /* set stamp and only need first user_data slot */
  iter->stamp = priv->stamp;
  iter->user_data = entry;
  iter->user_data2 = NULL;
  iter->user_data3 = NULL;

  return TRUE;
}

static GtkTreePath *entry_tree_path(ReplayFilterListEntry *entry)
{
  GtkTreePath *path = gtk_tree_path_new();
  gtk_tree_path_append_index(path, entry->index);
  return path;
}

/* convert the iter into path */
static GtkTreePath *replay_filter_list_get_path(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter)
{
  ReplayFilterList *self;
  ReplayFilterListPrivate *priv;
  GtkTreePath *path;
  ReplayFilterListEntry *entry;

  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), NULL);
  g_return_val_if_fail(iter != NULL, NULL);

  self = REPLAY_FILTER_LIST(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, NULL);
  g_return_val_if_fail(iter->user_data != NULL, NULL);

  entry = (ReplayFilterListEntry *)(iter->user_data);

  path = entry_tree_path(entry);

  return path;
}

static gchar **
dup_strv(gchar **strv)
{
  gchar **new = NULL;

  if (strv) {
    GPtrArray *array;
    guint len;
    guint i;

    /* duplicate names into a GPtrArray */
    len = g_strv_length(strv);
    array = g_ptr_array_sized_new(len);

    for (i = 0; i < len; i++) {
      g_ptr_array_add(array, g_strdup(strv[i]));
    }
    /* NULL terminate */
    g_ptr_array_add(array, NULL);
    /* check last element is NULL terminated */
    g_assert(array->pdata[array->len - 1] == NULL);

    /* get the strv */
    new = (gchar **)array->pdata;
    g_ptr_array_free(array, FALSE);
  }
  return new;
}

/* return's row's exported data columns - this is what
   gtk_tree_model_get uses */
static void replay_filter_list_get_value(GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      gint column,
                                      GValue *value)
{
  ReplayFilterList *self;
  ReplayFilterListPrivate *priv;
  ReplayFilterListEntry *entry;

  g_return_if_fail(column < REPLAY_FILTER_LIST_N_COLUMNS &&
                   column >= 0);
  g_return_if_fail(REPLAY_IS_FILTER_LIST(tree_model));
  g_return_if_fail(iter != NULL);

  self = REPLAY_FILTER_LIST(tree_model);
  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);

  entry = (ReplayFilterListEntry *)(iter->user_data);

  /* initialise the value */
  g_value_init(value, column_types[column]);

  switch (column)
  {
    case REPLAY_FILTER_LIST_COL_ENABLED:
      g_value_set_boolean(value, entry->enabled);
      break;

    case REPLAY_FILTER_LIST_COL_PATTERN:
      if (entry->pattern)
      {
        g_value_set_string(value, entry->pattern);
      }
      break;

    case REPLAY_FILTER_LIST_COL_NAMES:
      g_value_set_pointer(value, dup_strv(entry->names));
      break;

    case REPLAY_FILTER_LIST_COL_ACTION:
      g_value_set_int(value, entry->action);
      break;

    case REPLAY_FILTER_LIST_COL_ACTION_STRING:
      g_value_set_string(value, replay_filter_action_to_string(entry->action));
      break;

    case REPLAY_FILTER_LIST_COL_GROUP_NAME:
      if (entry->group_name)
      {
        g_value_set_string(value, entry->group_name);
      }
      break;

    case REPLAY_FILTER_LIST_COL_GROUP_COLOR:
      if (entry->group_color)
      {
        g_value_set_string(value, entry->group_color);
      }
      break;

    case REPLAY_FILTER_LIST_COL_OVERRIDE_NODE_COLOR:
      g_value_set_boolean(value, entry->override_node_color);
      break;

    case REPLAY_FILTER_LIST_COL_NODE_COLOR:
      if (entry->node_color)
      {
        g_value_set_string(value, entry->node_color);
      }
      break;

    default:
      g_assert_not_reached();
  }
}

/* set iter to next current node */
static gboolean replay_filter_list_iter_next(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter)
{
  ReplayFilterList *self;
  ReplayFilterListPrivate *priv;
  ReplayFilterListEntry *entry;

  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_FILTER_LIST(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)(iter->user_data);

  if (entry->next != NULL)
  {
    entry = entry->next;
    iter->stamp = priv->stamp;
    iter->user_data = entry;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/* set iter to first child of parent - if parent is null, set to root
   node of model */
static gboolean replay_filter_list_iter_children(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter,
                                              GtkTreeIter *parent)
{
  ReplayFilterList *self;
  ReplayFilterListPrivate *priv;

  g_return_val_if_fail(parent == NULL || parent->user_data != NULL, FALSE);

  /* we are a list - we have no parent */
  if (parent)
  {
    return FALSE;
  }
  else
  {
    g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), FALSE);
    g_return_val_if_fail(iter != NULL, FALSE);

    self = REPLAY_FILTER_LIST(tree_model);
    priv = self->priv;

    if (priv->num_entries > 0)
    {
      iter->stamp = priv->stamp;
      iter->user_data = priv->first;
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }
}

/* return TRUE if has children, otherwise FALSE */
static gboolean replay_filter_list_iter_has_child(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter)
{
  return FALSE;
}

/* get number of children of iter - if iter is null, get number of
   children of root */
static gint replay_filter_list_iter_n_children(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter)
{
  ReplayFilterList *self;
  ReplayFilterListPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), 0);
  g_return_val_if_fail(iter == NULL || iter->user_data != NULL, FALSE);

  self = REPLAY_FILTER_LIST(tree_model);
  priv = self->priv;

  if (iter)
  {
    return 0;
  }
  else
  {
    return priv->num_entries;
  }
}

/* get nth child of parent - if parent is null, get nth root node */
static gboolean replay_filter_list_iter_nth_child(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent,
                                               gint n)
{
  ReplayFilterList *self;
  ReplayFilterListPrivate *priv;
  ReplayFilterListEntry *entry;

  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(tree_model), FALSE);
  self = REPLAY_FILTER_LIST(tree_model);
  priv = self->priv;

  if (parent)
  {
    return FALSE;
  }
  else
  {
    if (n < priv->num_entries)
    {
      entry = priv->first;
      while (entry->index < n)
      {
        entry = entry->next;
      }

      g_assert(entry->index == n);
      iter->stamp = priv->stamp;
      iter->user_data = entry;
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }
}

/* point iter to parent of child */
static gboolean replay_filter_list_iter_parent(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            GtkTreeIter *child)
{
  /* we are a list - no nodes have parents */
  return FALSE;
}

ReplayFilterList *replay_filter_list_new(void)
{
  return (ReplayFilterList *)g_object_new(REPLAY_TYPE_FILTER_LIST, NULL);
}

ReplayFilterList *replay_filter_list_new_from_file(const gchar *file_name,
                                             GError **error)
{
  ReplayFilterList *filter_list = NULL;
  gboolean ret;
  GKeyFile *key_file;

  key_file = g_key_file_new();
  /* filter list */
  ret = g_key_file_load_from_file(key_file, file_name, 0, error);
  if (ret)
  {
    unsigned int i;
    gchar **indexes;
    gsize length;

    filter_list = replay_filter_list_new();

    indexes = g_key_file_get_groups(key_file, &length);
    for (i = 0; i < length; i++)
    {
      gchar *pattern;
      ReplayFilterAction action;
      gboolean override_node_color;
      gchar *group_name, *group_color, *node_color, *action_string;
      gchar **names = NULL;

      action_string = g_key_file_get_string(key_file, indexes[i],
                                            REPLAY_FILTER_LIST_KEY_ACTION, NULL);
      if (g_strcmp0(action_string, "hide_nodes") == 0)
      {
        action = REPLAY_FILTER_HIDE_NODES;
      }
      else if (g_strcmp0(action_string, "group_nodes") == 0)
      {
        action = REPLAY_FILTER_GROUP_NODES;
      }
      else if (g_strcmp0(action_string, "set_node_properties") == 0)
      {
        action = REPLAY_FILTER_SET_NODE_PROPERTIES;
      }
      else
      {
        g_warning("Invalid filter action '%s' at index %d in file %s - skipping this filter",
                  action_string, i, file_name);
        g_free(action_string);
        continue;
      }

      pattern = g_key_file_get_string(key_file, indexes[i],
                                      REPLAY_FILTER_LIST_KEY_PATTERN, NULL);
      names = g_key_file_get_string_list(key_file, indexes[i],
                                         REPLAY_FILTER_LIST_KEY_NAMES,
                                         NULL,
                                         NULL);
      group_name = g_key_file_get_string(key_file, indexes[i],
                                         REPLAY_FILTER_LIST_KEY_GROUP_NAME, NULL);
      group_color = g_key_file_get_string(key_file, indexes[i],
                                          REPLAY_FILTER_LIST_KEY_GROUP_COLOR, NULL);
      override_node_color = g_key_file_get_boolean(key_file, indexes[i],
                                                   REPLAY_FILTER_LIST_KEY_OVERRIDE_NODE_COLOR,
                                                   NULL);
      node_color = g_key_file_get_string(key_file, indexes[i],
                                         REPLAY_FILTER_LIST_KEY_NODE_COLOR,
                                         NULL);
      /* add filter to filter list - get values from widgets in
         AddFilterData */
      replay_filter_list_insert_entry(filter_list, -1, TRUE,
                                   pattern,
                                   names,
                                   action,
                                   group_name,
                                   group_color,
                                   override_node_color,
                                   node_color);
      g_strfreev(names);
      g_free(action_string);
      g_free(pattern);
      g_free(group_name);
      g_free(group_color);
      g_free(node_color);
    }
    g_strfreev(indexes);
  }
  else
  {
    g_warning("error importing filters from file %s", file_name);
  }
  g_key_file_free(key_file);

  return filter_list;

}
gboolean replay_filter_list_to_file(ReplayFilterList *self,
                                 const gchar *file_name,
                                 GError **error)
{
  GtkTreeModel *model;
  GKeyFile *key_file;
  GFile *file;
  GFileOutputStream *out;
  GtkTreeIter iter;
  gboolean keep_going;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_FILTER_LIST(self), ret);

  model = GTK_TREE_MODEL(self);

  /* create key file */
  key_file = g_key_file_new();

  for (keep_going = gtk_tree_model_get_iter_first(model, &iter);
       keep_going;
       keep_going = gtk_tree_model_iter_next(model, &iter))
  {
    GtkTreePath *path;
    ReplayFilterAction action;
    gboolean enabled, override_node_color;
    gchar *group_name, *group_color, *pattern, *node_color;
    gchar **names;
    gchar *key = NULL;

    path = gtk_tree_model_get_path(model, &iter);
    gtk_tree_model_get(model,
                       &iter,
                       REPLAY_FILTER_LIST_COL_PATTERN, &pattern,
                       REPLAY_FILTER_LIST_COL_NAMES, &names,
                       REPLAY_FILTER_LIST_COL_ENABLED, &enabled,
                       REPLAY_FILTER_LIST_COL_ACTION, &action,
                       REPLAY_FILTER_LIST_COL_GROUP_NAME, &group_name,
                       REPLAY_FILTER_LIST_COL_GROUP_COLOR, &group_color,
                       REPLAY_FILTER_LIST_COL_OVERRIDE_NODE_COLOR, &override_node_color,
                       REPLAY_FILTER_LIST_COL_NODE_COLOR, &node_color,
                       -1);

    key = g_strdup_printf("%d", gtk_tree_path_get_indices(path)[0]);
    g_key_file_set_boolean(key_file, key,
                           REPLAY_FILTER_LIST_KEY_ENABLED,
                           enabled);
    if (pattern)
    {
      g_key_file_set_string(key_file, key,
                            REPLAY_FILTER_LIST_KEY_PATTERN,
                            pattern);
      g_free(pattern);
    }
    if (g_strv_length(names) > 0)
    {
      g_key_file_set_string_list(key_file, key,
                                 REPLAY_FILTER_LIST_KEY_NAMES,
                                 (const gchar * const *)names,
                                 g_strv_length(names));
    }

    switch (action)
    {
      case REPLAY_FILTER_HIDE_NODES:
        g_key_file_set_string(key_file, key,
                              REPLAY_FILTER_LIST_KEY_ACTION, "hide_nodes");
        break;

      case REPLAY_FILTER_GROUP_NODES:
        g_key_file_set_string(key_file, key,
                              REPLAY_FILTER_LIST_KEY_ACTION, "group_nodes");
        g_key_file_set_string(key_file, key,
                              REPLAY_FILTER_LIST_KEY_GROUP_NAME, group_name);
        g_key_file_set_string(key_file, key,
                              REPLAY_FILTER_LIST_KEY_GROUP_COLOR, group_color);
        break;

      case REPLAY_FILTER_SET_NODE_PROPERTIES:
        g_key_file_set_boolean(key_file, key,
                               REPLAY_FILTER_LIST_KEY_OVERRIDE_NODE_COLOR,
                               override_node_color);
        g_key_file_set_string(key_file, key,
                              REPLAY_FILTER_LIST_KEY_NODE_COLOR,
                              node_color);
        break;

      default:
        g_assert_not_reached();
    }
    g_strfreev(names);
    g_free(group_name);
    g_free(group_color);
    g_free(node_color);
    g_free(key);
  }
  file = g_file_new_for_path(file_name);
  out = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, error);
  if (out) {
    gchar *file_data;
    gsize file_size;
    gsize bytes_written;
    file_data = g_key_file_to_data(key_file,
                                   &file_size,
                                   NULL);
    ret = g_output_stream_write_all(G_OUTPUT_STREAM(out), file_data,
                                    file_size, &bytes_written, NULL, error);
    if (ret) {
      ret = g_output_stream_close(G_OUTPUT_STREAM(out), NULL, error);
    } else {
      /* ignore error on close */
      g_output_stream_close(G_OUTPUT_STREAM(out), NULL, NULL);
    }
    g_free(file_data);
    g_object_unref(out);
  }
  g_object_unref(file);
  g_key_file_free(key_file);

  return ret;
}
/*
 * inserts entry at given index (or as close as possible) and emits row
 * inserted
 */
static void insert_entry_at_index(ReplayFilterList *self,
                                  ReplayFilterListEntry *entry,
                                  gint i)
{
  ReplayFilterListEntry *next = NULL;
  ReplayFilterListEntry *prev = NULL;
  ReplayFilterListPrivate *priv;
  GtkTreeIter iter;
  GtkTreePath *tree_path;

  priv = self->priv;
  /* next is where we want to be */
  next = priv->first;
  while (next && next->index < i)
  {
    prev = next;
    next = next->next;
  }
  if (priv->num_entries == 0)
  {
    g_assert(priv->first == NULL && priv->last == NULL &&
             next == NULL && prev == NULL && i == 0);
    priv->first = entry;
    priv->last = entry;
  }
  /* see if we are to be the new last entry */
  else if (priv->last && i == priv->last->index + 1)
  {
    g_assert(priv->last == prev && next == NULL);
    priv->last->next = entry;
    priv->last = entry;
  }
  else if (next)
  {
    /* if first child is next (ie who we want to be) set it to be us instead */
    if (priv->first == next)
    {
      g_assert(next->index == 0);
      priv->first = entry;
    }
    g_assert(i == next->index);
    next->prev = entry;
  }
  /* point prev element to us if it exists */
  if (prev)
  {
    prev->next = entry;
  }
  /* set our index */
  entry->index = i;
  entry->prev = prev;
  entry->next = next;

  priv->num_entries++;

  /* now increment index of everything which comes after us */
  for (next = entry->next; next != NULL; next = next->next)
  {
    next->index++;
    g_assert(next->index == next->prev->index + 1);
  }

  /* signal that we have inserted a new row and where it was inserted */
  tree_path = entry_tree_path(entry);
  replay_filter_list_get_iter(GTK_TREE_MODEL(self), &iter, tree_path);
  gtk_tree_model_row_inserted(GTK_TREE_MODEL(self), tree_path, &iter);
  gtk_tree_path_free(tree_path);
}

static gboolean
entry_set_names(ReplayFilterListEntry *entry,
                gchar **names)
{
  g_strfreev(entry->names);
  entry->names = dup_strv(names);
  return TRUE;
}
static gboolean
entry_set_pattern(ReplayFilterListEntry *entry,
                  const gchar *pattern)
{
  gboolean changed = FALSE;

  if ((entry->pattern == NULL && pattern != NULL) ||
      (entry->pattern != NULL && pattern == NULL) ||
      (entry->pattern != NULL && pattern != NULL &&
       strcmp(entry->pattern, pattern) != 0))
  {
    if (entry->pattern)
    {
      g_free(entry->pattern);
    }
    /* check pattern is not empty string */
    if (pattern && strcmp(pattern, "") != 0)
    {
      entry->pattern = g_strdup(pattern);
    }
    else
    {
      entry->pattern = NULL;
    }
    changed = TRUE;
  }
  return changed;
}

static gboolean
entry_set_action(ReplayFilterListEntry *entry,
                 ReplayFilterAction action)
{
  gboolean changed = FALSE;

  if (entry->action != action)
  {
    entry->action = action;
    changed = TRUE;
  }

  return changed;
}

static gboolean
entry_set_group_name(ReplayFilterListEntry *entry,
                const gchar *group_name)
{
  gboolean changed = FALSE;

  if ((entry->group_name == NULL && group_name != NULL) ||
      (entry->group_name != NULL && group_name == NULL) ||
      (entry->group_name != NULL && group_name != NULL &&
       strcmp(entry->group_name, group_name) != 0))
  {
    if (entry->group_name)
    {
      g_free(entry->group_name);
    }
    /* check group_name is not empty string */
    if (group_name && strcmp(group_name, "") != 0)
    {
      entry->group_name = g_strdup(group_name);
    }
    else
    {
      entry->group_name = NULL;
    }
    changed = TRUE;
  }
  return changed;
}

static gboolean
entry_set_group_color(ReplayFilterListEntry *entry,
                const gchar *group_color)
{
  gboolean changed = FALSE;

  if ((entry->group_color == NULL && group_color != NULL) ||
      (entry->group_color != NULL && group_color == NULL) ||
      (entry->group_color != NULL && group_color != NULL &&
       strcmp(entry->group_color, group_color) != 0))
  {
    if (entry->group_color)
    {
      g_free(entry->group_color);
    }
    /* check group_color is not empty string */
    if (group_color && strcmp(group_color, "") != 0)
    {
      entry->group_color = g_strdup(group_color);
    }
    else
    {
      entry->group_color = NULL;
    }
    changed = TRUE;
  }
  return changed;
}

static gboolean
entry_set_node_color(ReplayFilterListEntry *entry,
                         const gchar *node_color)
{
  gboolean changed = FALSE;

  if ((entry->node_color == NULL && node_color != NULL) ||
      (entry->node_color != NULL && node_color == NULL) ||
      (entry->node_color != NULL && node_color != NULL &&
       strcmp(entry->node_color, node_color) != 0))
  {
    GdkRGBA rgba;
    if (entry->node_color)
    {
      g_free(entry->node_color);
    }
    /* check node_color is valid */
    if (node_color && gdk_rgba_parse(&rgba, node_color))
    {
      entry->node_color = g_strdup(node_color);
    }
    else
    {
      entry->node_color = NULL;
    }
    changed = TRUE;
  }

  return changed;
}

static gboolean
entry_set_override_node_color(ReplayFilterListEntry *entry,
                gboolean override_node_color)
{
  gboolean changed = FALSE;

  if (entry->override_node_color != override_node_color)
  {
    entry->override_node_color = override_node_color;
    changed = TRUE;
  }

  return changed;
}

void replay_filter_list_insert_entry(ReplayFilterList *self,
                                  gint i,
                                  gboolean enabled,
                                  const gchar *pattern,
                                  gchar **names,
                                  ReplayFilterAction action,
                                  const gchar *group_name,
                                  const gchar *group_color,
                                  gboolean override_node_color,
                                  const gchar *node_color)
{
  ReplayFilterListPrivate *priv;
  ReplayFilterListEntry *entry;

  g_return_if_fail(REPLAY_IS_FILTER_LIST(self));

  priv = self->priv;

  /* add entry - clamp index to be between 0 and num_entries */
  if (i < 0 || i > priv->num_entries)
  {
    i = priv->num_entries;
  }

  /* create new ReplayFilterList entry */
  entry = g_malloc0(sizeof(*entry));
  entry->enabled = enabled;
  entry_set_names(entry, names);
  entry_set_pattern(entry, pattern);
  entry_set_action(entry, action);
  entry_set_group_name(entry, group_name);
  entry_set_group_color(entry, group_color);
  entry_set_override_node_color(entry, override_node_color);
  entry_set_node_color(entry, node_color);
  insert_entry_at_index(self, entry, i);
}

/*
 * removes the given entry, emitting row-deleted BEFOREhand since this is
 * seemingly standard practice amongst GtkTreeModel's
 */
static void remove_entry(ReplayFilterList *self,
                         ReplayFilterListEntry *entry)
{
  GtkTreePath *tree_path;

  /* emit row deleted */
  tree_path = entry_tree_path(entry);
  gtk_tree_model_row_deleted(GTK_TREE_MODEL(self),
                             tree_path);
  gtk_tree_path_free(tree_path);

  /* then remove entry */
  _remove_entry(self, entry);
}

gboolean replay_filter_list_update_entry(ReplayFilterList *self,
                                      GtkTreeIter *iter,
                                      const gchar *pattern,
                                      ReplayFilterAction action,
                                      const gchar *group_name,
                                      const gchar *group_color,
                                      gboolean override_node_color,
                                      const gchar *node_color)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  gboolean changed = FALSE;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  changed = (entry_set_pattern(entry, pattern) |
             entry_set_action(entry, action) |
             entry_set_group_name(entry, group_name) |
             entry_set_group_color(entry, group_color) |
             entry_set_override_node_color(entry, override_node_color) |
             entry_set_node_color(entry, node_color));

  if (changed)
  {
    GtkTreePath *tree_path;
    GtkTreeIter iter2;
    /* signal that we have modified row */
    tree_path = entry_tree_path(entry);
    replay_filter_list_get_iter(GTK_TREE_MODEL(self), &iter2, tree_path);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, &iter2);
    gtk_tree_path_free(tree_path);
  }
  return TRUE;
}

gboolean replay_filter_list_set_enabled(ReplayFilterList *self,
                                     GtkTreeIter *iter,
                                     gboolean enabled)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  if (entry->enabled != enabled)
  {
    GtkTreePath *tree_path;
    entry->enabled = enabled;
    /* signal that we have modified row */
    tree_path = entry_tree_path(entry);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
    gtk_tree_path_free(tree_path);
  }
  return TRUE;
}

gboolean replay_filter_list_set_pattern(ReplayFilterList *self,
                                     GtkTreeIter *iter,
                                     gchar *pattern)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  gboolean changed = FALSE;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  changed = entry_set_pattern(entry, pattern);

  if (changed)
  {
    /* signal that we have modified row */
    GtkTreePath *tree_path = entry_tree_path(entry);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
    gtk_tree_path_free(tree_path);
  }
  return changed;
}

gboolean replay_filter_list_set_names(ReplayFilterList *self,
                                   GtkTreeIter *iter,
                                   gchar **names)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  GtkTreePath *tree_path;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  entry_set_names(entry, names);
  /* signal that we have modified row */
  tree_path = entry_tree_path(entry);
  gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
  gtk_tree_path_free(tree_path);

  return TRUE;
}

static gint find_name(ReplayFilterListEntry *entry,
                      const gchar *name)
{
  guint i;

  /* search to see if it exists */
  for (i = 0; i < g_strv_length(entry->names); i++)
  {
    if (!g_ascii_strcasecmp(name, entry->names[i]))
    {
      return i;
    }
  }
  return -1;
}


gboolean replay_filter_list_add_to_names(ReplayFilterList *self,
                                      GtkTreeIter *iter,
                                      const gchar *name)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  GtkTreePath *tree_path;
  gboolean ret = FALSE;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  if (find_name(entry, name) < 0)
  {
    GPtrArray *names;
    names = g_ptr_array_new();
    names->pdata = (void **)entry->names;
    /* this includes the NULL terminator */
    names->len = g_strv_length(entry->names) + 1;
    /* overwrite trailing NULL with new element */
    names->pdata[names->len - 1] = g_strdup(name);
    /* re-add trailing NULL */
    g_ptr_array_add(names, NULL);
    /* check last element is NULL terminated */
    g_assert(names->pdata[names->len - 1] == NULL);
    /* just incase pdata got moved */
    entry->names = (gchar **)names->pdata;
    g_ptr_array_free(names, FALSE);

    /* signal that we have modified row */
    tree_path = entry_tree_path(entry);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
    gtk_tree_path_free(tree_path);

    ret = TRUE;
  }
  return ret;
}

gboolean replay_filter_list_has_name(ReplayFilterList *self,
                                  GtkTreeIter *iter,
                                  const gchar *name)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  return (find_name(entry, name) >= 0);
}


gboolean replay_filter_list_remove_from_names(ReplayFilterList *self,
                                           GtkTreeIter *iter,
                                           const gchar *name)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  GtkTreePath *tree_path;
  int i = -1;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  i = find_name(entry, name);

  if (i >= 0)
  {
    /* remove the matching name */
    gchar *match;
    GPtrArray *names = g_ptr_array_new();
    names->pdata = (void **)entry->names;
    /* add 1 for null terminator */
    names->len = g_strv_length(entry->names) + 1;
    match = g_ptr_array_remove_index(names, i);
    g_assert(match != NULL);
    /* check last element is NULL terminated */
    g_assert(names->pdata[names->len - 1] == NULL);
    g_free(match);
    entry->names = (gchar **)names->pdata;
    g_ptr_array_free(names, FALSE);
    /* signal that we have modified row */
    tree_path = entry_tree_path(entry);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
    gtk_tree_path_free(tree_path);
  }

  return (i >= 0);
}

gboolean replay_filter_list_set_action(ReplayFilterList *self,
                                    GtkTreeIter *iter,
                                    ReplayFilterAction action)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  gboolean changed = FALSE;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  changed = entry_set_action(entry, action);

  if (changed)
  {
    /* signal that we have modified row */
    GtkTreePath *tree_path = entry_tree_path(entry);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
    gtk_tree_path_free(tree_path);
  }
  return changed;
}

gboolean replay_filter_list_set_group_name(ReplayFilterList *self,
                                        GtkTreeIter *iter,
                                        const gchar *group_name)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  gboolean changed = FALSE;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  changed = entry_set_group_name(entry, group_name);

  if (changed)
  {
    /* signal that we have modified row */
    GtkTreePath *tree_path = entry_tree_path(entry);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
    gtk_tree_path_free(tree_path);
  }
  return changed;
}

gboolean replay_filter_list_set_group_color(ReplayFilterList *self,
                                        GtkTreeIter *iter,
                                        const gchar *group_color)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  gboolean changed = FALSE;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  changed = entry_set_group_color(entry, group_color);

  if (changed)
  {
    /* signal that we have modified row */
    GtkTreePath *tree_path = entry_tree_path(entry);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
    gtk_tree_path_free(tree_path);
  }
  return changed;
}

gboolean replay_filter_list_set_node_color(ReplayFilterList *self,
                                            GtkTreeIter *iter,
                                            const gchar *node_color)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;
  gboolean changed = FALSE;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  changed = entry_set_node_color(entry, node_color);

  if (changed)
  {
    /* signal that we have modified row */
    GtkTreePath *tree_path = entry_tree_path(entry);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self), tree_path, iter);
    gtk_tree_path_free(tree_path);
  }
  return changed;
}

gboolean replay_filter_list_remove_entry(ReplayFilterList *self,
                                      GtkTreeIter *iter)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  remove_entry(self, entry);

  free_entry(entry);
  return TRUE;
}

gboolean replay_filter_list_move_entry_up(ReplayFilterList *self,
                                       GtkTreeIter *iter)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  g_return_val_if_fail(entry != priv->first, FALSE);

  remove_entry(self, entry);
  insert_entry_at_index(self, entry, entry->index - 1);
  return TRUE;
}

gboolean replay_filter_list_move_entry_down(ReplayFilterList *self,
                                         GtkTreeIter *iter)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayFilterListEntry *)iter->user_data;

  g_return_val_if_fail(entry != priv->last, FALSE);

  remove_entry(self, entry);
  insert_entry_at_index(self, entry, entry->index + 1);
  return TRUE;
}

gboolean replay_filter_list_find_group(ReplayFilterList *self,
                                    GtkTreeIter *last,
                                    GtkTreeIter *iter,
                                    const gchar *group_name)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;

  priv = self->priv;

  g_return_val_if_fail(!last || last->stamp == priv->stamp, FALSE);

  entry = last ? ((ReplayFilterListEntry *)last->user_data)->next : priv->first;
  while (entry != NULL)
  {
    if (entry->group_name && strcmp(entry->group_name, group_name) == 0)
    {
      break;
    }
    entry = entry->next;
  }

  if (entry)
  {
    iter->stamp = priv->stamp;
    iter->user_data = entry;
    return TRUE;
  }
  return FALSE;
}

void replay_filter_list_remove_all(ReplayFilterList *self)
{
  ReplayFilterListEntry *entry;
  ReplayFilterListPrivate *priv;

  priv = self->priv;

  while ((entry = priv->first) != NULL)
  {
    remove_entry(self, entry);
    free_entry(entry);
  }
}

void replay_filter_list_add_names_to_group(ReplayFilterList *self,
                                           gchar **names,
                                           const gchar *group_name,
                                           const gchar *group_color)
{
  if (group_name && strcmp(group_name, "") != 0)
  {
    replay_filter_list_insert_entry(self, -1, TRUE, NULL, names, REPLAY_FILTER_GROUP_NODES,
                                 group_name, group_color, FALSE, NULL);
  }
}

void replay_filter_list_disable_all_groups_with_name(ReplayFilterList *self,
                                                     const gchar *group_name)
{
  GtkTreeIter fl_iter;
  GtkTreeIter *last = NULL;

  while (replay_filter_list_find_group(self, last, &fl_iter, group_name))
  {
    replay_filter_list_set_enabled(self, &fl_iter, FALSE);
    last = &fl_iter;
  }
}

const gchar *
replay_filter_action_to_string(ReplayFilterAction action)
{
  const gchar *string;

  switch (action)
  {
    case REPLAY_FILTER_GROUP_NODES:
      string = _("Group nodes");
      break;

    case REPLAY_FILTER_HIDE_NODES:
      string = _("Hide nodes");
      break;

    case REPLAY_FILTER_SET_NODE_PROPERTIES:
      string = _("Set node properties");
      break;

    default:
      string = _("Invalid action");
      g_warning("Invalid action %d", action);
  }
  return string;
}
