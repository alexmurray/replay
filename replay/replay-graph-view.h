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

#ifndef __REPLAY_GRAPH_VIEW_H__
#define __REPLAY_GRAPH_VIEW_H__

#include <gtk/gtk.h>
#include <replay/replay-event-store.h>
#include <replay/replay-node-tree.h>
#include <replay/replay-timestamp.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_GRAPH_VIEW \
  (replay_graph_view_get_type())
#define REPLAY_GRAPH_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_GRAPH_VIEW, ReplayGraphView))
#define REPLAY_GRAPH_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), STATE_GRAPH, ReplayGraphViewClass))
#define REPLAY_IS_GRAPH_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_GRAPH_VIEW))
#define REPLAY_IS_GRAPH_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_GRAPH_VIEW))
#define REPLAY_GRAPH_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_GRAPH_VIEW, ReplayGraphViewClass))

typedef struct _ReplayGraphViewPrivate ReplayGraphViewPrivate;
typedef struct _ReplayGraphView {
  GtkDrawingArea parent;

  /* private */
  ReplayGraphViewPrivate *priv;
} ReplayGraphView;

/**
 * ReplayGraphViewClass:
 * @parent_class: The parent class
 * @processing: Signal emitted when processing events in the graph view
 *
 */
typedef struct _ReplayGraphViewClass {
  GtkDrawingAreaClass parent_class;

  void (* processing) (ReplayGraphView *replay_graph_view,
                      gboolean processing);

} ReplayGraphViewClass;

/* public functions */
GType replay_graph_view_get_type(void);
GtkWidget *replay_graph_view_new(void);
void replay_graph_view_unset_data(ReplayGraphView *self);
void replay_graph_view_set_data(ReplayGraphView *self,
                             ReplayEventStore *event_store,
                             ReplayNodeTree *node_tree);
void replay_graph_view_set_label_nodes(ReplayGraphView *self,
                                    gboolean label_nodes);
void replay_graph_view_set_label_edges(ReplayGraphView *self,
                                    gboolean label_edges);
void replay_graph_view_set_absolute_time(ReplayGraphView *self,
                                      gboolean absolute_time);
void replay_graph_view_set_show_hud(ReplayGraphView *self,
                                 gboolean show_hud);
void replay_graph_view_set_two_dimensional(ReplayGraphView *self,
                                        gboolean two_dimensional);
void replay_graph_view_set_selected_node_ids(ReplayGraphView *self,
                                          const gchar **node_ids);
gchar **replay_graph_view_get_selected_node_ids(ReplayGraphView *self);
void replay_graph_view_set_max_fps(ReplayGraphView *self,
                                gdouble max_fps);
gdouble replay_graph_view_get_max_fps(ReplayGraphView *self);

G_END_DECLS


#endif /* __REPLAY_GRAPH_VIEW_H__ */
