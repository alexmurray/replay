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

/* replay-graphviz-file-loader.h */

#ifndef __REPLAY_GRAPHVIZ_FILE_LOADER_H__
#define __REPLAY_GRAPHVIZ_FILE_LOADER_H__

#include <replay/replay-file-loader.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_GRAPHVIZ_FILE_LOADER            (replay_graphviz_file_loader_get_type())
#define REPLAY_GRAPHVIZ_FILE_LOADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_GRAPHVIZ_FILE_LOADER, ReplayGraphvizFileLoader))
#define REPLAY_GRAPHVIZ_FILE_LOADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REPLAY_TYPE_GRAPHVIZ_FILE_LOADER, ReplayGraphvizFileLoaderClass))
#define REPLAY_IS_GRAPHVIZ_FILE_LOADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_GRAPHVIZ_FILE_LOADER))
#define REPLAY_IS_GRAPHVIZ_FILE_LOADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REPLAY_TYPE_GRAPHVIZ_FILE_LOADER))
#define REPLAY_GRAPHVIZ_FILE_LOADER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REPLAY_TYPE_GRAPHVIZ_FILE_LOADER, ReplayGraphvizFileLoaderClass))

typedef struct _ReplayGraphvizFileLoader        ReplayGraphvizFileLoader;
typedef struct _ReplayGraphvizFileLoaderClass   ReplayGraphvizFileLoaderClass;

struct _ReplayGraphvizFileLoader
{
  ReplayFileLoader parent;
};

struct _ReplayGraphvizFileLoaderClass
{
  /*< private >*/
  ReplayFileLoaderClass parent_class;
};

GType          replay_graphviz_file_loader_get_type     (void);
ReplayFileLoader *replay_graphviz_file_loader_new(void);

G_END_DECLS

#endif /* __REPLAY_GRAPHVIZ_FILE_LOADER_H__ */
