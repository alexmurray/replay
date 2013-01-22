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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <math.h>
#include <replay/replay-events.h>
#include <replay/replay-event-processor.h>
#include <replay/replay-event-source.h>

#define REPLAY_EVENT_PROCESSOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_EVENT_PROCESSOR, ReplayEventProcessorPrivate))

/* function definitions */
static void replay_event_processor_class_init(ReplayEventProcessorClass *klass);

static void replay_event_processor_init(ReplayEventProcessor *replay_event_processor);

static void replay_event_processor_finalize(GObject *object);

/* error handling */
#define REPLAY_EVENT_PROCESSOR_ERROR replay_event_processor_error_quark()

static GQuark replay_event_processor_error_quark(void)
{
  return g_quark_from_static_string("replay-event-processor-error-quark");
}

/* different error codes */
enum {
  INVALID_EVENT_TYPE_ERROR = 0,
  INVALID_EVENT_TIMESTAMP_ERROR,
  INVALID_NAME_ERROR,
  INVALID_ACTIVITY_ERROR,
  INVALID_PROPERTY_ERROR,
};

typedef struct _Node
{
  gchar *name;
  ReplayNodeCreateEvent *create;
  ReplayEventInterval *create_interval;
  ReplayEventInterval *color_interval;
  /* store each of the activity intervals in a hash table */
  GHashTable *activities;
  /* we keep edge add events in a hash table so we can autogenerate edge remove
   * events for any remaining edges when the node is deleted for consistency
   * when running backwards */
  GHashTable *edges;
} Node;

typedef struct _Edge
{
  gchar *name;
  ReplayEdgeCreateEvent *add;
} Edge;

struct _ReplayEventProcessorPrivate
{
  ReplayEventStore *event_store;
  ReplayNodeTree *node_tree;
  ReplayIntervalList *interval_list;
  ReplayMessageTree *message_tree;

  GHashTable *nodes;
  GHashTable *edges;
  GHashTable *msgs;
  gint64 timestamp;
};


/* define ReplayEventProcessor as inheriting from ReplayEventSource */
G_DEFINE_TYPE(ReplayEventProcessor, replay_event_processor, REPLAY_TYPE_EVENT_SOURCE);


static void replay_event_processor_class_init(ReplayEventProcessorClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  /* override parent methods */
  obj_class->finalize = replay_event_processor_finalize;

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayEventProcessorPrivate));
}

static Node *node_new(ReplayNodeCreateEvent *event)
{
  Node *node = g_slice_new0(typeof(*node));
  node->create = g_object_ref(event);
  node->name = g_strdup(replay_node_create_event_get_id(event));
  node->activities = g_hash_table_new_full(g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           (GDestroyNotify)replay_event_interval_unref);
  node->edges = g_hash_table_new_full(g_str_hash,
                                      g_str_equal,
                                      g_free,
                                      (GDestroyNotify)g_object_unref);
  return node;
}

static void node_free(Node *node)
{
  g_object_unref(node->create);
  replay_event_interval_unref(node->create_interval);
  if (node->color_interval)
  {
    replay_event_interval_unref(node->color_interval);
    node->color_interval = NULL;
  }
  g_free(node->name);
  g_hash_table_destroy(node->activities);
  g_hash_table_destroy(node->edges);
  g_slice_free(typeof(*node), node);
}

static Edge *edge_new(ReplayEvent *event)
{
  Edge *edge = g_slice_new0(typeof(*edge));
  edge->add = g_object_ref(event);
  edge->name = g_strdup(replay_edge_create_event_get_id(REPLAY_EDGE_CREATE_EVENT(event)));
  return edge;
}

static void edge_free(Edge *edge)
{
  g_object_unref(edge->add);
  g_free(edge->name);
  g_slice_free(typeof(*edge), edge);
}

static void replay_event_processor_init(ReplayEventProcessor *self)
{
  ReplayEventProcessorPrivate *priv;

  self->priv = priv = REPLAY_EVENT_PROCESSOR_GET_PRIVATE(self);

  /* we keep a Node structure for each node in a hash table - free Nodes when
   * destroying table */
  priv->nodes = g_hash_table_new_full(g_str_hash, g_str_equal,
                                      NULL, (GDestroyNotify)node_free);
  /* we keep a Edge structure for each node in a hash table - free Edges when
   * destroying table */
  priv->edges = g_hash_table_new_full(g_str_hash, g_str_equal,
                                      NULL, (GDestroyNotify)edge_free);
  /* we keep copies of message names from msg send events along with their
   * associated interval in hash table to check for uniqueness - free names
   * and intervals when destroying table */
  priv->msgs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                     g_free,
                                     (GDestroyNotify)replay_event_interval_unref);
}

static void replay_event_processor_finalize(GObject *object)
{
  ReplayEventProcessor *self;
  ReplayEventProcessorPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_PROCESSOR(object));

  self = REPLAY_EVENT_PROCESSOR(object);
  priv = REPLAY_EVENT_PROCESSOR_GET_PRIVATE(self);

  if (priv->event_store)
  {
    g_object_unref(priv->event_store);
  }
  if (priv->node_tree)
  {
    g_object_unref(priv->node_tree);
  }
  if (priv->interval_list)
  {
    g_object_unref(priv->interval_list);
  }
  if (priv->message_tree)
  {
    g_object_unref(priv->message_tree);
  }

  g_hash_table_destroy(priv->nodes);
  g_hash_table_destroy(priv->edges);
  g_hash_table_destroy(priv->msgs);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_event_processor_parent_class)->finalize(object);
}

ReplayEventProcessor*
replay_event_processor_new(ReplayEventStore *event_store,
                        ReplayNodeTree *node_tree,
                        ReplayIntervalList *interval_list,
                        ReplayMessageTree *message_tree)
{
  ReplayEventProcessor *self;
  ReplayEventProcessorPrivate *priv;

  g_return_val_if_fail((event_store != NULL && node_tree != NULL &&
                        interval_list != NULL && message_tree != NULL),
                       NULL);

  self = g_object_new(REPLAY_TYPE_EVENT_PROCESSOR,
                      "name", _("Event Processor"),
                      NULL);

  priv = self->priv;

  priv->event_store = g_object_ref(event_store);
  priv->node_tree = g_object_ref(node_tree);
  priv->interval_list = g_object_ref(interval_list);
  priv->message_tree = g_object_ref(message_tree);

  return self;
}

static gboolean validate_name(const gchar *name, GError **error)
{
  gboolean ret = FALSE;

  if (name)
  {
    if (g_ascii_strcasecmp(name, ""))
    {
      /* also check name is valid pango markup as we could try and display it
       * later */
      ret = pango_parse_markup(name, -1, 0, NULL, NULL, NULL, error);
    }
    else
    {
      g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR, INVALID_NAME_ERROR,
                  _("Name is empty string"));
    }
  }
  else
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR, INVALID_NAME_ERROR,
                _("Name is NULL"));
  }
  return ret;
}

static gboolean validate_color(const gchar *color, GError **error)
{
  gboolean ret = FALSE;

  if (color)
  {
    if (g_ascii_strcasecmp(color, ""))
    {
      GdkRGBA rgba;
      ret = gdk_rgba_parse(&rgba, color);
      if (!ret)
      {
        g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR, INVALID_PROPERTY_ERROR,
                    _("Unable to parse color '%s'"), color);
      }
    }
    else
    {
      g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR, INVALID_PROPERTY_ERROR,
                  _("Color is empty string"));
    }
  }
  else
  {
    /* allow a NULL color */
    ret = TRUE;
  }
  return ret;
}

static gboolean validate_markup(const gchar *markup, GError **error)
{
  /* assume is fine unless is non-NULL so check for valid pango markup */
  gboolean ret = TRUE;

  /* check markup is valid pango markup so we can display it later */
  if (markup)
  {
    ret = pango_parse_markup(markup, -1, 0, NULL, NULL, NULL, error);
  }
  return ret;
}

static gboolean
node_exists(ReplayEventProcessor *self,
            const gchar *name)
{
  ReplayEventProcessorPrivate *priv;

  priv = self->priv;

  return (g_hash_table_lookup(priv->nodes, name) != NULL);
}

static gboolean
validate_node_exists(ReplayEventProcessor *self,
                     const gchar *name,
                     GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = validate_name(name, error)))
  {
    goto error;
  }
  if (!(ret = node_exists(self, name)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_NAME_ERROR,
                _("Node '%s' does not exist"),
                name);
  }
error:
  return ret;
}

static gboolean
validate_node_does_not_exist(ReplayEventProcessor *self,
                             const gchar *name,
                             GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = validate_name(name, error)))
  {
    goto error;
  }
  if (!(ret = !node_exists(self, name)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_NAME_ERROR,
                _("Node '%s' already exists"),
                name);
  }
error:
  return ret;
}

static gboolean
activity_exists(ReplayEventProcessor *self,
                const gchar *node_name,
                const gchar *name)
{
  ReplayEventProcessorPrivate *priv;
  Node *node;

  priv = self->priv;

  node = (Node *)g_hash_table_lookup(priv->nodes, node_name);
  return (g_hash_table_lookup(node->activities, name) != NULL);
}

static gboolean
validate_activity_exists(ReplayEventProcessor *self,
                         const gchar *node_name,
                         const gchar *name,
                         GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = validate_name(name, error)))
  {
    goto error;
  }
  if (!(ret = activity_exists(self, node_name, name)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_ACTIVITY_ERROR,
                _("Activity '%s' does not exist for node '%s'"),
                name, node_name);
  }
error:
  return ret;
}

static gboolean
validate_activity_does_not_exist(ReplayEventProcessor *self,
                                 const gchar *node_name,
                                 const gchar *name,
                                 GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = validate_name(name, error)))
  {
    goto error;
  }
  if (!(ret = !activity_exists(self, node_name, name)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_ACTIVITY_ERROR,
                _("Activity '%s' already exists for node '%s'"),
                name, node_name);
  }
error:
  return ret;
}

static gboolean validate_level(gdouble level, GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = (level > 0 && level <= 1.0)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_ACTIVITY_ERROR,
                _("level %f is invalid, must be greater than 0.0 and less than or equal to 1.0"),
                level);
  }

  return ret;
}

static gboolean
edge_exists(ReplayEventProcessor *self,
            const gchar *name)
{
  ReplayEventProcessorPrivate *priv;

  priv = self->priv;

  return (g_hash_table_lookup(priv->edges, name) != NULL);
}

static gboolean
validate_edge_exists(ReplayEventProcessor *self,
                     const gchar *name,
                     GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = validate_name(name, error)))
  {
    goto error;
  }
  if (!(ret = edge_exists(self, name)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_NAME_ERROR,
                _("Edge '%s' does not exist"),
                name);
  }
error:
  return ret;
}

static gboolean
validate_edge_does_not_exist(ReplayEventProcessor *self,
                             const gchar *name,
                             GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = validate_name(name, error)))
  {
    goto error;
  }
  if (!(ret = !edge_exists(self, name)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_NAME_ERROR,
                _("Edge '%s' already exists"),
                name);
  }
error:
  return ret;
}

static gboolean
msg_exists(ReplayEventProcessor *self,
           const gchar *name)
{
  ReplayEventProcessorPrivate *priv;

  priv = self->priv;

  return (g_hash_table_lookup(priv->msgs, name) != NULL);
}

static gboolean
validate_msg_does_not_exist(ReplayEventProcessor *self,
                            const gchar *name,
                            GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = validate_name(name, error)))
  {
    goto error;
  }
  if (!(ret = !msg_exists(self, name)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_NAME_ERROR,
                _("Message '%s' already exists"),
                name);
  }
error:
  return ret;
}

static gboolean
validate_msg_exists(ReplayEventProcessor *self,
                    const gchar *name,
                    GError **error)
{
  gboolean ret = FALSE;

  if (!(ret = validate_name(name, error)))
  {
    goto error;
  }
  if (!(ret = msg_exists(self, name)))
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                INVALID_NAME_ERROR,
                _("Message '%s' must already exist"),
                name);
  }
error:
  return ret;
}

static gboolean
validate_prop(const gchar *name,
              GVariant *prop,
              GError **error)
{
  gboolean invalid = FALSE;

  /* we only validate a couple different property types, namely 'label' (which
   * has to be a valid string) and 'color' which has to be a valid string
   * describing a color */
  if (g_ascii_strcasecmp(name, "label") == 0 ||
      g_ascii_strcasecmp(name, "description") == 0)
  {

    if (!g_variant_type_equal(g_variant_get_type(prop), G_VARIANT_TYPE_STRING))
    {
      g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                  INVALID_PROPERTY_ERROR,
                  _("%s property must be of type G_TYPE_STRING"),
                  name);
      invalid = TRUE;
      goto error;
    }

    if (!validate_markup(g_variant_get_string(prop, NULL), error))
    {
      gchar *message = (*error)->message;
      /* escape markup so can still be used even though is invalid */
      gchar *esc_markup = g_markup_escape_text(g_variant_get_string(prop, NULL),
                                               -1);

      /* error now contains a copy of the invalid string which makes the error
         message itself invalid markup so have to replace error message */
      (*error)->message = g_strdup_printf("Property '%s' is not valid Pango markup: %s",
                                          name, esc_markup);
      g_free(message);
      invalid = TRUE;
      goto error;
    }
  }
  else if (g_ascii_strcasecmp(name, "color") == 0)
  {
    const gchar *color;
    if (!g_variant_type_equal(g_variant_get_type(prop), G_VARIANT_TYPE_STRING))
    {
      g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                  INVALID_PROPERTY_ERROR,
                  _("color property must be of type G_TYPE_STRING"));
      invalid = TRUE;
      goto error;
    }
    color = g_variant_get_string(prop, NULL);
    if (!validate_color(color, error))
    {
      invalid = TRUE;
      goto error;
    }
  }
  else if (g_ascii_strcasecmp(name, "level") == 0)
  {
    gdouble level;
    if (!g_variant_type_equal(g_variant_get_type(prop), G_VARIANT_TYPE_DOUBLE))
    {
      g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR,
                  INVALID_PROPERTY_ERROR,
                  _("level property must be of type G_TYPE_DOUBLE"));
      invalid = TRUE;
      goto error;
    }
    level = g_variant_get_double(prop);
    if (!validate_level(level, error))
    {
      invalid = TRUE;
      goto error;
    }
  }

  error:
  return !invalid;
}

static gboolean
validate_props(ReplayEventProcessor *self,
               GVariant *props,
               GError **error)
{
  GVariantIter iter;
  gchar *key;
  GVariant *value;

  g_variant_iter_init(&iter, props);
  while (g_variant_iter_loop(&iter, "{sv}", &key, &value))
  {
    gboolean ret = validate_prop(key, value, error);
    if (!ret)
    {
      // manually unref key and value since we are breaking out of while loop
      // - otherwise g_variant_iter_loop will unref for us
      g_free(key);
      g_variant_unref(value);
      break;
    }
  }
  return (*error == NULL);
}

static gboolean
validate_event(ReplayEventProcessor *self,
               ReplayEvent *event,
               GError **error)
{
  ReplayEventProcessorPrivate *priv;
  const gchar *name, *node;
  GVariant *props;
  gboolean ret = FALSE;

  priv = self->priv;

  if (replay_event_get_timestamp(event) < priv->timestamp)
  {
    g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR, INVALID_EVENT_TIMESTAMP_ERROR,
                _("Invalid event: Timestamp is prior to last event"));
    goto error;
  }

  if (REPLAY_IS_NODE_CREATE_EVENT(event)) {
      name = replay_node_create_event_get_id(REPLAY_NODE_CREATE_EVENT(event));

      /* node must not exist already if this is a create event */
      if (!(ret = validate_node_does_not_exist(self, name, error)))
      {
        g_prefix_error(error, _("Invalid node create event: "));
        goto error;
      }
      props = replay_node_create_event_get_props(REPLAY_NODE_CREATE_EVENT(event));
      if (!(ret = validate_props(self, props, error)))
      {
        g_prefix_error(error, _("Invalid node properties event: "));
        goto error;
      }
  } else if (REPLAY_IS_NODE_DELETE_EVENT(event)) {
      name = replay_node_delete_event_get_id(REPLAY_NODE_DELETE_EVENT(event));

      /* node must exist already if this is a delete event */
      if (!(ret = validate_node_exists(self, name, error)))
      {
        g_prefix_error(error, _("Invalid node delete event: "));
        goto error;
      }
  } else if (REPLAY_IS_EDGE_CREATE_EVENT(event)) {
      name = replay_edge_create_event_get_id(REPLAY_EDGE_CREATE_EVENT(event));

      /* edge must not exist already if this is a create event */
      if (!(ret = validate_edge_does_not_exist(self, name, error)))
      {
        g_prefix_error(error, _("Invalid edge create event: "));
        goto error;
      }
      name = replay_edge_create_event_get_tail(REPLAY_EDGE_CREATE_EVENT(event));
      if (!(ret = validate_node_exists(self, name, error)))
      {
        g_prefix_error(error, _("Invalid edge create event: Tail node: "));
        goto error;
      }
  } else if (REPLAY_IS_EDGE_DELETE_EVENT(event)) {
      name = replay_edge_delete_event_get_id(REPLAY_EDGE_DELETE_EVENT(event));

      /* edge must exist already if this is a delete event */
      if (!(ret = validate_edge_exists(self, name, error)))
      {
        g_prefix_error(error, _("Invalid edge delete event: "));
        goto error;
      }
  } else if (REPLAY_IS_NODE_PROPS_EVENT(event)) {
      name = replay_node_props_event_get_id(REPLAY_NODE_PROPS_EVENT(event));

      if (!(ret = validate_node_exists(self, name, error)))
      {
        g_prefix_error(error, _("Invalid node properties event: "));
        goto error;
      }
      props = replay_node_props_event_get_props(REPLAY_NODE_PROPS_EVENT(event));
      if (!(ret = validate_props(self, props, error)))
      {
        g_prefix_error(error, _("Invalid node properties event: "));
        goto error;
      }
  } else if (REPLAY_IS_ACTIVITY_START_EVENT(event)) {
      node = replay_activity_start_event_get_node(REPLAY_ACTIVITY_START_EVENT(event));
      name = replay_activity_start_event_get_id(REPLAY_ACTIVITY_START_EVENT(event));
      props = replay_activity_start_event_get_props(REPLAY_ACTIVITY_START_EVENT(event));

      if (!(ret = validate_node_exists(self, node, error)))
      {
        g_prefix_error(error, _("Invalid activity start event: "));
        goto error;
      }
      if (!(ret = validate_activity_does_not_exist(self, node, name, error)))
      {
        g_prefix_error(error, _("Invalid activity start event: "));
        goto error;
      }
      if (!(ret = validate_props(self, props, error)))
      {
        g_prefix_error(error, _("Invalid node properties event: "));
        goto error;
      }
  } else if (REPLAY_IS_ACTIVITY_END_EVENT(event)) {
      node = replay_activity_end_event_get_node(REPLAY_ACTIVITY_END_EVENT(event));
      name = replay_activity_end_event_get_id(REPLAY_ACTIVITY_END_EVENT(event));

      if (!(ret = validate_node_exists(self, node, error)))
      {
        g_prefix_error(error, _("Invalid activity end event: "));
        goto error;
      }
      if (!(ret = validate_activity_exists(self, node, name, error)))
      {
        g_prefix_error(error, _("Invalid activity end event: "));
        goto error;
      }
  } else if (REPLAY_IS_EDGE_PROPS_EVENT(event)) {
      name = replay_edge_props_event_get_id(REPLAY_EDGE_PROPS_EVENT(event));
      if (!(ret = validate_edge_exists(self, name, error)))
      {
        g_prefix_error(error, _("Invalid edge properties event: "));
        goto error;
      }
      props = replay_edge_props_event_get_props(REPLAY_EDGE_PROPS_EVENT(event));
      if (!(ret = validate_props(self, props, error)))
      {
        g_prefix_error(error, _("Invalid edge properties event: "));
        goto error;
      }
  } else if (REPLAY_IS_MSG_SEND_EVENT(event)) {
      name = replay_msg_send_event_get_id(REPLAY_MSG_SEND_EVENT(event));

      /* can't send multiple messages with same name (unless we recycle them
       * after receiving */
      if (!(ret = validate_msg_does_not_exist(self, name, error)))
      {
        g_prefix_error(error, _("Invalid message send event: "));
        goto error;
      }

      /* check node exists */
      name = replay_msg_send_event_get_node(REPLAY_MSG_SEND_EVENT(event));
      if (!(ret = validate_node_exists(self, name, error)))
      {
        g_prefix_error(error, _("Invalid message send event: "));
        goto error;
      }
      /* check edge exists - but can be NULL */
      name = replay_msg_send_event_get_edge(REPLAY_MSG_SEND_EVENT(event));
      if (name != NULL)
      {
        if (!(ret = validate_edge_exists(self, name, error)))
        {
          g_prefix_error(error, _("Invalid message send event: "));
          goto error;
        }
      }
      props = replay_msg_send_event_get_props(REPLAY_MSG_SEND_EVENT(event));
      if (!(ret = validate_props(self, props, error)))
      {
        g_prefix_error(error, _("Invalid message send event: "));
        goto error;
      }
  } else if (REPLAY_IS_MSG_RECV_EVENT(event)) {
      name = replay_msg_recv_event_get_id(REPLAY_MSG_RECV_EVENT(event));

      /* can't receive a msg which we have not yet sent */
      if (!(ret = validate_msg_exists(self, name, error)))
      {
        g_prefix_error(error, _("Invalid message recv event: "));
        goto error;
      }
      /* check node exists */
      name = replay_msg_recv_event_get_node(REPLAY_MSG_RECV_EVENT(event));
      if (!(ret = validate_node_exists(self, name, error)))
      {
        g_prefix_error(error, _("Invalid message recv event: "));
        goto error;
      }
  } else {
      g_set_error(error, REPLAY_EVENT_PROCESSOR_ERROR, INVALID_EVENT_TYPE_ERROR,
                  _("Invalid event: Unknown type %s"), G_OBJECT_TYPE_NAME(event));
  }

error:
  return ret;
}

gboolean
replay_event_processor_process_event(ReplayEventProcessor *self,
                                  ReplayEvent *event,
                                  GError **error)
{
  ReplayEventProcessorPrivate *priv;
  Node *node;
  Edge *edge;
  gboolean ret = FALSE;
  GtkTreeIter iter;
  ReplayEventInterval *interval;
  GtkTreeIter start_iter, parent_iter, msg_iter;
  gboolean got_iter = FALSE;
  GtkTreePath *path;
  GList *list;
  gchar *name;

  g_return_val_if_fail(REPLAY_IS_EVENT_PROCESSOR(self), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  priv = self->priv;

  ret = validate_event(self, event, error);
  if (!ret)
  {
    goto error;
  }

  /* save timestamp from this event to compare future events against */
  priv->timestamp = replay_event_get_timestamp(event);

  /* actually process the event */
  if (REPLAY_IS_NODE_CREATE_EVENT(event)) {
      node = node_new(REPLAY_NODE_CREATE_EVENT(event));
      g_hash_table_insert(priv->nodes, node->name, node);

      if (!replay_node_tree_get_iter_for_id(priv->node_tree, NULL,
                                         replay_node_create_event_get_id(REPLAY_NODE_CREATE_EVENT(event))))
      {
        if (!(ret = replay_node_tree_insert_child(priv->node_tree,
                                               replay_node_create_event_get_id(REPLAY_NODE_CREATE_EVENT(event)),
                                               replay_node_create_event_get_props(REPLAY_NODE_CREATE_EVENT(event)),
                                               FALSE,
                                               NULL,
                                               -1,
                                               NULL,
                                               error)))
        {
          goto error;
        }
      }

      replay_event_store_append_event(priv->event_store, event, &iter);

      /* create the first color interval if this is setting node color */
      if (g_variant_lookup(replay_node_create_event_get_props(REPLAY_NODE_CREATE_EVENT(event)),
                           "color", "s", NULL))
      {
        node->color_interval = replay_event_interval_new(REPLAY_EVENT_INTERVAL_TYPE_NODE_COLOR,
                                                      &iter,
                                                      replay_event_get_timestamp(event),
                                                      &iter,
                                                      replay_event_get_timestamp(event));
        replay_interval_list_add_interval(priv->interval_list, node->color_interval);
      }

      /* add to interval list */
      node->create_interval = replay_event_interval_new(REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS,
                                                     &iter,
                                                     replay_event_get_timestamp(event),
                                                     &iter,
                                                     replay_event_get_timestamp(event));
      replay_interval_list_add_interval(priv->interval_list, node->create_interval);
  } else if (REPLAY_IS_NODE_DELETE_EVENT(event)) {
      node = g_hash_table_lookup(priv->nodes, replay_node_delete_event_get_id(REPLAY_NODE_DELETE_EVENT(event)));

      /* generate end activity events for any activities */
      for (list = g_hash_table_get_values(node->activities);
           list != NULL;
           list = list->next)
      {
        ReplayEvent *start_event;
        ReplayEvent *end_event;
        gchar *source;

        interval = (ReplayEventInterval *)list->data;

        replay_event_interval_get_start(interval, &start_iter, NULL);
        start_event = replay_event_store_get_event(priv->event_store,
                                                &start_iter);
        source = g_strdup_printf(_("Automatically generated by Replay source code %s: %d"), __FILE__, __LINE__);
        /* initialise with start event data */
        end_event = replay_activity_end_event_new(replay_event_get_timestamp(event), source,
                                               replay_activity_start_event_get_id(REPLAY_ACTIVITY_START_EVENT(start_event)),
                                               replay_activity_start_event_get_node(REPLAY_ACTIVITY_START_EVENT(start_event)));
        replay_event_source_emit_event(REPLAY_EVENT_SOURCE(self), end_event);
        g_object_unref(end_event);
        g_object_unref(start_event);
        g_free(source);
      }

      /* generate edge delete events for any remaining edges */
      for (list = g_hash_table_get_values(node->edges);
           list != NULL;
           list = list->next)
      {
        ReplayEvent *create_event, *delete_event;
        gchar *source;

        create_event = (ReplayEvent *)list->data;

        source = g_strdup_printf(_("Automatically generated by Replay source code %s: %d"), __FILE__, __LINE__);
        delete_event = replay_edge_delete_event_new(replay_event_get_timestamp(event), source,
                                                 replay_edge_create_event_get_id(REPLAY_EDGE_CREATE_EVENT(create_event)));
        replay_edge_delete_event_set_create_event(REPLAY_EDGE_DELETE_EVENT(delete_event),
                                               REPLAY_EDGE_CREATE_EVENT(create_event));
        replay_event_source_emit_event(REPLAY_EVENT_SOURCE(self), delete_event);
        g_object_unref(delete_event);
        g_free(source);
      }

      replay_event_store_append_event(priv->event_store, event, &iter);

      /* set the end of the color interval as this event */
      if (node->color_interval)
      {
        replay_interval_list_set_interval_end(priv->interval_list,
                                              node->color_interval,
                                              &iter,
                                              replay_event_get_timestamp(event));
        replay_event_interval_unref(node->color_interval);
        node->color_interval = NULL;
      }

      replay_interval_list_set_interval_end(priv->interval_list,
                                         node->create_interval,
                                         &iter,
                                         replay_event_get_timestamp(event));
      g_hash_table_remove(priv->nodes, replay_node_delete_event_get_id(REPLAY_NODE_DELETE_EVENT(event)));
      ret = TRUE;
  } else if (REPLAY_IS_NODE_PROPS_EVENT(event)) {
      replay_event_store_append_event(priv->event_store, event, &iter);

      node = g_hash_table_lookup(priv->nodes, replay_node_props_event_get_id(REPLAY_NODE_PROPS_EVENT(event)));

      /* create the first color interval if this is setting node color */
      if (g_variant_lookup(replay_node_props_event_get_props(REPLAY_NODE_PROPS_EVENT(event)),
                           "color", "s", NULL))
      {
        if (node->color_interval)
        {
          replay_interval_list_set_interval_end(priv->interval_list,
                                             node->color_interval,
                                             &iter,
                                             replay_event_get_timestamp(event));
          replay_event_interval_unref(node->color_interval);
          node->color_interval = NULL;
        }
        /* and create a new color interval for this color */
        node->color_interval = replay_event_interval_new(REPLAY_EVENT_INTERVAL_TYPE_NODE_COLOR,
                                                      &iter,
                                                      replay_event_get_timestamp(event),
                                                      &iter,
                                                      replay_event_get_timestamp(event));
        replay_interval_list_add_interval(priv->interval_list, node->color_interval);
      }

      ret = TRUE;
  } else if (REPLAY_IS_ACTIVITY_START_EVENT(event)) {
      replay_event_store_append_event(priv->event_store, event, &iter);

      /* create a new interval and add to interval list */
      node = g_hash_table_lookup(priv->nodes, replay_activity_start_event_get_node(REPLAY_ACTIVITY_START_EVENT(event)));
      interval = replay_event_interval_new(REPLAY_EVENT_INTERVAL_TYPE_NODE_ACTIVITY,
                                          &iter,
                                          replay_event_get_timestamp(event),
                                          &iter,
                                          replay_event_get_timestamp(event));
      replay_interval_list_add_interval(priv->interval_list,
                                     interval);
      /* give reference to hash table */
      g_hash_table_insert(node->activities,
                          g_strdup(replay_activity_start_event_get_id(REPLAY_ACTIVITY_START_EVENT(event))),
                          interval);
      ret = TRUE;
  } else if (REPLAY_IS_ACTIVITY_END_EVENT(event)) {
    ReplayActivityStartEvent *start_event;
      replay_event_store_append_event(priv->event_store, event, &iter);

      node = g_hash_table_lookup(priv->nodes, replay_activity_end_event_get_node(REPLAY_ACTIVITY_END_EVENT(event)));
      interval = g_hash_table_lookup(node->activities, replay_activity_end_event_get_id(REPLAY_ACTIVITY_END_EVENT(event)));
      /* interval must exist since we validated it earlier */

      /* set the start event */
      replay_event_interval_get_start(interval, &start_iter, NULL);
      start_event = REPLAY_ACTIVITY_START_EVENT(replay_event_store_get_event(priv->event_store, &start_iter));
      replay_activity_end_event_set_start_event(REPLAY_ACTIVITY_END_EVENT(event), start_event);
      g_object_unref(start_event);

      /* set this as the end of this interval */
      replay_interval_list_set_interval_end(priv->interval_list,
                                         interval, &iter,
                                         replay_event_get_timestamp(event));
      g_hash_table_remove(node->activities, replay_activity_end_event_get_id(REPLAY_ACTIVITY_END_EVENT(event)));
      ret = TRUE;
  } else if (REPLAY_IS_MSG_SEND_EVENT(event)) {
      replay_event_store_append_event(priv->event_store, event, &iter);
      path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                     &iter);
      /* try get iter for parent */
      got_iter = replay_message_tree_get_iter_for_name(priv->message_tree,
                                                    &parent_iter,
                                                    replay_msg_send_event_get_parent(REPLAY_MSG_SEND_EVENT(event)));
      name = g_strdup(replay_msg_send_event_get_id(REPLAY_MSG_SEND_EVENT(event)));
      /* add to interval list */
      interval = replay_event_interval_new(REPLAY_EVENT_INTERVAL_TYPE_MESSAGE_PASS,
                                        &iter, replay_event_get_timestamp(event),
                                        &iter, replay_event_get_timestamp(event));
      replay_interval_list_add_interval(priv->interval_list, interval);
      /* store this interval in the msgs hash table */
      g_hash_table_insert(priv->msgs, name, interval);

      replay_message_tree_append_child(priv->message_tree,
                                    got_iter ? &parent_iter : NULL,
                                    name,
                                    &msg_iter);
      replay_message_tree_set_send_event(priv->message_tree,
                                      &msg_iter,
                                      path);
      ret = TRUE;
  } else if (REPLAY_IS_MSG_RECV_EVENT(event)) {
    ReplayMsgSendEvent *send_event;
      replay_event_store_append_event(priv->event_store, event, &iter);
      path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                     &iter);

      /* entry should already exist since we check when validating event */
      replay_message_tree_get_iter_for_name(priv->message_tree,
                                         &msg_iter,
                                         replay_msg_recv_event_get_id(REPLAY_MSG_RECV_EVENT(event)));
      replay_message_tree_set_recv_event(priv->message_tree,
                                      &msg_iter,
                                      path);

      /* set send event from message tree */
      gtk_tree_model_get(GTK_TREE_MODEL(priv->message_tree),
                         &msg_iter,
                         REPLAY_MESSAGE_TREE_COL_MSG_SEND_EVENT, &send_event,
                         -1);
      replay_msg_recv_event_set_send_event(REPLAY_MSG_RECV_EVENT(event), send_event);
      g_object_unref(send_event);
      interval = (ReplayEventInterval *)
        g_hash_table_lookup(priv->msgs, replay_msg_recv_event_get_id(REPLAY_MSG_RECV_EVENT(event)));
      replay_interval_list_set_interval_end(priv->interval_list,
                                         interval, &iter,
                                         replay_event_get_timestamp(event));
      g_hash_table_remove(priv->msgs, replay_msg_recv_event_get_id(REPLAY_MSG_RECV_EVENT(event)));

      ret = TRUE;
  } else if (REPLAY_IS_EDGE_CREATE_EVENT(event)) {
      edge = edge_new(event);
      g_hash_table_insert(priv->edges, edge->name, edge);

      node = g_hash_table_lookup(priv->nodes, replay_edge_create_event_get_tail(REPLAY_EDGE_CREATE_EVENT(event)));
      g_hash_table_insert(node->edges, g_strdup(edge->name),
                          g_object_ref(event));
      replay_event_store_append_event(priv->event_store, event, &iter);
      ret = TRUE;
  } else if (REPLAY_IS_EDGE_DELETE_EVENT(event)) {
    ReplayEdgeCreateEvent *create_event;
      edge = g_hash_table_lookup(priv->edges, replay_edge_delete_event_get_id(REPLAY_EDGE_DELETE_EVENT(event)));

      replay_edge_delete_event_set_create_event(REPLAY_EDGE_DELETE_EVENT(event), edge->add);

      g_hash_table_remove(priv->edges, replay_edge_delete_event_get_id(REPLAY_EDGE_DELETE_EVENT(event)));

      create_event = replay_edge_delete_event_get_create_event(REPLAY_EDGE_DELETE_EVENT(event));
      node = g_hash_table_lookup(priv->nodes, replay_edge_create_event_get_tail(create_event));
      g_hash_table_remove(node->edges, replay_edge_delete_event_get_id(REPLAY_EDGE_DELETE_EVENT(event)));
      replay_event_store_append_event(priv->event_store, event, &iter);
      ret = TRUE;
  } else if (REPLAY_IS_EDGE_PROPS_EVENT(event)) {
      replay_event_store_append_event(priv->event_store, event, &iter);
  } else {
      g_assert_not_reached();
  }

error:
  return ret;
}
