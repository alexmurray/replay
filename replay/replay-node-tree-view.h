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
 * GTK Widget which simply displays an ReplayNodeTree - inherits from
 * GtkDrawingArea
 */

#ifndef __REPLAY_NODE_TREE_VIEW_H__
#define __REPLAY_NODE_TREE_VIEW_H__

#include <gtk/gtk.h>
#include <replay/replay-node-tree.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_NODE_TREE_VIEW \
  (replay_node_tree_view_get_type())
#define REPLAY_NODE_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_NODE_TREE_VIEW, ReplayNodeTreeView))
#define REPLAY_NODE_TREE_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_NODE_TREE_VIEW, ReplayNodeTreeViewClass))
#define REPLAY_IS_NODE_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_NODE_TREE_VIEW))
#define REPLAY_IS_NODE_TREE_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_NODE_TREE_VIEW))
#define REPLAY_NODE_TREE_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_NODE_TREE_VIEW, ReplayNodeTreeViewClass))

typedef struct _ReplayNodeTreeViewPrivate ReplayNodeTreeViewPrivate;
typedef struct _ReplayNodeTreeView {
  GtkDrawingArea parent;

  /* private */
  ReplayNodeTreeViewPrivate *priv;
} ReplayNodeTreeView;

typedef struct _ReplayNodeTreeViewClass {
  /*< private >*/
  GtkDrawingAreaClass parent_class;
} ReplayNodeTreeViewClass;

/* public functions */
GType replay_node_tree_view_get_type(void);
GtkWidget *replay_node_tree_view_new(GtkAdjustment *vadj);
void replay_node_tree_view_unset_data(ReplayNodeTreeView *replay_node_tree_view);
void replay_node_tree_view_set_data(ReplayNodeTreeView *replay_node_tree_view,
                                 GtkTreeModelFilter *f_node_tree);
gboolean replay_node_tree_view_set_y_resolution(ReplayNodeTreeView *replay_node_tree_view,
                                             gdouble y_resolution);
gdouble replay_node_tree_view_get_y_resolution(const ReplayNodeTreeView *replay_node_tree_view);
void replay_node_tree_view_draw(const ReplayNodeTreeView *replay_node_tree_view,
                             cairo_t *cr,
                             PangoLayout *layout);
gdouble replay_node_tree_view_drawable_width(const ReplayNodeTreeView *replay_node_tree_view);
gdouble replay_node_tree_view_drawable_height(const ReplayNodeTreeView *replay_node_tree_view);

G_END_DECLS

#endif /* __REPLAY_NODE_TREE_VIEW_H__ */
