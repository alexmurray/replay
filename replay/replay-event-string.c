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

#include <replay/replay-event-string.h>
#include <replay/replay-string-utils.h>
#include <replay/replay-events.h>

static gchar *
props_to_string(GVariant *props)
{
  gchar *string = g_strdup("  properties:\n");

  string = replay_strconcat_and_free(string, g_variant_print(props, FALSE));
  return string;
}

static gchar *
replay_event_details_to_string(const ReplayEvent *event)
{
  gchar *string = NULL;

  if (REPLAY_IS_NODE_CREATE_EVENT(event)) {
    ReplayNodeCreateEvent *node_create = REPLAY_NODE_CREATE_EVENT(event);
    string = g_strdup_printf("      name: %s\n",
                             replay_node_create_event_get_id(node_create));
    string = replay_strconcat_and_free(string,
                                    props_to_string(replay_node_create_event_get_props(node_create)));
  } else if (REPLAY_IS_NODE_DELETE_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n",
                             replay_node_delete_event_get_id(REPLAY_NODE_DELETE_EVENT(event)));
  } else if (REPLAY_IS_NODE_PROPS_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n",
                             replay_node_props_event_get_id(REPLAY_NODE_PROPS_EVENT(event)));
    string = replay_strconcat_and_free(string,
                                    props_to_string(replay_node_props_event_get_props(REPLAY_NODE_PROPS_EVENT(event))));
  } else if (REPLAY_IS_ACTIVITY_START_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n"
                             "      node: %s\n",
                             replay_activity_start_event_get_id(REPLAY_ACTIVITY_START_EVENT(event)),
                             replay_activity_start_event_get_node(REPLAY_ACTIVITY_START_EVENT(event)));
    string = replay_strconcat_and_free(string,
                                    props_to_string(replay_activity_start_event_get_props(REPLAY_ACTIVITY_START_EVENT(event))));
  } else if (REPLAY_IS_ACTIVITY_END_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n"
                             "      node: %s\n",
                             replay_activity_end_event_get_id(REPLAY_ACTIVITY_END_EVENT(event)),
                             replay_activity_end_event_get_node(REPLAY_ACTIVITY_END_EVENT(event)));
  } else if (REPLAY_IS_EDGE_CREATE_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n"
                             "      type: %s\n"
                             "      tail: %s\n"
                             "      head: %s\n",
                             replay_edge_create_event_get_id(REPLAY_EDGE_CREATE_EVENT(event)),
                             replay_edge_create_event_get_directed(REPLAY_EDGE_CREATE_EVENT(event)) ?
                             "DIRECTED" : "UNDIRECTED",
                             replay_edge_create_event_get_tail(REPLAY_EDGE_CREATE_EVENT(event)),
                             replay_edge_create_event_get_head(REPLAY_EDGE_CREATE_EVENT(event)));
  } else if (REPLAY_IS_EDGE_DELETE_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n",
                             replay_edge_delete_event_get_id(REPLAY_EDGE_DELETE_EVENT(event)));
  } else if (REPLAY_IS_EDGE_PROPS_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n",
                             replay_edge_props_event_get_id(REPLAY_EDGE_PROPS_EVENT(event)));
    string = replay_strconcat_and_free(string,
                                    props_to_string(replay_edge_props_event_get_props(REPLAY_EDGE_PROPS_EVENT(event))));
  } else if (REPLAY_IS_MSG_SEND_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n"
                             "      node: %s\n"
                             "      edge: %s\n"
                             "    parent: %s\n",
                             replay_msg_send_event_get_id(REPLAY_MSG_SEND_EVENT(event)),
                             replay_msg_send_event_get_node(REPLAY_MSG_SEND_EVENT(event)),
                             replay_msg_send_event_get_edge(REPLAY_MSG_SEND_EVENT(event)),
                             replay_msg_send_event_get_parent(REPLAY_MSG_SEND_EVENT(event)));
    string = replay_strconcat_and_free(string,
                                    props_to_string(replay_msg_send_event_get_props(REPLAY_MSG_SEND_EVENT(event))));
  } else if (REPLAY_IS_MSG_RECV_EVENT(event)) {
    string = g_strdup_printf("      name: %s\n"
                             "      node: %s\n",
                             replay_msg_recv_event_get_id(REPLAY_MSG_RECV_EVENT(event)),
                             replay_msg_recv_event_get_node(REPLAY_MSG_RECV_EVENT(event)));
  }
  return string;
}


gchar *
replay_event_to_string(ReplayEvent *event,
                       gint64 start)
{
  gchar *string = NULL;
  gint64 timestamp;

  if (event == NULL)
  {
    goto out;
  }

  timestamp = replay_event_get_timestamp(event) - start;
  string = g_strdup_printf("      type: %s\n"
                           "      timestamp: ",
                           G_OBJECT_TYPE_NAME(event));
  string = replay_strconcat_and_free(string,
                                     replay_timestamp_to_string(timestamp,
                                                                start == 0));

  string = replay_strconcat_and_free(string,
                                  g_strdup_printf("\n"
                                              "    source: %s\n",
                                              replay_event_get_source(event)));

  string = replay_strconcat_and_free(string,
                                  replay_event_details_to_string(event));
  string = replay_strconcat_and_free(string,
                                  g_strdup("\n"));
out:
  return string;
}
