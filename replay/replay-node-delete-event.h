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

#ifndef __REPLAY_NODE_DELETE_EVENT_H__
#define __REPLAY_NODE_DELETE_EVENT_H__

#include <replay/replay-event.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_NODE_DELETE_EVENT                                      \
   (replay_node_delete_event_get_type())
#define REPLAY_NODE_DELETE_EVENT(obj)                                      \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                REPLAY_TYPE_NODE_DELETE_EVENT,             \
                                ReplayNodeDeleteEvent))
#define REPLAY_NODE_DELETE_EVENT_CLASS(klass)                              \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             REPLAY_TYPE_NODE_DELETE_EVENT,                \
                             ReplayNodeDeleteEventClass))
#define REPLAY_IS_NODE_DELETE_EVENT(obj)                                   \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                REPLAY_TYPE_NODE_DELETE_EVENT))
#define REPLAY_IS_NODE_DELETE_EVENT_CLASS(klass)                           \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             REPLAY_TYPE_NODE_DELETE_EVENT))
#define REPLAY_NODE_DELETE_EVENT_GET_CLASS(obj)                            \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               REPLAY_TYPE_NODE_DELETE_EVENT,              \
                               ReplayNodeDeleteEventClass))

typedef struct _ReplayNodeDeleteEvent      ReplayNodeDeleteEvent;
typedef struct _ReplayNodeDeleteEventClass ReplayNodeDeleteEventClass;
typedef struct _ReplayNodeDeleteEventPrivate ReplayNodeDeleteEventPrivate;

struct _ReplayNodeDeleteEvent
{
    ReplayEvent parent;
    ReplayNodeDeleteEventPrivate *priv;
};

struct _ReplayNodeDeleteEventClass
{
    /*< private >*/
    ReplayEventClass parent_class;
};

GType replay_node_delete_event_get_type(void);
const gchar *replay_node_delete_event_get_id (ReplayNodeDeleteEvent *self);
ReplayEvent *replay_node_delete_event_new(gint64 timestamp,
                                    const gchar *source,
                                    const gchar *id);



G_END_DECLS

#endif /* __REPLAY_NODE_DELETE_EVENT_H__ */
