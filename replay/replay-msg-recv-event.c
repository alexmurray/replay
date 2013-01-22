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

#include <replay/replay-msg-recv-event.h>

/* ReplayMsgRecvEvent definition */
enum
{
  PROP_0,
  PROP_ID,
  PROP_NODE,
  PROP_EDGE,
  PROP_PARENT,
  PROP_PROPS,
  PROP_SEND_EVENT,
};

struct _ReplayMsgRecvEventPrivate
{
  gchar *id;
  gchar *node;
  gchar *edge;
  gchar *parent;
  GVariant *props;
  ReplayMsgSendEvent *send_event;
};

G_DEFINE_TYPE(ReplayMsgRecvEvent, replay_msg_recv_event, REPLAY_TYPE_EVENT);

static void replay_msg_recv_event_finalize(GObject *object);

static void
replay_msg_recv_event_get_property(GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ReplayMsgRecvEvent *self;

  g_return_if_fail(REPLAY_IS_MSG_RECV_EVENT(object));

  self = REPLAY_MSG_RECV_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string(value, self->priv->id);
      break;

    case PROP_NODE:
      g_value_set_string(value, self->priv->node);
      break;

    case PROP_EDGE:
      g_value_set_string(value, self->priv->edge);
      break;

    case PROP_PARENT:
      g_value_set_string(value, self->priv->parent);
      break;

    case PROP_PROPS:
      g_value_set_variant(value, self->priv->props);
      break;

    case PROP_SEND_EVENT:
      g_value_set_object(value, self->priv->send_event);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_msg_recv_event_set_property(GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ReplayMsgRecvEvent *self;

  g_return_if_fail(REPLAY_IS_MSG_RECV_EVENT(object));

  self = REPLAY_MSG_RECV_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      self->priv->id = g_value_dup_string(value);
      break;

    case PROP_NODE:
      self->priv->node = g_value_dup_string(value);
      break;

    case PROP_EDGE:
      self->priv->edge = g_value_dup_string(value);
      break;

    case PROP_PARENT:
      self->priv->parent = g_value_dup_string(value);
      break;

    case PROP_PROPS:
      self->priv->props = g_value_dup_variant(value);
      break;

    case PROP_SEND_EVENT:
      self->priv->send_event = g_value_dup_object(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_msg_recv_event_finalize(GObject *object)
{
  ReplayMsgRecvEvent *self;

  g_return_if_fail(REPLAY_IS_MSG_RECV_EVENT(object));

  self = REPLAY_MSG_RECV_EVENT(object);
  g_free(self->priv->id);
  g_free(self->priv->node);
  g_free(self->priv->edge);
  g_free(self->priv->parent);
  g_variant_unref(self->priv->props);
  g_clear_object(&self->priv->send_event);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_msg_recv_event_parent_class)->finalize(object);
}

static const gchar *
replay_msg_recv_event_get_node_id(ReplayEvent *event) {
  g_return_val_if_fail(REPLAY_IS_MSG_RECV_EVENT(event), NULL);
  return replay_msg_recv_event_get_node(REPLAY_MSG_RECV_EVENT(event));
}

static void
replay_msg_recv_event_class_init(ReplayMsgRecvEventClass *klass)
{
  ReplayEventClass *event_class = REPLAY_EVENT_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = replay_msg_recv_event_get_property;
  object_class->set_property = replay_msg_recv_event_set_property;
  object_class->finalize = replay_msg_recv_event_finalize;
  event_class->get_node_id = replay_msg_recv_event_get_node_id;

  g_object_class_install_property(object_class,
                                  PROP_ID,
                                  g_param_spec_string("id",
                                                      "id",
                                                      "Human readable id of the message",
                                                      "no-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_NODE,
                                  g_param_spec_string("node",
                                                      "node",
                                                      "Human readable id of the node which sent the message",
                                                      "no-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_EDGE,
                                  g_param_spec_string("edge",
                                                      "edge",
                                                      "Human readable id of the edge which sent the message",
                                                      "no-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_PARENT,
                                  g_param_spec_string("parent",
                                                      "parent",
                                                      "Human readable parent id of the message",
                                                      "no-parent-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_PROPS,
                                  g_param_spec_variant("props",
                                                       "props",
                                                       "The properties of the object",
                                                       G_VARIANT_TYPE_VARDICT,
                                                       g_variant_new("a{sv}", NULL),
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_SEND_EVENT,
                                  g_param_spec_object("msg-send",
                                                      "msg-send",
                                                      "The ReplayEventMessageSend of this recv event",
                                                      REPLAY_TYPE_MSG_SEND_EVENT,
                                                      G_PARAM_READWRITE));
  g_type_class_add_private(klass, sizeof(ReplayMsgRecvEventPrivate));
}

static void
replay_msg_recv_event_init(ReplayMsgRecvEvent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_MSG_RECV_EVENT,
                                           ReplayMsgRecvEventPrivate);
}

/**
 * replay_msg_recv_event_get_id:
 * @self: A #ReplayMsgRecvEvent
 *
 * Return value: the id of the edge created by this event
 *
 * Get the id of a edge in a #ReplayMsgRecvEvent event
 */
const gchar *
replay_msg_recv_event_get_id(ReplayMsgRecvEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_MSG_RECV_EVENT(self), NULL);
  return self->priv->id;
}

/**
 * replay_msg_recv_event_get_node:
 * @self: A #ReplayMsgRecvEvent
 *
 * Return value: the node of the edge created by this event
 *
 * Get the node of a edge in a #ReplayMsgRecvEvent event
 */
const gchar *
replay_msg_recv_event_get_node(ReplayMsgRecvEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_MSG_RECV_EVENT(self), NULL);
  return self->priv->node;
}

/**
 * replay_msg_recv_event_set_send_event:
 * @self: A #ReplayMsgRecvEvent
 * @event: The #ReplayMsgSendEvent which corresponds to this recv event
 *
 * Set the send_event of a #ReplayMsgRecvEvent event
 */
void
replay_msg_recv_event_set_send_event(ReplayMsgRecvEvent *self,
                                  ReplayMsgSendEvent *event)
{
  g_return_if_fail(REPLAY_IS_MSG_RECV_EVENT(self));
  g_clear_object(&self->priv->send_event);
  self->priv->send_event = g_object_ref(event);
}

/**
 * replay_msg_recv_event_get_send_event:
 * @self: A #ReplayMsgRecvEvent
 *
 * Return value: (transfer none): the send_event of the edge created by this event
 *
 * Get the send_event of a edge in a #ReplayMsgRecvEvent event
 */
ReplayMsgSendEvent *
replay_msg_recv_event_get_send_event(ReplayMsgRecvEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_MSG_RECV_EVENT(self), NULL);
  return self->priv->send_event;
}

/**
 * replay_msg_recv_event_new:
 * @timestamp: The timestamp when this event occurred in usecs since the epoch
 * @source: A string describing the source of this event
 * @id: A globally unique id for the message
 * @node: The node which is receiving this message
 *
 * Creates a #ReplayEvent describing the receiving of a message by a node
 *
 * Returns: (transfer full): A new #ReplayEvent
 */
ReplayEvent *replay_msg_recv_event_new(gint64 timestamp,
                                 const gchar *source,
                                 const gchar *id,
                                 const gchar *node)
{
  return g_object_new(REPLAY_TYPE_MSG_RECV_EVENT,
                      "timestamp", timestamp,
                      "source", source,
                      "id", id,
                      "node", node,
                      NULL);
}
