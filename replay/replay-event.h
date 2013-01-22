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

#ifndef __REPLAY_EVENT_H__
#define __REPLAY_EVENT_H__

#include <glib-object.h>
#include <replay/replay-property-store.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_EVENT                                                  \
   (replay_event_get_type())
#define REPLAY_EVENT(obj)                                                  \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                REPLAY_TYPE_EVENT,                         \
                                ReplayEvent))
#define REPLAY_EVENT_CLASS(klass)                                          \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             REPLAY_TYPE_EVENT,                            \
                             ReplayEventClass))
#define REPLAY_IS_EVENT(obj)                                               \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                REPLAY_TYPE_EVENT))
#define REPLAY_IS_EVENT_CLASS(klass)                                       \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             REPLAY_TYPE_EVENT))
#define REPLAY_EVENT_GET_CLASS(obj)                                        \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               REPLAY_TYPE_EVENT,                          \
                               ReplayEventClass))

typedef struct _ReplayEvent      ReplayEvent;
typedef struct _ReplayEventClass ReplayEventClass;
typedef struct _ReplayEventPrivate ReplayEventPrivate;

struct _ReplayEvent
{
    GObject parent;
    ReplayEventPrivate *priv;
};

/**
 * ReplayEventClass:
 * @parent_class: The parent class
 * @get_node_id: Get the id of the primary node for this event (if any)
 *
 */
struct _ReplayEventClass
{
  GObjectClass parent_class;

  /* vtable */
  const gchar *(*get_node_id)(ReplayEvent *self);
};

GType replay_event_get_type(void);
gint64 replay_event_get_timestamp(ReplayEvent *self);
const gchar *replay_event_get_source(ReplayEvent *self);
const gchar *replay_event_get_node_id(ReplayEvent *self);

G_END_DECLS

#endif /* __REPLAY_EVENT_H__ */
