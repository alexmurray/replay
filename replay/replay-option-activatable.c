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
 * SECTION:replay-option-activatable
 * @short_description: Interface for plugins to provide command-line options
 * @include: replay/replay-option-activatable.h
 * @see_also: #PeasActivatable
 *
 * #ReplayOptionActivatable is an interface which plugins which want to provide
 * command-line options should implement
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-option-activatable.h>
#include <glib.h>

G_DEFINE_INTERFACE (ReplayOptionActivatable, replay_option_activatable, G_TYPE_OBJECT);

static void
replay_option_activatable_default_init(ReplayOptionActivatableInterface *iface)
{
  static gboolean inited = FALSE;

  if (!inited) {
    g_object_interface_install_property(iface, g_param_spec_pointer("context",
                                                                    "The GOptionContext for this option_activatable",
                                                                    "The GOptionContext which this option_activatable has been activated upon",
                                                                    G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    inited = TRUE;
  }
}

/**
 * replay_option_activatable_activate:
 * @self: self
 */
void
replay_option_activatable_activate(ReplayOptionActivatable *self)
{
  ReplayOptionActivatableInterface *iface;

  g_return_if_fail(REPLAY_IS_OPTION_ACTIVATABLE(self));

  iface = REPLAY_OPTION_ACTIVATABLE_GET_IFACE(self);

  if (iface->activate != NULL) {
    iface->activate(self);
  }
}
