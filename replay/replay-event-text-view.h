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

#ifndef __REPLAY_EVENT_TEXT_VIEW_H__
#define __REPLAY_EVENT_TEXT_VIEW_H__

#include <gtk/gtk.h>
#include <replay/replay-timestamp.h>
#include <replay/replay-node-tree.h>
#include <replay/replay-event-store.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_EVENT_TEXT_VIEW \
  (replay_event_text_view_get_type())
#define REPLAY_EVENT_TEXT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_EVENT_TEXT_VIEW, ReplayEventTextView))
#define REPLAY_EVENT_TEXT_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_EVENT_TEXT_VIEW, ReplayEventTextViewClass))
#define REPLAY_IS_EVENT_TEXT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_EVENT_TEXT_VIEW))
#define REPLAY_IS_EVENT_TEXT_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_EVENT_TEXT_VIEW))
#define REPLAY_EVENT_TEXT_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_EVENT_TEXT_VIEW, ReplayEventTextViewClass))

typedef struct _ReplayEventTextViewPrivate ReplayEventTextViewPrivate;
typedef struct _ReplayEventTextView {
  GtkTextView parent;

  /* private */
  ReplayEventTextViewPrivate *priv;
} ReplayEventTextView;

typedef struct _ReplayEventTextViewClass {
  /*< private >*/
  GtkTextViewClass parent_class;
} ReplayEventTextViewClass;

GType replay_event_text_view_get_type(void);
GtkWidget *replay_event_text_view_new(void);
void replay_event_text_view_unset_data(ReplayEventTextView *event_text_view);
void replay_event_text_view_set_data(ReplayEventTextView *event_text_view,
                                  ReplayEventStore *event_store,
                                  ReplayNodeTree *node_tree);
void replay_event_text_view_set_absolute_time(ReplayEventTextView *event_text_view,
                                           gboolean absolute_time);
G_END_DECLS

#endif
