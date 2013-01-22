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
 * GTK Widget for displaying our events
 */

#ifndef __REPLAY_TIMELINE_VIEW_H__
#define __REPLAY_TIMELINE_VIEW_H__

#include <gtk/gtk.h>
#include <replay/replay-event.h>
#include <replay/replay-timeline.h>
#include <replay/replay-event-store.h>
#include <replay/replay-node-tree.h>
#include <replay/replay-interval-list.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_TIMELINE_VIEW \
  (replay_timeline_view_get_type())
#define REPLAY_TIMELINE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_TIMELINE_VIEW, ReplayTimelineView))
#define REPLAY_TIMELINE_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), EVENT_VIEW, ReplayTimelineViewClass))
#define REPLAY_IS_TIMELINE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_TIMELINE_VIEW))
#define REPLAY_IS_TIMELINE_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_TIMELINE_VIEW))
#define REPLAY_TIMELINE_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_TIMELINE_VIEW, ReplayTimelineViewClass))

typedef struct _ReplayTimelineViewPrivate ReplayTimelineViewPrivate;

typedef struct _ReplayTimelineView {
  GtkGrid parent;

  /* private */
  ReplayTimelineViewPrivate *priv;
} ReplayTimelineView;

/**
 * ReplayTimelineViewClass:
 * @parent_class: The parent class
 * @rendering: Signal emitted whilst timeline view is rendering
 *
 */
typedef struct _ReplayTimelineViewClass {
  GtkGridClass parent_class;

  void (* rendering) (ReplayTimelineView *timeline_view,
                      gboolean rendering);
} ReplayTimelineViewClass;

GType replay_timeline_view_get_type(void);
GtkWidget *replay_timeline_view_new(void);
void replay_timeline_view_unset_data(ReplayTimelineView *self);
void replay_timeline_view_set_data(ReplayTimelineView *self,
                                ReplayNodeTree *nodes,
                                ReplayIntervalList *intervals);
void replay_timeline_view_set_x_resolution(ReplayTimelineView *self,
                                        gdouble x_resolution);
void replay_timeline_view_set_label_messages(ReplayTimelineView *self,
                                          gboolean label_messages);
gdouble replay_timeline_view_get_x_resolution(const ReplayTimelineView *self);
void replay_timeline_view_set_absolute_time(ReplayTimelineView *self, gboolean absolute_time);
void replay_timeline_view_begin_print(ReplayTimelineView *self,
                                   GtkPrintOperation *operation,
                                   GtkPrintContext *context);

void replay_timeline_view_draw_page(ReplayTimelineView *self,
                                 GtkPrintOperation *operation,
                                 GtkPrintContext *context,
                                 int page_nr);

void replay_timeline_view_end_print(ReplayTimelineView *self,
                                 GtkPrintOperation *operation,
                                 GtkPrintContext *context);
void replay_timeline_view_export_to_file(ReplayTimelineView *self,
                                      const char *filename);


void replay_timeline_view_redraw_marks(ReplayTimelineView *self);
void replay_timeline_view_set_hpaned_position(ReplayTimelineView *self,
                                           gint position);
gint replay_timeline_view_get_hpaned_position(ReplayTimelineView *self);
void replay_timeline_view_set_vpaned_position(ReplayTimelineView *self,
                                           gint position);
gint replay_timeline_view_get_vpaned_position(ReplayTimelineView *self);

G_END_DECLS

#endif /* __REPLAY_TIMELINE_VIEW_H__ */
