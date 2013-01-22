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

#ifndef __REPLAY_GRAPHVIZ_PLUGIN_H__
#define __REPLAY_GRAPHVIZ_PLUGIN_H__

#include <libpeas/peas.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define REPLAY_TYPE_GRAPHVIZ_PLUGIN		(replay_graphviz_plugin_get_type ())
#define REPLAY_GRAPHVIZ_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), REPLAY_TYPE_GRAPHVIZ_PLUGIN, ReplayGraphvizPlugin))
#define REPLAY_GRAPHVIZ_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), REPLAY_TYPE_GRAPHVIZ_PLUGIN, ReplayGraphvizPluginClass))
#define REPLAY_IS_GRAPHVIZ_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), REPLAY_TYPE_GRAPHVIZ_PLUGIN))
#define REPLAY_IS_GRAPHVIZ_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), REPLAY_TYPE_GRAPHVIZ_PLUGIN))
#define REPLAY_GRAPHVIZ_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), REPLAY_TYPE_GRAPHVIZ_PLUGIN, ReplayGraphvizPluginClass))

/* Private structure type */
typedef struct _ReplayGraphvizPluginPrivate	ReplayGraphvizPluginPrivate;

/*
 * Main object structure
 */
typedef struct _ReplayGraphvizPlugin ReplayGraphvizPlugin;

struct _ReplayGraphvizPlugin
{
  PeasExtensionBase parent_instance;

  /*< private >*/
  ReplayGraphvizPluginPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _ReplayGraphvizPluginClass	ReplayGraphvizPluginClass;

struct _ReplayGraphvizPluginClass
{
  /*< private >*/
  PeasExtensionBaseClass parent_class;
};

/*
 * Public methods
 */
GType	replay_graphviz_plugin_get_type		(void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module);

G_END_DECLS

#endif /* __REPLAY_GRAPHVIZ_PLUGIN_H__ */
