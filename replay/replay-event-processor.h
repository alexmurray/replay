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

#ifndef __REPLAY_EVENT_PROCESSOR_H__
#define __REPLAY_EVENT_PROCESSOR_H__

#include <gtk/gtk.h>
#include <replay/replay-event-source.h>
#include <replay/replay-event-store.h>
#include <replay/replay-node-tree.h>
#include <replay/replay-interval-list.h>
#include <replay/replay-message-tree.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_EVENT_PROCESSOR                     \
  (replay_event_processor_get_type())
#define REPLAY_EVENT_PROCESSOR(obj)                                             \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_EVENT_PROCESSOR, ReplayEventProcessor))
#define REPLAY_EVENT_PROCESSOR_CLASS(obj)                                     \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_EVENT_PROCESSOR, ReplayEventProcessorClass))
#define REPLAY_IS_EVENT_PROCESSOR(obj)                                  \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_EVENT_PROCESSOR))
#define REPLAY_IS_EVENT_PROCESSOR_CLASS(obj)                        \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_EVENT_PROCESSOR))
#define REPLAY_EVENT_PROCESSOR_GET_CLASS(obj)                                   \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_EVENT_PROCESSOR, ReplayEventProcessorClass))

typedef struct _ReplayEventProcessorClass ReplayEventProcessorClass;
typedef struct _ReplayEventProcessor ReplayEventProcessor;
typedef struct _ReplayEventProcessorPrivate ReplayEventProcessorPrivate;

struct _ReplayEventProcessor
{
  ReplayEventSource parent;

  /* private */
  ReplayEventProcessorPrivate *priv;
};

struct _ReplayEventProcessorClass {
  /*< private >*/
  ReplayEventSourceClass parent_class;
};


GType replay_event_processor_get_type(void);
ReplayEventProcessor *replay_event_processor_new(ReplayEventStore *event_store,
                                           ReplayNodeTree *node_tree,
                                           ReplayIntervalList *interval_list,
                                           ReplayMessageTree *message_tree);
gboolean replay_event_processor_process_event(ReplayEventProcessor *self,
                                           ReplayEvent *event,
                                           GError **error);

G_END_DECLS

#endif /* __REPLAY_EVENT_PROCESSOR_H__ */
