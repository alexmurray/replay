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

/*
 * A tree like model of all the events, which implements the GtkTreeModel
 * interface
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-message-tree.h>
#include <replay/replay-timestamp.h>

#define REPLAY_MESSAGE_TREE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_MESSAGE_TREE, ReplayMessageTreePrivate))

/* signals we emit */
enum
{
  CURRENT_CHANGED = 0,
  LAST_SIGNAL
};

static guint replay_message_tree_signals[LAST_SIGNAL] = {0};

/* function definitions */
static void replay_message_tree_class_init(ReplayMessageTreeClass *klass);

static void replay_message_tree_tree_model_init(GtkTreeModelIface *iface);

static void replay_message_tree_init(ReplayMessageTree *self);

static void replay_message_tree_dispose(GObject *object);
static void replay_message_tree_finalize(GObject *object);

static GtkTreeModelFlags replay_message_tree_get_flags(GtkTreeModel *tree_model);

static gint replay_message_tree_get_n_columns(GtkTreeModel *tree_model);

static GType replay_message_tree_get_column_type(GtkTreeModel *tree_model,
                                              gint index);
static gboolean replay_message_tree_get_iter(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter,
                                          GtkTreePath *path);
static GtkTreePath *replay_message_tree_get_path(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter);
static void replay_message_tree_get_value(GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       gint column,
                                       GValue *value);
static gboolean replay_message_tree_iter_next(GtkTreeModel *tree_model,
                                           GtkTreeIter *iter);
static gboolean replay_message_tree_iter_children(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent);
static gboolean replay_message_tree_iter_has_child(GtkTreeModel *tree_model,
                                                GtkTreeIter *iter);
static gint replay_message_tree_iter_n_children(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter);
static gboolean replay_message_tree_iter_nth_child(GtkTreeModel *tree_model,
                                                GtkTreeIter *iter,
                                                GtkTreeIter *parent,
                                                gint n);
static gboolean replay_message_tree_iter_parent(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             GtkTreeIter *child);

typedef struct _ReplayMessageTreeEntry ReplayMessageTreeEntry;

/*!
 * This structure represents a row in the ReplayMessageTree
 */
struct _ReplayMessageTreeEntry
{
  gchar *name;
  /* Iter to this send event in the ReplayEventStore */
  GtkTreeIter send_list_iter;
  gboolean send_iter_set;
  /* Iter to this recv event in the ReplayEventStore */
  GtkTreeIter recv_list_iter;
  gboolean recv_iter_set;

  ReplayMessageTreeEntry *parent; /* the original event which caused this one - we can form a
                                  tree this way */
  ReplayMessageTreeEntry *next; /* the next one on this level (ie the next with the same
                                parent) */
  ReplayMessageTreeEntry *prev; /* the prev one on this level (ie the prev with the same
                                parent) */

  ReplayMessageTreeEntry *first_child; /* the first child of this event */
  ReplayMessageTreeEntry *last_child; /* the last child of this event */
  gint num_children;

  GtkTreePath *tree_path; /* since the order of events in the tree is constant,
                             we keep a copy of the tree_path here which is
                             created when the ReplayMessageTreeEntry itself is
                             created and inserted into the tree */
};


struct _ReplayMessageTreePrivate
{
  gint num_children;
  ReplayMessageTreeEntry *first_child;
  ReplayMessageTreeEntry *last_child;

  gint stamp; /* Random integer to check if iter belongs to model */

  ReplayEventStore *event_store;

  /* we keep a hash table containing a hash of the STRING of the GtkTreePath of
   * the location of the ReplayEvent within the ReplayEventStore (ie. hash of
   * (gtk_tree_path_to_string(event->list_path)) as key to its corresponding
   * ReplayMessageTreeEntry structure in our tree */
  GHashTable *path_table;

  /* also keep a hash table mapping names to entries */
  GHashTable *name_table;
};


/* define ReplayMessageTree as inheriting from GObject and implementing the GtkTreeModel
   interface */
G_DEFINE_TYPE_EXTENDED(ReplayMessageTree,
                       replay_message_tree,
                       G_TYPE_OBJECT,
                       0,
                       G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                                             replay_message_tree_tree_model_init));

static void replay_message_tree_class_init(ReplayMessageTreeClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  /* override gobject method methods */
  obj_class->dispose = replay_message_tree_dispose;
  obj_class->finalize = replay_message_tree_finalize;

  /* install class signals */
  replay_message_tree_signals[CURRENT_CHANGED] = g_signal_new("current-changed",
                                                           G_OBJECT_CLASS_TYPE(obj_class),
                                                           G_SIGNAL_RUN_LAST,
                                                           G_STRUCT_OFFSET(ReplayMessageTreeClass, current_changed),
                                                           NULL, NULL,
                                                           g_cclosure_marshal_VOID__BOXED,
                                                           G_TYPE_NONE,
                                                           1,
                                                           GTK_TYPE_TREE_ITER);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayMessageTreePrivate));
}

static void replay_message_tree_tree_model_init(GtkTreeModelIface *iface)
{
  iface->get_flags = replay_message_tree_get_flags;
  iface->get_n_columns = replay_message_tree_get_n_columns;
  iface->get_column_type = replay_message_tree_get_column_type;
  iface->get_iter = replay_message_tree_get_iter;
  iface->get_path = replay_message_tree_get_path;
  iface->get_value = replay_message_tree_get_value;
  iface->iter_next = replay_message_tree_iter_next;
  iface->iter_children = replay_message_tree_iter_children;
  iface->iter_has_child = replay_message_tree_iter_has_child;
  iface->iter_n_children = replay_message_tree_iter_n_children;
  iface->iter_nth_child = replay_message_tree_iter_nth_child;
  iface->iter_parent = replay_message_tree_iter_parent;
}

static void replay_message_tree_init(ReplayMessageTree *self)
{
  ReplayMessageTreePrivate *priv;

  self->priv = priv = REPLAY_MESSAGE_TREE_GET_PRIVATE(self);

  priv->num_children = 0;
  priv->first_child = NULL;
  priv->last_child = NULL;
  priv->stamp = g_random_int(); /* random int to check whether an iter
                                 * belongs to our model */
  priv->path_table = g_hash_table_new_full(g_str_hash,
                                           g_str_equal,
                                           g_free, /* free keys when destroyed
                                                    * as are allocated
                                                    * strings */
                                           NULL); /* don't autofree values when
                                                   * removing */
  priv->name_table = g_hash_table_new(g_str_hash,
                                      g_str_equal);
}

static void free_entry(ReplayMessageTreeEntry *entry)
{
  ReplayMessageTreeEntry *child;

  while ((child = entry->first_child) != NULL)
  {
    entry->first_child = child->next;
    free_entry(child);
  }
  gtk_tree_path_free(entry->tree_path);
  g_free(entry->name);
  g_free(entry);
}

static void replay_message_tree_unset_data(ReplayMessageTree *self)
{
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  priv = self->priv;

  if (priv->event_store)
  {
    g_object_unref(priv->event_store);
    priv->event_store = NULL;
  }
  while ((entry = priv->first_child) != NULL)
  {
    priv->first_child = entry->next;
    free_entry(entry);
  }
  priv->num_children = 0;
  priv->last_child = NULL;
  g_hash_table_remove_all(priv->path_table);
  g_hash_table_remove_all(priv->name_table);
}

static void replay_message_tree_dispose(GObject *object)
{
  g_return_if_fail(REPLAY_IS_MESSAGE_TREE(object));

  replay_message_tree_unset_data(REPLAY_MESSAGE_TREE(object));
  /* call parent dispose */
  G_OBJECT_CLASS(replay_message_tree_parent_class)->dispose(object);
}

static void replay_message_tree_finalize(GObject *object)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE(object));

  self = REPLAY_MESSAGE_TREE(object);
  priv = self->priv;

  g_hash_table_destroy(priv->path_table);
  g_hash_table_destroy(priv->name_table);

  while ((entry = priv->first_child) != NULL)
  {
    priv->first_child = entry->next;
    free_entry(entry);
  }
  /* call parent finalize */
  G_OBJECT_CLASS(replay_message_tree_parent_class)->finalize(object);
}

static GtkTreeModelFlags replay_message_tree_get_flags(GtkTreeModel *tree_model)
{
  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), (GtkTreeModelFlags)0);

  return (GTK_TREE_MODEL_ITERS_PERSIST);
}

static gint replay_message_tree_get_n_columns(GtkTreeModel *tree_model)
{
  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), 0);

  return REPLAY_MESSAGE_TREE_N_COLUMNS;
}

static GType column_types[REPLAY_MESSAGE_TREE_N_COLUMNS] = {
  G_TYPE_POINTER, /* REPLAY_MESSAGE_TREE_COL_MSG_SEND_EVENT */
  G_TYPE_POINTER, /* REPLAY_MESSAGE_TREE_COL_MSG_RECV_EVENT */
  G_TYPE_BOOLEAN, /* REPLAY_MESSAGE_TREE_COL_CURRENT */
};

static GType replay_message_tree_get_column_type(GtkTreeModel *tree_model,
                                              gint col)
{
  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), G_TYPE_INVALID);
  g_return_val_if_fail(((col < REPLAY_MESSAGE_TREE_N_COLUMNS) &&
                        (col >= 0)), G_TYPE_INVALID);

  return column_types[col];
}

/* Create an iter from the given path */
static gboolean replay_message_tree_get_iter(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter,
                                          GtkTreePath *path)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;
  gint *indices, depth;
  gint i;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), FALSE);
  g_return_val_if_fail(path != NULL, FALSE);

  self = REPLAY_MESSAGE_TREE(tree_model);
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
      /* go to indices position */
      for (j = 0; j < indices[i]; j++)
      {
        if (entry->next)
        {
          entry = entry->next;
        }
        else
        {
          return FALSE;
        }
      }

      /* descend if not at last level */
      if (i < depth - 1)
      {
        if (entry->first_child)
        {
          entry = entry->first_child;
        }
        else
        {
          return FALSE;
        }
      }
    }
    g_assert(gtk_tree_path_compare(path, entry->tree_path) == 0);
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
static GtkTreePath *replay_message_tree_get_path(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  GtkTreePath *path;
  ReplayMessageTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), NULL);
  g_return_val_if_fail(iter != NULL, NULL);

  self = REPLAY_MESSAGE_TREE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, NULL);
  g_return_val_if_fail(iter->user_data != NULL, NULL);

  entry = (ReplayMessageTreeEntry *)iter->user_data;
  path = gtk_tree_path_copy(entry->tree_path);

  return path;
}

/* return's row's exported data columns - this is what gtk_tree_model_get
   uses */
static void replay_message_tree_get_value(GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       gint column,
                                       GValue *value)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayEvent *replay_event = NULL;
  ReplayMessageTreeEntry *entry;
  gboolean current = FALSE;

  g_return_if_fail(column < REPLAY_MESSAGE_TREE_N_COLUMNS &&
                   column >= 0);
  g_return_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model));
  g_return_if_fail(iter != NULL);

  self = REPLAY_MESSAGE_TREE(tree_model);
  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);
  g_return_if_fail(iter->user_data != NULL);

  /* initialise the value */
  g_value_init(value, column_types[column]);

  entry = (ReplayMessageTreeEntry *)(iter->user_data);

  switch (column)
  {
    case REPLAY_MESSAGE_TREE_COL_MSG_SEND_EVENT:
      /* get send ReplayEvent from ReplayEventStore */
      if (entry->send_iter_set)
      {
        replay_event = replay_event_store_get_event(priv->event_store,
                                             &(entry->send_list_iter));
        g_value_set_pointer(value, replay_event);
      }
      break;

    case REPLAY_MESSAGE_TREE_COL_MSG_RECV_EVENT:
      /* get recv ReplayEvent from ReplayEventStore */
      if (entry->recv_iter_set)
      {
        replay_event = replay_event_store_get_event(priv->event_store,
                                             &(entry->recv_list_iter));
        g_value_set_pointer(value, replay_event);
      }
      break;

    case REPLAY_MESSAGE_TREE_COL_IS_CURRENT:
      if (entry->send_iter_set)
      {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->event_store),
                           &(entry->send_list_iter),
                           REPLAY_EVENT_STORE_COL_IS_CURRENT, &current,
                           -1);
      }
      if (!current && entry->recv_iter_set)
      {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->event_store),
                           &(entry->recv_list_iter),
                           REPLAY_EVENT_STORE_COL_IS_CURRENT, &current,
                           -1);
      }
      g_value_set_boolean(value, current);
      break;

    default:
      g_assert_not_reached();
  }
}

/* set iter to next current node */
static gboolean replay_message_tree_iter_next(GtkTreeModel *tree_model,
                                           GtkTreeIter *iter)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_MESSAGE_TREE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayMessageTreeEntry *)(iter->user_data);

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
   model */
static gboolean replay_message_tree_iter_children(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  g_return_val_if_fail(parent == NULL || parent->user_data != NULL, FALSE);
  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_MESSAGE_TREE(tree_model);
  priv = self->priv;

  if (parent)
  {
    g_return_val_if_fail(parent->stamp == priv->stamp, FALSE);

    entry = (ReplayMessageTreeEntry *)(parent->user_data);

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
static gboolean replay_message_tree_iter_has_child(GtkTreeModel *tree_model,
                                                GtkTreeIter *iter)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_MESSAGE_TREE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);

  entry = (ReplayMessageTreeEntry *)(iter->user_data);

  return (entry->num_children != 0);
}

/* get number of children of iter - if iter is null, get number of children of
   root */
static gint replay_message_tree_iter_n_children(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), -1);
  self = REPLAY_MESSAGE_TREE(tree_model);
  priv = self->priv;

  if (iter)
  {
    g_return_val_if_fail(iter->stamp == priv->stamp, -1);
    entry = (ReplayMessageTreeEntry *)(iter->user_data);
    return (entry->num_children);
  }
  else
  {
    return priv->num_children;
  }
}

/* get nth child of parent - if parent is null, get nth root node */
static gboolean replay_message_tree_iter_nth_child(GtkTreeModel *tree_model,
                                                GtkTreeIter *iter,
                                                GtkTreeIter *parent,
                                                gint n)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;
  gint i;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), FALSE);
  self = REPLAY_MESSAGE_TREE(tree_model);
  priv = self->priv;

  if (parent)
  {
    g_return_val_if_fail(parent->stamp == priv->stamp, FALSE);
    entry = ((ReplayMessageTreeEntry *)(parent->user_data))->first_child;
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
static gboolean replay_message_tree_iter_parent(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             GtkTreeIter *child)
{
  ReplayMessageTree *self;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(tree_model), FALSE);
  g_return_val_if_fail(child != NULL, FALSE);

  self = REPLAY_MESSAGE_TREE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(child->stamp == priv->stamp, FALSE);
  g_return_val_if_fail(child->user_data != NULL, FALSE);

  entry = (ReplayMessageTreeEntry *)(child->user_data);

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

/* forward row changed signals */
static void replay_event_store_row_changed(ReplayEventStore *event_store,
                                       GtkTreePath *path,
                                       GtkTreeIter *iter,
                                       ReplayMessageTree *self)
{
  GtkTreePath *list_path;
  GtkTreePath *tree_path;

  list_path = gtk_tree_model_get_path(GTK_TREE_MODEL(event_store),
                                      iter);
  tree_path = replay_message_tree_get_tree_path_from_list_path(self, list_path);
  if (tree_path)
  {
    GtkTreeIter tree_iter;
    replay_message_tree_get_iter(GTK_TREE_MODEL(self),
                              &tree_iter,
                              tree_path);
    /* emit signal that row changed */
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self),
                               tree_path,
                               &tree_iter);
    gtk_tree_path_free(tree_path);
  }
  gtk_tree_path_free(list_path);
}

static void replay_event_store_current_changed(ReplayEventStore *event_store,
                                           GtkTreeIter *iter,
                                           ReplayMessageTree *self)
{
  GtkTreeIter parent;
  gint i, n;
  GtkTreePath *parent_path, *list_path, *tree_path;
  gboolean emitted = FALSE;

  /* check all events that occur at the same time as the current one */
  gtk_tree_model_iter_parent(GTK_TREE_MODEL(event_store), &parent, iter);
  parent_path = gtk_tree_model_get_path(GTK_TREE_MODEL(event_store),
                                        &parent);
  n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(event_store), &parent);

  for (i = 0; i < n && !emitted; i++)
  {
    list_path = gtk_tree_path_copy(parent_path);
    gtk_tree_path_append_index(list_path, i);

    tree_path = replay_message_tree_get_tree_path_from_list_path(self, list_path);
    if (tree_path)
    {
      GtkTreeIter tree_iter;
      replay_message_tree_get_iter(GTK_TREE_MODEL(self),
                                &tree_iter,
                                tree_path);
      gtk_tree_path_free(tree_path);

      /* emit signal that current changed */
      g_signal_emit(self,
                    replay_message_tree_signals[CURRENT_CHANGED],
                    0,
                    &tree_iter);
      emitted = TRUE;
    }
    gtk_tree_path_free(list_path);
  }
  gtk_tree_path_free(parent_path);
}

ReplayMessageTree *replay_message_tree_new(ReplayEventStore *event_store)
{
  ReplayMessageTree *self = (ReplayMessageTree *)g_object_new(REPLAY_TYPE_MESSAGE_TREE, NULL);
  ReplayMessageTreePrivate *priv = self->priv;

  replay_message_tree_unset_data(self);
  priv->event_store = g_object_ref(event_store);
  g_signal_connect(priv->event_store,
                   "current-changed",
                   G_CALLBACK(replay_event_store_current_changed),
                   self);
  g_signal_connect(priv->event_store,
                   "row-changed",
                   G_CALLBACK(replay_event_store_row_changed),
                   self);

  return self;
}

static void insert_event_with_parent(ReplayMessageTree *self,
                                     ReplayMessageTreeEntry *entry,
                                     ReplayMessageTreeEntry *parent)
{
  ReplayMessageTreePrivate *priv;

  priv = self->priv;

  entry->parent = parent;

  if (parent)
  {
    if (parent->last_child)
    {
      parent->last_child->next = entry;
      entry->prev = parent->last_child;
      /* create a tree path which is next after the one before us */
      entry->tree_path = gtk_tree_path_copy(entry->prev->tree_path);
      gtk_tree_path_next(entry->tree_path);
    }
    else
    {
      g_assert(parent->first_child == NULL);
      parent->first_child = entry;
      entry->parent = parent;
      /* create a tree path which is the first child of our parent */
      entry->tree_path = gtk_tree_path_copy(entry->parent->tree_path);
      gtk_tree_path_down(entry->tree_path);
    }
    parent->last_child = entry;
    parent->num_children++;
  }
  else
  {
    if (priv->last_child)
    {
      priv->last_child->next = entry;
      entry->prev = priv->last_child;
      /* create a tree path which is next after the one before us */
      entry->tree_path = gtk_tree_path_copy(entry->prev->tree_path);
      gtk_tree_path_next(entry->tree_path);
    }
    else
    {
      g_assert(priv->first_child == NULL);
      priv->first_child = entry;
      entry->parent = parent;
      /* create a tree path which is the first child of our parent */
      entry->tree_path = gtk_tree_path_new_first();
    }
    priv->last_child = entry;
    priv->num_children++;
  }
}

gboolean replay_message_tree_set_send_event(ReplayMessageTree *self,
                                         GtkTreeIter *iter,
                                         GtkTreePath *send_list_path)
{
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(self), FALSE);

  priv = self->priv;

  g_return_val_if_fail(priv->stamp == iter->stamp, FALSE);

  entry = (ReplayMessageTreeEntry *)(iter->user_data);

  if (send_list_path &&
      (entry->send_iter_set =
       gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                               &(entry->send_list_iter),
                               send_list_path)))
  {
    gchar *path_string = gtk_tree_path_to_string(send_list_path);
    g_hash_table_insert(priv->path_table,
                        path_string,
                        entry);
  }
  return entry->send_iter_set;
}

gboolean replay_message_tree_set_recv_event(ReplayMessageTree *self,
                                         GtkTreeIter *iter,
                                         GtkTreePath *recv_list_path)
{
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(self), FALSE);

  priv = self->priv;

  g_return_val_if_fail(priv->stamp == iter->stamp, FALSE);

  entry = (ReplayMessageTreeEntry *)(iter->user_data);

  if (recv_list_path &&
      (entry->recv_iter_set =
       gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                               &(entry->recv_list_iter),
                               recv_list_path)))
  {
    gchar *path_string = gtk_tree_path_to_string(recv_list_path);
    g_hash_table_insert(priv->path_table,
                        path_string,
                        entry);
  }
  return entry->recv_iter_set;
}

void replay_message_tree_append_child(ReplayMessageTree *self,
                                   GtkTreeIter *parent,
                                   const gchar *name,
                                   GtkTreeIter *child_iter)
{
  ReplayMessageTreeEntry *parent_event = NULL;
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry;
  GtkTreeIter iter;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE(self));
  g_return_if_fail(name != NULL);

  priv = self->priv;

  /* if parent is non-null, otherwise append as a root node */
  if (parent)
  {
    g_return_if_fail(parent->stamp == priv->stamp);
    g_return_if_fail(parent->user_data != NULL);
    parent_event = (ReplayMessageTreeEntry *)parent->user_data;
  }

  entry = g_malloc0(sizeof(*entry));
  entry->name = g_strdup(name);
  entry->parent = parent_event;

  insert_event_with_parent(self, entry, parent_event);

  g_hash_table_insert(priv->name_table, entry->name, entry);

  gtk_tree_model_get_iter(GTK_TREE_MODEL(self),
                          &iter,
                          entry->tree_path);

  /* give back an iter to this newly added entry */
  if (child_iter)
  {
    child_iter->user_data = iter.user_data;
    child_iter->user_data2 = iter.user_data2;
    child_iter->user_data3 = iter.user_data3;
    child_iter->stamp = iter.stamp;
  }
  /* emit signals that we've added a row */
  gtk_tree_model_row_inserted(GTK_TREE_MODEL(self),
                              entry->tree_path,
                              &iter);
}

gboolean replay_message_tree_get_iter_for_name(ReplayMessageTree *self,
                                            GtkTreeIter *iter,
                                            const gchar *name)
{
  ReplayMessageTreeEntry *entry;
  ReplayMessageTreePrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(self), FALSE);

  priv = self->priv;

  if (name != NULL)
  {
    entry = (ReplayMessageTreeEntry *)g_hash_table_lookup(priv->name_table, name);
    if ((ret = (entry != NULL)))
    {
      if (iter != NULL)
      {
        iter->user_data = entry;
        iter->stamp = priv->stamp;
      }
    }
  }
  return ret;
}

/**
 * replay_message_tree_get_event_store:
 * @self: A #ReplayMessageTree
 *
 * Returns: (transfer none): the event store associated with this message tree
 */
ReplayEventStore *replay_message_tree_get_event_store(ReplayMessageTree *self)
{
  ReplayMessageTreePrivate *priv;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(self), NULL);

  priv = self->priv;

  return priv->event_store;
}

GtkTreePath *replay_message_tree_get_tree_path_from_list_path(ReplayMessageTree *self,
                                                           GtkTreePath *list_path)
{
  ReplayMessageTreePrivate *priv;
  ReplayMessageTreeEntry *entry = NULL;
  gchar *path_string;
  GtkTreePath *tree_path = NULL;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(self), tree_path);
  priv = self->priv;

  path_string = gtk_tree_path_to_string(list_path);
  entry = g_hash_table_lookup(priv->path_table,
                              path_string);
  g_free(path_string);
  if (entry)
  {
    g_assert(entry->tree_path);
    tree_path = gtk_tree_path_copy(entry->tree_path);
  }
  return tree_path;
}

GtkTreePath *replay_message_tree_get_send_list_path_from_tree_path(ReplayMessageTree *self,
                                                                GtkTreePath *tree_path)
{
  ReplayMessageTreePrivate *priv;
  GtkTreeIter iter;
  GtkTreePath *list_path = NULL;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(self), list_path);
  priv = self->priv;

  if (replay_message_tree_get_iter(GTK_TREE_MODEL(self), &iter, tree_path))
  {
    ReplayMessageTreeEntry *entry = (ReplayMessageTreeEntry *)iter.user_data;
    if (entry->send_iter_set)
    {
      list_path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                          &(entry->send_list_iter));
    }
  }

  return list_path;
}

GtkTreePath *replay_message_tree_get_recv_list_path_from_tree_path(ReplayMessageTree *self,
                                                                GtkTreePath *tree_path)
{
  ReplayMessageTreePrivate *priv;
  GtkTreeIter iter;
  GtkTreePath *list_path = NULL;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(self), list_path);
  priv = self->priv;

  if (replay_message_tree_get_iter(GTK_TREE_MODEL(self), &iter, tree_path))
  {
    ReplayMessageTreeEntry *entry = (ReplayMessageTreeEntry *)iter.user_data;
    if (entry->recv_iter_set)
    {
      list_path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                          &(entry->recv_list_iter));
    }
  }

  return list_path;
}

gboolean replay_message_tree_set_current(ReplayMessageTree *self,
                                      GtkTreeIter *iter)
{
  ReplayMessageTreePrivate *priv;
  GtkTreePath *path, *list_path;
  GtkTreeIter list_iter;

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);
  g_return_val_if_fail(iter->user_data != NULL, FALSE);

  path = replay_message_tree_get_path(GTK_TREE_MODEL(self), iter);
  /* set current on the replay-event-list to send event */
  list_path = replay_message_tree_get_send_list_path_from_tree_path(self,
                                                                 path);
  if (list_path == NULL)
  {
    list_path = replay_message_tree_get_recv_list_path_from_tree_path(self,
                                                                   path);
  }
  g_assert(list_path);
  gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                          &list_iter,
                          list_path);
  replay_event_store_set_current(priv->event_store,
                             &list_iter);
  gtk_tree_path_free(list_path);
  gtk_tree_path_free(path);

  return TRUE;
}

/* caller must free returned GtkTreeIter using gtk_tree_iter_free() */
GtkTreeIter *replay_message_tree_get_current(ReplayMessageTree *self)
{
  ReplayMessageTreePrivate *priv;
  GtkTreeIter *copy = NULL;

  priv = self->priv;

  if (priv->event_store)
  {
    GtkTreeIter list_iter;
    gboolean current;

    current = replay_event_store_get_current(priv->event_store, &list_iter);
    if (current)
    {
      GtkTreePath *list_path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                                       &list_iter);
      GtkTreePath *tree_path = replay_message_tree_get_tree_path_from_list_path(self,
                                                                             list_path);
      if (tree_path)
      {
        GtkTreeIter tree_iter;
        replay_message_tree_get_iter(GTK_TREE_MODEL(self),
                                  &tree_iter,
                                  tree_path);
        copy = gtk_tree_iter_copy(&tree_iter);
      }
    }
  }
  return copy;
}
