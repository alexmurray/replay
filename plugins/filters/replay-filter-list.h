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

/*
 * GObject which implements the GtkTreeModel interface to allow user to enter
 * a number of filter strings to pre-hide nodes
 */

#ifndef __REPLAY_FILTER_LIST_H__
#define __REPLAY_FILTER_LIST_H__

#include <gtk/gtk.h>
#include <string.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_FILTER_LIST \
  (replay_filter_list_get_type())
#define REPLAY_FILTER_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_FILTER_LIST, ReplayFilterList))
#define REPLAY_FILTER_LIST_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), FILTER_LIST, ReplayFilterListClass))
#define REPLAY_IS_FILTER_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_FILTER_LIST))
#define REPLAY_IS_FILTER_LIST_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_FILTER_LIST))
#define REPLAY_FILTER_LIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_FILTER_LIST, ReplayFilterListClass))

/* the data columns that we export via the tree model interface */
enum
{
  REPLAY_FILTER_LIST_COL_ENABLED = 0,
  REPLAY_FILTER_LIST_COL_PATTERN,
  REPLAY_FILTER_LIST_COL_NAMES,
  REPLAY_FILTER_LIST_COL_ACTION,
  REPLAY_FILTER_LIST_COL_ACTION_STRING,
  REPLAY_FILTER_LIST_COL_GROUP_NAME,
  REPLAY_FILTER_LIST_COL_GROUP_COLOR,
  REPLAY_FILTER_LIST_COL_OVERRIDE_NODE_COLOR,
  REPLAY_FILTER_LIST_COL_NODE_COLOR,
  REPLAY_FILTER_LIST_N_COLUMNS
};

typedef enum {
  REPLAY_FILTER_HIDE_NODES = 0,
  REPLAY_FILTER_GROUP_NODES,
  REPLAY_FILTER_SET_NODE_PROPERTIES
} ReplayFilterAction;

typedef struct _ReplayFilterListClass ReplayFilterListClass;
typedef struct _ReplayFilterList ReplayFilterList;
typedef struct _ReplayFilterListPrivate ReplayFilterListPrivate;
struct _ReplayFilterList
{
  GObject parent;

  ReplayFilterListPrivate *priv;
};

struct _ReplayFilterListClass {
  /*< private >*/
  GObjectClass parent_class;
};


GType replay_filter_list_get_type(void);
ReplayFilterList *replay_filter_list_new(void);
ReplayFilterList *replay_filter_list_new_from_file(const gchar *file_name,
                                             GError **error);
gboolean replay_filter_list_to_file(ReplayFilterList *self,
                                 const gchar *file_name,
                                 GError **error);
/*
 * node_ids is a GArray of ReplayNodeIDs - filter list will take ownership of the
 * GArray
 */
void replay_filter_list_insert_entry(ReplayFilterList *filter_list,
                                  gint index,
                                  gboolean enabled,
                                  const gchar *pattern,
                                  gchar **names,
                                  ReplayFilterAction action,
                                  const gchar *group_name,
                                  const gchar *group_color,
                                  gboolean override_color,
                                  const gchar *color);
gboolean replay_filter_list_update_entry(ReplayFilterList *filter_list,
                                      GtkTreeIter *iter,
                                      const gchar *pattern,
                                      ReplayFilterAction action,
                                      const gchar *group_name,
                                      const gchar *group_color,
                                      gboolean override_color,
                                      const gchar *color);

gboolean replay_filter_list_set_enabled(ReplayFilterList *list,
                                     GtkTreeIter *iter,
                                     gboolean enabled);
gboolean replay_filter_list_set_pattern(ReplayFilterList *list,
                                     GtkTreeIter *iter,
                                     gchar *pattern);
gboolean replay_filter_list_set_names(ReplayFilterList *list,
                                   GtkTreeIter *iter,
                                   gchar **names);
gboolean replay_filter_list_add_to_names(ReplayFilterList *filter_list,
                                      GtkTreeIter *iter,
                                      const gchar *node);
gboolean replay_filter_list_has_name(ReplayFilterList *filter_list,
                                  GtkTreeIter *iter,
                                  const gchar *name);
gboolean replay_filter_list_remove_from_names(ReplayFilterList *filter_list,
                                           GtkTreeIter *iter,
                                           const gchar *node);
gboolean replay_filter_list_set_action(ReplayFilterList *list,
                                    GtkTreeIter *iter,
                                    ReplayFilterAction action);
gboolean replay_filter_list_set_group_name(ReplayFilterList *list,
                                        GtkTreeIter *iter,
                                        const gchar *group_name);
gboolean replay_filter_list_set_group_color(ReplayFilterList *list,
                                        GtkTreeIter *iter,
                                        const gchar *group_color);
gboolean replay_filter_list_set_node_color(ReplayFilterList *list,
                                        GtkTreeIter *iter,
                                        const gchar *color);
gboolean replay_filter_list_remove_entry(ReplayFilterList *filter_list,
                                      GtkTreeIter *iter);
gboolean replay_filter_list_move_entry_up(ReplayFilterList *filter_list,
                                       GtkTreeIter *iter);
gboolean replay_filter_list_move_entry_down(ReplayFilterList *filter_list,
                                         GtkTreeIter *iter);
gboolean replay_filter_list_find_group(ReplayFilterList *filter_list,
                                    GtkTreeIter *last,
                                    GtkTreeIter *iter,
                                    const gchar *group_name);
void replay_filter_list_remove_all(ReplayFilterList *filter_list);
void replay_filter_list_add_names_to_group(ReplayFilterList *filter_list,
                                           gchar **names,
                                           const gchar *group_name,
                                           const gchar *group_color);
void replay_filter_list_disable_all_groups_with_name(ReplayFilterList *filter_list,
                                                  const gchar *group_name);

const gchar *replay_filter_action_to_string(ReplayFilterAction action);

G_END_DECLS

#endif /* __REPLAY_FILTER_LIST_H__ */
