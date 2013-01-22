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
 * GtkDialog subclass to allow loading, saving and editing of filters
 */

#ifndef __REPLAY_FILTERS_DIALOG_H__
#define __REPLAY_FILTERS_DIALOG_H__

#include <gtk/gtk.h>
#include <replay/replay-node-tree.h>
#include "replay-filter-list.h"

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_FILTERS_DIALOG \
  (replay_filters_dialog_get_type())
#define REPLAY_FILTERS_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_FILTERS_DIALOG, ReplayFiltersDialog))
#define REPLAY_FILTERS_DIALOG_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), REPLAY_FILTERS_DIALOG, ReplayFiltersDialogClass))
#define REPLAY_IS_FILTERS_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_FILTERS_DIALOG))
#define REPLAY_IS_FILTERS_DIALOG_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_FILTERS_DIALOG))
#define REPLAY_FILTERS_DIALOG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_FILTERS_DIALOG, ReplayFiltersDialogClass))

typedef struct _ReplayFiltersDialog ReplayFiltersDialog;
typedef struct _ReplayFiltersDialogClass ReplayFiltersDialogClass;
typedef struct _ReplayFiltersDialogPrivate ReplayFiltersDialogPrivate;

struct _ReplayFiltersDialog {
  GtkDialog parent;

  /* private */
  ReplayFiltersDialogPrivate *priv;
};

struct _ReplayFiltersDialogClass {
  /*< private >*/
  GtkDialogClass parent_class;
};

/* public functions */
GType replay_filters_dialog_get_type(void);
GtkWidget *replay_filters_dialog_new(void);

void replay_filters_dialog_set_filter_list(ReplayFiltersDialog *self,
                                        ReplayFilterList *list);
ReplayFilterList *replay_filters_dialog_get_filter_list(ReplayFiltersDialog *self);

void replay_filters_dialog_set_node_tree(ReplayFiltersDialog *filters_dialog,
                                      ReplayNodeTree *tree);

G_END_DECLS

#endif /* __REPLAY_FILTERS_DIALOG_H__ */
