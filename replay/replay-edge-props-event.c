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

#include <replay/replay-edge-props-event.h>

/* ReplayEdgePropsEvent definition */
enum
{
  PROP_0,
  PROP_ID,
  PROP_PROPS,
};

struct _ReplayEdgePropsEventPrivate
{
  gchar *id;
  GVariant *props;
};

G_DEFINE_TYPE(ReplayEdgePropsEvent, replay_edge_props_event, REPLAY_TYPE_EVENT);

static void replay_edge_props_event_finalize(GObject *object);

static void
replay_edge_props_event_get_property(GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ReplayEdgePropsEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_PROPS_EVENT(object));

  self = REPLAY_EDGE_PROPS_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string(value, self->priv->id);
      break;

    case PROP_PROPS:
      g_value_set_variant(value, self->priv->props);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_edge_props_event_set_property(GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ReplayEdgePropsEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_PROPS_EVENT(object));

  self = REPLAY_EDGE_PROPS_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      self->priv->id = g_value_dup_string(value);
      break;

    case PROP_PROPS:
      self->priv->props = g_value_dup_variant(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_edge_props_event_finalize(GObject *object)
{
  ReplayEdgePropsEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_PROPS_EVENT(object));

  self = REPLAY_EDGE_PROPS_EVENT(object);
  g_free(self->priv->id);
  g_variant_unref(self->priv->props);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_edge_props_event_parent_class)->finalize(object);
}

static void
replay_edge_props_event_class_init(ReplayEdgePropsEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = replay_edge_props_event_get_property;
  object_class->set_property = replay_edge_props_event_set_property;
  object_class->finalize = replay_edge_props_event_finalize;

  g_object_class_install_property(object_class,
                                  PROP_ID,
                                  g_param_spec_string("id",
                                                      "id",
                                                      "Human readable id of the object",
                                                      "no-id-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_PROPS,
                                  g_param_spec_variant("props",
                                                       "props",
                                                       "The properties of the object",
                                                       G_VARIANT_TYPE_VARDICT,
                                                       g_variant_new("a{sv}", NULL),
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private(klass, sizeof(ReplayEdgePropsEventPrivate));
}

static void
replay_edge_props_event_init(ReplayEdgePropsEvent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_EDGE_PROPS_EVENT,
                                           ReplayEdgePropsEventPrivate);
}

/**
 * replay_edge_props_event_get_id:
 * @self: A #ReplayEdgePropsEvent
 *
 * Return value: the id of the edge propsd by this event
 *
 * Get the id of a edge in a #ReplayEdgePropsEvent event
 */
const gchar *
replay_edge_props_event_get_id(ReplayEdgePropsEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_PROPS_EVENT(self), NULL);
  return self->priv->id;
}

/**
 * replay_edge_props_event_get_props:
 * @self: A #ReplayEdgePropsEvent
 *
 * Return value: (transfer none): the props of the edge propsd by this event
 *
 * Get the props of a edge in a #ReplayEdgePropsEvent event
 */
GVariant *
replay_edge_props_event_get_props(ReplayEdgePropsEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_PROPS_EVENT(self), NULL);
  return self->priv->props;
}

/**
 * replay_edge_props_event_new:
 * @timestamp: The timestamp when this event occurred in usecs since the epoch
 * @source: A string describing the source of this event
 * @id: A globally unique id to identify the edge
 * @props: The #GVariant describing the props for this node
 *
 * Creates a #ReplayEvent describing the setting of props of a edge
 *
 * Returns: (transfer full): A new #ReplayEvent
 */
ReplayEvent *
replay_edge_props_event_new(gint64 timestamp,
                         const gchar *source,
                         const gchar *id,
                         GVariant *props)
{
  return g_object_new(REPLAY_TYPE_EDGE_PROPS_EVENT,
                      "timestamp", timestamp,
                      "source", source,
                      "id", id,
                      "props", props,
                      NULL);
}
