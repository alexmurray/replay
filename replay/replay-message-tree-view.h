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
 * A GTK Widget which inherits from GtkTreeView which displays the
 * data in an ReplayMessageTree in a causal view
 */

#ifndef __REPLAY_MESSAGE_TREE_VIEW_H__
#define __REPLAY_MESSAGE_TREE_VIEW_H__

#include <gtk/gtk.h>
#include <replay/replay-message-tree.h>
#include <replay/replay-node-tree.h>
#include <replay/replay-timestamp.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_MESSAGE_TREE_VIEW \
  (replay_message_tree_view_get_type())
#define REPLAY_MESSAGE_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_MESSAGE_TREE_VIEW, ReplayMessageTreeView))
#define REPLAY_MESSAGE_TREE_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_MESSAGE_TREE_VIEW, ReplayMessageTreeViewClass))
#define REPLAY_IS_MESSAGE_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_MESSAGE_TREE_VIEW))
#define REPLAY_IS_MESSAGE_TREE_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_MESSAGE_TREE_VIEW))
#define REPLAY_MESSAGE_TREE_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_MESSAGE_TREE_VIEW, ReplayMessageTreeViewClass))

typedef struct _ReplayMessageTreeViewPrivate ReplayMessageTreeViewPrivate;
typedef struct _ReplayMessageTreeView {
  GtkTreeView parent;

  /* private */
  ReplayMessageTreeViewPrivate *priv;
} ReplayMessageTreeView;

typedef struct _ReplayMessageTreeViewClass {
  /*< private >*/
  GtkTreeViewClass parent_class;
} ReplayMessageTreeViewClass;

/* public functions */
GType replay_message_tree_view_get_type(void);
GtkWidget *replay_message_tree_view_new(void);
void replay_message_tree_view_unset_data(ReplayMessageTreeView *self);
void replay_message_tree_view_set_data(ReplayMessageTreeView *self,
                                    ReplayMessageTree *message_tree,
                                    ReplayNodeTree *node_tree);
gboolean replay_message_tree_view_get_absolute_time(const ReplayMessageTreeView *self);
void replay_message_tree_view_set_absolute_time(ReplayMessageTreeView *self, gboolean absolute_time);

G_END_DECLS

#endif /* __REPLAY_MESSAGE_TREE_VIEW_H__ */
