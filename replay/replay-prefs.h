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

#ifndef __REPLAY_PREFS_H__
#define __REPLAY_PREFS_H__

#include <glib-object.h>
#include <replay/replay-timeline-view.h>
#include <replay/replay-event-text-view.h>
#include <replay/replay-message-tree-view.h>
#include <replay/replay-graph-view.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_PREFS \
  (replay_prefs_get_type())
#define REPLAY_PREFS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_PREFS, ReplayPrefs))
#define REPLAY_PREFS_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_PREFS, ReplayPrefsClass))
#define REPLAY_IS_PREFS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_PREFS))
#define REPLAY_IS_PREFS_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_PREFS))
#define REPLAY_PREFS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_PREFS, ReplayPrefsClass))

typedef struct _ReplayPrefs ReplayPrefs;
typedef struct _ReplayPrefsClass ReplayPrefsClass;
typedef struct _ReplayPrefsPrivate ReplayPrefsPrivate;

struct _ReplayPrefs {
  GObject parent;

  /* private */
  ReplayPrefsPrivate *priv;
};

struct _ReplayPrefsClass {
  /*< private >*/
  GObjectClass parent_class;
};

/* public functions */
GType replay_prefs_get_type(void);
ReplayPrefs *replay_prefs_new(ReplayTimelineView *timeline_view,
                              ReplayEventTextView *text_view,
                              ReplayMessageTreeView *message_tree_view,
                              ReplayGraphView *graph_view);
gboolean replay_prefs_get_absolute_time(ReplayPrefs *self);
void replay_prefs_set_absolute_time(ReplayPrefs *self,
                                    gboolean absolute);
gboolean replay_prefs_get_label_nodes(ReplayPrefs *self);
void replay_prefs_set_label_nodes(ReplayPrefs *self,
                                  gboolean label_nodes);
gboolean replay_prefs_get_label_edges(ReplayPrefs *self);
void replay_prefs_set_label_edges(ReplayPrefs *self,
                                  gboolean label_edges);
gboolean replay_prefs_get_label_messages(ReplayPrefs *self);
void replay_prefs_set_label_messages(ReplayPrefs *self,
                                     gboolean label_messages);

G_END_DECLS

#endif /* __REPLAY_PREFS_H__ */
