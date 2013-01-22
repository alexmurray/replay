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

/**
 * SECTION:replay-event
 * @short_description: Describes events which form the basis of visualization
 *
 * #ReplayEvent objects contain data specific to each type of event in replay
 *
 * The standard way to create and use #ReplayEvent<!-- -->s is as
 * |[
 *  ReplayEvent *event;
 *  gchar *source;
 *  struct timeval tv;
 *  gint64 timestamp;
 *
 *  // create a node idd foo, color blue
 *  gettimeofday(&tv, NULL);
 *  timestamp = (tv.tv_sec * G_USEC_PER_SEC) + tv.tv_usec;
 *  source = g_strdup_printf("%s: %d", __FILE__, __LINE__);
 *  event = replay_node_create_event_new(timestamp, source, "foo",
 *                                    "color", G_TYPE_STRING, "blue");
 *  // use event
 *
 *  g_object_unref(event);
 *  g_free(source);
 *  ]|
 *
 **/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <replay/replay-event.h>
#include <replay/replay-property-store.h>

#define g_strdup_safe(string) ((string) ? g_strdup(string) : NULL)

/* ReplayEvent definition */
enum
{
  PROP_0,
  PROP_TIMESTAMP,
  PROP_SOURCE
};

struct _ReplayEventPrivate
{
  gint64 timestamp;
  gchar *source;
};

G_DEFINE_TYPE(ReplayEvent, replay_event, G_TYPE_OBJECT);

static void replay_event_finalize(GObject *object);

static void
replay_event_get_property(GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  ReplayEvent *self;

  g_return_if_fail(REPLAY_IS_EVENT(object));

  self = REPLAY_EVENT(object);

  switch (property_id) {
    case PROP_TIMESTAMP:
      g_value_set_int64(value, self->priv->timestamp);
      break;

    case PROP_SOURCE:
      g_value_set_string(value, self->priv->source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_event_set_property(GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  ReplayEvent *self;

  g_return_if_fail(REPLAY_IS_EVENT(object));

  self = REPLAY_EVENT(object);

  switch (property_id) {
    case PROP_TIMESTAMP:
      self->priv->timestamp = g_value_get_int64(value);
      break;

    case PROP_SOURCE:
      self->priv->source = g_value_dup_string(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_event_finalize(GObject *object)
{
  ReplayEvent *self;

  g_return_if_fail(REPLAY_IS_EVENT(object));

  self = REPLAY_EVENT(object);
  g_free(self->priv->source);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_event_parent_class)->finalize(object);
}

static void
replay_event_class_init(ReplayEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = replay_event_get_property;
  object_class->set_property = replay_event_set_property;
  object_class->finalize = replay_event_finalize;

  g_object_class_install_property(object_class,
                                  PROP_TIMESTAMP,
                                  g_param_spec_int64("timestamp",
                                                     "timestamp in usecs",
                                                     "The timestamp of the event in usecs since epoch",
                                                     -G_MAXINT64,
                                                     G_MAXINT64,
                                                     0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(object_class,
                                  PROP_SOURCE,
                                  g_param_spec_string("source",
                                                      "source",
                                                      "Human readable source of the event source",
                                                      "no-source-set",
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private(klass, sizeof(ReplayEventPrivate));
}

static void
replay_event_init(ReplayEvent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_EVENT,
                                           ReplayEventPrivate);
  self->priv->source = NULL;
  self->priv->timestamp = -G_MAXINT64;
}

/**
 * replay_event_get_timestamp:
 * @self: A #ReplayEvent
 *
 * Return value: the timestamp of this #ReplayEvent in usecs since epoch
 *
 * Get the timestamp of a #ReplayEvent.
 */
gint64
replay_event_get_timestamp(ReplayEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EVENT(self), 0);
  return self->priv->timestamp;
}

/**
 * replay_event_get_source:
 * @self: A #ReplayEvent
 *
 * Return value: the source of this #ReplayEvent
 *
 * Get the source of a #ReplayEvent.
 */
const gchar *
replay_event_get_source(ReplayEvent *self)
{
  g_return_val_if_fail(REPLAY_IS_EVENT(self), NULL);
  return self->priv->source;
}


/**
 * replay_event_get_node_id:
 * @self: A #ReplayEvent
 *
 * Return value: the id of the primary node affected by this #ReplayEvent
 *
 * Get the id of the primary node which is affected by this #ReplayEvent.
 */
const gchar *
replay_event_get_node_id(ReplayEvent *self)
{
  const gchar *node_id = NULL;

  if (self)
  {
    ReplayEventClass *klass = REPLAY_EVENT_GET_CLASS(self);
    if (klass->get_node_id != NULL) {
      node_id = klass->get_node_id(self);
    }
  }
  return node_id;
}
