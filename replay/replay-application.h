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

#ifndef __REPLAY_APPLICATION_H__
#define __REPLAY_APPLICATION_H__

#include <gtk/gtk.h>
#include <replay/replay-network-server.h>
#include <replay/replay-file-loader.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_APPLICATION                 \
        (replay_application_get_type())
#define REPLAY_APPLICATION(obj)                                         \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_APPLICATION, ReplayApplication))
#define REPLAY_APPLICATION_CLASS(obj)                                   \
        (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_APPLICATION, ReplayApplicationClass))
#define REPLAY_IS_APPLICATION(obj)                                      \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_APPLICATION))
#define REPLAY_IS_APPLICATION_CLASS(obj)                                \
        (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_APPLICATION))
#define REPLAY_APPLICATION_GET_CLASS(obj)                               \
        (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_APPLICATION, ReplayApplicationClass))

typedef struct _ReplayApplication ReplayApplication;
typedef struct _ReplayApplicationClass ReplayApplicationClass;
typedef struct _ReplayApplicationPrivate ReplayApplicationPrivate;

struct _ReplayApplication {
        GtkApplication parent;

        /* private */
        ReplayApplicationPrivate *priv;
};

/**
 * ReplayApplicationClass:
 * @parent_class: The parent class
 * @file_loader_added: Signal emitted when a new file loader is added
 * @file_loader_removed: Signal emitted when a file loader is removed
 *
 */
struct _ReplayApplicationClass {
        GtkApplicationClass parent_class;

        /* signal emitted when a new file loader is added */
        void (*file_loader_added)(ReplayApplication *self,
                                  ReplayFileLoader *loader);
        /* signal emitted when a file loader is removed */
        void (*file_loader_removed)(ReplayApplication *self,
                                    ReplayFileLoader *loader);
};

/* public functions */
GType replay_application_get_type(void);
ReplayApplication *replay_application_new(void);
gboolean replay_application_display_help(ReplayApplication *self,
                                         const gchar *section,
                                         GError **error);
gboolean replay_application_add_network_server(ReplayApplication *self,
                                               ReplayNetworkServer *server,
                                               GError **error);
void replay_application_remove_network_server(ReplayApplication *self,
                                              ReplayNetworkServer *server);
void replay_application_add_file_loader(ReplayApplication *self,
                                        ReplayFileLoader *loader);
void replay_application_remove_file_loader(ReplayApplication *self,
                                           ReplayFileLoader *loader);
const GList *replay_application_get_file_loaders(ReplayApplication *self);

G_END_DECLS

#endif /* __REPLAY_APPLICATION_H__ */
