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
 * SECTION:replay-property-store
 * @short_description: Stores a collection of #GVariant dictionaries
 * @see_also: #GVariant
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-property-store.h>
#include <glib.h>
#include <stdio.h>

G_DEFINE_TYPE(ReplayPropertyStore, replay_property_store, G_TYPE_OBJECT);

enum {
  PROP_PROPS = 1,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _ReplayPropertyStorePrivate
{
  GVariant *props;
  GList *variants;
};

static void
replay_property_store_get_property(GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  ReplayPropertyStore *self = REPLAY_PROPERTY_STORE(object);

  switch (prop_id) {
    case PROP_PROPS:
      g_value_set_variant(value, replay_property_store_get_props(self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
update_props(ReplayPropertyStore *self)
{
  ReplayPropertyStorePrivate *priv = self->priv;
  GVariant *props = NULL;

  if (priv->variants)
  {
    GList *list = NULL;
    GVariantBuilder builder;
    GHashTable *keys = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    /* add each variant from the dictionaries into the builder to create a
       top-level version - make sure each key is unique */
    for (list = priv->variants; list != NULL; list = list->next)
    {
      GVariant *dict = (GVariant *)list->data;
      GVariantIter iter;
      gchar *key;
      GVariant *value;

      g_variant_iter_init(&iter, dict);
      while (g_variant_iter_loop(&iter, "{sv}", &key, &value))
      {
        if (!g_hash_table_lookup(keys, key))
        {
          gchar *copy = g_strdup(key);
          g_variant_builder_add(&builder, "{sv}", key, value);
          g_hash_table_insert(keys, copy, copy);
        }
      }
    }
    g_hash_table_destroy(keys);
    props = g_variant_ref_sink(g_variant_builder_end(&builder));
  }
  else
  {
    props = g_variant_ref_sink(g_variant_new("a{sv}", NULL));
  }
  if (priv->props) {
    g_variant_unref(priv->props);
  }
  priv->props = props;
  g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_PROPS]);
}

static void
replay_property_store_init(ReplayPropertyStore *object)
{
  ReplayPropertyStore *self;
  ReplayPropertyStorePrivate *priv;

  g_return_if_fail(REPLAY_IS_PROPERTY_STORE(object));

  self = REPLAY_PROPERTY_STORE(object);
  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                                  REPLAY_TYPE_PROPERTY_STORE,
                                                  ReplayPropertyStorePrivate);
  /* GVariant dictionaries are inserted in a list */
  priv->variants = NULL;
  update_props(self);
}

static void
replay_property_store_finalize(GObject *object)
{
  ReplayPropertyStore *self;

  g_return_if_fail(REPLAY_IS_PROPERTY_STORE(object));

  self = REPLAY_PROPERTY_STORE(object);

  g_list_free_full(self->priv->variants, (GDestroyNotify)g_variant_unref);
  self->priv->variants = NULL;

  G_OBJECT_CLASS(replay_property_store_parent_class)->finalize(object);
}

static void
replay_property_store_class_init(ReplayPropertyStoreClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = replay_property_store_get_property;
  object_class->finalize = replay_property_store_finalize;

  properties[PROP_PROPS] = g_param_spec_variant("props",
                                                "props",
                                                "A #GVariant of the top-level properties",
                                                G_VARIANT_TYPE_VARDICT,
                                                NULL,
                                                G_PARAM_READABLE);
  g_object_class_install_property(object_class, PROP_PROPS, properties[PROP_PROPS]);
  g_type_class_add_private(klass, sizeof(ReplayPropertyStorePrivate));
}

ReplayPropertyStore *
replay_property_store_new(void)
{
  return g_object_new(REPLAY_TYPE_PROPERTY_STORE, NULL);
}

void
replay_property_store_push(ReplayPropertyStore *self,
                        GVariant *prop)
{
  g_return_if_fail(REPLAY_IS_PROPERTY_STORE(self));
  g_return_if_fail(prop != NULL);

  self->priv->variants = g_list_prepend(self->priv->variants, g_variant_ref_sink(prop));
  update_props(self);
}

void
replay_property_store_pop(ReplayPropertyStore *self,
                       GVariant *prop)
{
  GList *item;

  g_return_if_fail(REPLAY_IS_PROPERTY_STORE(self));
  g_return_if_fail(prop != NULL);

  item = g_list_find(self->priv->variants, prop);
  if (item)
  {
    /* drop our reference to prop which the list contained */
    self->priv->variants = g_list_delete_link(self->priv->variants, item);
    g_variant_unref(prop);
    update_props(self);
  }
}

/**
 * replay_property_store_lookup:
 * @self: A #ReplayPropertyStore
 * @name: The name of a property to lookup
 *
 * Searches for the top-most #GVariant with the given @name, returning a
 * reference to the property if found. If the property with the given @name does
 * not exist in @self, NULL is returned.

 * Returns: (transfer full): A reference to #GVariant if found, otherwise NULL.
 */
GVariant *
replay_property_store_lookup(ReplayPropertyStore *self,
                          const gchar *name)
{
  GVariant *prop = NULL;

  g_return_val_if_fail(REPLAY_IS_PROPERTY_STORE(self), NULL);
  g_return_val_if_fail(name != NULL, NULL);

  if (self->priv->props)
  {
    prop = g_variant_lookup_value(self->priv->props, name, NULL);
  }
  return prop;
}

/**
 * replay_property_store_get_props:
 * @self: A #ReplayPropertyStore
 *
 * A #GVariant containing all of the top-level properties within this
 * #ReplayPropertyStore
 *
 * Returns: (transfer none): a #GVariant dictionary containing the properties
 * inside the store.
 */
GVariant *
replay_property_store_get_props(ReplayPropertyStore *self)
{
  g_return_val_if_fail(REPLAY_IS_PROPERTY_STORE(self), NULL);
  return self->priv->props;
}

void
replay_property_store_clear(ReplayPropertyStore *self)
{
  g_return_if_fail(REPLAY_IS_PROPERTY_STORE(self));
  g_list_free_full(self->priv->variants, (GDestroyNotify)g_variant_unref);
  self->priv->variants = NULL;
  update_props(self);
}
