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

#ifndef __REPLAY_TASK_H__
#define __REPLAY_TASK_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_TASK            (replay_task_get_type())
#define REPLAY_TASK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_TASK, ReplayTask))
#define REPLAY_TASK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REPLAY_TYPE_TASK, ReplayTaskClass))
#define REPLAY_IS_TASK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_TASK))
#define REPLAY_IS_TASK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REPLAY_TYPE_TASK))
#define REPLAY_TASK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REPLAY_TYPE_TASK, ReplayTaskClass))

/**
 * ReplayTask:
 *
 * Base class which represents a task being performed by replay
 */
typedef struct _ReplayTask        ReplayTask;
typedef struct _ReplayTaskClass   ReplayTaskClass;
typedef struct _ReplayTaskPrivate ReplayTaskPrivate;

struct _ReplayTask
{
  GObject parent;

  /*< private >*/
  ReplayTaskPrivate *priv;
};

/**
 * ReplayTaskClass:
 * @parent_class: The parent class
 * @cancel: Cancel the task
 * @error: Signal emitted by #replay_task_emit_error
 * @progress: Signal emitted by #replay_task_emit_progress
 *
 */
struct _ReplayTaskClass
{
  GObjectClass parent_class;

  /* public virtual methods */
  void (*cancel) (ReplayTask *self);

  /* signals */
  void (*error)(ReplayTask *self,
                const gchar *error);
  void (*progress)(ReplayTask *self,
                   gdouble fraction);
};

GType replay_task_get_type(void);
ReplayTask *replay_task_new(const gchar *icon,
                      const gchar *description);
void replay_task_set_icon(ReplayTask *self, const gchar *icon);
void replay_task_set_description(ReplayTask *self, const gchar *description);
const gchar *replay_task_get_icon(ReplayTask *self);
const gchar *replay_task_get_description(ReplayTask *self);
void replay_task_emit_progress(ReplayTask *self, gdouble fraction);
void replay_task_emit_error(ReplayTask *self, const gchar *error);
void replay_task_cancel(ReplayTask *self);

G_END_DECLS

#endif /* __REPLAY_TASK_H__ */
