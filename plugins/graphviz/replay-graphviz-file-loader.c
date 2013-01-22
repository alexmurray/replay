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

/* replay-graphviz-file-loader.c */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <graphviz/cgraph.h>
#include <stdio.h>
#include <replay/replay-events.h>
#include "replay-graphviz-file-loader.h"

G_DEFINE_TYPE(ReplayGraphvizFileLoader, replay_graphviz_file_loader, REPLAY_TYPE_FILE_LOADER);

#define DEFAULT_COLOR "#666"

static void impl_load_file(ReplayFileLoader *self, GFile *file);

static void
replay_graphviz_file_loader_class_init(ReplayGraphvizFileLoaderClass *klass)
{
  ReplayFileLoaderClass *loader_class = REPLAY_FILE_LOADER_CLASS(klass);

  loader_class->load_file = impl_load_file;
}

static void
replay_graphviz_file_loader_init(ReplayGraphvizFileLoader *self)
{
  /* nothing to do */
}

static void
process_graph(ReplayGraphvizFileLoader *self, Agraph_t *graph,
              const gchar *filename)
{
  Agnode_t *node;
  ReplayEvent *event;
  guint edge_count = 0;
  guint8 directed;

  g_return_if_fail(graph != NULL);

  /* do all nodes first, then edges */
  for (node = agfstnode(graph); node != NULL; node = agnxtnode(graph, node))
  {
    gchar *name = NULL;
    gchar *color = NULL;
    gchar *label = NULL;
    name = agnameof(node);
    color = agget(node, (char *)"color");

    if (!color || g_ascii_strcasecmp(color, "") == 0)
    {
      color = (gchar *)"#666";
    }
    /* duplicate color */
    color = g_strdup(color);
    label = agget(node, (char *)"label");
    if (!label || g_ascii_strcasecmp(label, "") == 0)
    {
      label = name;
    }
    label = g_strdup(label);
    /* init with standard event stuff */
    event = replay_node_create_event_new((gint64)0, filename, name,
                                      g_variant_new_parsed("{'color': %v, 'label': %v}",
                                                           g_variant_new_string(color),
                                                           g_variant_new_string(label)));
    replay_event_source_emit_event(REPLAY_EVENT_SOURCE(self), event);
    g_object_unref(event);
    g_free(label);
    g_free(color);
  }

  directed = agisdirected(graph);

  /* now do edges */
  for (node = agfstnode(graph); node != NULL; node = agnxtnode(graph, node))
  {
    Agedge_t *edge;

    for (edge = agfstout(graph, node); edge != NULL; edge = agnxtout(graph, edge))
    {
      gchar *name, *head, *tail;
      gchar *color;
      gchar *label = NULL;
      Agnode_t *node2 = aghead(edge);

      /* agnameof may use an internal buffer which gets overwritten between
       * calls so need to take copies of name before passing to replay_event* */
      name = g_strdup(agnameof(edge));
      tail = g_strdup(agnameof(node));
      head = g_strdup(agnameof(node2));

      /* edge name may be empty string - allocate one if needed */
      if (name == NULL || g_ascii_strcasecmp(name, "") == 0)
      {
        g_free(name);
        name = g_strdup_printf("e%u", edge_count++);
      }
      color = agget(edge, (char *)"color");
      if (!color || g_ascii_strcasecmp(color, "") == 0)
      {
        color = (gchar *)"#666";
      }
      color = g_strdup(color);
      label = agget(edge, (char *)"label");
      if (!label || g_ascii_strcasecmp(label, "") == 0)
      {
        label = name;
      }
      label = g_strdup(label);

      /* init with standard event stuff */
      event = replay_edge_create_event_new((gint64)0, filename, name, directed, tail, head,
                                        g_variant_new_parsed("{'color': %v, 'label': %v, 'weight': %v}",
                                                             g_variant_new_string(color),
                                                             g_variant_new_string(label),
                                                             g_variant_new_double(1.0)));
      replay_event_source_emit_event(REPLAY_EVENT_SOURCE(self), event);
      g_object_unref(event);

      g_free(label);
      g_free(color);
      g_free(name);
      g_free(tail);
      g_free(head);
    }
  }
}

static void
impl_load_file(ReplayFileLoader *loader,
               GFile *file)
{
  FILE *graph_file;
  gchar *filename;

  g_return_if_fail(REPLAY_IS_GRAPHVIZ_FILE_LOADER(loader));

  filename = g_file_get_path(file);

  graph_file = fopen(filename, "r");
  if (graph_file)
  {
    Agraph_t *graph = agread(graph_file, NULL);
    if (graph)
    {
      process_graph(REPLAY_GRAPHVIZ_FILE_LOADER(loader), graph, filename);
      replay_file_loader_emit_progress(REPLAY_FILE_LOADER(loader), 1.0);
      agclose(graph);
    }
    else
    {
      gchar *error = g_strdup_printf(_("Error reading graph from file '%s'"), filename);
      replay_event_source_emit_error(REPLAY_EVENT_SOURCE(loader), error);
      g_free(error);
    }
    fclose(graph_file);
  }
  else
  {
    gchar *error = g_strdup_printf(_("Error opening graph file '%s'"), filename);
    replay_event_source_emit_error(REPLAY_EVENT_SOURCE(loader), error);
    g_free(error);
  }

  g_free(filename);

  /* call parent load_file */
  REPLAY_FILE_LOADER_CLASS(replay_graphviz_file_loader_parent_class)->load_file(loader, file);
}

/**
 * replay_graphviz_file_loader_new:
 *
 * Creates a new instance of #ReplayGraphvizFileLoader.
 *
 * Return value: the newly created #ReplayGraphvizFileLoader instance
 */
ReplayFileLoader *replay_graphviz_file_loader_new(void)
{
  ReplayGraphvizFileLoader *self;

  self = g_object_new(REPLAY_TYPE_GRAPHVIZ_FILE_LOADER,
                      "name", _("Graphviz Dot Files"),
                      "pattern", "*.dot",
                      NULL);

  return REPLAY_FILE_LOADER(self);
}
