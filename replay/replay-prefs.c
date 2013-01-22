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
#include <replay/replay.h>
#include <replay/replay-prefs.h>
#include "replay-enums.h"

#define ABSOLUTE_TIME_PATH "absolute-time"
#define LABEL_NODES_PATH "label-nodes"
#define LABEL_EDGES_PATH "label-edges"
#define LABEL_MESSAGES_PATH "label-messages"

/* inherit from GObject */
G_DEFINE_TYPE(ReplayPrefs, replay_prefs, G_TYPE_OBJECT);

enum {
        PROP_TIMELINE_VIEW = 1,
        PROP_EVENT_TEXT_VIEW,
        PROP_MESSAGE_TREE_VIEW,
        PROP_GRAPH_VIEW,
        PROP_ABSOLUTE_TIME,
        PROP_LABEL_NODES,
        PROP_LABEL_EDGES,
        PROP_LABEL_MESSAGES,
        PROP_LAST
};

static GParamSpec *properties[PROP_LAST];
static void replay_prefs_finalize(GObject *object);

struct _ReplayPrefsPrivate
{
        ReplayTimelineView *timeline_view;
        ReplayEventTextView *event_text_view;
        ReplayMessageTreeView *message_tree_view;
        ReplayGraphView *graph_view;

        gboolean absolute_time;
        gboolean label_nodes;
        gboolean label_edges;
        gboolean label_messages;

        /* for storing prefs in gsettings */
        GSettings *settings;
};

#define REPLAY_PREFS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_PREFS, ReplayPrefsPrivate))

/*!
 * GObject related stuff
 */
static void
replay_prefs_get_property(GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
        ReplayPrefs *self = REPLAY_PREFS(object);

        switch (prop_id) {
        case PROP_ABSOLUTE_TIME:
                g_value_set_boolean(value, replay_prefs_get_absolute_time(self));
                break;

        case PROP_LABEL_NODES:
                g_value_set_boolean(value, replay_prefs_get_label_nodes(self));
                break;

        case PROP_LABEL_EDGES:
                g_value_set_boolean(value, replay_prefs_get_label_edges(self));
                break;

        case PROP_LABEL_MESSAGES:
                g_value_set_boolean(value, replay_prefs_get_label_messages(self));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
                break;
        }
}

static void
replay_prefs_set_property(GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
        ReplayPrefs *self = REPLAY_PREFS(object);
        ReplayPrefsPrivate *priv = self->priv;

        switch (prop_id) {
        case PROP_TIMELINE_VIEW:
                priv->timeline_view = REPLAY_TIMELINE_VIEW
                        (g_value_dup_object(value));
                replay_timeline_view_set_absolute_time(priv->timeline_view,
                                                       priv->absolute_time);
                replay_timeline_view_set_label_messages(priv->timeline_view,
                                                        priv->label_messages);
                break;

        case PROP_EVENT_TEXT_VIEW:
                priv->event_text_view = REPLAY_EVENT_TEXT_VIEW
                        (g_value_dup_object(value));
                replay_event_text_view_set_absolute_time(priv->event_text_view,
                                                         priv->absolute_time);
                break;

        case PROP_MESSAGE_TREE_VIEW:
                priv->message_tree_view = REPLAY_MESSAGE_TREE_VIEW
                        (g_value_dup_object(value));
                replay_message_tree_view_set_absolute_time(priv->message_tree_view,
                                                           priv->absolute_time);
                break;

        case PROP_GRAPH_VIEW:
                priv->graph_view = REPLAY_GRAPH_VIEW
                        (g_value_dup_object(value));
                replay_graph_view_set_absolute_time(priv->graph_view,
                                                    priv->absolute_time);
                replay_graph_view_set_label_nodes(priv->graph_view,
                                                  priv->label_nodes);
                replay_graph_view_set_label_edges(priv->graph_view,
                                                  priv->label_edges);
                break;

        case PROP_ABSOLUTE_TIME:
                replay_prefs_set_absolute_time(self,
                                               g_value_get_boolean(value));
                break;

        case PROP_LABEL_NODES:
                replay_prefs_set_label_nodes(self, g_value_get_boolean(value));
                break;

        case PROP_LABEL_EDGES:
                replay_prefs_set_label_edges(self, g_value_get_boolean(value));
                break;

        case PROP_LABEL_MESSAGES:
                replay_prefs_set_label_messages(self,
                                                g_value_get_boolean(value));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
                break;
        }
}

static void replay_prefs_class_init(ReplayPrefsClass *klass)
{
        GObjectClass *obj_class;

        obj_class = G_OBJECT_CLASS(klass);

        obj_class->get_property = replay_prefs_get_property;
        obj_class->set_property = replay_prefs_set_property;
        obj_class->finalize = replay_prefs_finalize;

        properties[PROP_TIMELINE_VIEW] = g_param_spec_object("timeline-view",
                                                             "timeline-view",
                                                             "The ReplayTimelineView",
                                                             REPLAY_TYPE_TIMELINE_VIEW,
                                                             G_PARAM_WRITABLE |
                                                             G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property(obj_class, PROP_TIMELINE_VIEW,
                                        properties[PROP_TIMELINE_VIEW]);

        properties[PROP_EVENT_TEXT_VIEW] = g_param_spec_object("event-text-view",
                                                               "event-text-view",
                                                               "The ReplayEventTextView",
                                                               REPLAY_TYPE_EVENT_TEXT_VIEW,
                                                               G_PARAM_WRITABLE |
                                                               G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property(obj_class, PROP_EVENT_TEXT_VIEW,
                                        properties[PROP_EVENT_TEXT_VIEW]);

        properties[PROP_MESSAGE_TREE_VIEW] = g_param_spec_object("message-tree-view",
                                                                 "message-tree-view",
                                                                 "The ReplayMessageTreeView",
                                                                 REPLAY_TYPE_MESSAGE_TREE_VIEW,
                                                                 G_PARAM_WRITABLE |
                                                                 G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property(obj_class, PROP_MESSAGE_TREE_VIEW,
                                        properties[PROP_MESSAGE_TREE_VIEW]);

        properties[PROP_GRAPH_VIEW] = g_param_spec_object("graph-view",
                                                          "graph-view",
                                                          "The ReplayGraphView",
                                                          REPLAY_TYPE_GRAPH_VIEW,
                                                          G_PARAM_WRITABLE |
                                                          G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property(obj_class, PROP_GRAPH_VIEW,
                                        properties[PROP_GRAPH_VIEW]);

        properties[PROP_ABSOLUTE_TIME] = g_param_spec_boolean("absolute-time",
                                                              "absolute-time",
                                                              "Show times as absolute or relative",
                                                              FALSE,
                                                              G_PARAM_READWRITE);
        g_object_class_install_property(obj_class, PROP_ABSOLUTE_TIME,
                                        properties[PROP_ABSOLUTE_TIME]);

        properties[PROP_LABEL_NODES] = g_param_spec_boolean("label-nodes",
                                                            "label-nodes",
                                                            "Label nodes in the graph view",
                                                            FALSE,
                                                            G_PARAM_READWRITE);
        g_object_class_install_property(obj_class, PROP_LABEL_NODES,
                                        properties[PROP_LABEL_NODES]);

        properties[PROP_LABEL_EDGES] = g_param_spec_boolean("label-edges",
                                                            "label-edges",
                                                            "Label edges in the graph view",
                                                            FALSE,
                                                            G_PARAM_READWRITE);
        g_object_class_install_property(obj_class, PROP_LABEL_EDGES,
                                        properties[PROP_LABEL_EDGES]);

        properties[PROP_LABEL_MESSAGES] = g_param_spec_boolean("label-messages",
                                                               "label-messages",
                                                               "Label messages in the timeline view",
                                                               FALSE,
                                                               G_PARAM_READWRITE);
        g_object_class_install_property(obj_class, PROP_LABEL_MESSAGES,
                                        properties[PROP_LABEL_MESSAGES]);

        /* add private data */
        g_type_class_add_private(obj_class, sizeof(ReplayPrefsPrivate));

}


static void _set_absolute_time(ReplayPrefs *self,
                               gboolean absolute_time)
{
        ReplayPrefsPrivate *priv = self->priv;

        priv->absolute_time = absolute_time;
        replay_timeline_view_set_absolute_time(priv->timeline_view,
                                               priv->absolute_time);
        replay_event_text_view_set_absolute_time(priv->event_text_view,
                                                 priv->absolute_time);
        replay_message_tree_view_set_absolute_time(priv->message_tree_view,
                                                   priv->absolute_time);
        replay_graph_view_set_absolute_time(priv->graph_view,
                                            priv->absolute_time);
}

static void _set_label_nodes(ReplayPrefs *self,
                             gboolean label_nodes)
{
        ReplayPrefsPrivate *priv = self->priv;

        priv->label_nodes = label_nodes;
        replay_graph_view_set_label_nodes(priv->graph_view,
                                          priv->label_nodes);
}

static void _set_label_edges(ReplayPrefs *self,
                             gboolean label_edges)
{
        ReplayPrefsPrivate *priv = self->priv;

        priv->label_edges = label_edges;
        replay_graph_view_set_label_edges(priv->graph_view,
                                          priv->label_edges);
}

static void _set_label_messages(ReplayPrefs *self,
                                gboolean label_messages)
{
        ReplayPrefsPrivate *priv = self->priv;

        priv->label_messages = label_messages;
        replay_timeline_view_set_label_messages(priv->timeline_view,
                                                priv->label_messages);
}

static void settings_changed(GSettings *settings,
                             gchar *key,
                             gpointer data)
{
        ReplayPrefs *self;
        g_return_if_fail(REPLAY_IS_PREFS(data));

        self = REPLAY_PREFS(data);

        if (strcmp(key, ABSOLUTE_TIME_PATH) == 0) {
                _set_absolute_time(self, g_settings_get_boolean(settings, key));
        } else if (strcmp(key, LABEL_NODES_PATH) == 0) {
                _set_label_nodes(self, g_settings_get_boolean(settings, key));
        } else if (strcmp(key, LABEL_EDGES_PATH) == 0) {
                _set_label_edges(self, g_settings_get_boolean(settings, key));
        } else if (strcmp(key, LABEL_MESSAGES_PATH) == 0) {
                _set_label_messages(self, g_settings_get_boolean(settings, key));
        }
}

static void replay_prefs_init(ReplayPrefs *self)
{
        ReplayPrefsPrivate *priv;

        self->priv = priv = REPLAY_PREFS_GET_PRIVATE(self);

        /* init the gsettings stuff */
        priv->settings = g_settings_new("au.gov.defence.dsto.parallax.replay");
        g_signal_connect(priv->settings, "changed",
                         G_CALLBACK(settings_changed), self);

        /* setup et according to save prefs */
        priv->absolute_time = TRUE;
        priv->absolute_time = g_settings_get_boolean(priv->settings,
                                                     ABSOLUTE_TIME_PATH);
        priv->label_nodes = FALSE;
        priv->label_nodes = g_settings_get_boolean(priv->settings,
                                                   LABEL_NODES_PATH);
        priv->label_edges = FALSE;
        priv->label_edges = g_settings_get_boolean(priv->settings,
                                                   LABEL_EDGES_PATH);
        priv->label_messages = FALSE;
        priv->label_messages = g_settings_get_boolean(priv->settings,
                                                      LABEL_MESSAGES_PATH);
}

ReplayPrefs *
replay_prefs_new(ReplayTimelineView *timeline_view,
                 ReplayEventTextView *event_text_view,
                 ReplayMessageTreeView *message_tree_view,
                 ReplayGraphView *graph_view)
{
        ReplayPrefs *self;

        g_return_val_if_fail(REPLAY_IS_TIMELINE_VIEW(timeline_view) &&
                             REPLAY_IS_EVENT_TEXT_VIEW(event_text_view) &&
                             REPLAY_IS_MESSAGE_TREE_VIEW(message_tree_view) &&
                             REPLAY_IS_GRAPH_VIEW(graph_view), NULL);


        self = g_object_new(REPLAY_TYPE_PREFS,
                            "timeline-view", timeline_view,
                            "event-text-view", event_text_view,
                            "message-tree-view", message_tree_view,
                            "graph-view", graph_view,
                            NULL);
        return self;
}

static void replay_prefs_finalize(GObject *object)
{
        ReplayPrefsPrivate *priv;

        g_return_if_fail(REPLAY_IS_PREFS(object));
        priv = REPLAY_PREFS(object)->priv;

        g_object_unref(priv->settings);

        g_object_unref(priv->timeline_view);
        g_object_unref(priv->graph_view);
        g_object_unref(priv->message_tree_view);
        g_object_unref(priv->event_text_view);
}

gboolean replay_prefs_get_absolute_time(ReplayPrefs *self)
{
        g_return_val_if_fail(REPLAY_IS_PREFS(self), FALSE);

        return self->priv->absolute_time;
}

void replay_prefs_set_absolute_time(ReplayPrefs *self,
                                    gboolean absolute_time)
{
        g_return_if_fail(REPLAY_IS_PREFS(self));

        if (absolute_time != self->priv->absolute_time) {
                g_settings_set_boolean(self->priv->settings,
                                       ABSOLUTE_TIME_PATH,
                                       absolute_time);
        }
}

gboolean replay_prefs_get_label_nodes(ReplayPrefs *self)
{
        g_return_val_if_fail(REPLAY_IS_PREFS(self), FALSE);

        return self->priv->label_nodes;
}


void replay_prefs_set_label_nodes(ReplayPrefs *self,
                                  gboolean label_nodes)
{
        g_return_if_fail(REPLAY_IS_PREFS(self));

        if (label_nodes != self->priv->label_nodes) {
                g_settings_set_boolean(self->priv->settings, LABEL_NODES_PATH,
                                       label_nodes);
        }
}

gboolean replay_prefs_get_label_edges(ReplayPrefs *self)
{
        g_return_val_if_fail(REPLAY_IS_PREFS(self), FALSE);

        return self->priv->label_edges;
}

void replay_prefs_set_label_edges(ReplayPrefs *self,
                                  gboolean label_edges)
{
        g_return_if_fail(REPLAY_IS_PREFS(self));

        if (label_edges != self->priv->label_edges) {
                g_settings_set_boolean(self->priv->settings, LABEL_EDGES_PATH,
                                       label_edges);
        }
}

gboolean replay_prefs_get_label_messages(ReplayPrefs *self)
{
        g_return_val_if_fail(REPLAY_IS_PREFS(self), FALSE);

        return self->priv->label_messages;
}

void replay_prefs_set_label_messages(ReplayPrefs *self,
                                     gboolean label_messages)
{
        g_return_if_fail(REPLAY_IS_PREFS(self));

        if (label_messages != self->priv->label_messages) {
                g_settings_set_boolean(self->priv->settings,
                                       LABEL_MESSAGES_PATH,
                                       label_messages);
        }
}
