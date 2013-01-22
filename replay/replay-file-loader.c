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

/* replay-file-loader.c */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-file-loader.h>

/* subclass ReplayEventSource */
G_DEFINE_TYPE(ReplayFileLoader, replay_file_loader, REPLAY_TYPE_EVENT_SOURCE);

struct _ReplayFileLoaderPrivate
{
  gchar *mime_type;
  gchar *pattern;
};

/* properties */
enum
{
  PROP_INVALID,
  PROP_MIME_TYPE,
  PROP_PATTERN
};

enum
{
  PROGRESS,
  CANCEL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

static void
replay_file_loader_get_property(GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ReplayFileLoader *self;
  ReplayFileLoaderPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILE_LOADER(object));

  self = REPLAY_FILE_LOADER(object);
  priv = self->priv;

  switch (property_id) {
    case PROP_MIME_TYPE:
      g_value_set_string(value, priv->mime_type);
      break;

    case PROP_PATTERN:
      g_value_set_string(value, priv->pattern);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_file_loader_set_property(GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ReplayFileLoader *self;
  ReplayFileLoaderPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILE_LOADER(object));

  self = REPLAY_FILE_LOADER(object);
  priv = self->priv;

  switch (property_id) {
    case PROP_MIME_TYPE:
      priv->mime_type = g_value_dup_string(value);
      break;

    case PROP_PATTERN:
      priv->pattern = g_value_dup_string(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_file_loader_finalize(GObject *object)
{
  ReplayFileLoaderPrivate *priv;

  g_return_if_fail(REPLAY_IS_FILE_LOADER(object));

  priv = REPLAY_FILE_LOADER(object)->priv;

  g_free(priv->mime_type);
  g_free(priv->pattern);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_file_loader_parent_class)->finalize(object);
}

static void
dummy_load_file(ReplayFileLoader *self, GFile *file)
{
  return;
}

static void
dummy_cancel(ReplayFileLoader *self)
{
  return;
}

static void
replay_file_loader_class_init(ReplayFileLoaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GParamSpec *spec;

  klass->load_file = dummy_load_file;
  klass->cancel = dummy_cancel;

  object_class->get_property = replay_file_loader_get_property;
  object_class->set_property = replay_file_loader_set_property;
  object_class->finalize     = replay_file_loader_finalize;

  /**
   * ReplayFileLoader::progress:
   * @self: A #ReplayFileLoader
   *
   * The progress signal.
   */
  signals[PROGRESS] = g_signal_new("progress",
                                   REPLAY_TYPE_FILE_LOADER,
                                   G_SIGNAL_RUN_LAST,
                                   G_STRUCT_OFFSET(ReplayFileLoaderClass, progress),
                                   NULL,
                                   NULL,
                                   g_cclosure_marshal_VOID__DOUBLE,
                                   G_TYPE_NONE,
                                   1,
                                   G_TYPE_DOUBLE);

  /**
   * ReplayFileLoader::cancel:
   * @self: A #ReplayFileLoader
   *
   * The cancel signal.
   */
  signals[CANCEL] = g_signal_new("cancel",
                                 REPLAY_TYPE_FILE_LOADER,
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET(ReplayFileLoaderClass, cancel),
                                 NULL,
                                 NULL,
                                 g_cclosure_marshal_VOID__VOID,
                                 G_TYPE_NONE,
                                 0);

  /* mime_type property */
  spec = g_param_spec_string("mime_type",
                             "Mime type of this ReplayFileLoader",
                             "Get mime_type of this file loader",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property(object_class,
                                  PROP_MIME_TYPE,
                                  spec);

  /* pattern property */
  spec = g_param_spec_string("pattern",
                             "Pattern of this ReplayFileLoader",
                             "Get pattern of this file loader",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property(object_class,
                                  PROP_PATTERN,
                                  spec);

  g_type_class_add_private(klass, sizeof(ReplayFileLoaderPrivate));
}

static void
replay_file_loader_init(ReplayFileLoader *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                           REPLAY_TYPE_FILE_LOADER,
                                           ReplayFileLoaderPrivate);
}

/**
 * replay_file_loader_load_file:
 * @self: A #ReplayFileLoader
 * @file: The #GFile to load
 *
 */
void
replay_file_loader_load_file(ReplayFileLoader *self,
                          GFile *file)
{
  gchar *name;

  g_return_if_fail(REPLAY_IS_FILE_LOADER(self));

  /* cancel any existing load */
  replay_file_loader_cancel(self);

  REPLAY_FILE_LOADER_GET_CLASS(self)->load_file(self, file);
  /* set our description based on this file */
  name = g_file_get_parse_name(file);
  replay_event_source_set_description(REPLAY_EVENT_SOURCE(self),
                                   name);
  g_free(name);
}

/**
 * replay_file_loader_get_mime_type:
 * @self: The #ReplayFileLoader
 *
 * Returns: The mime_type of the file loader
 */
const gchar *replay_file_loader_get_mime_type(ReplayFileLoader *self)
{
  g_return_val_if_fail(REPLAY_IS_FILE_LOADER(self), NULL);

  return self->priv->mime_type;
}

/**
 * replay_file_loader_get_pattern:
 * @self: The #ReplayFileLoader
 *
 * Returns: The pattern of the file loader
 */
const gchar *replay_file_loader_get_pattern(ReplayFileLoader *self)
{
  g_return_val_if_fail(REPLAY_IS_FILE_LOADER(self), NULL);

  return self->priv->pattern;
}

/**
 * replay_file_loader_new:
 * @name: A name to describe this file loader
 * @mime_type: (allow-none): A mime-type to describe the file types to load
 * @pattern: (allow-none): A glob style pattern to describe the file types to load
 *
 * Creates a new instance of #ReplayFileLoader.
 *
 * Return value: the newly created #ReplayFileLoader instance
 */
ReplayFileLoader *replay_file_loader_new(const gchar *name,
                                   const gchar *mime_type,
                                   const gchar *pattern)
{
  ReplayFileLoader *self;

  self = g_object_new(REPLAY_TYPE_FILE_LOADER,
                      "name", name,
                      "pattern", pattern,
                      "mime-type", mime_type,
                      NULL);
  return self;
}

void replay_file_loader_emit_progress(ReplayFileLoader *self, gdouble fraction)
{
  g_return_if_fail(REPLAY_IS_FILE_LOADER(self));
  g_signal_emit(self, signals[PROGRESS], 0 , fraction);
}

void replay_file_loader_cancel(ReplayFileLoader *self)
{
  g_return_if_fail(REPLAY_IS_FILE_LOADER(self));
  g_signal_emit(self, signals[CANCEL], 0);
}
