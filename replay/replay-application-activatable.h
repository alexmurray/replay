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

#ifndef __REPLAY_APPLICATION_ACTIVATABLE_H__
#define __REPLAY_APPLICATION_ACTIVATABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_APPLICATION_ACTIVATABLE              (replay_application_activatable_get_type ())
#define REPLAY_APPLICATION_ACTIVATABLE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_APPLICATION_ACTIVATABLE, ReplayApplicationActivatable))
#define REPLAY_APPLICATION_ACTIVATABLE_IFACE(obj)        (G_TYPE_CHECK_CLASS_CAST ((obj), REPLAY_TYPE_APPLICATION_ACTIVATABLE, ReplayApplicationActivatableInterface))
#define REPLAY_IS_APPLICATION_ACTIVATABLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_APPLICATION_ACTIVATABLE))
#define REPLAY_APPLICATION_ACTIVATABLE_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), REPLAY_TYPE_APPLICATION_ACTIVATABLE, ReplayApplicationActivatableInterface))

typedef struct _ReplayApplicationActivatable ReplayApplicationActivatable;
typedef struct _ReplayApplicationActivatableInterface ReplayApplicationActivatableInterface;

struct _ReplayApplicationActivatable
{
  GObject parent;
};

/**
 * ReplayApplicationActivatableInterface:
 * @parent_iface: The parent interface
 * @activate: Called to activate the #ReplayApplicationActivatable
 * @deactivate: Called to deactivate the #ReplayApplicationActivatable
 *
 */
struct _ReplayApplicationActivatableInterface
{
  GTypeInterface parent_iface;

  void (* activate) (ReplayApplicationActivatable *self);
  void (* deactivate) (ReplayApplicationActivatable *self);
};

GType replay_application_activatable_get_type(void);

void replay_application_activatable_activate(ReplayApplicationActivatable *self);
void replay_application_activatable_deactivate(ReplayApplicationActivatable *self);

G_END_DECLS

#endif /* __REPLAY_APPLICATION_ACTIVATABLE_H__ */
