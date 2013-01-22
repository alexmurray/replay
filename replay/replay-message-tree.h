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
 * A tree like model of all the events, which implements the GtkTreeModel
 * interface
 */

#ifndef __REPLAY_MESSAGE_TREE_H__
#define __REPLAY_MESSAGE_TREE_H__

#include <gtk/gtk.h>
#include <replay/replay-event-store.h>
#include <replay/replay-event.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_MESSAGE_TREE                         \
  (replay_message_tree_get_type())
#define REPLAY_MESSAGE_TREE(obj)                                           \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_MESSAGE_TREE, ReplayMessageTree))
#define REPLAY_MESSAGE_TREE_CLASS(obj)                                   \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_MESSAGE_TREE, ReplayMessageTreeClass))
#define REPLAY_IS_MESSAGE_TREE(obj)                              \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_MESSAGE_TREE))
#define REPLAY_IS_MESSAGE_TREE_CLASS(obj)                    \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_MESSAGE_TREE))
#define REPLAY_MESSAGE_TREE_GET_CLASS(obj)                                     \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_MESSAGE_TREE, ReplayMessageTreeClass))

/* the data columns that we export via the tree model interface */
enum
{
  REPLAY_MESSAGE_TREE_COL_MSG_SEND_EVENT = 0,
  REPLAY_MESSAGE_TREE_COL_MSG_RECV_EVENT,
  REPLAY_MESSAGE_TREE_COL_IS_CURRENT,
  REPLAY_MESSAGE_TREE_N_COLUMNS
};

typedef struct _ReplayMessageTreeClass ReplayMessageTreeClass;
typedef struct _ReplayMessageTree ReplayMessageTree;
typedef struct _ReplayMessageTreePrivate ReplayMessageTreePrivate;

struct _ReplayMessageTree
{
  GObject parent;

  ReplayMessageTreePrivate *priv;
};

/**
 * ReplayMessageTreeClass:
 * @parent_class: The parent class
 * @current_changed: Emitted when the current active message event changes
 *
 */
struct _ReplayMessageTreeClass {
  GObjectClass parent_class;

  /* signals */
  void (* current_changed)(ReplayEventStore *event_store,
                           const GtkTreeIter * const current);
};


GType replay_message_tree_get_type(void);
ReplayMessageTree *replay_message_tree_new(ReplayEventStore *event_store);
gboolean replay_message_tree_set_send_event(ReplayMessageTree *self,
                                         GtkTreeIter *iter,
                                         GtkTreePath *src_list_path);
gboolean replay_message_tree_set_recv_event(ReplayMessageTree *self,
                                         GtkTreeIter *iter,
                                         GtkTreePath *recv_list_path);
gboolean replay_message_tree_get_iter_for_name(ReplayMessageTree *self,
                                            GtkTreeIter *iter,
                                            const gchar *name);
void replay_message_tree_append_child(ReplayMessageTree *it,
                                   GtkTreeIter *parent,
                                   const char *name,
                                   GtkTreeIter *child_iter);
ReplayEventStore *replay_message_tree_get_event_store(ReplayMessageTree *self);
GtkTreePath *replay_message_tree_get_tree_path_from_list_path(ReplayMessageTree *self,
                                                           GtkTreePath *list_path);
GtkTreePath *replay_message_tree_get_send_list_path_from_tree_path(ReplayMessageTree *self,
                                                                GtkTreePath *tree_path);
GtkTreePath *replay_message_tree_get_recv_list_path_from_tree_path(ReplayMessageTree *self,
                                                                GtkTreePath *tree_path);
gboolean replay_message_tree_set_current(ReplayMessageTree *self,
                                      GtkTreeIter *iter);
GtkTreeIter *replay_message_tree_get_current(ReplayMessageTree *self);
gboolean replay_message_tree_is_current(ReplayMessageTree *self,
                                     GtkTreeIter *iter);

G_END_DECLS

#endif /* __REPLAY_MESSAGE_TREE_H__ */
