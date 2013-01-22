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

/* replay-network-server.h */

#ifndef __REPLAY_NETWORK_SERVER_H__
#define __REPLAY_NETWORK_SERVER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <replay/replay-event.h>
#include <replay/replay-event-source.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_NETWORK_SERVER            (replay_network_server_get_type())
#define REPLAY_NETWORK_SERVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_NETWORK_SERVER, ReplayNetworkServer))
#define REPLAY_NETWORK_SERVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REPLAY_TYPE_NETWORK_SERVER, ReplayNetworkServerClass))
#define REPLAY_IS_NETWORK_SERVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_NETWORK_SERVER))
#define REPLAY_IS_NETWORK_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REPLAY_TYPE_NETWORK_SERVER))
#define REPLAY_NETWORK_SERVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REPLAY_TYPE_NETWORK_SERVER, ReplayNetworkServerClass))

typedef struct _ReplayNetworkServer        ReplayNetworkServer;
typedef struct _ReplayNetworkServerClass   ReplayNetworkServerClass;
typedef struct _ReplayNetworkServerPrivate ReplayNetworkServerPrivate;

struct _ReplayNetworkServer
{
  ReplayEventSource parent;

  /*< private >*/
  ReplayNetworkServerPrivate *priv;
};

/**
 * ReplayNetworkServerClass:
 * @parent_class: The parent class
 * @load_events: Load events from the #GSocketConnection
 * @disconnected: Signal emitted when disconnected by #replay_network_server_emit_disconnected
 *
 */
struct _ReplayNetworkServerClass
{
  ReplayEventSourceClass parent_class;

  /* public virtual methods */
  void (*load_events) (ReplayNetworkServer *self, GSocketConnection *conn);

  /* signals */
  void (*disconnected) (ReplayNetworkServer *self);
};

GType replay_network_server_get_type(void);
ReplayNetworkServer *replay_network_server_new(void);
void replay_network_server_load_events(ReplayNetworkServer *self,
                                          GSocketConnection *conn);
void replay_network_server_emit_disconnected(ReplayNetworkServer *self);
guint16 replay_network_server_get_port(ReplayNetworkServer *self);

G_END_DECLS

#endif /* __REPLAY_NETWORK_SERVER_H__ */
