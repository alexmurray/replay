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
 * SECTION:replay-window-activatable
 * @short_description: Interface for Replay plugins
 * @include: replay/replay-window-activatable.h
 * @see_also: #PeasActivatable
 *
 * #ReplayWindowActivatable is an interface which plugins which require access to a
 * #ReplayWindow should implement.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-window-activatable.h>
#include <replay/replay-window.h>

G_DEFINE_INTERFACE (ReplayWindowActivatable, replay_window_activatable, G_TYPE_OBJECT);

static void
replay_window_activatable_default_init(ReplayWindowActivatableInterface *iface)
{
  static gboolean inited = FALSE;

  if (!inited) {
    g_object_interface_install_property(iface, g_param_spec_object("window",
                                                                   "The ReplayWindow for this plugin",
                                                                   "The ReplayWindow which this plugin has been activated upon",
                                                                   REPLAY_TYPE_WINDOW,
                                                                   G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    inited = TRUE;
  }
}

/**
 * replay_window_activatable_activate:
 * @self: self
 */
void
replay_window_activatable_activate(ReplayWindowActivatable *self)
{
  ReplayWindowActivatableInterface *iface;

  g_return_if_fail(REPLAY_IS_WINDOW_ACTIVATABLE(self));

  iface = REPLAY_WINDOW_ACTIVATABLE_GET_IFACE(self);

  if (iface->activate != NULL) {
    iface->activate(self);
  }
}

/**
 * replay_window_activatable_deactivate:
 * @self: self
 */
void
replay_window_activatable_deactivate(ReplayWindowActivatable *self)
{
  ReplayWindowActivatableInterface *iface;

  g_return_if_fail(REPLAY_IS_WINDOW_ACTIVATABLE(self));

  iface = REPLAY_WINDOW_ACTIVATABLE_GET_IFACE(self);

  if (iface->deactivate != NULL) {
    iface->deactivate(self);
  }
}
