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

#ifndef __REPLAY_WINDOW_ACTIVATABLE_H__
#define __REPLAY_WINDOW_ACTIVATABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_WINDOW_ACTIVATABLE              (replay_window_activatable_get_type ())
#define REPLAY_WINDOW_ACTIVATABLE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_WINDOW_ACTIVATABLE, ReplayWindowActivatable))
#define REPLAY_WINDOW_ACTIVATABLE_IFACE(obj)        (G_TYPE_CHECK_CLASS_CAST ((obj), REPLAY_TYPE_WINDOW_ACTIVATABLE, ReplayWindowActivatableInterface))
#define REPLAY_IS_WINDOW_ACTIVATABLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_WINDOW_ACTIVATABLE))
#define REPLAY_WINDOW_ACTIVATABLE_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), REPLAY_TYPE_WINDOW_ACTIVATABLE, ReplayWindowActivatableInterface))

typedef struct _ReplayWindowActivatable ReplayWindowActivatable;
typedef struct _ReplayWindowActivatableInterface ReplayWindowActivatableInterface;

struct _ReplayWindowActivatable
{
  GObject parent;
};

/**
 * ReplayWindowActivatableInterface:
 * @parent_iface: The parent interface
 * @activate: Called to activate the #ReplayWindowActivatable
 * @deactivate: Called to deactivate the #ReplayWindowActivatable
 *
 */
struct _ReplayWindowActivatableInterface
{
  GTypeInterface parent_iface;

  void (* activate) (ReplayWindowActivatable *self);
  void (* deactivate) (ReplayWindowActivatable *self);
};

GType replay_window_activatable_get_type(void);

void replay_window_activatable_activate(ReplayWindowActivatable *self);
void replay_window_activatable_deactivate(ReplayWindowActivatable *self);

G_END_DECLS

#endif /* __REPLAY_WINDOW_ACTIVATABLE_H__ */
