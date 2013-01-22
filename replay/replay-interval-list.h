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
 * A GObject which contains pairs of ReplayEvent's which together form
 * intervals - pairs occur as:
 *
 * REPLAY_MSG_SEND <-> REPLAY_MSG_RECV -> msg send
 * REPLAY_MSG_RECV <-> REPLAY_MSG_SUSPEND -> running
 * REPLAY_MSG_RECV <-> REPLAY_MSG_SEND + REPLAY_MSG_RETURN flag -> running
 * REPLAY_MSG_SEND + REPLAY_MSG_SYNC flag <->  REPLAY_MSG_RECV + REPLAY_MSG_SYNC -> waiting
 *
 * We provide an API which allows ReplayEventInterval's to be added, and which can
 * then be queried to get a GList of ReplayEventInterval's which occur in that range,
 * AND which span that range as well.
 *
 * Internally we have 2 arrays - one which orders the ReplayEventIntervals in
 * increasing order based on start time, and one which orders them in
 * increasing order based on end time. This way, to search for ALL intervals in
 * a given span (and which span the given span), we return the set of intervals
 * from the start time ordered list which have start time greater than or equal
 * to start of interval, AND the set of intervals from the end time ordered
 * list which have end time greater than or equal to end of interval. We also
 * want all intervals which span OVER the given time span (ie. which start
 * before the given start and end after the given end). This is a little
 * trickier, but not too hard - once we find the first interval which starts
 * just after the start, we get all the elements before this one from the start
 * ordered array, and sort them based upon END time - we then add to our list
 * those which have end time greater than the search interval end
 * time. Conversely we could do the same for the end time array, so we can do
 * either - so we pick the one of the two arrays which has the least remaining
 * elements to sort and then search.
 */

#ifndef __REPLAY_INTERVAL_LIST_H__
#define __REPLAY_INTERVAL_LIST_H__

#include <gtk/gtk.h>
#include <replay/replay-event-store.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_INTERVAL_LIST \
  (replay_interval_list_get_type())
#define REPLAY_INTERVAL_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_INTERVAL_LIST, ReplayIntervalList))
#define REPLAY_INTERVAL_LIST_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_INTERVAL_LIST, ReplayIntervalListClass))
#define REPLAY_IS_INTERVAL_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_INTERVAL_LIST))
#define REPLAY_IS_INTERVAL_LIST_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_INTERVAL_LIST))
#define REPLAY_INTERVAL_LIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_INTERVAL_LIST, ReplayIntervalListClass))


typedef struct _ReplayIntervalListClass ReplayIntervalListClass;
typedef struct _ReplayIntervalList ReplayIntervalList;
typedef struct _ReplayIntervalListPrivate ReplayIntervalListPrivate;
typedef struct _ReplayEventInterval ReplayEventInterval;

struct _ReplayIntervalList
{
  GObject parent;

  /* private */
  ReplayIntervalListPrivate *priv;
};

/**
 * ReplayIntervalListClass:
 * @parent_class: The parent class
 * @interval_inserted: Emitted when a new #ReplayEventInterval is inserted
 * @interval_changed: Emitted when a #ReplayEventInterval is changed
 *
 */
struct _ReplayIntervalListClass {
  GObjectClass parent_class;

  void (* interval_inserted)(ReplayIntervalList *self,
                             const ReplayEventInterval *interval);
  void (* interval_changed)(ReplayIntervalList *self,
                            const ReplayEventInterval *interval);
};

/**
 * ReplayEventIntervalType:
 * @REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS: Node created / deleted events
 * @REPLAY_EVENT_INTERVAL_TYPE_NODE_COLOR: Node set color events
 * @REPLAY_EVENT_INTERVAL_TYPE_NODE_ACTIVITY: Node start and end activity events
 * @REPLAY_EVENT_INTERVAL_TYPE_MESSAGE_PASS: Message send and receive events
 * @REPLAY_EVENT_INTERVAL_NUM_TYPES: The number of different @ReplayEventIntervalType's
 *
 * The type of an interval in the interval list
 */
typedef enum _ReplayEventIntervalType
{
  REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS = 0,
  REPLAY_EVENT_INTERVAL_TYPE_NODE_COLOR,
  REPLAY_EVENT_INTERVAL_TYPE_NODE_ACTIVITY,
  REPLAY_EVENT_INTERVAL_TYPE_MESSAGE_PASS,
  REPLAY_EVENT_INTERVAL_NUM_TYPES,
} ReplayEventIntervalType;

GType replay_interval_list_get_type(void);
ReplayIntervalList *replay_interval_list_new(ReplayEventStore *event_store);

guint replay_interval_list_num_intervals(ReplayIntervalList *self,
                                      ReplayEventIntervalType type);
void replay_interval_list_add_interval(ReplayIntervalList *self,
                                    ReplayEventInterval *interval);
GList *replay_interval_list_lookup(ReplayIntervalList *self,
                                ReplayEventIntervalType type,
                                const gint64 start_usecs,
                                const gint64 end_usecs,
                                guint *n);
ReplayEventInterval *replay_event_interval_ref(ReplayEventInterval *interval);

GType replay_event_interval_get_type(void);
/*
 * Creates a new ReplayEventInterval with refcount of 1 - caller must call
 * unref_event_interval when done
 */
ReplayEventInterval *replay_event_interval_new(ReplayEventIntervalType type,
                                         const GtkTreeIter *start,
                                         gint64 start_usecs,
                                         const GtkTreeIter *end,
                                         gint64 end_usecs);

void replay_interval_list_set_interval_end(ReplayIntervalList *self,
                                        ReplayEventInterval *interval,
                                        const GtkTreeIter *end,
                                        gint64 end_usecs);
void replay_event_interval_unref(ReplayEventInterval *interval);

ReplayEventIntervalType replay_event_interval_get_interval_type(ReplayEventInterval *interval);
void replay_event_interval_get_start(ReplayEventInterval *interval,
                                  GtkTreeIter *start, gint64 *start_usecs);
void replay_event_interval_get_end(ReplayEventInterval *interval, GtkTreeIter *end,
                                gint64 *end_usecs);
ReplayEventStore *replay_interval_list_get_event_store(ReplayIntervalList *self);

G_END_DECLS

#endif /* __REPLAY_INTERVAL_LIST_H__ */
