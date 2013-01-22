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
* GtkWindow subclass designed to run the entire replay application
*/

#ifndef __REPLAY_WINDOW_H__
#define __REPLAY_WINDOW_H__

#include <gtk/gtk.h>
#include <netinet/in.h>
#include <replay/replay-application.h>
#include <replay/replay-event-store.h>
#include <replay/replay-node-tree.h>
#include <replay/replay-graph-view.h>
#include <replay/replay-message-tree-view.h>
#include <replay/replay-event-source.h>
#include <replay/replay-task.h>
#include <replay/replay-log.h>
#include <replay/replay-network-server.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_WINDOW \
  (replay_window_get_type())
#define REPLAY_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_WINDOW, ReplayWindow))
#define REPLAY_WINDOW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_WINDOW, ReplayWindowClass))
#define REPLAY_IS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_WINDOW))
#define REPLAY_IS_WINDOW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_WINDOW))
#define REPLAY_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_WINDOW, ReplayWindowClass))

typedef struct _ReplayWindow ReplayWindow;
typedef struct _ReplayWindowClass ReplayWindowClass;
typedef struct _ReplayWindowPrivate ReplayWindowPrivate;

struct _ReplayWindow {
  GtkApplicationWindow parent;

  /* private */
  ReplayWindowPrivate *priv;
};

/**
 * ReplayWindowClass:
 * @parent_class: The parent class
 * @replay_event: Signal emitted when the window has received a new event
 *
 */
struct _ReplayWindowClass {
  GtkApplicationWindowClass parent_class;

  /* signal emitted when a new event arrives */
  void (*replay_event)(ReplayWindow *window,
                    ReplayEvent *event);
};

/**
 * ReplayWindowDisplayFlags:
 * @REPLAY_WINDOW_DISPLAY_MESSAGE_TREE: Show the message tree
 * @REPLAY_WINDOW_DISPLAY_TIMELINE: Show the timeline
 * @REPLAY_WINDOW_DISPLAY_GRAPH: Show the graph
 * @REPLAY_WINDOW_DISPLAY_ALL: Show all views
 *
 * Which views to display in the window
 */
typedef enum
{
  REPLAY_WINDOW_DISPLAY_MESSAGE_TREE = 1 << 0,
  REPLAY_WINDOW_DISPLAY_TIMELINE = 1 << 1,
  REPLAY_WINDOW_DISPLAY_GRAPH = 1 << 2,
  REPLAY_WINDOW_DISPLAY_ALL = (REPLAY_WINDOW_DISPLAY_MESSAGE_TREE | REPLAY_WINDOW_DISPLAY_TIMELINE | REPLAY_WINDOW_DISPLAY_GRAPH)
} ReplayWindowDisplayFlags;

/* public functions */
GType replay_window_get_type(void);
void replay_window_load_file(ReplayWindow *replay_window,
                          GFile *file);
void replay_window_close(ReplayWindow *replay_window);
GtkWidget *replay_window_new(ReplayApplication *application);
GtkUIManager *replay_window_get_ui_manager(ReplayWindow *self);
void replay_window_set_event_source(ReplayWindow *replay_window,
                                 ReplayEventSource *source);
ReplayEventSource *replay_window_get_event_source(ReplayWindow *self);
void replay_window_set_display_flags(ReplayWindow *replay_window,
                                  ReplayWindowDisplayFlags flags);
ReplayWindowDisplayFlags replay_window_get_display_flags(ReplayWindow *self);
GtkActionGroup *replay_window_get_global_action_group(ReplayWindow *self);
GtkActionGroup *replay_window_get_doc_action_group(ReplayWindow *self);
ReplayEventStore *replay_window_get_event_store(ReplayWindow *self);
ReplayNodeTree *replay_window_get_node_tree(ReplayWindow *self);
ReplayGraphView *replay_window_get_graph_view(ReplayWindow *self);
ReplayMessageTreeView *replay_window_get_message_tree_view(ReplayWindow *self);
ReplayLog *replay_window_get_log(ReplayWindow *self);

void replay_window_register_task(ReplayWindow *self, ReplayTask *task);

G_END_DECLS

#endif /* __REPLAY_WINDOW_H__ */
