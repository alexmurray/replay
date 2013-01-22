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

/* GtkTreeModel interface */

#ifndef __REPLAY_NODE_TREE_H__
#define __REPLAY_NODE_TREE_H__

#include <gtk/gtk.h>
#include <replay/replay-event.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_NODE_TREE                      \
  (replay_node_tree_get_type())
#define REPLAY_NODE_TREE(obj)                                              \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_NODE_TREE, ReplayNodeTree))
#define REPLAY_NODE_TREE_CLASS(obj)                                    \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_NODE_TREE, ReplayNodeTreeClass))
#define REPLAY_IS_NODE_TREE(obj)                             \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_NODE_TREE))
#define REPLAY_IS_NODE_TREE_CLASS(obj)                     \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_NODE_TREE))
#define REPLAY_NODE_TREE_GET_CLASS(obj)                                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_NODE_TREE, ReplayNodeTreeClass))

/* the data columns that we export via the tree model interface */
enum
{
  REPLAY_NODE_TREE_COL_GROUP = 0,
  REPLAY_NODE_TREE_COL_ID,
  REPLAY_NODE_TREE_COL_LABEL,
  REPLAY_NODE_TREE_COL_COLOR,
  REPLAY_NODE_TREE_COL_HIDDEN,
  REPLAY_NODE_TREE_COL_REPLAYIBLE,
  REPLAY_NODE_TREE_N_COLUMNS
};

typedef struct _ReplayNodeTreeClass ReplayNodeTreeClass;
typedef struct _ReplayNodeTree ReplayNodeTree;
typedef struct _ReplayNodeTreePrivate ReplayNodeTreePrivate;
struct _ReplayNodeTree
{
  GObject parent;

  /* private */
  ReplayNodeTreePrivate *priv;
};

struct _ReplayNodeTreeClass {
  /*< private >*/
  GObjectClass parent_class;
};


GType replay_node_tree_get_type(void);
ReplayNodeTree *replay_node_tree_new(void);
gboolean replay_node_tree_get_iter_for_id(ReplayNodeTree *self,
                                       GtkTreeIter *iter,
                                       const gchar *id);
gboolean replay_node_tree_insert_child(ReplayNodeTree *self,
                                    const gchar *node,
                                    GVariant *props,
                                    gboolean hidden,
                                    GtkTreeIter *parent,
                                    int index,
                                    GtkTreeIter *child_iter,
                                    GError **error);
GtkTreePath *replay_node_tree_get_tree_path(ReplayNodeTree *self,
                                         const gchar *node);
void replay_node_tree_push_props(ReplayNodeTree *self,
                              GtkTreeIter *iter,
                              GVariant *props);
void replay_node_tree_pop_props(ReplayNodeTree *self,
                             GtkTreeIter *iter,
                             GVariant *props);
void replay_node_tree_push_override_props(ReplayNodeTree *self,
                                       GtkTreeIter *iter,
                                       GVariant *props);
void replay_node_tree_pop_override_props(ReplayNodeTree *self,
                                      GtkTreeIter *iter,
                                      GVariant *props);
void replay_node_tree_clear_override_props(ReplayNodeTree *self,
                                           GtkTreeIter *iter);
GVariant *replay_node_tree_get_prop(ReplayNodeTree *self,
                                 GtkTreeIter *iter,
                                 const gchar *name);
GVariant *replay_node_tree_get_props(ReplayNodeTree *self,
                                  GtkTreeIter *iter);
gboolean replay_node_tree_set_hidden(ReplayNodeTree *self,
                                  GtkTreeIter *iter,
                                  gboolean hidden);
gboolean replay_node_tree_find_group(ReplayNodeTree *self,
                                  const gchar *group_id,
                                  GtkTreeIter *iter);
gboolean replay_node_tree_create_group(ReplayNodeTree *self,
                                       const gchar *group,
                                       const gchar *color,
                                    GtkTreeIter *iter,
                                    GError **error);
gboolean replay_node_tree_move_to_group(ReplayNodeTree *self,
                                     GtkTreeIter *group_iter,
                                     GtkTreeIter *iter);

gboolean replay_node_tree_remove_from_group(ReplayNodeTree *self,
                                         GtkTreeIter *iter);
gboolean replay_node_tree_remove_group(ReplayNodeTree *self,
                                    GtkTreeIter *iter);

void replay_node_tree_remove_all_groups(ReplayNodeTree *self);
void replay_node_tree_unhide_all(ReplayNodeTree *self);
void replay_node_tree_clear_all_override_props(ReplayNodeTree *self);
gboolean replay_node_tree_request_hidden(ReplayNodeTree *self,
                                      GtkTreeIter *iter,
                                      gboolean hidden_request);

G_END_DECLS

#endif /* __REPLAY_NODE_TREE_H__ */
