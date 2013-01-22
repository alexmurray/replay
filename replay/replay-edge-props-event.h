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

#ifndef __REPLAY_XXX_EVENT_H__
#define __REPLAY_XXX_EVENT_H__

#include <replay/replay-event.h>

G_BEGIN_DECLS


#define REPLAY_TYPE_EDGE_PROPS_EVENT                                  \
   (replay_edge_props_event_get_type())
#define REPLAY_EDGE_PROPS_EVENT(obj)                                  \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                REPLAY_TYPE_EDGE_PROPS_EVENT,         \
                                ReplayEdgePropsEvent))
#define REPLAY_EDGE_PROPS_EVENT_CLASS(klass)                          \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             REPLAY_TYPE_EDGE_PROPS_EVENT,            \
                             ReplayEdgePropsEventClass))
#define REPLAY_IS_EDGE_PROPS_EVENT(obj)                               \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                REPLAY_TYPE_EDGE_PROPS_EVENT))
#define REPLAY_IS_EDGE_PROPS_EVENT_CLASS(klass)                       \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             REPLAY_TYPE_EDGE_PROPS_EVENT))
#define REPLAY_EDGE_PROPS_EVENT_GET_CLASS(obj)                        \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               REPLAY_TYPE_EDGE_PROPS_EVENT,          \
                               ReplayEdgePropsEventClass))

typedef struct _ReplayEdgePropsEvent      ReplayEdgePropsEvent;
typedef struct _ReplayEdgePropsEventClass ReplayEdgePropsEventClass;
typedef struct _ReplayEdgePropsEventPrivate ReplayEdgePropsEventPrivate;

struct _ReplayEdgePropsEvent
{
    ReplayEvent parent;
    ReplayEdgePropsEventPrivate *priv;
};

struct _ReplayEdgePropsEventClass
{
    /*< private >*/
    ReplayEventClass parent_class;
};

GType replay_edge_props_event_get_type(void);
const gchar *replay_edge_props_event_get_id(ReplayEdgePropsEvent *self);
GVariant *replay_edge_props_event_get_props(ReplayEdgePropsEvent *self);

ReplayEvent *replay_edge_props_event_new(gint64 timestamp,
                                   const gchar *source,
                                   const gchar *id,
                                   GVariant *props);


G_END_DECLS

#endif /* __REPLAY_XXX_EVENT_H__ */
