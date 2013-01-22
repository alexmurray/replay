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

#include <replay/replay-activity-start-event.h>

/* ReplayActivityStartEvent definition */
enum
{
  PROP_0,
  PROP_ID,
  PROP_NODE,
  PROP_PROPS,
};

struct _ReplayActivityStartEventPrivate
{
  gchar *id;
  gchar *node;
  GVariant *props;
};

G_DEFINE_TYPE(ReplayActivityStartEvent, replay_activity_start_event, REPLAY_TYPE_EVENT);

static void replay_activity_start_event_finalize(GObject *object);

static void
replay_activity_start_event_get_property(GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ReplayActivityStartEvent *self;

  g_return_if_fail(REPLAY_IS_ACTIVITY_START_EVENT(object));

  self = REPLAY_ACTIVITY_START_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string(value, self->priv->id);
      break;

    case PROP_NODE:
      g_value_set_string(value, self->priv->node);
      break;

    case PROP_PROPS:
      g_value_set_variant(value, self->priv->props);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_activity_start_event_set_property(GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ReplayActivityStartEvent *self;

  g_return_if_fail(REPLAY_IS_ACTIVITY_START_EVENT(object));

  self = REPLAY_ACTIVITY_START_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      self->priv->id = g_value_dup_string(value);
      break;

    case PROP_NODE:
      self->priv->node = g_value_dup_string(value);
      break;

    case PROP_PROPS:
      self->priv->props = g_value_dup_variant(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_activity_start_event_finalize(GObject *object)
{
  ReplayActivityStartEvent *self;

  g_return_if_fail(REPLAY_IS_ACTIVITY_START_EVENT(object));

  self = REPLAY_ACTIVITY_START_EVENT(object);
  g_free(self->priv->id);
  g_free(self->priv->node);
  g_variant_unref(self->priv->props);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_activity_start_event_parent_class)->finalize(object);
}

static const gchar *
replay_activity_start_event_get_node_id(ReplayEvent *event) {
  g_return_val_if_fail(REPLAY_IS_ACTIVITY_START_EVENT(event), NULL);
  return replay_activity_start_event_get_node(REPLAY_ACTIVITY_START_EVENT(event));
}

static void
replay_activity_start_event_class_init(ReplayActivityStartEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  ReplayEventClass *event_class = REPLAY_EVENT_CLASS(klass);

  object_class->get_property = replay_activity_start_event_get_property;
  object_class->set_property = replay_activity_start_event_set_property;
  object_class->finalize = replay_activity_start_event_finalize;
  event_class->get_node_id = replay_activity_start_event_get_node_id;

  g_object_class_install_property(object_class,
                                  PROP_ID,
                                  g_param_spec_string("id",
                                                      "id",
                                                      "Human readable id of the activity",
                                                      "no-id-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_NODE,
                                  g_param_spec_string("node",
                                                      "node",
                                                      "Human readable id of the node which this activity is started upon",
                                                      "no-id-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_PROPS,
                                  g_param_spec_variant("props",
                                                       "props",
                                                       "The properties of the activity",
                                                       G_VARIANT_TYPE_VARDICT,
                                                       g_variant_new("a{sv}", NULL),
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private(klass, sizeof(ReplayActivityStartEventPrivate));
}

static void
replay_activity_start_event_init(ReplayActivityStartEvent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_ACTIVITY_START_EVENT,
                                           ReplayActivityStartEventPrivate);
}

/**
 * replay_activity_start_event_get_id:
 * @self: A #ReplayActivityStartEvent
 *
 * Return value: the id of the activity created by this event
 *
 * Get the id of the activity in a #ReplayActivityStartEvent event
 */
const gchar *
replay_activity_start_event_get_id(ReplayActivityStartEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_ACTIVITY_START_EVENT(self), NULL);
  return self->priv->id;
}

/**
 * replay_activity_start_event_get_node:
 * @self: A #ReplayActivityStartEvent
 *
 * Return value: the node of the activity created by this event
 *
 * Get the node of the activity in a #ReplayActivityStartEvent event
 */
const gchar *
replay_activity_start_event_get_node(ReplayActivityStartEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_ACTIVITY_START_EVENT(self), NULL);
  return self->priv->node;
}

/**
 * replay_activity_start_event_get_props:
 * @self: A #ReplayActivityStartEvent
 *
 * Return value: (transfer none): the props of the activity created by this event
 *
 * Get the props of the activity in a #ReplayActivityStartEvent event
 */
GVariant *
replay_activity_start_event_get_props(ReplayActivityStartEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_ACTIVITY_START_EVENT(self), NULL);
  return self->priv->props;
}

/**
 * replay_activity_start_event_new:
 * @timestamp: The timestamp when this event occurred in usecs since the epoch
 * @source: A string describing the source of this event
 * @id: A unique id for this activity
 * @node: The id identifying the node which this activity is for
 * @props: The #GVariant describing the props for this activity
 *
 * Creates a #ReplayEvent describing the start of an activity on a node
 *
 * Returns: (transfer full): A new #ReplayEvent
 */
ReplayEvent *
replay_activity_start_event_new(gint64 timestamp,
                             const gchar *source,
                             const gchar *id,
                             const gchar *node,
                             GVariant *props)
{
 return g_object_new(REPLAY_TYPE_ACTIVITY_START_EVENT,
                     "timestamp", timestamp,
                     "source", source,
                     "id", id,
                     "node", node,
                     "props", props,
                     NULL);
}
