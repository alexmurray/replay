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

/* A time-ordered tree of all the events, which implements the GtkTreeModel
   interface */

#ifndef __REPLAY_EVENT_STORE_H__
#define __REPLAY_EVENT_STORE_H__

#include <gtk/gtk.h>
#include <replay/replay-event.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_EVENT_STORE                     \
  (replay_event_store_get_type())
#define REPLAY_EVENT_STORE(obj)                                             \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_EVENT_STORE, ReplayEventStore))
#define REPLAY_EVENT_STORE_CLASS(obj)                                     \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_EVENT_STORE, ReplayEventStoreClass))
#define REPLAY_IS_EVENT_STORE(obj)                                  \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_EVENT_STORE))
#define REPLAY_IS_EVENT_STORE_CLASS(obj)                        \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_EVENT_STORE))
#define REPLAY_EVENT_STORE_GET_CLASS(obj)                                   \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_EVENT_STORE, ReplayEventStoreClass))

/* the data columns that we export via the tree model interface */
enum
{
  REPLAY_EVENT_STORE_COL_EVENT = 0,
  REPLAY_EVENT_STORE_COL_IS_CURRENT,
  REPLAY_EVENT_STORE_N_COLUMNS
};

typedef struct _ReplayEventStoreClass ReplayEventStoreClass;
typedef struct _ReplayEventStore ReplayEventStore;
typedef struct _ReplayEventStorePrivate ReplayEventStorePrivate;

struct _ReplayEventStore
{
  GObject parent;

  ReplayEventStorePrivate *priv;
};

/**
 * ReplayEventStoreClass:
 * @parent_class: The parent class
 * @current_changed: Signal emitted when the current #GtkTreeIter has changed
 * @times_changed: Signal emitted when the time bounds of the event store have changed
 *
 */
struct _ReplayEventStoreClass {
  GObjectClass parent_class;

  /* signals */
  void (* current_changed)(ReplayEventStore *replay_event_store,
                           const GtkTreeIter * const current);
  void (* times_changed)(ReplayEventStore *replay_event_store,
                         gint64 start_timestamp,
                         gint64 end_timestamp);
};


GType replay_event_store_get_type(void);
ReplayEventStore *replay_event_store_new(void);
void replay_event_store_append_event(ReplayEventStore *replay_event_store,
                                  ReplayEvent *event,
                                  GtkTreeIter *iter);
gboolean replay_event_store_set_current(ReplayEventStore *replay_event_store,
                                     GtkTreeIter *iter);
gboolean replay_event_store_get_current(ReplayEventStore *replay_event_store,
                                     GtkTreeIter *iter);
gint64 replay_event_store_get_start_timestamp(ReplayEventStore *replay_event_store);
gint64 replay_event_store_get_end_timestamp(ReplayEventStore *replay_event_store);
ReplayEvent *replay_event_store_get_event(ReplayEventStore *self,
                                    GtkTreeIter *iter);
GList *replay_event_store_get_events(ReplayEventStore *self,
                                  GtkTreeIter *iter);

G_END_DECLS

#endif /* __REPLAY_EVENT_STORE_H__ */
