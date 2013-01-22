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

#ifndef __REPLAY_EVENT_SOURCE_H__
#define __REPLAY_EVENT_SOURCE_H__

#include <glib-object.h>
#include <replay/replay-event.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_EVENT_SOURCE                 (replay_event_source_get_type ())
#define REPLAY_EVENT_SOURCE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_EVENT_SOURCE, ReplayEventSource))
#define REPLAY_EVENT_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REPLAY_TYPE_EVENT_SOURCE, ReplayEventSourceClass))
#define REPLAY_IS_EVENT_SOURCE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_EVENT_SOURCE))
#define REPLAY_IS_EVENT_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REPLAY_TYPE_EVENT_SOURCE))
#define REPLAY_EVENT_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REPLAY_TYPE_EVENT_SOURCE, ReplayEventSourceClass))

/**
 * ReplayEventSource:
 *
 * Base class which represents a source of #ReplayEvent<!-- -->s
 */
typedef struct _ReplayEventSource ReplayEventSource;
typedef struct _ReplayEventSourceClass ReplayEventSourceClass;
typedef struct _ReplayEventSourcePrivate ReplayEventSourcePrivate;

struct _ReplayEventSource
{
  GObject parent;

  /*< private >*/
  ReplayEventSourcePrivate *priv;
};

/**
 * ReplayEventSourceClass:
 * @parent_class: The parent class
 * @event: Emitted when events are produced by this event source via #replay_event_source_emit_event
 * @error: Emitted when errors are produced by this error source via #replay_error_source_emit_error
 *
 */
struct _ReplayEventSourceClass
{
  GObjectClass parent_class;

  /* signals */
  void (*event) (ReplayEventSource *self,
                 ReplayEvent *event);
  void (*error) (ReplayEventSource *self,
                 const gchar *error);
};

GType replay_event_source_get_type(void);

ReplayEventSource *replay_event_source_new(const gchar *name);
const gchar *replay_event_source_get_name(ReplayEventSource *self);
void replay_event_source_set_description(ReplayEventSource *self, const gchar *description);
const gchar *replay_event_source_get_description(ReplayEventSource *self);
void replay_event_source_emit_event(ReplayEventSource *self, ReplayEvent *event);
void replay_event_source_emit_error(ReplayEventSource *self, const gchar *error);

G_END_DECLS

#endif /* __REPLAY_EVENT_SOURCE_H__ */
