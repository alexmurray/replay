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
 * SECTION:replay-log
 * @short_description: Stores a log of messages
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <replay/replay-log.h>
#include "replay-enums.h"
#include "replay-marshallers.h"

G_DEFINE_TYPE(ReplayLog, replay_log, GTK_TYPE_TREE_STORE);

/* signals we emit */
enum
{
  APPENDED = 0,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

static GType types[REPLAY_LOG_NUM_COLS] =
{
  G_TYPE_INT, /* log level */
  G_TYPE_STRING, /* time */
  G_TYPE_STRING, /* source */
  G_TYPE_STRING, /* brief / details */
};

static void
replay_log_init(ReplayLog *self)
{
  gtk_tree_store_set_column_types(GTK_TREE_STORE(self),
                                  REPLAY_LOG_NUM_COLS, types);
}

static void
replay_log_class_init(ReplayLogClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  /* install class signals */
  signals[APPENDED] = g_signal_new("appended",
                                   G_OBJECT_CLASS_TYPE(obj_class),
                                   G_SIGNAL_RUN_LAST,
                                   G_STRUCT_OFFSET(ReplayLogClass,
                                                   appended),
                                   NULL, NULL,
                                   replay_marshal_VOID__ENUM_STRING_STRING_STRING_STRING,
                                   G_TYPE_NONE,
                                   5,
                                   REPLAY_TYPE_LOG_LEVEL, G_TYPE_STRING, G_TYPE_STRING,
                                   G_TYPE_STRING, G_TYPE_STRING);
}

ReplayLog *
replay_log_new(void)
{
  return g_object_new(REPLAY_TYPE_LOG, NULL);
}

/**
 * replay_log_append:
 * @self: The #ReplayLog to log the message to
 * @level: The #ReplayLogLevel of the message
 * @source: The source of the log message (i.e. a component or plugin etc)
 * @brief: A brief, one-line summary of the message
 * @details: (allow-none): An optional piece of extended information for this message
 *
 * Log a message at the #REPLAY_LOG_LEVEL_ERROR level
 */
void replay_log_append(ReplayLog *self,
                    ReplayLogLevel level,
                    const gchar *source,
                    const gchar *brief,
                    const gchar *details)
{
  GtkTreeIter iter;
  gchar *time_str = NULL;
  gchar buffer[10];
  time_t t;
  struct tm *tmp;

  g_return_if_fail(REPLAY_IS_LOG(self));

  t = time(NULL);
  tmp = localtime(&t);
  if (tmp && strftime(buffer, sizeof(buffer), "%H:%M:%S", tmp))
  {
    time_str = buffer;
  }

  gtk_tree_store_append(GTK_TREE_STORE(self), &iter, NULL);
  gtk_tree_store_set(GTK_TREE_STORE(self), &iter,
                     REPLAY_LOG_COL_LOG_LEVEL, level,
                     REPLAY_LOG_COL_TIME, time_str,
                     REPLAY_LOG_COL_SOURCE, source,
                     REPLAY_LOG_COL_BRIEF_DETAILS, brief,
                     -1);

  if (details != NULL)
  {
    GtkTreeIter child;
    gtk_tree_store_append(GTK_TREE_STORE(self), &child, &iter);
    gtk_tree_store_set(GTK_TREE_STORE(self), &child,
                       REPLAY_LOG_COL_LOG_LEVEL, level,
                       REPLAY_LOG_COL_BRIEF_DETAILS, details,
                       -1);
  }
  g_signal_emit(self, signals[APPENDED], 0, level, time_str, source, brief, details);
}

void
replay_log_clear(ReplayLog *self)
{
  g_return_if_fail(REPLAY_IS_LOG(self));

  gtk_tree_store_clear(GTK_TREE_STORE(self));
}

static const gchar *log_levels[] = { N_("Error"), N_("Information"), N_("Debug") };

const gchar *
replay_log_level_to_string(ReplayLogLevel level) {
  g_return_val_if_fail(level < REPLAY_NUM_LOG_LEVELS, NULL);
  return gettext(log_levels[level]);
}
