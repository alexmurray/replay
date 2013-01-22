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

#ifndef __REPLAY_MSG_RECV_EVENT_H__
#define __REPLAY_MSG_RECV_EVENT_H__

#include <replay/replay-event.h>
#include <replay/replay-msg-send-event.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_MSG_RECV_EVENT                                         \
   (replay_msg_recv_event_get_type())
#define REPLAY_MSG_RECV_EVENT(obj)                                         \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                REPLAY_TYPE_MSG_RECV_EVENT,                \
                                ReplayMsgRecvEvent))
#define REPLAY_MSG_RECV_EVENT_CLASS(klass)                                 \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             REPLAY_TYPE_MSG_RECV_EVENT,                   \
                             ReplayMsgRecvEventClass))
#define REPLAY_IS_MSG_RECV_EVENT(obj)                                      \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                REPLAY_TYPE_MSG_RECV_EVENT))
#define REPLAY_IS_MSG_RECV_EVENT_CLASS(klass)                              \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             REPLAY_TYPE_MSG_RECV_EVENT))
#define REPLAY_MSG_RECV_EVENT_GET_CLASS(obj)                               \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               REPLAY_TYPE_MSG_RECV_EVENT,                 \
                               ReplayMsgRecvEventClass))

typedef struct _ReplayMsgRecvEvent      ReplayMsgRecvEvent;
typedef struct _ReplayMsgRecvEventClass ReplayMsgRecvEventClass;
typedef struct _ReplayMsgRecvEventPrivate ReplayMsgRecvEventPrivate;

struct _ReplayMsgRecvEvent
{
    ReplayEvent parent;
    ReplayMsgRecvEventPrivate *priv;
};

struct _ReplayMsgRecvEventClass
{
    /*< private >*/
    ReplayEventClass parent_class;
};

GType replay_msg_recv_event_get_type(void);
const gchar *replay_msg_recv_event_get_id(ReplayMsgRecvEvent *self);
const gchar *replay_msg_recv_event_get_node(ReplayMsgRecvEvent *self);
ReplayMsgSendEvent *replay_msg_recv_event_get_send_event(ReplayMsgRecvEvent *self);
void replay_msg_recv_event_set_send_event(ReplayMsgRecvEvent *self,
                                       ReplayMsgSendEvent *event);


ReplayEvent *replay_msg_recv_event_new(gint64 timestamp,
                                 const gchar *source,
                                 const gchar *id,
                                 const gchar *node);


G_END_DECLS

#endif /* __REPLAY_MSG_RECV_EVENT_H__ */
