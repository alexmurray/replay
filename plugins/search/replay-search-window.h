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

#ifndef __REPLAY_SEARCH_WINDOW_H__
#define __REPLAY_SEARCH_WINDOW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_SEARCH_WINDOW                  \
  (replay_search_window_get_type())
#define REPLAY_SEARCH_WINDOW(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),                    \
                              REPLAY_TYPE_SEARCH_WINDOW,   \
                              ReplaySearchWindow))
#define REPLAY_SEARCH_WINDOW_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           REPLAY_TYPE_SEARCH_WINDOW,      \
                           ReplaySearchWindowClass))
#define REPLAY_IS_SEARCH_WINDOW(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),                    \
                              REPLAY_TYPE_SEARCH_WINDOW))
#define REPLAY_IS_SEARCH_WINDOW_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                     \
                           REPLAY_TYPE_SEARCH_WINDOW))
#define REPLAY_SEARCH_WINDOW_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS((obj),                     \
                             REPLAY_TYPE_SEARCH_WINDOW,    \
                             ReplaySearchWindowClass))

typedef struct _ReplaySearchWindow      ReplaySearchWindow;
typedef struct _ReplaySearchWindowClass ReplaySearchWindowClass;
typedef struct _ReplaySearchWindowPrivate ReplaySearchWindowPrivate;

struct _ReplaySearchWindowClass
{
  GtkWindowClass parent_class;

  /* signals */
  void (*search)(ReplaySearchWindow *self,
                 gboolean up);
  void (*cancel)(ReplaySearchWindow *self);
};

struct _ReplaySearchWindow
{
  GtkWindow parent;
  ReplaySearchWindowPrivate *priv;
};

typedef enum {
  REPLAY_SEARCH_WINDOW_STATE_IDLE = 0,
  REPLAY_SEARCH_WINDOW_STATE_ACTIVE = 1,
  REPLAY_SEARCH_WINDOW_STATE_ERROR = 2
} ReplaySearchWindowState;

GType replay_search_window_get_type(void) G_GNUC_CONST;
GtkWindow *replay_search_window_new(void);
void replay_search_window_set_text(ReplaySearchWindow *self,
                                const gchar *text);
const gchar *replay_search_window_get_text(ReplaySearchWindow *self);
ReplaySearchWindowState replay_search_window_get_state(ReplaySearchWindow *self);
void replay_search_window_set_state(ReplaySearchWindow *self,
                                 ReplaySearchWindowState state);

G_END_DECLS

#endif /* __REPLAY_SEARCH_WINDOW_H__ */
