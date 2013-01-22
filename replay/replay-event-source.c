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
 * SECTION:replay-event-source
 * @short_description: Base class for types which emit #ReplayEvent<!-- -->s
 * @include: replay/replay-event-source.h
 * @see_also: #ReplayEvent
 *
 * #ReplayEventSource is a base class for objects which act as a
 * source of #ReplayEvent<!-- -->s. #ReplayEvent<!-- -->s must be emitted in
 * time-order. A #ReplayEventSource can emit new #ReplayEvent<!-- -->s by calling
 * replay_event_source_emit_event(). If an error is encountered, a #ReplayEventSource
 * should call replay_event_source_emit_error().
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-event-source.h>

/* signals emitted by ReplayEventSource */
enum
{
  EVENT = 0,
  ERROR,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_DESCRIPTION,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

struct _ReplayEventSourcePrivate
{
  gchar *name;
  gchar *description;
};

G_DEFINE_TYPE(ReplayEventSource, replay_event_source, G_TYPE_OBJECT);

static void replay_event_source_finalize(GObject *object);

static void
replay_event_source_get_property(GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ReplayEventSource *self;

  g_return_if_fail(REPLAY_IS_EVENT_SOURCE(object));

  self = REPLAY_EVENT_SOURCE(object);

  switch (property_id) {
    case PROP_NAME:
      g_value_set_string(value, self->priv->name);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string(value, self->priv->description);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_event_source_set_property(GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  ReplayEventSource *self;

  g_return_if_fail(REPLAY_IS_EVENT_SOURCE(object));

  self = REPLAY_EVENT_SOURCE(object);

  switch (property_id) {
    case PROP_NAME:
      self->priv->name = g_value_dup_string(value);
      break;

    case PROP_DESCRIPTION:
      self->priv->description = g_value_dup_string(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_event_source_finalize(GObject *object)
{
  ReplayEventSource *self;

  g_return_if_fail(REPLAY_IS_EVENT_SOURCE(object));

  self = REPLAY_EVENT_SOURCE(object);
  g_free(self->priv->name);
  g_free(self->priv->description);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_event_source_parent_class)->finalize(object);
}

static void
replay_event_source_class_init(ReplayEventSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = replay_event_source_get_property;
  object_class->set_property = replay_event_source_set_property;
  object_class->finalize = replay_event_source_finalize;

  /**
   * ReplayEventSource::event:
   * @self: The event source
   * @event: The event to emit
   *
   * The ::event signal is emitted for each new event from this
   * #ReplayEventSource
   */
  signals[EVENT] = g_signal_new("event",
                                REPLAY_TYPE_EVENT_SOURCE,
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET(ReplayEventSourceClass,
                                                event),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__BOXED,
                                G_TYPE_NONE,
                                1,
                                REPLAY_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * ReplayErrorSource::error:
   * @self: The event source
   * @error: The error to emit
   *
   * The ::error signal is emitted when an error occurs during event handling
   * #ReplayErrorSource
   */
  signals[ERROR] = g_signal_new("error",
                                REPLAY_TYPE_EVENT_SOURCE,
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET(ReplayEventSourceClass,
                                                error),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__STRING,
                                G_TYPE_NONE,
                                1,
                                G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  properties[PROP_NAME] = g_param_spec_string("name",
                                              "Name",
                                              "Human readable name of the event source",
                                              "no-name-set",
                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property(object_class,
                                  PROP_NAME,
                                  properties[PROP_NAME]);
  properties[PROP_DESCRIPTION] = g_param_spec_string("description",
                                                     "Description",
                                                     "Human readable description of the event source",
                                                     "no-description-set",
                                                     G_PARAM_READWRITE);
  g_object_class_install_property(object_class,
                                  PROP_DESCRIPTION,
                                  properties[PROP_DESCRIPTION]);

  g_type_class_add_private(klass, sizeof(ReplayEventSourcePrivate));
}

static void
replay_event_source_init(ReplayEventSource *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_EVENT_SOURCE,
                                           ReplayEventSourcePrivate);
}

/**
 * replay_event_source_new:
 * @name: A name to describe this event source
 *
 * Return value: the newly created #ReplayEventSource instance
 *
 * Creates a new instance of a #ReplayEventSource.
 */
ReplayEventSource *
replay_event_source_new(const gchar *name)
{
  return g_object_new(REPLAY_TYPE_EVENT_SOURCE, "name", name, NULL);
}

/**
 * replay_event_source_get_name:
 * @self: A #ReplayEventSource
 *
 * Return value: the name describing this #ReplayEventSource
 *
 * Get the name of a #ReplayEventSource.
 */
const gchar *
replay_event_source_get_name(ReplayEventSource *self)
{
  g_return_val_if_fail(REPLAY_IS_EVENT_SOURCE(self), NULL);
  return self->priv->name;
}

/**
 * replay_event_source_get_description:
 * @self: A #ReplayEventSource
 *
 * Return value: the description describing this #ReplayEventSource
 *
 * Get the description of a #ReplayEventSource.
 */
const gchar *
replay_event_source_get_description(ReplayEventSource *self)
{
  g_return_val_if_fail(REPLAY_IS_EVENT_SOURCE(self), NULL);
  return self->priv->description;
}

/**
 * replay_event_source_set_description:
 * @self: A #ReplayEventSource
 * @description: A string to describe the current event source
 *
 * Set the description of a #ReplayEventSource.
 */
void
replay_event_source_set_description(ReplayEventSource *self,
                                 const gchar *description)
{
  g_return_if_fail(REPLAY_IS_EVENT_SOURCE(self));
  g_free(self->priv->description);
  self->priv->description = g_strdup(description);
  g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DESCRIPTION]);
}

/**
 * replay_event_source_emit_event:
 * @self: The #ReplayEventSource which is producing this event
 * @event: The new event which has been produced
 *
 * This should only be called by objects which implement the #ReplayEventSource
 * interface to emit the "event" signal - the event should be dynamically
 * allocated and should not be used after a call to this function except to
 * drop a reference to the event. The standard way to use this is as:
 *
 * |[
 * // dynamically allocate an event
 * ReplayEvent *event = replay_event_new();
 *
 * // initialize event as required
 *
 * // emit the event
 * replay_event_source_emit_event(REPLAY_EVENT_SOURCE(self), event);
 *
 * // drop reference to the event
 * g_object_unref(event);
 * ]|
 */
void
replay_event_source_emit_event(ReplayEventSource *self,
                            ReplayEvent *event)
{
  g_return_if_fail(REPLAY_IS_EVENT_SOURCE(self));
  g_return_if_fail(event != NULL);

  g_signal_emit(self, signals[EVENT], 0, event);
}

/**
 * replay_event_source_emit_error:
 * @self: The #ReplayEventSource which encounters an error
 * @error: A string describing the error
 *
 */
void
replay_event_source_emit_error(ReplayEventSource *self,
                            const gchar *error)
{
  g_return_if_fail(REPLAY_IS_EVENT_SOURCE(self));

  g_signal_emit(self, signals[ERROR], 0, error);
}
