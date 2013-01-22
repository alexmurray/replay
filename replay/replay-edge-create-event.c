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

#include <replay/replay-edge-create-event.h>

/* ReplayEdgeCreateEvent definition */
enum
{
  PROP_0,
  PROP_ID,
  PROP_DIRECTED,
  PROP_TAIL,
  PROP_HEAD,
  PROP_PROPS,
};

struct _ReplayEdgeCreateEventPrivate
{
  gchar *id;
  gboolean directed;
  gchar *tail;
  gchar *head;
  GVariant *props;
};

G_DEFINE_TYPE(ReplayEdgeCreateEvent, replay_edge_create_event, REPLAY_TYPE_EVENT);

static void replay_edge_create_event_finalize(GObject *object);

static void
replay_edge_create_event_get_property(GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ReplayEdgeCreateEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(object));

  self = REPLAY_EDGE_CREATE_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string(value, self->priv->id);
      break;

    case PROP_DIRECTED:
      g_value_set_boolean(value, self->priv->directed);
      break;

    case PROP_TAIL:
      g_value_set_string(value, self->priv->tail);
      break;

    case PROP_HEAD:
      g_value_set_string(value, self->priv->head);
      break;

    case PROP_PROPS:
      g_value_set_variant(value, self->priv->props);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_edge_create_event_set_property(GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ReplayEdgeCreateEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(object));

  self = REPLAY_EDGE_CREATE_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      self->priv->id = g_value_dup_string(value);
      break;

    case PROP_DIRECTED:
      self->priv->directed = g_value_get_boolean(value);
      break;

    case PROP_TAIL:
      self->priv->tail = g_value_dup_string(value);
      break;

    case PROP_HEAD:
      self->priv->head = g_value_dup_string(value);
      break;

    case PROP_PROPS:
      self->priv->props = g_value_dup_variant(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_edge_create_event_finalize(GObject *object)
{
  ReplayEdgeCreateEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(object));

  self = REPLAY_EDGE_CREATE_EVENT(object);
  g_free(self->priv->id);
  g_variant_unref(self->priv->props);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_edge_create_event_parent_class)->finalize(object);
}

static const gchar *
replay_edge_create_event_get_node_id(ReplayEvent *event) {
  g_return_val_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(event), NULL);
  return replay_edge_create_event_get_tail(REPLAY_EDGE_CREATE_EVENT(event));
}

static void
replay_edge_create_event_class_init(ReplayEdgeCreateEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  ReplayEventClass *event_class = REPLAY_EVENT_CLASS(klass);

  object_class->get_property = replay_edge_create_event_get_property;
  object_class->set_property = replay_edge_create_event_set_property;
  object_class->finalize = replay_edge_create_event_finalize;
  event_class->get_node_id = replay_edge_create_event_get_node_id;

  g_object_class_install_property(object_class,
                                  PROP_ID,
                                  g_param_spec_string("id",
                                                      "id",
                                                      "Human readable id of the object",
                                                      "no-id-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_DIRECTED,
                                  g_param_spec_boolean("directed",
                                                      "directed",
                                                      "Human readable directed of the object",
                                                     FALSE,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_TAIL,
                                  g_param_spec_string("tail",
                                                      "tail",
                                                      "Human readable tail of the object",
                                                      "no-tail-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_HEAD,
                                  g_param_spec_string("head",
                                                      "head",
                                                      "Human readable head of the object",
                                                      "no-head-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_PROPS,
                                  g_param_spec_variant("props",
                                                       "props",
                                                       "The properties of the object",
                                                       G_VARIANT_TYPE_VARDICT,
                                                       g_variant_new("a{sv}", NULL),
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private(klass, sizeof(ReplayEdgeCreateEventPrivate));
}

static void
replay_edge_create_event_init(ReplayEdgeCreateEvent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_EDGE_CREATE_EVENT,
                                           ReplayEdgeCreateEventPrivate);
}

/**
 * replay_edge_create_event_get_id:
 * @self: A #ReplayEdgeCreateEvent
 *
 * Return value: the id of the edge created by this event
 *
 * Get the id of a edge in a #ReplayEdgeCreateEvent event
 */
const gchar *
replay_edge_create_event_get_id(ReplayEdgeCreateEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(self), NULL);
  return self->priv->id;
}

/**
 * replay_edge_create_event_get_directed:
 * @self: A #ReplayEdgeCreateEvent
 *
 * Return value: the directed of the edge created by this event
 *
 * Get the directed of a edge in a #ReplayEdgeCreateEvent event
 */
gboolean
replay_edge_create_event_get_directed(ReplayEdgeCreateEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(self), FALSE);
  return self->priv->directed;
}

/**
 * replay_edge_create_event_get_tail:
 * @self: A #ReplayEdgeCreateEvent
 *
 * Return value: the tail of the edge created by this event
 *
 * Get the tail of a edge in a #ReplayEdgeCreateEvent event
 */
const gchar *
replay_edge_create_event_get_tail(ReplayEdgeCreateEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(self), NULL);
  return self->priv->tail;
}

/**
 * replay_edge_create_event_get_head:
 * @self: A #ReplayEdgeCreateEvent
 *
 * Return value: the head of the edge created by this event
 *
 * Get the head of a edge in a #ReplayEdgeCreateEvent event
 */
const gchar *
replay_edge_create_event_get_head(ReplayEdgeCreateEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(self), NULL);
  return self->priv->head;
}

/**
 * replay_edge_create_event_get_props:
 * @self: A #ReplayEdgeCreateEvent
 *
 * Return value: (transfer none): the props of the edge created by this event
 *
 * Get the props of a edge in a #ReplayEdgeCreateEvent event
 */
GVariant *
replay_edge_create_event_get_props(ReplayEdgeCreateEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_CREATE_EVENT(self), NULL);
  return self->priv->props;
}

/**
 * replay_edge_create_event_new:
 * @timestamp: The timestamp when this event occurred in usecs since the epoch
 * @source: A string describing the source of this event
 * @id: A unique to identify the edge
 * @directed: Whether is this a directed or undirected edge
 * @tail: The unique id of the node which is the tail for this edge
 * @head: The unique id of the node which is the head for this edge
 * @props: The #GVariant describing the props for this edge
 *
 * Creates a #ReplayEvent describing the creation of an edge
 *
 * Returns: (transfer full): A new #ReplayEvent
 */
ReplayEvent *
replay_edge_create_event_new(gint64 timestamp,
                          const gchar *source,
                          const gchar *id,
                          gboolean directed,
                          const gchar *tail,
                          const gchar *head,
                          GVariant *props)
{
  return g_object_new(REPLAY_TYPE_EDGE_CREATE_EVENT,
                      "timestamp", timestamp,
                      "source", source,
                      "id", id,
                      "directed", directed,
                      "tail", tail,
                      "head", head,
                      "props", props,
                      NULL);
}
