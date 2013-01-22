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

#include <replay/replay-edge-delete-event.h>

/* ReplayEdgeDeleteEvent definition */
enum
{
  PROP_0,
  PROP_ID,
  PROP_CREATE_EVENT,
};

struct _ReplayEdgeDeleteEventPrivate
{
  gchar *id;
  ReplayEdgeCreateEvent *create_event;
};

G_DEFINE_TYPE(ReplayEdgeDeleteEvent, replay_edge_delete_event, REPLAY_TYPE_EVENT);

static void replay_edge_delete_event_finalize(GObject *object);

static void
replay_edge_delete_event_get_property(GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ReplayEdgeDeleteEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_DELETE_EVENT(object));

  self = REPLAY_EDGE_DELETE_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string(value, self->priv->id);
      break;

    case PROP_CREATE_EVENT:
      g_value_set_object(value, self->priv->create_event);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_edge_delete_event_set_property(GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ReplayEdgeDeleteEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_DELETE_EVENT(object));

  self = REPLAY_EDGE_DELETE_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      self->priv->id = g_value_dup_string(value);
      break;

    case PROP_CREATE_EVENT:
      self->priv->create_event = g_value_get_object(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_edge_delete_event_finalize(GObject *object)
{
  ReplayEdgeDeleteEvent *self;

  g_return_if_fail(REPLAY_IS_EDGE_DELETE_EVENT(object));

  self = REPLAY_EDGE_DELETE_EVENT(object);
  g_free(self->priv->id);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_edge_delete_event_parent_class)->finalize(object);
}

static void
replay_edge_delete_event_class_init(ReplayEdgeDeleteEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = replay_edge_delete_event_get_property;
  object_class->set_property = replay_edge_delete_event_set_property;
  object_class->finalize = replay_edge_delete_event_finalize;

  g_object_class_install_property(object_class,
                                  PROP_ID,
                                  g_param_spec_string("id",
                                                      "id",
                                                      "Human readable id of the object",
                                                      "no-id-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_CREATE_EVENT,
                                  g_param_spec_object("create-event",
                                                      "create-event",
                                                      "The ReplayEdgeCreateEvent which created this edge",
                                                      REPLAY_TYPE_EDGE_CREATE_EVENT,
                                                      G_PARAM_READWRITE));

  g_type_class_add_private(klass, sizeof(ReplayEdgeDeleteEventPrivate));
}

static void
replay_edge_delete_event_init(ReplayEdgeDeleteEvent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_EDGE_DELETE_EVENT,
                                           ReplayEdgeDeleteEventPrivate);
}

/**
 * replay_edge_delete_event_get_id:
 * @self: A #ReplayEdgeDeleteEvent
 *
 * Return value: the id of the edge deleted by this event
 *
 * Get the id of a edge in a #ReplayEdgeDeleteEvent event
 */
const gchar *
replay_edge_delete_event_get_id(ReplayEdgeDeleteEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_DELETE_EVENT(self), NULL);
  return self->priv->id;
}

/**
 * replay_edge_delete_event_set_create_event:
 * @self: A #ReplayMsgRecvEvent
 * @event: The #ReplayEdgeCreateEvent which corresponds to this recv event
 *
 * Set the create_event of a #ReplayMsgRecvEvent event
 */
void
replay_edge_delete_event_set_create_event(ReplayEdgeDeleteEvent *self,
                                       ReplayEdgeCreateEvent *event)
{
  g_return_if_fail(REPLAY_IS_EDGE_DELETE_EVENT(self));
  g_clear_object(&self->priv->create_event);
  self->priv->create_event = g_object_ref(event);
}

/**
 * replay_edge_delete_event_get_create_event:
 * @self: A #ReplayMsgRecvEvent
 *
 * Return value: (transfer none): the create_event of the edge created by this event
 *
 * Get the create_event of a edge in a #ReplayMsgRecvEvent event
 */
ReplayEdgeCreateEvent *
replay_edge_delete_event_get_create_event(ReplayEdgeDeleteEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EDGE_DELETE_EVENT(self), NULL);
  return self->priv->create_event;
}

/**
 * replay_edge_delete_event_new:
 * @timestamp: The timestamp when this event occurred in usecs since the epoch
 * @source: A string describing the source of this event
 * @id: A globally unique id to identify the edge
 *
 * Creates a #ReplayEvent describing the deletion of a edge
 *
 * Returns: (transfer full): A new #ReplayEvent
 */
ReplayEvent *
replay_edge_delete_event_new(gint64 timestamp,
                          const gchar *source,
                          const gchar *id)
{
  return g_object_new(REPLAY_TYPE_EDGE_DELETE_EVENT,
                      "timestamp", timestamp,
                      "source", source,
                      "id", id,
                      NULL);
}
