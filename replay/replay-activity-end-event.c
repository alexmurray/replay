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

#include <replay/replay-activity-end-event.h>

/* ReplayActivityEndEvent definition */
enum
{
  PROP_0,
  PROP_ID,
  PROP_NODE,
  PROP_START_EVENT,
};

struct _ReplayActivityEndEventPrivate
{
  gchar *id;
  gchar *node;
  ReplayActivityStartEvent *start_event;
};

G_DEFINE_TYPE(ReplayActivityEndEvent, replay_activity_end_event, REPLAY_TYPE_EVENT);

static void replay_activity_end_event_finalize(GObject *object);

static void
replay_activity_end_event_get_property(GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ReplayActivityEndEvent *self;

  g_return_if_fail(REPLAY_IS_ACTIVITY_END_EVENT(object));

  self = REPLAY_ACTIVITY_END_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string(value, self->priv->id);
      break;

    case PROP_NODE:
      g_value_set_string(value, self->priv->node);
      break;

    case PROP_START_EVENT:
      g_value_set_object(value, self->priv->start_event);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_activity_end_event_set_property(GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ReplayActivityEndEvent *self;

  g_return_if_fail(REPLAY_IS_ACTIVITY_END_EVENT(object));

  self = REPLAY_ACTIVITY_END_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      self->priv->id = g_value_dup_string(value);
      break;

    case PROP_NODE:
      self->priv->node = g_value_dup_string(value);
      break;

    case PROP_START_EVENT:
      self->priv->start_event = g_value_get_object(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_activity_end_event_finalize(GObject *object)
{
  ReplayActivityEndEvent *self;

  g_return_if_fail(REPLAY_IS_ACTIVITY_END_EVENT(object));

  self = REPLAY_ACTIVITY_END_EVENT(object);
  g_free(self->priv->id);
  g_free(self->priv->node);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_activity_end_event_parent_class)->finalize(object);
}

static void
replay_activity_end_event_class_init(ReplayActivityEndEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = replay_activity_end_event_get_property;
  object_class->set_property = replay_activity_end_event_set_property;
  object_class->finalize = replay_activity_end_event_finalize;

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
                                                      "Human readable id of the node which this activity is ended upon",
                                                      "no-id-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_START_EVENT,
                                  g_param_spec_object("start-event",
                                                      "start-event",
                                                      "The ReplayActivityStartEvent which started this activity",
                                                      REPLAY_TYPE_ACTIVITY_START_EVENT,
                                                      G_PARAM_READWRITE));
  g_type_class_add_private(klass, sizeof(ReplayActivityEndEventPrivate));
}

static void
replay_activity_end_event_init(ReplayActivityEndEvent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_ACTIVITY_END_EVENT,
                                           ReplayActivityEndEventPrivate);
}

/**
 * replay_activity_end_event_get_id:
 * @self: A #ReplayActivityEndEvent
 *
 * Return value: the id of the activity created by this event
 *
 * Get the id of the activity in a #ReplayActivityEndEvent event
 */
const gchar *
replay_activity_end_event_get_id(ReplayActivityEndEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_ACTIVITY_END_EVENT(self), NULL);
  return self->priv->id;
}

/**
 * replay_activity_end_event_get_node:
 * @self: A #ReplayActivityEndEvent
 *
 * Return value: the node of the activity created by this event
 *
 * Get the node of the activity in a #ReplayActivityEndEvent event
 */
const gchar *
replay_activity_end_event_get_node(ReplayActivityEndEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_ACTIVITY_END_EVENT(self), NULL);
  return self->priv->node;
}

/**
 * replay_activity_end_event_get_start_event:
 * @self: A #ReplayActivityEndEvent
 *
 * Return value: (transfer none): the #ReplayActivityStartEvent of the activity created by this event
 *
 * Get the start_event of the activity in a #ReplayActivityEndEvent event
 */
ReplayActivityStartEvent *
replay_activity_end_event_get_start_event(ReplayActivityEndEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_ACTIVITY_END_EVENT(self), NULL);
  return self->priv->start_event;
}

/**
 * replay_activity_end_event_set_start_event:
 * @self: A #ReplayActivityEndEvent
 * @event: The #ReplayActivityStartEvent which corresponds to this
 * #ReplayActivityEndEvent
 *
 * set the start_event of the activity in a #ReplayActivityEndEvent event
 */
void
replay_activity_end_event_set_start_event(ReplayActivityEndEvent *self,
                                       ReplayActivityStartEvent *event)
{
  g_return_if_fail(REPLAY_IS_ACTIVITY_END_EVENT(self));
  g_clear_object(&self->priv->start_event);
  self->priv->start_event = g_object_ref(event);
}

/**
 * replay_activity_end_event_new:
 * @timestamp: The timestamp when this event occurred in usecs since the epoch
 * @source: A string describing the source of this event
 * @id: A unique id for this activity
 * @node: The id identifying the node which this activity is for
 *
 * Creates a #ReplayEvent describing the end of an activity on a node
 *
 * Returns: (transfer full): A new #ReplayEvent
 */
ReplayEvent *
replay_activity_end_event_new(gint64 timestamp,
                           const gchar *source,
                           const gchar *id,
                           const gchar *node)
{
  return g_object_new(REPLAY_TYPE_ACTIVITY_END_EVENT,
                      "timestamp", timestamp,
                      "source", source,
                      "id", id,
                      "node", node,
                      NULL);
}
