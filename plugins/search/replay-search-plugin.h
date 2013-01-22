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

#ifndef __REPLAY_SEARCH_PLUGIN_H__
#define __REPLAY_SEARCH_PLUGIN_H__

#include <libpeas/peas.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define REPLAY_TYPE_SEARCH_PLUGIN		(replay_search_plugin_get_type ())
#define REPLAY_SEARCH_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), REPLAY_TYPE_SEARCH_PLUGIN, ReplaySearchPlugin))
#define REPLAY_SEARCH_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), REPLAY_TYPE_SEARCH_PLUGIN, ReplaySearchPluginClass))
#define REPLAY_IS_SEARCH_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), REPLAY_TYPE_SEARCH_PLUGIN))
#define REPLAY_IS_SEARCH_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), REPLAY_TYPE_SEARCH_PLUGIN))
#define REPLAY_SEARCH_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), REPLAY_TYPE_SEARCH_PLUGIN, ReplaySearchPluginClass))

/* Private structure type */
typedef struct _ReplaySearchPluginPrivate	ReplaySearchPluginPrivate;

/*
 * Main object structure
 */
typedef struct _ReplaySearchPlugin ReplaySearchPlugin;

struct _ReplaySearchPlugin
{
  PeasExtensionBase parent_instance;

  /*< private >*/
  ReplaySearchPluginPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _ReplaySearchPluginClass	ReplaySearchPluginClass;

struct _ReplaySearchPluginClass
{
  /*< private >*/
  PeasExtensionBaseClass parent_class;
};

/*
 * Public methods
 */
GType	replay_search_plugin_get_type		(void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module);

G_END_DECLS

#endif /* __REPLAY_SEARCH_PLUGIN_H__ */
