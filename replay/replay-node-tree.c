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

/* A tree like model of all the nodes, which implements the GtkTreeModel
 * interface */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <replay/replay-node-tree.h>

#define REPLAY_NODE_TREE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_NODE_TREE, ReplayNodeTreePrivate))

/* error handling */
#define REPLAY_NODE_TREE_ERROR replay_node_tree_error_quark()

static GQuark replay_node_tree_error_quark(void)
{
  return g_quark_from_static_string("replay-node-tree-error-quark");
}

/* different error codes */
enum {
  DUPLICATE_NAME_ERROR = 0,
};

/* function definitions */
static void replay_node_tree_class_init(ReplayNodeTreeClass *klass);

static void replay_node_tree_tree_model_init(GtkTreeModelIface *iface);

static void replay_node_tree_init(ReplayNodeTree *self);

static void replay_node_tree_finalize(GObject *object);

static GtkTreeModelFlags replay_node_tree_get_flags(GtkTreeModel *tree_model);

static gint replay_node_tree_get_n_columns(GtkTreeModel *tree_model);

static GType replay_node_tree_get_column_type(GtkTreeModel *tree_model,
                                           gint index);

static gboolean replay_node_tree_get_iter(GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       GtkTreePath *path);
static GtkTreePath *replay_node_tree_get_path(GtkTreeModel *tree_model,
                                           GtkTreeIter *iter);
static void replay_node_tree_get_value(GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    gint column,
                                    GValue *value);
static gboolean replay_node_tree_iter_next(GtkTreeModel *tree_model,
                                        GtkTreeIter *iter);
static gboolean replay_node_tree_iter_children(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            GtkTreeIter *parent);
static gboolean replay_node_tree_iter_has_child(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter);
static gint replay_node_tree_iter_n_children(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter);
static gboolean replay_node_tree_iter_nth_child(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             GtkTreeIter *parent,
                                             gint n);
static gboolean replay_node_tree_iter_parent(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter,
                                          GtkTreeIter *child);

typedef struct _ReplayNodeTreeEntry ReplayNodeTreeEntry;

/* we dynamically allocate one for each object in the system */
struct _ReplayNodeTreeEntry
{
  /* is this a group entry or just a standard object entry */
  gboolean group;
  ReplayNodeTreeEntry *parent;
  ReplayNodeTreeEntry *next;
  ReplayNodeTreeEntry *prev;

  /* the children of this entry - only valid if this is a group entry */
  ReplayNodeTreeEntry *first_child;
  ReplayNodeTreeEntry *last_child;
  gint num_children;

  /* name */
  char *name;
  ReplayPropertyStore *prop_store;
  /* overridden props - take precedence with props */
  ReplayPropertyStore *oprop_store;
  GVariant *props;

  /* nodes can be requested as hidden or not - if request hidden will always
   * be hidden - if requested as not hidden, may still be hidden if either its
   * parent is hidden, or all of its children are hidden */
  gboolean hidden_request;

  /* an entry's hidden value based on hidden request and parent / children
   * hidden values */
  gboolean hidden;

  /* so we can do row_changed easily */
  gboolean was_hidden;

  /* when an entry is first created, we store it's initial order in the flat
   * tree so we can know how to order entries in groups, and out of groups */
  gint order;
  /* the actual position of this entry in its parent list */
  gint index;
};

struct _ReplayNodeTreePrivate
{
  gint num_children;
  ReplayNodeTreeEntry *first_child;
  ReplayNodeTreeEntry *last_child;

  /* added to speed things up for get_value implementation */
  gint stamp; /* Random integer to check if iter belongs to model */

  /* to speed up look-ups based on name */
  GHashTable *hash_table;
};


/* define ReplayNodeTree as inheriting from GObject and implementing the
 * GtkTreeModel interface */
G_DEFINE_TYPE_EXTENDED(ReplayNodeTree,
                       replay_node_tree,
                       G_TYPE_OBJECT,
                       0,
                       G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                                             replay_node_tree_tree_model_init));


static void replay_node_tree_class_init(ReplayNodeTreeClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  /* override parent methods */
  obj_class->finalize = replay_node_tree_finalize;

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayNodeTreePrivate));
}

static void replay_node_tree_tree_model_init(GtkTreeModelIface *iface)
{
  iface->get_flags = replay_node_tree_get_flags;
  iface->get_n_columns = replay_node_tree_get_n_columns;
  iface->get_column_type = replay_node_tree_get_column_type;
  iface->get_iter = replay_node_tree_get_iter;
  iface->get_path = replay_node_tree_get_path;
  iface->get_value = replay_node_tree_get_value;
  iface->iter_next = replay_node_tree_iter_next;
  iface->iter_children = replay_node_tree_iter_children;
  iface->iter_has_child = replay_node_tree_iter_has_child;
  iface->iter_n_children = replay_node_tree_iter_n_children;
  iface->iter_nth_child = replay_node_tree_iter_nth_child;
  iface->iter_parent = replay_node_tree_iter_parent;
}

static void replay_node_tree_init(ReplayNodeTree *self)
{
  ReplayNodeTreePrivate *priv;

  self->priv = priv = REPLAY_NODE_TREE_GET_PRIVATE(self);

  priv->num_children = 0;
  priv->first_child = NULL;
  priv->last_child = NULL;
  priv->stamp = g_random_int(); /* random int to check whether an iter
                                 * belongs to our model */
  priv->hash_table = g_hash_table_new(g_str_hash,
                                      g_str_equal);
}

static void free_entry(ReplayNodeTreeEntry *entry)
{
  g_free(entry->name);
  if (entry->props)
  {
    g_variant_unref(entry->props);
  }
  g_clear_object(&entry->prop_store);
  g_clear_object(&entry->oprop_store);
  g_free(entry);
}

static GtkTreePath *entry_tree_path(const ReplayNodeTreeEntry *entry)
{
  GtkTreePath *path = NULL;

  g_return_val_if_fail(entry != NULL, path);

  path = gtk_tree_path_new();

  while (entry != NULL)
  {
    gtk_tree_path_prepend_index(path, entry->index);
    entry = entry->parent;
  }
  return path;
}

/* Removes the entry but emits no signals */
static void _remove_entry(ReplayNodeTree *self,
                          ReplayNodeTreeEntry * const entry)
{
  ReplayNodeTreePrivate *priv = self->priv;
  ReplayNodeTreeEntry * const parent = entry->parent;
  ReplayNodeTreeEntry * const prev = entry->prev;
  ReplayNodeTreeEntry * const next = entry->next;
  ReplayNodeTreeEntry **pparent_last_child, **pparent_first_child;
  gint *pparent_num_children;

  /* use pointers to first, last and num children in either root, or parent
   * node so we can operate generically and don't need specific code for each
   * case */
  pparent_first_child = parent ? &(parent->first_child) : &(priv->first_child);
  pparent_last_child = parent ? &(parent->last_child) : &(priv->last_child);
  pparent_num_children = parent ? &(parent->num_children) : &(priv->num_children);

  /* if this is the first child, update first child to be entry after this
   * one */
  if (*pparent_first_child == entry)
  {
    g_assert(*pparent_num_children > 1 || *pparent_last_child == entry);
    *pparent_first_child = entry->next;
  }

  /* if this is the last child, update last child to be entry before this
   * one */
  if (*pparent_last_child == entry)
  {
    *pparent_last_child = entry->prev;
  }
  /* decrement number of children */
  (*pparent_num_children)--;

  /* if parent now has no children (and we actually have a real parent node
   * -ie. we are not a child of the root of the tree), emit toggle_child */
  if (*pparent_num_children == 0)
  {
    g_assert(*pparent_first_child == NULL &&
             *pparent_last_child == NULL);
  }

  /* if we have something after us, point it to the entry before us */
  if (next)
  {
    ReplayNodeTreeEntry *_next = next;
    next->prev = prev;
    /* set index of everything after us to be one less */
    while (_next != NULL)
    {
      _next->index--;
      g_assert((_next->prev == NULL && _next->index == 0) ||
               (_next->index == _next->prev->index + 1));
      _next = _next->next;
    }
  }
  /* if we have something before us, point it to the entry after us */
  if (prev)
  {
    prev->next = next;
  }
  /* remove our pointers */
  entry->parent = NULL;
  entry->next = NULL;
  entry->prev = NULL;
}


/*
 * emits row_deleted BEFORE removing the entry (since this is apparently
 * standard behaviour for GtkTreeModels - although it would make more sense
 * to do it after but oh well), then emits child_toggled if we remove the
 * last child of an entry
 */
static void remove_entry(ReplayNodeTree *self,
                         ReplayNodeTreeEntry * const entry)
{
  ReplayNodeTreeEntry *parent = entry->parent;
  GtkTreePath *entry_path = entry_tree_path(entry);

  /* emit row_deleted */
  gtk_tree_model_row_deleted(GTK_TREE_MODEL(self), entry_path);
  gtk_tree_path_free(entry_path);

  /* then remove row */
  _remove_entry(self, entry);

  /* emit toggle_child if needed */
  if (parent && parent->num_children == 0)
  {
    GtkTreePath *parent_path = entry_tree_path(parent);
    GtkTreeIter parent_iter;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(self), &parent_iter, parent_path))
    {
      gtk_tree_model_row_has_child_toggled(GTK_TREE_MODEL(self), parent_path, &parent_iter);
    }
    else
    {
      g_assert_not_reached();
    }
    gtk_tree_path_free(parent_path);
  }
}

static void replay_node_tree_finalize(GObject *object)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;

  g_return_if_fail(REPLAY_IS_NODE_TREE(object));

  self = REPLAY_NODE_TREE(object);
  priv = self->priv;

  while (priv->first_child != NULL)
  {
    ReplayNodeTreeEntry *entry = priv->first_child;
    _remove_entry(self, entry);
    free_entry(entry);
  }

  g_assert(priv->num_children == 0);
  g_hash_table_destroy(priv->hash_table);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_node_tree_parent_class)->finalize(object);
}

static GtkTreeModelFlags replay_node_tree_get_flags(GtkTreeModel *tree_model)
{
  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), (GtkTreeModelFlags)0);

  return (GTK_TREE_MODEL_ITERS_PERSIST);
}

static gint replay_node_tree_get_n_columns(GtkTreeModel *tree_model)
{
  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), 0);

  return REPLAY_NODE_TREE_N_COLUMNS;
}

static GType column_types[REPLAY_NODE_TREE_N_COLUMNS] = {
  G_TYPE_BOOLEAN,  /* REPLAY_NODE_TREE_COL_GROUP, */
  G_TYPE_STRING,  /* REPLAY_NODE_TREE_COL_ID */
  G_TYPE_STRING,  /* REPLAY_NODE_TREE_COL_LABEL */
  G_TYPE_STRING,  /* REPLAY_NODE_TREE_COL_COLOR */
  G_TYPE_BOOLEAN,  /* REPLAY_NODE_TREE_COL_HIDDEN */
  G_TYPE_BOOLEAN,  /* REPLAY_NODE_TREE_COL_REPLAYIBLE - for speed for filtermodels, */
};

static GType replay_node_tree_get_column_type(GtkTreeModel *tree_model,
                                           gint col)
{
  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), G_TYPE_INVALID);
  g_return_val_if_fail(((col < REPLAY_NODE_TREE_N_COLUMNS) &&
                        (col >= 0)), G_TYPE_INVALID);

  return column_types[col];
}

/* Create an iter from the given path */
static gboolean replay_node_tree_get_iter(GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       GtkTreePath *path)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;
  GtkTreePath *entry_path;
  gint *indices, depth;
  gint i;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), FALSE);
  g_return_val_if_fail(path != NULL, FALSE);

  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  indices = gtk_tree_path_get_indices(path);
  depth = gtk_tree_path_get_depth(path);

  /* need to walk our tree down to the position - for each indices, go along */
  entry = priv->first_child;
  if (entry)
  {
    for (i = 0; i < depth; i++)
    {
      int j;
      g_assert(entry->index == 0 &&
               (i == 0 || entry->parent != NULL));

      /* go to indices position */
      for (j = 0; j < indices[i]; j++)
      {
        if (entry->next)
        {
          entry = entry->next;
        }
        else
        {
          printf("%s: %d: %s: couldn't get index %d at depth %d - last element %s (%d) has no next\n", __FILE__, __LINE__, __func__, indices[depth], depth, entry->name, entry->index);
          return FALSE;
        }
      }
      g_assert(entry->index == indices[i]);

      /* descend if not at last level */
      if (i < depth - 1)
      {
        if (entry->first_child)
        {
          entry = entry->first_child;
        }
        else
        {
          printf("%s: %d: %s: couldn't get child depth %d - last parent element %s (%d) has no first_child\n", __FILE__, __LINE__, __func__, depth, entry->name, entry->index);
          return FALSE;
        }
      }
    }
    entry_path = entry_tree_path(entry);
    g_assert(gtk_tree_path_compare(path, entry_path) == 0);
    gtk_tree_path_free(entry_path);
    g_assert(entry != NULL);

    /* set stamp and only need first user_data slot */
    iter->stamp = priv->stamp;
    iter->user_data = entry;
    iter->user_data2 = NULL;
    iter->user_data3 = NULL;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/* convert the iter into path */
static GtkTreePath *replay_node_tree_get_path(GtkTreeModel *tree_model,
                                           GtkTreeIter *iter)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  GtkTreePath *path;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), NULL);
  g_return_val_if_fail(iter != NULL, NULL);

  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, NULL);
  g_return_val_if_fail(iter->user_data != NULL, NULL);

  entry = (ReplayNodeTreeEntry *)iter->user_data;
  path = entry_tree_path(entry);

  return path;
}

static GVariant *
entry_get_property(ReplayNodeTreeEntry *entry,
                   const gchar *name)
{
  GVariant *prop = NULL;

  prop = replay_property_store_lookup(entry->oprop_store, name);
  if (!prop)
  {
    prop = replay_property_store_lookup(entry->prop_store, name);
  }
  return prop;
}

/* return's row's exported data columns - this is what gtk_tree_model_get
 * uses */
static void replay_node_tree_get_value(GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    gint column,
                                    GValue *value)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;
  GVariant *prop;

  g_return_if_fail(column < REPLAY_NODE_TREE_N_COLUMNS &&
                   column >= 0);
  g_return_if_fail(REPLAY_IS_NODE_TREE(tree_model));
  g_return_if_fail(iter != NULL);

  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  /* initialise the value */
  g_value_init(value, column_types[column]);

  switch (column)
  {
    case REPLAY_NODE_TREE_COL_GROUP:
      g_value_set_boolean(value, entry->group);
      break;

    case REPLAY_NODE_TREE_COL_ID:
      g_value_set_string(value, entry->name);
      break;

    case REPLAY_NODE_TREE_COL_LABEL:
      prop = entry_get_property(entry, "label");
      if (prop)
      {
        g_value_set_string(value, g_variant_get_string(prop, NULL));
        g_variant_unref(prop);
      }
      else
      {
        g_value_set_string(value, entry->name);
      }
      break;

    case REPLAY_NODE_TREE_COL_COLOR:
      prop = entry_get_property(entry, "color");
      if (prop)
      {
        g_value_set_string(value, g_variant_get_string(prop, NULL));
        g_variant_unref(prop);
      }
      else
      {
        g_value_set_string(value, NULL);
      }
      break;

    case REPLAY_NODE_TREE_COL_HIDDEN:
      g_value_set_boolean(value, entry->hidden);
      break;

    case REPLAY_NODE_TREE_COL_REPLAYIBLE:
      g_value_set_boolean(value, !entry->hidden);
      break;


    default:
      g_assert_not_reached();
  }
}

/* set iter to next current node */
static gboolean replay_node_tree_iter_next(GtkTreeModel *tree_model,
                                        GtkTreeIter *iter)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  if (entry->next)
  {
    iter->stamp = priv->stamp;
    iter->user_data = entry->next;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/* set iter to first child of parent - if parent is null, set to root node of
 * model */
static gboolean replay_node_tree_iter_children(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            GtkTreeIter *parent)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(parent == NULL || parent->user_data != NULL, FALSE);
  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  if (parent)
  {
    g_return_val_if_fail(parent->stamp == priv->stamp, FALSE);

    entry = (ReplayNodeTreeEntry *)(parent->user_data);

    if (entry->first_child)
    {
      iter->stamp = priv->stamp;
      iter->user_data = entry->first_child;
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }
  else
  {
    if (priv->first_child && priv->last_child && priv->num_children > 0)
    {
      iter->stamp = priv->stamp;
      iter->user_data = priv->first_child;
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }
}

/* return TRUE if has children, otherwise FALSE */
static gboolean replay_node_tree_iter_has_child(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  return (entry->num_children != 0);
}

/* get number of children of iter - if iter is null, get number of children of
 * root */
static gint replay_node_tree_iter_n_children(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), -1);
  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  if (iter)
  {
    g_return_val_if_fail(iter->stamp == priv->stamp, -1);
    entry = (ReplayNodeTreeEntry *)(iter->user_data);
    return (entry->num_children);
  }
  else
  {
    return priv->num_children;
  }
}

/* get nth child of parent - if parent is null, get nth root node */
static gboolean replay_node_tree_iter_nth_child(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             GtkTreeIter *parent,
                                             gint n)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;
  gint i;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), FALSE);
  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  if (parent)
  {
    g_return_val_if_fail(parent->stamp == priv->stamp, FALSE);
    entry = ((ReplayNodeTreeEntry *)(parent->user_data))->first_child;
  }
  else
  {
    entry = priv->first_child;
  }

  if (entry != NULL)
  {
    for (i = 0; i < n; i++)
    {
      /* catch if n is invalid */
      if (entry->next == NULL)
      {
        return FALSE;
      }
      entry = entry->next;
    }
    iter->stamp = priv->stamp;
    iter->user_data = entry;
    return TRUE;
  }
  return FALSE;
}

/* point iter to parent of child */
static gboolean replay_node_tree_iter_parent(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter,
                                          GtkTreeIter *child)
{
  ReplayNodeTree *self;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(tree_model), FALSE);
  g_return_val_if_fail(child != NULL, FALSE);

  self = REPLAY_NODE_TREE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(child->stamp == priv->stamp, FALSE);
  g_return_val_if_fail(child->user_data != NULL, FALSE);

  entry = (ReplayNodeTreeEntry *)(child->user_data);

  if (entry->parent)
  {
    iter->stamp = priv->stamp;
    iter->user_data = entry->parent;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

ReplayNodeTree *replay_node_tree_new(void)
{
  ReplayNodeTree *self = (ReplayNodeTree *)g_object_new(REPLAY_TYPE_NODE_TREE, NULL);

  return self;
}

static ReplayNodeTreeEntry *replay_node_tree_find_object(ReplayNodeTree *self,
                                                   const gchar *id)
{
  ReplayNodeTreePrivate *priv;

  priv = self->priv;

  return (ReplayNodeTreeEntry *)g_hash_table_lookup(priv->hash_table, id);
}

GtkTreePath *replay_node_tree_get_tree_path(ReplayNodeTree *self,
                                         const gchar *id)
{
  ReplayNodeTreeEntry *entry = NULL;
  GtkTreePath *path = NULL;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), FALSE);
  g_return_val_if_fail(id != NULL, FALSE);

  entry = replay_node_tree_find_object(self, id);
  if (entry)
  {
    path = entry_tree_path(entry);
  }
  return path;
}

gboolean replay_node_tree_get_iter_for_id(ReplayNodeTree *self,
                                       GtkTreeIter *iter,
                                       const gchar *id)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), FALSE);
  g_return_val_if_fail(id != NULL, FALSE);

  priv = self->priv;

  entry = replay_node_tree_find_object(self, id);
  if (entry != NULL)
  {
    if (iter)
    {
      iter->stamp = priv->stamp;
      iter->user_data = entry;
    }
    return TRUE;
  }
  return FALSE;
}


static void emit_row_changed(ReplayNodeTree *self,
                             ReplayNodeTreeEntry *entry)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  ReplayNodeTreePrivate *priv;

  priv = self->priv;

  iter.stamp = priv->stamp;
  iter.user_data = entry;

  path = entry_tree_path(entry);

  gtk_tree_model_row_changed(GTK_TREE_MODEL(self), path, &iter);

  gtk_tree_path_free(path);
}

/* sets the entry as hidden and propagates to all of its children */
static void hide_entry_recursive(ReplayNodeTreeEntry *entry)
{
  /* hide all children */
  ReplayNodeTreeEntry *child = entry->first_child;
  g_assert(!entry->hidden);

  while (child != NULL)
  {
    hide_entry_recursive(child);
    child = child->next;
  }

  entry->hidden = TRUE;
}


static void hide_if_requested_recursive(ReplayNodeTreeEntry *entry)
{
  g_assert(!entry->hidden);

  if (entry->hidden_request)
  {
    /* hide this entry and all children */
    hide_entry_recursive(entry);
  }
  else if (entry->num_children > 0)
  {
    ReplayNodeTreeEntry *child = entry->first_child;
    gboolean all_children_hidden = TRUE;
    while (child != NULL && all_children_hidden)
    {
      /* propagate child's potential hidden request */
      hide_if_requested_recursive(child);
      all_children_hidden &= child->hidden;
      child = child->next;
    }
    if (all_children_hidden)
    {
      /* no need to call hide_entry() as we don't need to recursively hide
       * children as will have done so above */
      entry->hidden = TRUE;
    }
  }
}


/* sets the entry's was_hidden to hidden and hidden to FALSE. then calls same
 * on all children */
static void _unhide_entry_recursive(ReplayNodeTreeEntry *entry)
{
  ReplayNodeTreeEntry *child = entry->first_child;

  entry->was_hidden = entry->hidden;
  entry->hidden = FALSE;
  while (child != NULL)
  {
    _unhide_entry_recursive(child);
    child = child->next;
  }
}

static void emit_if_hidden_changed_recursive(ReplayNodeTree *self,
                                             ReplayNodeTreeEntry *entry)
{
  ReplayNodeTreeEntry *child = entry->first_child;
  while (child != NULL)
  {
    emit_if_hidden_changed_recursive(self, child);
    child = child->next;
  }

  if (entry->hidden != entry->was_hidden)
  {
    emit_row_changed(self, entry);
  }
}

static void hidden_requests_changed(ReplayNodeTree *self)
{
  ReplayNodeTreeEntry *entry;
  ReplayNodeTreePrivate *priv;

  priv = self->priv;

  entry = priv->first_child;
  while (entry != NULL)
  {
    _unhide_entry_recursive(entry);
    entry = entry->next;
  }

  entry = priv->first_child;
  while (entry != NULL)
  {
    hide_if_requested_recursive(entry);
    entry = entry->next;
  }

  entry = priv->first_child;
  while (entry != NULL)
  {
    emit_if_hidden_changed_recursive(self, entry);
    entry = entry->next;
  }
}



/*
 * Inserts the entry as close to entry->order as possible and emits
 * row_inserted on the node_tree
 */
static void insert_entry_with_parent(ReplayNodeTree *self,
                                     ReplayNodeTreeEntry *entry,
                                     ReplayNodeTreeEntry *parent)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry **pparent_last_child, **pparent_first_child;
  gint *pparent_num_children;
  gint i = 0;
  ReplayNodeTreeEntry *next;
  ReplayNodeTreeEntry *prev;
  GtkTreeIter iter;
  GtkTreePath *entry_path;

  priv = self->priv;

  /* use pointers to first, last and num children in either root, or parent
   * node so we can operate generically and don't need specific code for each
   * case */
  pparent_first_child = parent ? &(parent->first_child) : &(priv->first_child);
  pparent_last_child = parent ? &(parent->last_child) : &(priv->last_child);
  pparent_num_children = parent ? &(parent->num_children) : &(priv->num_children);

  /* append to end if requested order is -1 - also set an order in this case
   * too */
  if (entry->order == -1)
  {
    if (*pparent_last_child)
    {
      entry->order = (*pparent_last_child)->index + 1;
    }
    else
    {
      entry->order = 0;
    }
  }

  /* find index location we take the spot of next and push everything from next
   * onwards up 1 */
  next = *pparent_first_child;
  prev = NULL;
  /* skip forwards till we find where we want to be - skip groups with same
   * order as us since they don't count */
  while (next != NULL && (next->order < entry->order ||
                          (next->order == entry->order && next->group)))
  {
    i = next->index + 1;
    prev = next;
    next = prev->next;
  }
  /* if has no children, make us only child */
  if (*pparent_num_children == 0)
  {
    g_assert(*pparent_first_child == NULL && *pparent_last_child == NULL);
    g_assert(next == NULL && prev == NULL);
    *pparent_first_child = entry;
    *pparent_last_child = entry;
  }
  /* if we are going after the current last child, make sure we're parent's
   * last child now */
  else if (*pparent_last_child &&
           i == (*pparent_last_child)->index + 1)
  {
    g_assert(*pparent_last_child == prev && next == NULL);
    *pparent_last_child = entry;
  }
  else if (next)
  {
    /* if parents first child is next (ie who we want to be) set it to be us
     * instead */
    if (*pparent_first_child == next)
    {
      g_assert(next->index == 0);
      *pparent_first_child = entry;
    }
    g_assert(i == next->index);
    next->prev = entry;
  }
  /* point prev element to us if it exists */
  if (prev)
  {
    prev->next = entry;
  }
  /* set our index - this is our actual location */
  entry->index = i;
  entry->parent = parent;
  entry->prev = prev;
  entry->next = next;
  /* increments parents number of children */
  (*pparent_num_children)++;
  /* now increment index of everything which comes after us */
  for (next = entry->next; next != NULL; next = next->next)
  {
    next->index++;
    g_assert(next->index == next->prev->index + 1);
  }

  /* check consistency */
  g_assert((entry->prev == NULL && entry->next == NULL && entry->index == 0 &&
            ((entry->parent == NULL &&
              priv->first_child == entry &&
              priv->last_child == entry &&
              priv->num_children == 1) ||
             (entry->parent->first_child == entry &&
              entry->parent->last_child == entry &&
              entry->parent->num_children == 1))) ||
           (((entry->prev == NULL && entry->index == 0) ||
             (entry->index == entry->prev->index + 1)) &&
            (entry->next == NULL || entry->index + 1 == entry->next->index)));

  entry_path = entry_tree_path(entry);
  /* emit row inserted */
  if (gtk_tree_model_get_iter(GTK_TREE_MODEL(self), &iter, entry_path))
  {
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(self), entry_path, &iter);
  }
  else
  {
    g_assert_not_reached();
  }

  /* emit toggle_child for parent if needed */
  if (entry->parent && entry->parent->num_children == 1)
  {
    GtkTreePath *parent_path = entry_tree_path(entry->parent);
    GtkTreeIter parent_iter;
    g_assert(entry->parent->first_child == entry &&
             entry->parent->last_child == entry);
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(self), &parent_iter, parent_path))
    {
      gtk_tree_model_row_has_child_toggled(GTK_TREE_MODEL(self), parent_path, &parent_iter);
    }
    else
    {
      g_assert_not_reached();
    }
    gtk_tree_path_free(parent_path);
  }
  gtk_tree_path_free(entry_path);
}


gboolean replay_node_tree_request_hidden(ReplayNodeTree *self,
                                      GtkTreeIter *iter,
                                      gboolean hidden_request)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), FALSE);

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  if (entry->hidden_request != hidden_request)
  {
    entry->hidden_request = hidden_request;
    hidden_requests_changed(self);
  }

  return TRUE;
}

static void
update_props(ReplayNodeTree *self,
             ReplayNodeTreeEntry *entry)
{
  GVariant *props, *oprops;

  /* we may have one without the other */
  props = replay_property_store_get_props(entry->prop_store);
  oprops = replay_property_store_get_props(entry->oprop_store);
  if (props || oprops)
  {
    GVariantBuilder builder;
    GVariantIter iter;
    gchar *key;
    GVariant *value;
    GHashTable *keys = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    /* try oprops first */
    if (oprops)
    {
      g_variant_iter_init(&iter, oprops);
      while (g_variant_iter_loop(&iter, "{sv}", &key, &value))
      {
        if (!g_hash_table_lookup(keys, key))
        {
          gchar *copy = g_strdup(key);
          g_variant_builder_add(&builder, "{sv}", key, value);
          g_hash_table_insert(keys, copy, copy);
        }
      }
    }
    if (props)
    {
      g_variant_iter_init(&iter, props);
      while (g_variant_iter_loop(&iter, "{sv}", &key, &value))
      {
        if (!g_hash_table_lookup(keys, key))
        {
          gchar *copy = g_strdup(key);
          g_variant_builder_add(&builder, "{sv}", key, value);
          g_hash_table_insert(keys, copy, copy);
        }
      }
    }
    // reuse props
    props = g_variant_ref_sink(g_variant_builder_end(&builder));
  }
  else
  {
    props = g_variant_ref_sink(g_variant_new("a{sv}", NULL));
  }
  if (entry->props)
  {
    g_variant_unref(entry->props);
  }
  entry->props = props;
  emit_row_changed(self, entry);
}

static gboolean _replay_node_tree_insert_child(ReplayNodeTree *self,
                                            gboolean group,
                                            const gchar *name,
                                            GVariant *props,
                                            gboolean hidden,
                                            GtkTreeIter *parent,
                                            int i,
                                            GtkTreeIter *child_iter,
                                            GError **error)
{
  ReplayNodeTreeEntry *parent_entry = NULL;
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;
  gboolean ret = FALSE;

  priv = self->priv;

  if (g_hash_table_lookup(priv->hash_table, name))
  {
    /* node with this name already exists, return an error */
    g_set_error(error, REPLAY_NODE_TREE_ERROR, DUPLICATE_NAME_ERROR,
                "Node with name '%s' already exists", name);
    goto out;
  }

  /* if parent is non-null, otherwise append as a root node */
  if (parent)
  {
    g_return_val_if_fail(parent->stamp == priv->stamp, FALSE);
    g_return_val_if_fail(parent->user_data != NULL, FALSE);
    /* parent must be a group entry */
    parent_entry = (ReplayNodeTreeEntry *)parent->user_data;
    g_return_val_if_fail(parent_entry->group, FALSE);
  }

  /* create new ReplayNodeTree entry */
  entry = g_malloc0(sizeof(*entry));
  entry->name = g_strdup(name);
  entry->prop_store = replay_property_store_new();
  entry->oprop_store = replay_property_store_new();

  entry->hidden_request = hidden;
  entry->hidden = entry->hidden_request;
  entry->group = group;
  entry->order = i;

  /* add to hash table */
  g_hash_table_insert(priv->hash_table, entry->name, entry);

  /* will emit row_inserted for us */
  insert_entry_with_parent(self, entry, parent_entry);

  /* now merge in event properties - also emits row changed... */
  replay_property_store_push(entry->prop_store, props);
  update_props(self, entry);

  hidden_requests_changed(self);

  /* give back an iter to this newly added object */
  if (child_iter)
  {
    child_iter->user_data = entry;
    child_iter->user_data2 = NULL;
    child_iter->user_data3 = NULL;
    child_iter->stamp = priv->stamp;
  }
  ret = TRUE;

out:
  return ret;
}

gboolean replay_node_tree_insert_child(ReplayNodeTree *self,
                                    const gchar *name,
                                    GVariant *props,
                                    gboolean hidden,
                                    GtkTreeIter *parent,
                                    int i,
                                    GtkTreeIter *child_iter,
                                    GError **error)
{
  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), FALSE);
  g_return_val_if_fail(name != NULL, FALSE);
  return _replay_node_tree_insert_child(self, FALSE, name, props, hidden,
                                     parent, i, child_iter, error);
}

gboolean replay_node_tree_find_group(ReplayNodeTree *self,
                                  const gchar *group_name,
                                  GtkTreeIter *iter)
{
  ReplayNodeTreePrivate *priv;
  gboolean ret = FALSE;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), ret);

  priv = self->priv;

  entry = g_hash_table_lookup(priv->hash_table, group_name);
  if (entry && entry->group)
  {
    iter->user_data = entry;
    iter->stamp = priv->stamp;
    ret = TRUE;
  }
  return ret;
}

gboolean replay_node_tree_create_group(ReplayNodeTree *self,
                                    const gchar *group_name,
                                    const gchar *group_color,
                                    GtkTreeIter *iter,
                                    GError **error)
{
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), ret);

  if (!replay_node_tree_find_group(self, group_name, iter))
  {
    /* always add groups to top of graph at index 0 */
    ret = _replay_node_tree_insert_child(self, TRUE, group_name,
                                         g_variant_new_parsed("{'color': %v, 'label': %v}",
                                                              g_variant_new_string(group_color),
                                                              g_variant_new_string(group_name)),
                                         FALSE, NULL, 0, iter,
                                         error);
  }
  return ret;
}


static void move_entry_to_new_parent(ReplayNodeTree *self,
                                     ReplayNodeTreeEntry *entry,
                                     ReplayNodeTreeEntry *parent)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *old_parent;

  priv = self->priv;

  old_parent = entry->parent;

  /* remove entry from current position */
  remove_entry(self, entry);

  /* insert entry with parent */
  insert_entry_with_parent(self, entry, parent);

  /* if we were last child, remove old parent too */
  if (old_parent && old_parent->num_children == 0)
  {
    /* remove from object hash table */
    g_hash_table_remove(priv->hash_table,
                        old_parent->name);

    remove_entry(self, old_parent);
    free_entry(old_parent);
  }
  hidden_requests_changed(self);
}

gboolean replay_node_tree_move_to_group(ReplayNodeTree *self,
                                     GtkTreeIter *group_iter,
                                     GtkTreeIter *iter)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *parent;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), FALSE);

  priv = self->priv;

  g_return_val_if_fail(!group_iter || group_iter->stamp == priv->stamp, FALSE);
  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  parent = group_iter ? (ReplayNodeTreeEntry *)(group_iter->user_data) : NULL;
  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  /* new parent must be NULL or a group entry */
  g_return_val_if_fail(!parent || parent->group, FALSE);
  g_return_val_if_fail(entry != parent, FALSE);

  move_entry_to_new_parent(self, entry, parent);
  return TRUE;
}

static void move_entry_to_grand_parent(ReplayNodeTree *self,
                                       ReplayNodeTreeEntry *entry)
{
  ReplayNodeTreeEntry *new_parent;

  g_assert(entry->parent && entry->parent->group);

  new_parent = entry->parent->parent;
  /* new parent must be NULL or a group entry */
  g_assert(!new_parent || new_parent->group);

  move_entry_to_new_parent(self, entry, new_parent);
}

static gboolean replay_node_tree_remove_group_entry(ReplayNodeTree *self,
                                                 ReplayNodeTreeEntry *entry)
{
  ReplayNodeTreePrivate *priv;
  gboolean entry_removed = FALSE;

  priv = self->priv;

  g_return_val_if_fail(entry->group, FALSE);
  /* manually remove group entry if we removed its last child */
  while (!entry_removed && entry->first_child != NULL)
  {
    entry_removed = (entry->num_children == 1);
    move_entry_to_grand_parent(self, entry->first_child);
  }
  if (!entry_removed)
  {
    /* remove from object hash table */
    g_hash_table_remove(priv->hash_table,
                        entry->name);

    remove_entry(self, entry);

    hidden_requests_changed(self);
    free_entry(entry);
  }
  return TRUE;
}

gboolean replay_node_tree_remove_group(ReplayNodeTree *self,
                                    GtkTreeIter *iter)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), FALSE);

  priv = self->priv;
  g_return_val_if_fail(priv->stamp == iter->stamp, FALSE);
  entry = (ReplayNodeTreeEntry *)iter->user_data;

  return replay_node_tree_remove_group_entry(self, entry);
}

gboolean replay_node_tree_remove_from_group(ReplayNodeTree *self,
                                         GtkTreeIter *iter)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), FALSE);

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  g_return_val_if_fail(entry != NULL, FALSE);
  g_return_val_if_fail(entry->parent != NULL, FALSE);
  g_return_val_if_fail(entry->parent->group, FALSE);

  move_entry_to_grand_parent(self, entry);
  return TRUE;
}

void replay_node_tree_remove_all_groups(ReplayNodeTree *self)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_NODE_TREE(self));

  priv = self->priv;

  entry = priv->first_child;

  while (entry != NULL)
  {
    if (entry->group)
    {
      replay_node_tree_remove_group_entry(self, entry);
      /* these may have got moved to start so go from there again */
      entry = priv->first_child;
    }
    else
    {
      entry = entry->next;
    }
  }
}

static void
clear_override_props_recursive(ReplayNodeTree *self,
                               ReplayNodeTreeEntry *entry)
{
  ReplayNodeTreeEntry *child;

  replay_property_store_clear(entry->oprop_store);
  update_props(self, entry);

  /* do children */
  child = entry->first_child;
  while (child != NULL)
  {
    clear_override_props_recursive(self, child);
    child = child->next;
  }
}

void replay_node_tree_clear_all_override_props(ReplayNodeTree *self)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_NODE_TREE(self));

  priv = self->priv;

  entry = priv->first_child;

  while (entry != NULL)
  {
    clear_override_props_recursive(self, entry);
    entry = entry->next;
  }
}

static void unhide_entry_recursive(ReplayNodeTree *self,
                                   ReplayNodeTreeEntry *entry)
{
  ReplayNodeTreeEntry *child;
  gboolean changed = FALSE;

  if (entry->hidden_request)
  {
    entry->hidden_request = FALSE;
    changed = TRUE;
  }
  if (entry->hidden)
  {
    entry->hidden = FALSE;
    changed = TRUE;
  }

  if (changed)
  {
    emit_row_changed(self, entry);
  }

  /* do children */
  child = entry->first_child;
  while (child != NULL)
  {
    unhide_entry_recursive(self, child);
    child = child->next;
  }
}

void replay_node_tree_unhide_all(ReplayNodeTree *self)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_NODE_TREE(self));

  priv = self->priv;

  entry = priv->first_child;

  while (entry != NULL)
  {
    unhide_entry_recursive(self, entry);
    entry = entry->next;
  }
}

void replay_node_tree_push_props(ReplayNodeTree *self,
                              GtkTreeIter *iter,
                              GVariant *props)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_NODE_TREE(self));

  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  replay_property_store_push(entry->prop_store, props);
  update_props(self, entry);
}

void replay_node_tree_pop_props(ReplayNodeTree *self,
                             GtkTreeIter *iter,
                             GVariant *props)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_NODE_TREE(self));

  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  replay_property_store_pop(entry->prop_store, props);

  update_props(self, entry);
}

void replay_node_tree_push_override_props(ReplayNodeTree *self,
                                       GtkTreeIter *iter,
                                       GVariant *props)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_NODE_TREE(self));

  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  replay_property_store_push(entry->oprop_store, props);
  update_props(self, entry);
}

void replay_node_tree_pop_override_props(ReplayNodeTree *self,
                                      GtkTreeIter *iter,
                                      GVariant *props)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_NODE_TREE(self));

  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  replay_property_store_pop(entry->oprop_store, props);
  update_props(self, entry);
}


void replay_node_tree_clear_override_props(ReplayNodeTree *self,
                                        GtkTreeIter *iter)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_NODE_TREE(self));

  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  replay_property_store_clear(entry->oprop_store);
  update_props(self, entry);
}

/**
 * replay_node_tree_get_prop:
 * @self: A #ReplayNodeTree
 * @iter: A #GtkTreeIter referencing the node in the tree to lookup
 * @name: The name of the property to lookup
 *
 * Looks up the current value of the #GVariant with the given name for the
 * specified node, returning a reference to the #GVariant if found,
 * otherwise NULL. Must be freed using g_variant_unref() when finished
 *
 * Returns: (transfer full): A reference to the #GVariant if found, otherwise NULL.
 *
 */
GVariant *
replay_node_tree_get_prop(ReplayNodeTree *self,
                       GtkTreeIter *iter,
                       const gchar *name)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), NULL);
  g_return_val_if_fail(iter != NULL, NULL);
  g_return_val_if_fail(name != NULL, NULL);

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, NULL);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);

  return entry_get_property(entry, name);
}

/**
 * replay_node_tree_get_props:
 * @self: A #ReplayNodeTree
 * @iter: A #GtkTreeIter referencing the node in the tree to lookup
 *
 * Returns a #GVariant containing the properties for this
 * node.
 *
 * Returns: (transfer none): a  #GVariant containing the properties for this
 * node.
 */
GVariant *
replay_node_tree_get_props(ReplayNodeTree *self,
                        GtkTreeIter *iter)
{
  ReplayNodeTreePrivate *priv;
  ReplayNodeTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE(self), NULL);
  g_return_val_if_fail(iter != NULL, NULL);

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, NULL);

  entry = (ReplayNodeTreeEntry *)(iter->user_data);
  return entry->props;
}
