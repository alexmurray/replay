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

#ifndef __REPLAY_EDGE_CREATE_EVENT_H__
#define __REPLAY_EDGE_CREATE_EVENT_H__

#include <replay/replay-event.h>

G_BEGIN_DECLS


#define REPLAY_TYPE_EDGE_CREATE_EVENT                                      \
   (replay_edge_create_event_get_type())
#define REPLAY_EDGE_CREATE_EVENT(obj)                                      \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                REPLAY_TYPE_EDGE_CREATE_EVENT,             \
                                ReplayEdgeCreateEvent))
#define REPLAY_EDGE_CREATE_EVENT_CLASS(klass)                              \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             REPLAY_TYPE_EDGE_CREATE_EVENT,                \
                             ReplayEdgeCreateEventClass))
#define REPLAY_IS_EDGE_CREATE_EVENT(obj)                                   \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                REPLAY_TYPE_EDGE_CREATE_EVENT))
#define REPLAY_IS_EDGE_CREATE_EVENT_CLASS(klass)                           \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             REPLAY_TYPE_EDGE_CREATE_EVENT))
#define REPLAY_EDGE_CREATE_EVENT_GET_CLASS(obj)                            \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               REPLAY_TYPE_EDGE_CREATE_EVENT,              \
                               ReplayEdgeCreateEventClass))

typedef struct _ReplayEdgeCreateEvent      ReplayEdgeCreateEvent;
typedef struct _ReplayEdgeCreateEventClass ReplayEdgeCreateEventClass;
typedef struct _ReplayEdgeCreateEventPrivate ReplayEdgeCreateEventPrivate;

struct _ReplayEdgeCreateEvent
{
    ReplayEvent parent;
    ReplayEdgeCreateEventPrivate *priv;
};

struct _ReplayEdgeCreateEventClass
{
    /*< private >*/
    ReplayEventClass parent_class;
};

GType replay_edge_create_event_get_type(void);
gboolean replay_edge_create_event_get_directed(ReplayEdgeCreateEvent *self);
const gchar *replay_edge_create_event_get_id(ReplayEdgeCreateEvent *self);
const gchar *replay_edge_create_event_get_tail(ReplayEdgeCreateEvent *self);
const gchar *replay_edge_create_event_get_head(ReplayEdgeCreateEvent *self);
GVariant *replay_edge_create_event_get_props(ReplayEdgeCreateEvent *self);

ReplayEvent *replay_edge_create_event_new(gint64 timestamp,
                                    const gchar *source,
                                    const gchar *id,
                                    gboolean directed,
                                    const gchar *tail,
                                    const gchar *head,
                                    GVariant *props);

G_END_DECLS

#endif /* __REPLAY_EDGE_CREATE_EVENT_H__ */
