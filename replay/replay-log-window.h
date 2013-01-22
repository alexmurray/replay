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

#ifndef __REPLAY_LOG_WINDOW_H__
#define __REPLAY_LOG_WINDOW_H__

#include <gtk/gtk.h>
#include <replay/replay-log.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_LOG_WINDOW            (replay_log_window_get_type())
#define REPLAY_LOG_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_LOG_WINDOW, ReplayLogWindow))
#define REPLAY_LOG_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REPLAY_TYPE_LOG_WINDOW, ReplayLogWindowClass))
#define REPLAY_IS_LOG_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_LOG_WINDOW))
#define REPLAY_IS_LOG_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REPLAY_TYPE_LOG_WINDOW))
#define REPLAY_LOG_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REPLAY_TYPE_LOG_WINDOW, ReplayLogWindowClass))

typedef struct _ReplayLogWindow        ReplayLogWindow;
typedef struct _ReplayLogWindowClass   ReplayLogWindowClass;
typedef struct _ReplayLogWindowPrivate ReplayLogWindowPrivate;

struct _ReplayLogWindow
{
  GtkWindow parent;

  /*< private >*/
  ReplayLogWindowPrivate *priv;
};

struct _ReplayLogWindowClass
{
  /*< private >*/
  GtkWindowClass parent_class;
};

GType replay_log_window_get_type(void);
GtkWidget *replay_log_window_new(ReplayLog *log);
void replay_log_window_set_log(ReplayLogWindow *self,
                            ReplayLog *log);

G_END_DECLS

#endif /* __REPLAY_LOG_WINDOW_H__ */
