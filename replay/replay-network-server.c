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

/* replay-network-server.c */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-network-server.h>

/* subclass ReplayEventSource */
G_DEFINE_TYPE(ReplayNetworkServer, replay_network_server, REPLAY_TYPE_EVENT_SOURCE);

struct _ReplayNetworkServerPrivate
{
  guint16 port;
};

/* signals */
enum
{
  DISCONNECTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

/* properties */
enum
{
  PROP_INVALID,
  PROP_PORT,
};

static void
replay_network_server_get_property(GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ReplayNetworkServerPrivate *priv;

  g_return_if_fail(REPLAY_IS_NETWORK_SERVER(object));

  priv = REPLAY_NETWORK_SERVER(object)->priv;

  switch (property_id) {
    case PROP_PORT:
      g_value_set_uint(value, priv->port);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_network_server_set_property(GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ReplayNetworkServerPrivate *priv;

  g_return_if_fail(REPLAY_IS_NETWORK_SERVER(object));

  priv = REPLAY_NETWORK_SERVER(object)->priv;

  switch (property_id)
  {
    case PROP_PORT:
      priv->port = g_value_get_uint(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
replay_network_server_finalize(GObject *object)
{
  g_return_if_fail(REPLAY_IS_NETWORK_SERVER(object));

  /* nothing to do */

  /* call parent finalize */
  G_OBJECT_CLASS(replay_network_server_parent_class)->finalize(object);
}

static void
dummy_load_events(ReplayNetworkServer *self, GSocketConnection *conn)
{
  return;
}

static void
replay_network_server_class_init(ReplayNetworkServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GParamSpec *spec;

  object_class->get_property = replay_network_server_get_property;
  object_class->set_property = replay_network_server_set_property;
  object_class->finalize     = replay_network_server_finalize;

  klass->load_events = dummy_load_events;

  /* port property */
  spec = g_param_spec_uint("port",
                           "Port this ReplayNetworkServer listens on",
                           "Get port of this network event source",
                           0, G_MAXUINT, 0,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property(object_class,
                                  PROP_PORT,
                                  spec);

  /**
   * ReplayNetworkServer::disconnected:
   * @self: A #ReplayNetworkServer
   *
   * The disconnected signal.
   */
  signals[DISCONNECTED] = g_signal_new("disconnected",
                                       REPLAY_TYPE_NETWORK_SERVER,
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET(ReplayNetworkServerClass, disconnected),
                                       NULL,
                                       NULL,
                                       g_cclosure_marshal_VOID__VOID,
                                       G_TYPE_NONE,
                                       0);

  g_type_class_add_private(klass, sizeof(ReplayNetworkServerPrivate));

}

static void
replay_network_server_init(ReplayNetworkServer *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_NETWORK_SERVER,
                                           ReplayNetworkServerPrivate);
}

/**
 * replay_network_server_load_file:
 * @self: A #ReplayNetworkServer
 * @conn: The #GSocketConnection to load events from
 *
 */
void
replay_network_server_load_events(ReplayNetworkServer *self,
                               GSocketConnection *conn)
{
  GError *error = NULL;
  GSocketAddress *address;
  GInetAddress *inet;
  guint16 port;
  gchar *inet_str;
  gchar *description;

  g_return_if_fail(REPLAY_IS_NETWORK_SERVER(self));

  REPLAY_NETWORK_SERVER_GET_CLASS(self)->load_events(self, conn);

  /* set description to the remote address and port */
  address = g_socket_connection_get_remote_address(conn, NULL);
  if (address) {
    inet = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address));
    port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address));
    inet_str = g_inet_address_to_string(inet);
    description = g_strdup_printf("Remote client %s, port %d", inet_str, port);
    replay_event_source_set_description(REPLAY_EVENT_SOURCE(self), description);
    g_free(description);
    g_free(inet_str);
    g_object_unref(address);
  } else {
    replay_event_source_set_description(REPLAY_EVENT_SOURCE(self), error->message);
    g_clear_error(&error);
  }
}

/**
 * replay_network_server_get_port:
 * @self: The #ReplayNetworkServer
 *
 * Returns: The port this network event source listens on
 */
guint16 replay_network_server_get_port(ReplayNetworkServer *self)
{
  g_return_val_if_fail(REPLAY_IS_NETWORK_SERVER(self), 0);

  return self->priv->port;
}

/**
 * replay_network_server_new:
 *
 * Creates a new instance of #ReplayNetworkServer.
 *
 * Return value: the newly created #ReplayNetworkServer instance
 */
ReplayNetworkServer *replay_network_server_new(void)
{
  ReplayNetworkServer *self;

  self = g_object_new(REPLAY_TYPE_NETWORK_SERVER,
                      "name", "Network Server",
                      NULL);
  return self;
}

/**
 * replay_network_server_emit_disconnected:
 * @self: A #ReplayNetworkServer
 *
 * Emits the "disconnected" signal for this #ReplayNetworkServer.
 *
 */
void
replay_network_server_emit_disconnected(ReplayNetworkServer *self)
{
  g_return_if_fail(REPLAY_IS_NETWORK_SERVER(self));
  g_signal_emit(self, signals[DISCONNECTED], 0);
}
