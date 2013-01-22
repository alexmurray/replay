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
 * GTK Widget for displaying the contents of an InvocationTree plotted as a
 * graph - inherits from GtkDrawingArea
 */

#ifndef __REPLAY_EVENT_GRAPH_H__
#define __REPLAY_EVENT_GRAPH_H__

#include <gtk/gtk.h>
#include <replay/replay-node-tree.h>
#include <replay/replay-interval-list.h>
#include <replay/replay-event.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_EVENT_GRAPH \
  (replay_event_graph_get_type())
#define REPLAY_EVENT_GRAPH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_EVENT_GRAPH, ReplayEventGraph))
#define REPLAY_EVENT_GRAPH_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_EVENT_GRAPH, ReplayEventGraphClass))
#define REPLAY_IS_EVENT_GRAPH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_EVENT_GRAPH))
#define REPLAY_IS_EVENT_GRAPH_CLASS(obj)                 \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_EVENT_GRAPH))
#define REPLAY_EVENT_GRAPH_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_EVENT_GRAPH, ReplayEventGraphClass))

typedef struct _ReplayEventGraphPrivate ReplayEventGraphPrivate;

typedef struct _ReplayEventGraph {
  GtkDrawingArea parent;

  /* private */
  ReplayEventGraphPrivate *priv;
} ReplayEventGraph;

/**
 * ReplayEventGraphClass:
 * @parent_class: The parent class
 * @x_resolution_changed: Signal emitted when the x-resolution has changed
 * @y_resolution_changed: Signal emitted when the y-resolution has changed
 * @rendering: Signal emitted when the event graph is rendering
 *
 */
typedef struct _ReplayEventGraphClass {
  GtkDrawingAreaClass parent_class;

  /* we emit parent class signals, as well as x_resolution_changed and
   * y_resolution_changed */
  void (* x_resolution_changed) (ReplayEventGraph *replay_event_graph,
                                 gdouble x_resolution);
  void (* y_resolution_changed) (ReplayEventGraph *replay_event_graph,
                                 gdouble y_resolution);
  void (* rendering) (ReplayEventGraph *replay_event_graph,
                      gboolean rendering);
} ReplayEventGraphClass;

/* public functions */
GType replay_event_graph_get_type(void);
GtkWidget *replay_event_graph_new(GtkAdjustment *hadj, GtkAdjustment *vadj);
void replay_event_graph_unset_data(ReplayEventGraph *replay_event_graph);
void replay_event_graph_set_data(ReplayEventGraph *replay_event_graph,
                              ReplayIntervalList *interval_list,
                              GtkTreeModelFilter *f_node_tree);
gboolean replay_event_graph_set_x_resolution(ReplayEventGraph *replay_event_graph,
                                          gdouble x_resolution);
gdouble replay_event_graph_get_x_resolution(const ReplayEventGraph *replay_event_graph);
gboolean replay_event_graph_set_y_resolution(ReplayEventGraph *replay_event_graph,
                                          gdouble y_resolution);
gdouble replay_event_graph_get_y_resolution(const ReplayEventGraph *replay_event_graph);
void replay_event_graph_draw(ReplayEventGraph *replay_event_graph,
                          cairo_t *cr);
gdouble replay_event_graph_drawable_height(ReplayEventGraph *replay_event_graph);
gdouble replay_event_graph_drawable_width(ReplayEventGraph *replay_event_graph);
void replay_event_graph_set_label_messages(ReplayEventGraph *self,
                                        gboolean label_messages);

G_END_DECLS

#endif /* __REPLAY_EVENT_GRAPH_H__ */
