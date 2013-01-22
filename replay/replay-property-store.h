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

#ifndef __REPLAY_PROPERTY_STORE_H__
#define __REPLAY_PROPERTY_STORE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define REPLAY_TYPE_PROPERTY_STORE            (replay_property_store_get_type())
#define REPLAY_PROPERTY_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REPLAY_TYPE_PROPERTY_STORE, ReplayPropertyStore))
#define REPLAY_PROPERTY_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REPLAY_TYPE_PROPERTY_STORE, ReplayPropertyStoreClass))
#define REPLAY_IS_PROPERTY_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REPLAY_TYPE_PROPERTY_STORE))
#define REPLAY_IS_PROPERTY_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REPLAY_TYPE_PROPERTY_STORE))
#define REPLAY_PROPERTY_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REPLAY_TYPE_PROPERTY_STORE, ReplayPropertyStoreClass))

typedef struct _ReplayPropertyStore        ReplayPropertyStore;
typedef struct _ReplayPropertyStoreClass   ReplayPropertyStoreClass;
typedef struct _ReplayPropertyStorePrivate ReplayPropertyStorePrivate;

struct _ReplayPropertyStore
{
  GObject parent;

  /*< private >*/
  ReplayPropertyStorePrivate *priv;
};

struct _ReplayPropertyStoreClass
{
  /*< private >*/
  GObjectClass parent_class;
};

GType replay_property_store_get_type(void);
ReplayPropertyStore *replay_property_store_new(void);
void replay_property_store_push(ReplayPropertyStore *self,
                             GVariant *variant);
void replay_property_store_pop(ReplayPropertyStore *self,
                            GVariant *variant);
GVariant *replay_property_store_lookup(ReplayPropertyStore *self,
                                    const gchar *name);
GVariant *replay_property_store_get_props(ReplayPropertyStore *self);

void replay_property_store_clear(ReplayPropertyStore *self);

G_END_DECLS

#endif /* __REPLAY_PROPERTY_STORE_H__ */
