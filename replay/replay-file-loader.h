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

/* replay-file-loader.h */

#ifndef __REPLAY_FILE_LOADER_H__
#define __REPLAY_FILE_LOADER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <replay/replay-event-source.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_FILE_LOADER            (replay_file_loader_get_type())
#define REPLAY_FILE_LOADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_FILE_LOADER, ReplayFileLoader))
#define REPLAY_FILE_LOADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REPLAY_TYPE_FILE_LOADER, ReplayFileLoaderClass))
#define REPLAY_IS_FILE_LOADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_FILE_LOADER))
#define REPLAY_IS_FILE_LOADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REPLAY_TYPE_FILE_LOADER))
#define REPLAY_FILE_LOADER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REPLAY_TYPE_FILE_LOADER, ReplayFileLoaderClass))

typedef struct _ReplayFileLoader        ReplayFileLoader;
typedef struct _ReplayFileLoaderClass   ReplayFileLoaderClass;
typedef struct _ReplayFileLoaderPrivate ReplayFileLoaderPrivate;

struct _ReplayFileLoader
{
  ReplayEventSource parent;

  /*< private >*/
  ReplayFileLoaderPrivate *priv;
};

/**
 * ReplayFileLoaderClass:
 * @parent_class: The parent class
 * @load_file: Load the given #GFile
 * @cancel: Cancel any current file load
 * @progress: Signal emitted by #replay_file_loader_emit_progress
 *
 */
struct _ReplayFileLoaderClass
{
  ReplayEventSourceClass parent_class;

  /* public virtual methods */
  void (*load_file) (ReplayFileLoader *self, GFile *file);
  void (*cancel) (ReplayFileLoader *self);

  /* signals */
  void (*progress) (ReplayFileLoader *self, gdouble fraction);
};

GType          replay_file_loader_get_type     (void);
ReplayFileLoader *replay_file_loader_new          (const gchar *name, const gchar *mime_type, const gchar *pattern);
void           replay_file_loader_load_file    (ReplayFileLoader *self, GFile *file);
void           replay_file_loader_cancel       (ReplayFileLoader *self);
const gchar   *replay_file_loader_get_mime_type(ReplayFileLoader *self);
const gchar   *replay_file_loader_get_pattern  (ReplayFileLoader *self);
void           replay_file_loader_emit_progress(ReplayFileLoader *self, gdouble fraction);

G_END_DECLS

#endif /* __REPLAY_FILE_LOADER_H__ */
