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
 * SECTION:replay-application-activatable
 * @short_description: Interface for Replay plugins
 * @include: replay/replay-application-activatable.h
 * @see_also: #PeasActivatable
 *
 * #ReplayApplicationActivatable is an interface which plugins which require access
 * to a #ReplayApplication should implement
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-application-activatable.h>
#include <replay/replay-application.h>

G_DEFINE_INTERFACE (ReplayApplicationActivatable, replay_application_activatable, G_TYPE_OBJECT);

static void
replay_application_activatable_default_init(ReplayApplicationActivatableInterface *iface)
{
  static gboolean inited = FALSE;

  if (!inited) {
    g_object_interface_install_property(iface, g_param_spec_object("application",
                                                                   "The ReplayApplication for this plugin",
                                                                   "The ReplayApplication which this plugin has been activated upon",
                                                                   REPLAY_TYPE_APPLICATION,
                                                                   G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    inited = TRUE;
  }
}

/**
 * replay_application_activatable_activate:
 * @self: self
 */
void
replay_application_activatable_activate(ReplayApplicationActivatable *self)
{
  ReplayApplicationActivatableInterface *iface;

  g_return_if_fail(REPLAY_IS_APPLICATION_ACTIVATABLE(self));

  iface = REPLAY_APPLICATION_ACTIVATABLE_GET_IFACE(self);

  if (iface->activate != NULL) {
    iface->activate(self);
  }
}

/**
 * replay_application_activatable_deactivate:
 * @self: self
 */
void
replay_application_activatable_deactivate(ReplayApplicationActivatable *self)
{
  ReplayApplicationActivatableInterface *iface;

  g_return_if_fail(REPLAY_IS_APPLICATION_ACTIVATABLE(self));

  iface = REPLAY_APPLICATION_ACTIVATABLE_GET_IFACE(self);

  if (iface->deactivate != NULL) {
    iface->deactivate(self);
  }
}
