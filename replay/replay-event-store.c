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
 * A time ordered tree model of all the events, which implements the
 * GtkTreeModel interface
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-event-store.h>
#include "replay-marshallers.h"

#define REPLAY_EVENT_STORE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_EVENT_STORE, ReplayEventStorePrivate))

/* signals we emit */
enum
{
  CURRENT_CHANGED = 0,
  TIMES_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* function definitions */
static void replay_event_store_class_init(ReplayEventStoreClass *klass);

static void replay_event_store_tree_model_init(GtkTreeModelIface *iface);

static void replay_event_store_init(ReplayEventStore *self);

static void replay_event_store_finalize(GObject *object);

static GtkTreeModelFlags replay_event_store_get_flags(GtkTreeModel *tree_model);

static gint replay_event_store_get_n_columns(GtkTreeModel *tree_model);

static GType replay_event_store_get_column_type(GtkTreeModel *tree_model,
                                             gint index);

static gboolean replay_event_store_get_iter(GtkTreeModel *tree_model,
                                         GtkTreeIter *iter,
                                         GtkTreePath *path);
static GtkTreePath *replay_event_store_get_path(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter);
static void replay_event_store_get_value(GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      gint column,
                                      GValue *value);
static gboolean replay_event_store_iter_next(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter);
static gboolean replay_event_store_iter_children(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter,
                                              GtkTreeIter *parent);
static gboolean replay_event_store_iter_has_child(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter);
static gint replay_event_store_iter_n_children(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter);
static gboolean replay_event_store_iter_nth_child(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent,
                                               gint n);
static gboolean replay_event_store_iter_parent(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            GtkTreeIter *child);

typedef struct _ReplayEventStoreEvent
{
  ReplayEvent *replay_event;
  GtkTreePath *tree_path;
} ReplayEventStoreEvent;

typedef struct _ReplayEventStoreStep
{
  gint num_events;
  ReplayEventStoreEvent **events;

  gint64 timestamp;
  GtkTreePath *tree_path;
} ReplayEventStoreStep;

struct _ReplayEventStorePrivate
{
  gint num_steps;
  ReplayEventStoreStep **steps;

  /* current event - we emit a signal when this changes */
  GtkTreeIter *current;

  gint stamp; /* Random integer to check if iter belongs to model */

  /* protect ourself from concurrent accesses by different threads */
  GMutex mutex;
};


/* define ReplayEventStore as inheriting from GObject and implementing the
 * GtkTreeModel interface */
G_DEFINE_TYPE_EXTENDED(ReplayEventStore,
                       replay_event_store,
                       G_TYPE_OBJECT,
                       0,
                       G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                                             replay_event_store_tree_model_init));


static void replay_event_store_class_init(ReplayEventStoreClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  /* override parent methods */
  obj_class->finalize = replay_event_store_finalize;

  /* install class signals */
  signals[CURRENT_CHANGED] = g_signal_new("current-changed",
                                          G_OBJECT_CLASS_TYPE(obj_class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET(ReplayEventStoreClass, current_changed),
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__BOXED,
                                          G_TYPE_NONE,
                                          1,
                                          GTK_TYPE_TREE_ITER);

  signals[TIMES_CHANGED] = g_signal_new("times-changed",
                                        G_OBJECT_CLASS_TYPE(obj_class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET(ReplayEventStoreClass, times_changed),
                                        NULL, NULL,
                                        replay_marshal_VOID__INT64_INT64,
                                        G_TYPE_NONE,
                                        2,
                                        G_TYPE_POINTER,
                                        G_TYPE_POINTER);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayEventStorePrivate));
}

static void replay_event_store_tree_model_init(GtkTreeModelIface *iface)
{
  iface->get_flags = replay_event_store_get_flags;
  iface->get_n_columns = replay_event_store_get_n_columns;
  iface->get_column_type = replay_event_store_get_column_type;
  iface->get_iter = replay_event_store_get_iter;
  iface->get_path = replay_event_store_get_path;
  iface->get_value = replay_event_store_get_value;
  iface->iter_next = replay_event_store_iter_next;
  iface->iter_children = replay_event_store_iter_children;
  iface->iter_has_child = replay_event_store_iter_has_child;
  iface->iter_n_children = replay_event_store_iter_n_children;
  iface->iter_nth_child = replay_event_store_iter_nth_child;
  iface->iter_parent = replay_event_store_iter_parent;
}

static void replay_event_store_init(ReplayEventStore *self)
{
  ReplayEventStorePrivate *priv;

  self->priv = priv = REPLAY_EVENT_STORE_GET_PRIVATE(self);

  priv->num_steps = 0;
  priv->steps = NULL;
  priv->stamp = g_random_int(); /* random int to check whether an iter belongs
                                   to our model */
  g_mutex_init(&priv->mutex);
}

static void replay_event_store_finalize(GObject *object)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_STORE(object));

  self = REPLAY_EVENT_STORE(object);
  priv = self->priv;

  g_mutex_lock(&priv->mutex);

  if (priv->current)
  {
    gtk_tree_iter_free(priv->current);
    priv->current = NULL;
  }

  if (priv->num_steps > 0)
  {
    /* remove all events from memory */
    int i;
    for (i = 0; i < priv->num_steps; i++)
    {
      int j;
      for (j = 0; j < priv->steps[i]->num_events; j++)
      {
        g_object_unref(priv->steps[i]->events[j]->replay_event);
        gtk_tree_path_free(priv->steps[i]->events[j]->tree_path);
        g_free(priv->steps[i]->events[j]);
      }
      g_free(priv->steps[i]->events);
      g_free(priv->steps[i]);
    }
    g_free(priv->steps);
    priv->num_steps = 0;
  }

  g_mutex_unlock(&priv->mutex);
  g_mutex_clear(&priv->mutex);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_event_store_parent_class)->finalize(object);
}

static GtkTreeModelFlags replay_event_store_get_flags(GtkTreeModel *tree_model)
{
  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), (GtkTreeModelFlags)0);

  return (GTK_TREE_MODEL_ITERS_PERSIST);
}

static gint replay_event_store_get_n_columns(GtkTreeModel *tree_model)
{
  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), 0);

  return REPLAY_EVENT_STORE_N_COLUMNS;
}

static GType column_types[REPLAY_EVENT_STORE_N_COLUMNS] = {
  G_TYPE_POINTER, /* REPLAY_EVENT_STORE_COL_REPLAY_EVENT */
  G_TYPE_BOOLEAN, /* REPLAY_EVENT_STORE_COL_CURRENT */
};

static GType replay_event_store_get_column_type(GtkTreeModel *tree_model,
                                             gint col)
{
  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), G_TYPE_INVALID);
  g_return_val_if_fail(((col < REPLAY_EVENT_STORE_N_COLUMNS) &&
                        (col >= 0)), G_TYPE_INVALID);

  return column_types[col];
}

/* Create an iter from the given path */
static gboolean replay_event_store_get_iter(GtkTreeModel *tree_model,
                                         GtkTreeIter *iter,
                                         GtkTreePath *path)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;
  ReplayEventStoreEvent *event = NULL;
  ReplayEventStoreStep *step = NULL;
  gint *indices, i, j, depth;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), ret);
  g_return_val_if_fail(path != NULL, ret);

  self = REPLAY_EVENT_STORE(tree_model);
  priv = self->priv;

  indices = gtk_tree_path_get_indices(path);
  depth = gtk_tree_path_get_depth(path);

  g_assert(depth <= 2);

  i = indices[0];
  g_mutex_lock(&priv->mutex);
  if (i >= 0 && i < priv->num_steps)
  {
    step = priv->steps[i];

    if (depth == 2)
    {
      j = indices[1];

      if (j >= 0 && j < step->num_events)
      {
        event = priv->steps[i]->events[j];
        g_assert(gtk_tree_path_compare(path, event->tree_path) == 0);
        ret = TRUE;
      }
    }
    else
    {
      ret = TRUE;
      g_assert(gtk_tree_path_compare(path, step->tree_path) == 0);
    }

    /* set stamp and only need first user_data slot */
    if (ret)
    {
      iter->stamp = priv->stamp;
      iter->user_data = step;
      iter->user_data2 = event;
      iter->user_data3 = NULL;
    }
  }
  g_mutex_unlock(&priv->mutex);
  return ret;
}

/* convert the iter into path */
static GtkTreePath *replay_event_store_get_path(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;
  GtkTreePath *path;
  ReplayEventStoreStep *step;
  ReplayEventStoreEvent *event;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), NULL);
  g_return_val_if_fail(iter != NULL, NULL);

  self = REPLAY_EVENT_STORE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, NULL);
  g_return_val_if_fail(iter->user_data != NULL, NULL);

  step = (ReplayEventStoreStep *)(iter->user_data);
  event = (ReplayEventStoreEvent *)(iter->user_data2);

  if (event)
  {
    path = gtk_tree_path_copy(event->tree_path);
  }
  else
  {
    path = gtk_tree_path_copy(step->tree_path);
  }

  return path;
}

/* return's row's exported data columns - this is what gtk_tree_model_get
   uses */
static void replay_event_store_get_value(GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      gint column,
                                      GValue *value)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;
  ReplayEventStoreEvent *event;
  gboolean current = FALSE;

  g_return_if_fail(column < REPLAY_EVENT_STORE_N_COLUMNS &&
                   column >= 0);
  g_return_if_fail(REPLAY_IS_EVENT_STORE(tree_model));
  g_return_if_fail(iter != NULL);

  self = REPLAY_EVENT_STORE(tree_model);
  priv = self->priv;

  g_return_if_fail(iter->stamp == priv->stamp);

  event = (ReplayEventStoreEvent *)(iter->user_data2);

  /* initialise the value */
  g_value_init(value, column_types[column]);

  g_mutex_lock(&priv->mutex);
  switch (column)
  {
    case REPLAY_EVENT_STORE_COL_EVENT:
      if (event)
      {
        g_value_set_pointer(value, g_object_ref(event->replay_event));
      }
      break;

    case REPLAY_EVENT_STORE_COL_IS_CURRENT:
      current = (priv->current &&
                 (priv->current->user_data == iter->user_data));
      g_value_set_boolean(value, current);
      break;

    default:
      g_assert_not_reached();
  }
  g_mutex_unlock(&priv->mutex);
}

/* set iter to next current node */
static gboolean replay_event_store_iter_next(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;
  ReplayEventStoreStep *step;
  ReplayEventStoreEvent *event;
  gint *indices, depth;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), ret);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_EVENT_STORE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, ret);

  step = (ReplayEventStoreStep *)(iter->user_data);
  event = (ReplayEventStoreEvent *)(iter->user_data2);

  if (event)
  {
    indices = gtk_tree_path_get_indices(event->tree_path);
    depth = gtk_tree_path_get_depth(event->tree_path);
  }
  else
  {
    indices = gtk_tree_path_get_indices(step->tree_path);
    depth = gtk_tree_path_get_depth(step->tree_path);
  }


  g_assert(depth <= 2);

  g_mutex_lock(&priv->mutex);
  if (indices[0] + 1 < priv->num_steps)
  {
    step = priv->steps[indices[0] + 1];
    if (depth == 1 || indices[1] + 1 < step->num_events)
    {
      if (depth == 2)
      {
        event = step->events[indices[1] + 1];
      }
      iter->stamp = priv->stamp;
      iter->user_data = step;
      iter->user_data2 = event;
      iter->user_data3 = NULL;
      ret = TRUE;
    }
  }
  g_mutex_unlock(&priv->mutex);
  return ret;
}

/* set iter to first child of parent - if parent is null, set to root node of
   model */
static gboolean replay_event_store_iter_children(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter,
                                              GtkTreeIter *parent)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;
  ReplayEventStoreStep *step = NULL;
  ReplayEventStoreEvent *event = NULL;
  gboolean ret = FALSE;
  g_return_val_if_fail(parent == NULL ||
                       parent->user_data != NULL, ret);

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  self = REPLAY_EVENT_STORE(tree_model);
  priv = self->priv;

  g_mutex_lock(&priv->mutex);
  if (parent)
  {
    if (parent->stamp == priv->stamp &&
        parent->user_data2 == NULL)
    {
      step = (ReplayEventStoreStep *)parent->user_data;
      g_assert(step->num_events > 0);
      event = step->events[0];
      ret = TRUE;
    }
  }
  else
  {
    if (priv->num_steps > 0)
    {
      step = priv->steps[0];
      ret = TRUE;
    }
  }
  if (ret)
  {
    iter->stamp = priv->stamp;
    iter->user_data = step;
    iter->user_data2 = event;
    iter->user_data3 = NULL;
  }
  g_mutex_unlock(&priv->mutex);
  return ret;
}

/* return TRUE if has children, otherwise FALSE - if iter has user_data2 set,
 * points to an event so has no children, otherwise if user_data2 is NULL,
 * points to a Step so must have children */
static gboolean replay_event_store_iter_has_child(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter)
{
  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(iter->stamp == REPLAY_EVENT_STORE(tree_model)->priv->stamp,
                       FALSE);

  return (iter->user_data2 != NULL);
}

/* get number of children of iter - if iter is null, get number of children of
   root */
static gint replay_event_store_iter_n_children(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;
  gint n = 0;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), n);
  g_return_val_if_fail(iter == NULL || iter->user_data != NULL, n);

  self = REPLAY_EVENT_STORE(tree_model);
  priv = self->priv;

  if (!iter)
  {
    g_mutex_lock(&priv->mutex);
    n = priv->num_steps;
    g_mutex_unlock(&priv->mutex);
  }
  else
  {
    g_assert(iter->user_data2 == NULL);
    g_mutex_lock(&priv->mutex);
    n = ((ReplayEventStoreStep *)iter->user_data)->num_events;
    g_mutex_unlock(&priv->mutex);
  }
  return n;
}

/* get nth child of parent - if parent is null, get nth root node */
static gboolean replay_event_store_iter_nth_child(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent,
                                               gint n)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;
  ReplayEventStoreStep *step;
  ReplayEventStoreEvent *event = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), ret);
  self = REPLAY_EVENT_STORE(tree_model);
  priv = self->priv;

  g_mutex_lock(&priv->mutex);
  if (parent)
  {
    if (parent->stamp == priv->stamp &&
        parent->user_data2 == NULL)
    {
      step = (ReplayEventStoreStep *)parent->user_data;
      if (n < step->num_events)
      {
        event = step->events[n];
        g_assert(gtk_tree_path_get_indices(event->tree_path)[1] == n);
        ret = TRUE;
      }
    }
  }
  else
  {
    if (n < priv->num_steps)
    {
      step = priv->steps[n];
      g_assert(gtk_tree_path_get_indices(step->tree_path)[0] == n);
      ret = TRUE;
    }
  }
  if (ret)
  {
    iter->stamp = priv->stamp;
    iter->user_data = step;
    iter->user_data2 = event;
    iter->user_data3 = NULL;
  }
  g_mutex_unlock(&priv->mutex);
  return ret;
}

/* point iter to parent of child */
static gboolean replay_event_store_iter_parent(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            GtkTreeIter *child)
{
  ReplayEventStore *self;
  ReplayEventStorePrivate *priv;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(tree_model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(child != NULL && child->user_data2 != NULL, FALSE);
  self = REPLAY_EVENT_STORE(tree_model);
  priv = self->priv;

  g_return_val_if_fail(child->stamp == priv->stamp, FALSE);

  iter->stamp = child->stamp;
  iter->user_data = child->user_data;
  iter->user_data2 = NULL;
  iter->user_data3 = NULL;

  return TRUE;
}

/**
 * replay_event_store_get_event:
 * @self: a #ReplayEventStore
 * @iter: an iterator pointing to a single event in the store
 *
 * Get the #ReplayEvent pointed to by @iter
 *
 * Returns: (transfer full): The #ReplayEvent or NULL if iter is invalid
 */
ReplayEvent *replay_event_store_get_event(ReplayEventStore *self,
                                    GtkTreeIter *iter)
{
  ReplayEvent *event = NULL;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(self), event);
  g_return_val_if_fail(iter != NULL, event);
  g_return_val_if_fail(iter->user_data != NULL, event);

  g_return_val_if_fail(iter->stamp == self->priv->stamp, event);

  /* if no specific event selected, return the first one in the step */
  if (!iter->user_data2)
  {
    event = g_object_ref(((ReplayEventStoreStep *)iter->user_data)->events[0]->replay_event);
  }
  else
  {
    event = g_object_ref(((ReplayEventStoreEvent *)(iter->user_data2))->replay_event);
  }

  return event;
}

/**
 * replay_event_store_get_events:
 * @self: a #ReplayEventStore
 * @iter: an iterator pointing to a group or a single event in the store
 *
 * Get a #GList of all events which occur at the same time as this event in the
 * store
 *
 * Returns: (element-type ReplayEvent) (transfer full): A #GList of all events
 * which occur at the time of the event pointed to by iter
 */
GList *replay_event_store_get_events(ReplayEventStore *self,
                                  GtkTreeIter *iter)
{
  ReplayEventStorePrivate *priv;
  ReplayEventStoreStep *step;
  ReplayEventStoreEvent *event;
  GList *events = NULL;
  gint i;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(self), events);
  g_return_val_if_fail(iter != NULL, events);

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, events);

  step = (ReplayEventStoreStep *)iter->user_data;
  /* for speed prepend from last to first */
  g_mutex_lock(&priv->mutex);

  for (i = step->num_events - 1; i >= 0; i--)
  {
    event = step->events[i];
    events = g_list_prepend(events, g_object_ref(event->replay_event));
  }
  g_mutex_unlock(&priv->mutex);
  return events;
}

ReplayEventStore *replay_event_store_new(void)
{
  return REPLAY_EVENT_STORE(g_object_new(REPLAY_TYPE_EVENT_STORE, NULL));
}

void replay_event_store_append_event(ReplayEventStore *self,
                                  ReplayEvent *replay_event,
                                  GtkTreeIter *iter)
{
  ReplayEventStorePrivate *priv;
  GtkTreeIter new_iter = {0};
  ReplayEventStoreStep *step;
  ReplayEventStoreEvent *event;
  gint i;

  g_return_if_fail(REPLAY_IS_EVENT_STORE(self));

  priv = self->priv;

  g_mutex_lock(&priv->mutex);

  /* events must be in time order */
  if ((priv->num_steps > 0 &&
       priv->steps[priv->num_steps - 1]->timestamp <= replay_event_get_timestamp(replay_event)) ||
      priv->num_steps == 0)
  {
    /* add to step if is same timestamp as step, otherwise create new step with this
     * as its sole event */
    if (priv->num_steps > 0 &&
        replay_event_get_timestamp(replay_event) == priv->steps[priv->num_steps - 1]->timestamp)
    {
      step = priv->steps[priv->num_steps - 1];
    }
    else
    {
      i = priv->num_steps;
      priv->steps = g_realloc(priv->steps,
                              (priv->num_steps + 1) * sizeof(ReplayEventStoreStep *));
      priv->steps[i] = (ReplayEventStoreStep *)g_malloc0(sizeof(ReplayEventStoreStep));
      step = priv->steps[i];
      step->timestamp = replay_event_get_timestamp(replay_event);
      step->tree_path = gtk_tree_path_new();
      gtk_tree_path_append_index(step->tree_path, i);
      priv->num_steps++;
    }
    i = step->num_events;
    step->events = g_realloc(step->events,
                             (step->num_events + 1) * sizeof(ReplayEventStoreEvent *));
    step->events[i] = (ReplayEventStoreEvent *)g_malloc0(sizeof(ReplayEventStoreStep));
    event = step->events[i];

    event->replay_event = g_object_ref(replay_event);
    event->tree_path = gtk_tree_path_copy(step->tree_path);
    gtk_tree_path_append_index(event->tree_path, i);
    step->num_events++;

    /* announce that this event - and possibly new step - exists - drop lock
     * when doing so */
    g_mutex_unlock(&priv->mutex);
    if (step->num_events == 1)
    {
      gtk_tree_model_get_iter(GTK_TREE_MODEL(self),
                              &new_iter,
                              step->tree_path);
      gtk_tree_model_row_inserted(GTK_TREE_MODEL(self),
                                  step->tree_path,
                                  &new_iter);
      /* this has changed times */
      g_signal_emit(self,
                    signals[TIMES_CHANGED],
                    0,
                    replay_event_store_get_start_timestamp(self),
                    replay_event_store_get_end_timestamp(self));

    }
    gtk_tree_model_get_iter(GTK_TREE_MODEL(self),
                            &new_iter,
                            event->tree_path);

    gtk_tree_model_row_inserted(GTK_TREE_MODEL(self),
                                event->tree_path,
                                &new_iter);

    g_mutex_lock(&priv->mutex);
  }
  else
  {
    g_warning("Error appending event to event store: event occurs in the past... ignoring this event");
  }
  g_mutex_unlock(&priv->mutex);
  if (iter)
  {
    *iter = new_iter;
  }
}


gboolean replay_event_store_set_current(ReplayEventStore *self,
                                     GtkTreeIter *iter)
{
  ReplayEventStorePrivate *priv;
  GtkTreePath *path;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(self), FALSE);

  priv = self->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);
  g_return_val_if_fail(iter->user_data != NULL, FALSE);

  g_mutex_lock(&priv->mutex);
  /* free current if exists */
  if (priv->current)
  {
    GtkTreeIter *_iter;
    _iter = priv->current;
    /* set current to NULL so when we emit row changed, it is not current row
     * anymore */
    priv->current = NULL;
    g_mutex_unlock(&priv->mutex);
    path = replay_event_store_get_path(GTK_TREE_MODEL(self),
                                    _iter);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(self),
                               path,
                               _iter);
    gtk_tree_path_free(path);
    gtk_tree_iter_free(_iter);
    g_mutex_lock(&priv->mutex);
  }
  priv->current = gtk_tree_iter_copy(iter);

  /* make sure points to an actual event not just a step - always set to last
   * event in step */
  if (priv->current->user_data2 == NULL)
  {
    ReplayEventStoreStep *step = (ReplayEventStoreStep *)priv->current->user_data;
    g_assert(step->num_events > 0);
    priv->current->user_data2 = step->events[step->num_events - 1];
    g_assert(priv->current->user_data2 != NULL);
  }
  /* emit signal that current changed */
  g_mutex_unlock(&priv->mutex);
  g_signal_emit(self,
                signals[CURRENT_CHANGED],
                0,
                priv->current);

  path = replay_event_store_get_path(GTK_TREE_MODEL(self),
                                  priv->current);
  gtk_tree_model_row_changed(GTK_TREE_MODEL(self),
                             path,
                             priv->current);
  gtk_tree_path_free(path);
  return TRUE;
}

gboolean replay_event_store_get_current(ReplayEventStore *self,
                                     GtkTreeIter *iter)
{
  ReplayEventStorePrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(self), FALSE);

  priv = self->priv;

  g_mutex_lock(&priv->mutex);
  if (priv->current)
  {
    *iter = *priv->current;
    ret = TRUE;
  }
  g_mutex_unlock(&priv->mutex);
  return ret;
}

double replay_event_store_get_start_ms(ReplayEventStore *self)
{
  ReplayEventStorePrivate *priv;
  double start = 0.0f;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(self), 0.0f);

  priv = self->priv;

  g_mutex_lock(&priv->mutex);
  if (priv->num_steps > 0)
  {
    start = priv->steps[0]->timestamp;
  }
  g_mutex_unlock(&priv->mutex);
  return start;
}

gint64 replay_event_store_get_start_timestamp(ReplayEventStore *self)
{
  ReplayEventStorePrivate *priv;
  gint64 start = 0;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(self), 0);

  priv = self->priv;

  g_mutex_lock(&priv->mutex);
  if (priv->num_steps > 0)
  {
    start = priv->steps[0]->timestamp;
  }
  g_mutex_unlock(&priv->mutex);
  return start;
}

double replay_event_store_get_end_ms(ReplayEventStore *self)
{
  ReplayEventStorePrivate *priv;
  double end = 0.0;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(self), 0.0f);

  priv = self->priv;

  g_mutex_lock(&priv->mutex);
  if (priv->num_steps > 0)
  {
    end = priv->steps[priv->num_steps - 1]->timestamp;
  }
  g_mutex_unlock(&priv->mutex);
  return end;
}

gint64 replay_event_store_get_end_timestamp(ReplayEventStore *self)
{
  ReplayEventStorePrivate *priv;
  gint64 end = 0;

  g_return_val_if_fail(REPLAY_IS_EVENT_STORE(self), 0);

  priv = self->priv;

  g_mutex_lock(&priv->mutex);
  if (priv->num_steps > 0)
  {
    end = priv->steps[priv->num_steps - 1]->timestamp;
  }
  g_mutex_unlock(&priv->mutex);
  return end;
}
