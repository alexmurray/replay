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

#include <replay/replay-node-delete-event.h>

/* ReplayNodeDeleteEvent definition */
enum
{
  PROP_0,
  PROP_ID,
};

struct _ReplayNodeDeleteEventPrivate
{
  gchar *id;
};

G_DEFINE_TYPE(ReplayNodeDeleteEvent, replay_node_delete_event, REPLAY_TYPE_EVENT);

static void replay_node_delete_event_finalize(GObject *object);

static void
replay_node_delete_event_get_property(GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ReplayNodeDeleteEvent *self;

  g_return_if_fail(REPLAY_IS_NODE_DELETE_EVENT(object));

  self = REPLAY_NODE_DELETE_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      g_value_set_string(value, self->priv->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_node_delete_event_set_property(GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ReplayNodeDeleteEvent *self;

  g_return_if_fail(REPLAY_IS_NODE_DELETE_EVENT(object));

  self = REPLAY_NODE_DELETE_EVENT(object);

  switch (property_id) {
    case PROP_ID:
      self->priv->id = g_value_dup_string(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_node_delete_event_finalize(GObject *object)
{
  ReplayNodeDeleteEvent *self;

  g_return_if_fail(REPLAY_IS_NODE_DELETE_EVENT(object));

  self = REPLAY_NODE_DELETE_EVENT(object);
  g_free(self->priv->id);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_node_delete_event_parent_class)->finalize(object);
}

static const gchar *
replay_node_delete_event_get_node_id(ReplayEvent *event) {
  g_return_val_if_fail(REPLAY_IS_NODE_DELETE_EVENT(event), NULL);
  return replay_node_delete_event_get_id(REPLAY_NODE_DELETE_EVENT(event));
}

static void
replay_node_delete_event_class_init(ReplayNodeDeleteEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  ReplayEventClass *event_class = REPLAY_EVENT_CLASS(klass);

  object_class->get_property = replay_node_delete_event_get_property;
  object_class->set_property = replay_node_delete_event_set_property;
  object_class->finalize = replay_node_delete_event_finalize;
  event_class->get_node_id = replay_node_delete_event_get_node_id;

  g_object_class_install_property(object_class,
                                  PROP_ID,
                                  g_param_spec_string("id",
                                                      "id",
                                                      "Human readable id of the object",
                                                      "no-id-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private(klass, sizeof(ReplayNodeDeleteEventPrivate));
}

static void
replay_node_delete_event_init(ReplayNodeDeleteEvent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_NODE_DELETE_EVENT,
                                           ReplayNodeDeleteEventPrivate);
}

/**
 * replay_node_delete_event_get_id:
 * @self: A #ReplayNodeDeleteEvent
 *
 * Return value: the id of the node deleted by this event
 *
 * Get the id of a node in a #ReplayNodeDeleteEvent event
 */
const gchar *
replay_node_delete_event_get_id(ReplayNodeDeleteEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_NODE_DELETE_EVENT(self), NULL);
  return self->priv->id;
}

/**
 * replay_node_delete_event_new:
 * @timestamp: The timestamp when this event occurred in usecs since the epoch
 * @source: A string describing the source of this event
 * @id: A globally unique id to identify the node
 *
 * Creates a #ReplayEvent describing the deletion of a node
 *
 * Returns: (transfer full): A new #ReplayEvent
 */
ReplayEvent *
replay_node_delete_event_new(gint64 timestamp,
                          const gchar *source,
                          const gchar *id)
{
  return g_object_new(REPLAY_TYPE_NODE_DELETE_EVENT,
                      "timestamp", timestamp,
                      "source", source,
                      "id", id,
                      NULL);
}
