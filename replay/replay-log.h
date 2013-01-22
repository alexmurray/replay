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

#ifndef __REPLAY_LOG_H__
#define __REPLAY_LOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * ReplayLogLevel:
 * @REPLAY_LOG_LEVEL_ERROR: Indicates an error has occurred
 * @REPLAY_LOG_LEVEL_INFO: A message, not debugging but not an error
 * @REPLAY_LOG_LEVEL_DEBUG: General debugging
 * @REPLAY_NUM_LOG_LEVELS: The number of different log levels
 *
 * The importance of a log message
 */
typedef enum _ReplayLogLevel
{
  REPLAY_LOG_LEVEL_ERROR = 0,
  REPLAY_LOG_LEVEL_INFO = 1,
  REPLAY_LOG_LEVEL_DEBUG = 2,
  REPLAY_NUM_LOG_LEVELS
} ReplayLogLevel;

/**
 * replay_log_level_to_string:
 * @level: The #ReplayLogLevel
 *
 * Convert a #ReplayLogLevel to its string representation
 *
 * Returns: A string representation for each #ReplayLogLevel
 */
const gchar *replay_log_level_to_string(ReplayLogLevel level);

#define REPLAY_TYPE_LOG            (replay_log_get_type())
#define REPLAY_LOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_LOG, ReplayLog))
#define REPLAY_LOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REPLAY_TYPE_LOG, ReplayLogClass))
#define REPLAY_IS_LOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_LOG))
#define REPLAY_IS_LOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REPLAY_TYPE_LOG))
#define REPLAY_LOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REPLAY_TYPE_LOG, ReplayLogClass))

typedef struct _ReplayLog        ReplayLog;
typedef struct _ReplayLogClass   ReplayLogClass;
typedef struct _ReplayLogPrivate ReplayLogPrivate;

struct _ReplayLog
{
  GtkTreeStore parent;

  /*< private >*/
  ReplayLogPrivate *priv;
};

/**
 * ReplayLogClass:
 * @parent_class: The parent class
 * @appended: Emitted when a new log message is appended to the log
 *
 */
struct _ReplayLogClass
{
  GtkTreeStoreClass parent_class;

  void (* appended) (ReplayLog *self,
                     ReplayLogLevel level,
                     const gchar *time_,
                     const gchar *source,
                     const gchar *brief,
                     const gchar *details);
};

enum
{
  REPLAY_LOG_COL_LOG_LEVEL = 0,
  REPLAY_LOG_COL_TIME,
  REPLAY_LOG_COL_SOURCE,
  REPLAY_LOG_COL_BRIEF_DETAILS,
  REPLAY_LOG_NUM_COLS,
};

GType replay_log_get_type(void);
ReplayLog *replay_log_new(void);
void replay_log_append(ReplayLog *self,
                    ReplayLogLevel level,
                    const gchar *source,
                    const gchar *brief,
                    const gchar *details);
void replay_log_clear(ReplayLog *self);

/**
 * replay_log_error:
 * @log: The #ReplayLog to log the message to
 * @source: The source of the log message (i.e. a component or plugin etc)
 * @brief: A brief, one-line summary of the message
 * @details: An optional piece of extended information for this message
 *
 * Log a message at the #REPLAY_LOG_LEVEL_ERROR level
 */
#define replay_log_error(log, source, brief, details)       \
  replay_log_append(log, REPLAY_LOG_LEVEL_ERROR, source, brief, details)

/**
 * replay_log_info:
 * @log: The #ReplayLog to log the message to
 * @source: The source of the log message (i.e. a component or plugin etc)
 * @brief: A brief, one-line summary of the message
 * @details: An optional piece of extended information for this message
 *
 * Log a message at the #REPLAY_LOG_LEVEL_INFO level
 */
#define replay_log_info(log, source, brief, details)       \
  replay_log_append(log, REPLAY_LOG_LEVEL_INFO, source, brief, details)

/**
 * replay_log_debug:
 * @log: The #ReplayLog to log the message to
 * @source: The source of the log message (i.e. a component or plugin etc)
 * @brief: A brief, one-line summary of the message
 * @details: An optional piece of extended information for this message
 *
 * Log a message at the #REPLAY_LOG_LEVEL_DEBUG level
 */
#define replay_log_debug(log, source, brief, details)       \
  replay_log_append(log, REPLAY_LOG_LEVEL_DEBUG, source, brief, details)


G_END_DECLS

#endif /* __REPLAY_LOG_H__ */
