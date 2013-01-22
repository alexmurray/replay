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
 * SECTION:replay-task
 * @short_description: Base-class for objects which represent a task being
 * executed by the replay UI
 * @see_also: #ReplayFileLoader
 */

/* replay-task.c */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-task.h>

G_DEFINE_TYPE(ReplayTask, replay_task, G_TYPE_OBJECT);

enum
{
  ERROR,
  PROGRESS,
  CANCEL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

enum
{
  PROP_0,
  PROP_ICON,
  PROP_DESCRIPTION,
};

struct _ReplayTaskPrivate
{
  gchar *icon;
  gchar *description;
};

static void
replay_task_get_property(GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  ReplayTask *self;

  g_return_if_fail(REPLAY_IS_TASK(object));

  self = REPLAY_TASK(object);

  switch (property_id) {
    case PROP_ICON:
      g_value_set_string(value, replay_task_get_icon(self));
      break;

    case PROP_DESCRIPTION:
      g_value_set_string(value, replay_task_get_description(self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_task_set_property(GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  ReplayTask *self;

  g_return_if_fail(REPLAY_IS_TASK(object));

  self = REPLAY_TASK(object);

  switch (property_id) {
    case PROP_ICON:
      replay_task_set_icon(self, g_value_get_string(value));
      break;

    case PROP_DESCRIPTION:
      replay_task_set_description(self, g_value_get_string(value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
dummy_cancel(ReplayTask *self)
{
  return;
}

static void
replay_task_class_init(ReplayTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = replay_task_get_property;
  object_class->set_property = replay_task_set_property;

  klass->cancel = dummy_cancel;

  /**
   * ReplayTask::error:
   * @self: A #ReplayTask
   *
   * The error signal.
   */
  signals[ERROR] = g_signal_new("error",
                                REPLAY_TYPE_TASK,
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET(ReplayTaskClass, error),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__STRING,
                                G_TYPE_NONE,
                                1,
                                G_TYPE_STRING);

  signals[PROGRESS] = g_signal_new("progress",
                                  REPLAY_TYPE_TASK,
                                  G_SIGNAL_RUN_LAST,
                                  G_STRUCT_OFFSET(ReplayTaskClass, progress),
                                  NULL,
                                  NULL,
                                  g_cclosure_marshal_VOID__DOUBLE,
                                  G_TYPE_NONE,
                                  1,
                                  G_TYPE_DOUBLE);

  /**
   * ReplayTask::cancel:
   * @self: A #ReplayTask
   *
   * The cancel signal.
   */
  signals[CANCEL] = g_signal_new("cancel",
                                 REPLAY_TYPE_TASK,
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET(ReplayTaskClass, cancel),
                                 NULL,
                                 NULL,
                                 g_cclosure_marshal_VOID__VOID,
                                 G_TYPE_NONE,
                                 0);

  g_object_class_install_property(object_class,
                                  PROP_ICON,
                                  g_param_spec_string("icon",
                                                      "Icon",
                                                      "Icon name to describe this task",
                                                      "gtk-missing-image",
                                                      G_PARAM_READWRITE));

  g_object_class_install_property(object_class,
                                  PROP_DESCRIPTION,
                                  g_param_spec_string("description",
                                                      "Description",
                                                      "Description to describe this task",
                                                      "Unknown task",
                                                      G_PARAM_READWRITE));


  g_type_class_add_private(klass, sizeof(ReplayTaskPrivate));
}

static void
replay_task_init(ReplayTask *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_TASK,
                                           ReplayTaskPrivate);
  self->priv->icon = NULL;
  self->priv->description = NULL;
}

/**
 * replay_task_new:
 * @icon: An icon name to describe this task visually
 * @description: A description of this task to display whilst it is running
 *
 * Return value: the newly created #ReplayTask instance
 *
 * Creates a new instance of #ReplayTask.
 */
ReplayTask *replay_task_new(const gchar *icon,
                      const gchar *description)
{
  ReplayTask *self;

  self = g_object_new(REPLAY_TYPE_TASK, NULL);
  replay_task_set_icon(self, icon);
  replay_task_set_description(self, description);

  return self;
}

/**
 * replay_task_set_icon:
 * @self: The #ReplayTask
 * @icon: The icon to describe this task
 *
 */
void replay_task_set_icon(ReplayTask *self, const gchar *icon)
{
  g_return_if_fail(REPLAY_IS_TASK(self));

  g_free(self->priv->icon);

  self->priv->icon = icon ? g_strdup(icon) : NULL;
}

/**
 * replay_task_get_icon:
 * @self: The #ReplayTask
 *
 * Return value: The icon name for this task
 */
const gchar *replay_task_get_icon(ReplayTask *self)
{
  g_return_val_if_fail(REPLAY_IS_TASK(self), NULL);

  return self->priv->icon;
}
/**
 * replay_task_set_description:
 * @self: The #ReplayTask
 * @description: The description to describe this task
 *
 */
void replay_task_set_description(ReplayTask *self, const gchar *description)
{
  g_return_if_fail(REPLAY_IS_TASK(self));

  g_free(self->priv->description);

  self->priv->description = description ? g_strdup(description) : NULL;
}

/**
 * replay_task_get_description:
 * @self: The #ReplayTask
 *
 * Return value: The description of this #ReplayTask
 */
const gchar *replay_task_get_description(ReplayTask *self)
{
  g_return_val_if_fail(REPLAY_IS_TASK(self), NULL);

  return self->priv->description;
}
/**
 * replay_task_cancel:
 * @self: The #ReplayTask to cancel
 *
 * Cancel's the execution of this #ReplayTask by emitting the "cancel" signal
 */
void replay_task_cancel(ReplayTask *self)
{
  g_return_if_fail(REPLAY_IS_TASK(self));
  g_signal_emit(self, signals[CANCEL], 0);
}

/**
 * replay_task_emit_progress:
 * @self: The #ReplayTask to emit the progress signal upon
 * @fraction: The amount of progress (ranging from 0.0 to 1.0)
 *
 * Emits the "progress" signal on @self
 */
void replay_task_emit_progress(ReplayTask *self, gdouble fraction)
{
  g_return_if_fail(REPLAY_IS_TASK(self));
  g_signal_emit(self, signals[PROGRESS], 0, fraction);
}

/**
 * replay_task_emit_error:
 * @self: The #ReplayTask to emit the error signal upon
 * @error: The #GError to emit
 *
 * Emits the "error" signal on @self
 */
void replay_task_emit_error(ReplayTask *self, const gchar *error)
{
  g_return_if_fail(REPLAY_IS_TASK(self));
  g_signal_emit(self, signals[ERROR], 0, error);
}
