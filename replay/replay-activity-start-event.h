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

#ifndef __REPLAY_ACTIVITY_START_EVENT_H__
#define __REPLAY_ACTIVITY_START_EVENT_H__

#include <replay/replay-event.h>

G_BEGIN_DECLS


#define REPLAY_TYPE_ACTIVITY_START_EVENT                                   \
   (replay_activity_start_event_get_type())
#define REPLAY_ACTIVITY_START_EVENT(obj)                                   \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                REPLAY_TYPE_ACTIVITY_START_EVENT,          \
                                ReplayActivityStartEvent))
#define REPLAY_ACTIVITY_START_EVENT_CLASS(klass)                           \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             REPLAY_TYPE_ACTIVITY_START_EVENT,             \
                             ReplayActivityStartEventClass))
#define REPLAY_IS_ACTIVITY_START_EVENT(obj)                                \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                REPLAY_TYPE_ACTIVITY_START_EVENT))
#define REPLAY_IS_ACTIVITY_START_EVENT_CLASS(klass)                        \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             REPLAY_TYPE_ACTIVITY_START_EVENT))
#define REPLAY_ACTIVITY_START_EVENT_GET_CLASS(obj)                         \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               REPLAY_TYPE_ACTIVITY_START_EVENT,           \
                               ReplayActivityStartEventClass))

typedef struct _ReplayActivityStartEvent      ReplayActivityStartEvent;
typedef struct _ReplayActivityStartEventClass ReplayActivityStartEventClass;
typedef struct _ReplayActivityStartEventPrivate ReplayActivityStartEventPrivate;

struct _ReplayActivityStartEvent
{
    ReplayEvent parent;
    ReplayActivityStartEventPrivate *priv;
};

struct _ReplayActivityStartEventClass
{
    /*< private >*/
    ReplayEventClass parent_class;
};

GType replay_activity_start_event_get_type(void);
const gchar *replay_activity_start_event_get_id(ReplayActivityStartEvent *self);
const gchar *replay_activity_start_event_get_node(ReplayActivityStartEvent *self);
GVariant *replay_activity_start_event_get_props(ReplayActivityStartEvent *self);

ReplayEvent *replay_activity_start_event_new(gint64 timestamp,
                                       const gchar *source,
                                       const gchar *id,
                                       const gchar *node,
                                       GVariant *props);


G_END_DECLS

#endif /* __REPLAY_ACTIVITY_START_EVENT_H__ */
