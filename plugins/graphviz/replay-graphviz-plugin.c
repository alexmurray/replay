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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "replay-graphviz-plugin.h"
#include "replay-graphviz-file-loader.h"
#include <replay/replay-application-activatable.h>
#include <replay/replay-log.h>
#include <replay/replay-window.h>

#include <glib/gi18n-lib.h>

#define REPLAY_GRAPHVIZ_PLUGIN_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), REPLAY_TYPE_GRAPHVIZ_PLUGIN, ReplayGraphvizPluginPrivate))

struct _ReplayGraphvizPluginPrivate
{
  ReplayApplication *application;
  ReplayFileLoader *loader;
};

static void replay_application_activatable_iface_init(ReplayApplicationActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(ReplayGraphvizPlugin,
                               replay_graphviz_plugin,
                               PEAS_TYPE_EXTENSION_BASE,
                               0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(REPLAY_TYPE_APPLICATION_ACTIVATABLE,
                                                             replay_application_activatable_iface_init));

enum {
  PROP_APPLICATION = 1,
};

static void replay_graphviz_plugin_dispose(GObject *object);

static void
replay_graphviz_plugin_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  ReplayGraphvizPlugin *plugin = REPLAY_GRAPHVIZ_PLUGIN(object);

  switch (prop_id) {
    case PROP_APPLICATION:
      plugin->priv->application = REPLAY_APPLICATION(g_value_dup_object(value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
replay_graphviz_plugin_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  ReplayGraphvizPlugin *plugin = REPLAY_GRAPHVIZ_PLUGIN(object);

  switch (prop_id) {
    case PROP_APPLICATION:
      g_value_set_object(value, plugin->priv->application);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}
static void
replay_graphviz_plugin_init (ReplayGraphvizPlugin *plugin)
{
  plugin->priv = REPLAY_GRAPHVIZ_PLUGIN_GET_PRIVATE (plugin);
}

static void
replay_graphviz_plugin_dispose (GObject *object)
{
  ReplayGraphvizPlugin *self = REPLAY_GRAPHVIZ_PLUGIN(object);
  ReplayGraphvizPluginPrivate *priv = self->priv;

  if (priv->loader) {
    replay_application_remove_file_loader(priv->application, priv->loader);
    g_clear_object(&priv->loader);
  }
  g_clear_object(&priv->application);

  G_OBJECT_CLASS (replay_graphviz_plugin_parent_class)->dispose (object);
}

static void loader_progress_cb(ReplayFileLoader *loader,
                               gdouble fraction,
                               gpointer data)
{
  ReplayGraphvizPluginPrivate *priv;
  GList *windows;
  ReplayWindow *window = NULL;
  ReplayEventStore *store;
  gint n;

  priv = REPLAY_GRAPHVIZ_PLUGIN(data)->priv;

  if (fraction < 1.0)
  {
    return;
  }

  /* find the window using our file loader */
  for (windows = gtk_application_get_windows(GTK_APPLICATION(priv->application));
       windows != NULL;
       windows = windows->next)
  {
    ReplayWindow *w = REPLAY_WINDOW(windows->data);
    if (replay_window_get_event_source(w) == REPLAY_EVENT_SOURCE(priv->loader))
    {
      window = w;
      break;
    }
  }

  if (!window)
  {
    g_warning("Unable to find window corresponding to our active loader!!!");
    return;
  }

  /* hide timeline and message tree */
  replay_window_set_display_flags(window,
                                  (REPLAY_WINDOW_DISPLAY_ALL &
                                   ~REPLAY_WINDOW_DISPLAY_MESSAGE_TREE &
                                   ~REPLAY_WINDOW_DISPLAY_TIMELINE));

  /* jump to end of events */
  store = replay_window_get_event_store(window);

  n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store),
                                     NULL);
  if (n > 0)
  {
    GtkTreePath *path;
    GtkTreeIter iter;

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, n - 1);
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
    gtk_tree_path_free(path);
    replay_event_store_set_current(store, &iter);
  }
}

static void
replay_graphviz_plugin_activate(ReplayApplicationActivatable *plugin)
{
  ReplayGraphvizPlugin *self = REPLAY_GRAPHVIZ_PLUGIN(plugin);
  ReplayGraphvizPluginPrivate *priv = self->priv;

  priv->loader = replay_graphviz_file_loader_new();

  g_signal_connect(priv->loader, "progress", G_CALLBACK(loader_progress_cb),
                   self);
  replay_application_add_file_loader(priv->application, priv->loader);
}

static void
replay_graphviz_plugin_deactivate(ReplayApplicationActivatable *plugin)
{
  ReplayGraphvizPlugin *self = REPLAY_GRAPHVIZ_PLUGIN(plugin);
  ReplayGraphvizPluginPrivate *priv = self->priv;

  replay_application_remove_file_loader(priv->application, priv->loader);
  g_clear_object(&priv->loader);
}

static void
replay_graphviz_plugin_class_init (ReplayGraphvizPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (object_class, sizeof (ReplayGraphvizPluginPrivate));

  object_class->get_property = replay_graphviz_plugin_get_property;
  object_class->set_property = replay_graphviz_plugin_set_property;
  object_class->dispose = replay_graphviz_plugin_dispose;

  g_object_class_override_property(object_class, PROP_APPLICATION, "application");
}

static void
replay_application_activatable_iface_init(ReplayApplicationActivatableInterface *iface)
{
  iface->activate = replay_graphviz_plugin_activate;
  iface->deactivate = replay_graphviz_plugin_deactivate;
}

static void
replay_graphviz_plugin_class_finalize(ReplayGraphvizPluginClass *klass)
{
  /* nothing to do */
}

G_MODULE_EXPORT void
peas_register_types(PeasObjectModule *module)
{
  replay_graphviz_plugin_register_type(G_TYPE_MODULE(module));

  peas_object_module_register_extension_type(module,
                                             REPLAY_TYPE_APPLICATION_ACTIVATABLE,
                                             REPLAY_TYPE_GRAPHVIZ_PLUGIN);
}
