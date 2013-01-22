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
 * SECTION:replay-log-window
 * @short_description: Widget which displays a #ReplayLog
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <replay/replay-log-window.h>
#include <replay/replay-log.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE(ReplayLogWindow, replay_log_window, GTK_TYPE_WINDOW);

struct _ReplayLogWindowPrivate
{
  GtkActionGroup *actions;
  GtkWidget *tree_view;
  ReplayLog *log;
  GtkTreeModel *filter;
  ReplayLogLevel log_level;
};


/* properties */
enum
{
  PROP_INVALID,
  PROP_LOG,
};

/* callbacks for ui elements */
static void save_cb(GtkAction *action, gpointer data);
static void clear_cb(GtkAction *action, gpointer data);

static GtkActionEntry action_entries[] =
{
  /* name, stock_id, label, accelerator, tooltip, callback */

  { "SaveAction", GTK_STOCK_SAVE, N_("_Save As"), NULL,
    N_("Save error log"), G_CALLBACK(save_cb) },
  { "ClearAction", GTK_STOCK_CLEAR, N_("_Clear All"), NULL,
    N_("Clear error log"), G_CALLBACK(clear_cb) },
};

static const gchar * const ui = "<ui>"
                                "<toolbar name='ToolBar'>"
                                "<placeholder name='ToolItems'>"
                                "<toolitem name='SaveToolItem' action='SaveAction'/>"
                                "<toolitem name='ClearToolItem' action='ClearAction'/>"
                                "</placeholder>"
                                "</toolbar>"
                                "</ui>";

static void
replay_log_window_get_property(GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ReplayLogWindow *self;
  ReplayLogWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(object));

  self = REPLAY_LOG_WINDOW(object);
  priv = self->priv;

  switch (property_id)
  {
    case PROP_LOG:
      g_value_set_object(value, priv->log);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_log_window_set_property(GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ReplayLogWindow *self;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(object));

  self = REPLAY_LOG_WINDOW(object);

  switch (property_id)
  {
    case PROP_LOG:
      replay_log_window_set_log(self,
                                             REPLAY_LOG(g_value_get_object(value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
replay_log_window_finalize(GObject *object)
{
  ReplayLogWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(object));

  priv = REPLAY_LOG_WINDOW(object)->priv;

  if (priv->log)
  {
    g_object_unref(priv->filter);
    priv->filter = NULL;
    g_object_unref(priv->log);
    priv->log = NULL;
  }

  /* call parent finalize */
  G_OBJECT_CLASS(replay_log_window_parent_class)->finalize(object);
}


static void
combo_box_changed_cb(GtkComboBox *combo_box,
                     gpointer user_data)
{
  ReplayLogWindow *self;
  ReplayLogWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(user_data));

  self = REPLAY_LOG_WINDOW(user_data);
  priv = self->priv;

  priv->log_level = (ReplayLogLevel)gtk_combo_box_get_active(combo_box);
  gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(priv->filter));
}

static void
render_column(GtkTreeViewColumn *col,
              GtkCellRenderer *renderer,
              GtkTreeModel *model,
              GtkTreeIter *iter,
              gpointer data)
{
  gchar *markup;
  const gchar *fg[] = { "red", "black", "forestgreen" };
  ReplayLogLevel log_level;

  gtk_tree_model_get(model, iter,
                     GPOINTER_TO_INT(data), &markup,
                     REPLAY_LOG_COL_LOG_LEVEL, &log_level,
                     -1);

  g_object_set(renderer,
               "markup", markup,
               "foreground", fg[MIN(log_level, REPLAY_LOG_LEVEL_DEBUG)],
               NULL);
  g_free(markup);
}

static void
replay_log_window_init(ReplayLogWindow *self)
{
  ReplayLogWindowPrivate *priv;
  GtkTreeViewColumn *col;
  GtkWidget *scrolled_window, *vbox, *toolbar;
  GtkUIManager *ui_manager;
  GtkAccelGroup *accel_group;
  GtkCellRenderer *renderer;
  GtkToolItem *tool_item;
  GtkWidget *label, *combo_box;
  ReplayLogLevel log_level;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(self));

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                                  REPLAY_TYPE_LOG_WINDOW,
                                                  ReplayLogWindowPrivate);

  priv->tree_view = gtk_tree_view_new();
  renderer = gtk_cell_renderer_text_new();
  col = gtk_tree_view_column_new_with_attributes(_("Time"),
                                                 renderer,
                                                 NULL);
  gtk_tree_view_column_set_cell_data_func(col, renderer, render_column,
                                          GINT_TO_POINTER(REPLAY_LOG_COL_TIME),
                                          NULL);
  gtk_tree_view_column_set_resizable(col, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree_view),
                              col);
  col = gtk_tree_view_column_new_with_attributes(_("Source"),
                                                 renderer,
                                                 NULL);
  gtk_tree_view_column_set_cell_data_func(col, renderer, render_column,
                                          GINT_TO_POINTER(REPLAY_LOG_COL_SOURCE),
                                          NULL);
  gtk_tree_view_column_set_resizable(col, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree_view),
                              col);
  col = gtk_tree_view_column_new_with_attributes(_("Message"),
                                                 renderer,
                                                 NULL);
  gtk_tree_view_column_set_cell_data_func(col, renderer, render_column,
                                          GINT_TO_POINTER(REPLAY_LOG_COL_BRIEF_DETAILS),
                                          NULL);
  gtk_tree_view_column_set_resizable(col, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree_view),
                              col);

  /* set expander on message column */
  gtk_tree_view_set_expander_column(GTK_TREE_VIEW(priv->tree_view),
                                    col);

  scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled_window), priv->tree_view);

  /* create actions */
  priv->actions = gtk_action_group_new("Actions");
  gtk_action_group_set_translation_domain(priv->actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions(priv->actions, action_entries,
                               G_N_ELEMENTS(action_entries),
                               self);

  gtk_action_set_is_important(gtk_action_group_get_action(priv->actions,
                                                          "SaveAction"),
                              TRUE);
  gtk_action_set_is_important(gtk_action_group_get_action(priv->actions,
                                                          "ClearAction"),
                              TRUE);

  ui_manager = gtk_ui_manager_new();
  gtk_ui_manager_insert_action_group(ui_manager, priv->actions, 0);
  accel_group = gtk_ui_manager_get_accel_group(ui_manager);
  gtk_window_add_accel_group(GTK_WINDOW(self), accel_group);
  gtk_ui_manager_add_ui_from_string(ui_manager, ui, -1, NULL);

  /* pack into main vbox */
  toolbar = gtk_ui_manager_get_widget(ui_manager, "/ToolBar");

  /* append a separator to toolbar */
  tool_item = gtk_separator_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, -1);

  /* add label and spin_button to toolbar */
  label = gtk_label_new(_("Log Level"));
  gtk_misc_set_padding(GTK_MISC(label), 5, 0);
  tool_item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(tool_item), label);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, -1);
  combo_box = gtk_combo_box_text_new();
  for (log_level = REPLAY_LOG_LEVEL_ERROR;
       log_level <= REPLAY_LOG_LEVEL_DEBUG;
       log_level++)
  {
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combo_box),
                                   log_level,
                                   replay_log_level_to_string(log_level));
  }

  /* show info messages by default */
  priv->log_level = REPLAY_LOG_LEVEL_INFO;
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), priv->log_level);

  g_signal_connect(combo_box, "changed",
                   G_CALLBACK(combo_box_changed_cb),
                   self);

  tool_item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(tool_item), combo_box);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, -1);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show_all(vbox);

  gtk_container_add(GTK_CONTAINER(self), vbox);

  gtk_window_set_title(GTK_WINDOW(self), _("Replay Log"));
  gtk_window_set_default_size(GTK_WINDOW(self), 900, 600);

  g_object_unref(ui_manager);
}

static void
replay_log_window_class_init(ReplayLogWindowClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS(klass);
  GParamSpec *spec;

	object_class->set_property = replay_log_window_set_property;
	object_class->get_property = replay_log_window_get_property;
  object_class->finalize = replay_log_window_finalize;

  /* log property */
  spec = g_param_spec_object("log",
                             "The log to display",
                             "Get/Set log",
                             REPLAY_TYPE_LOG,
                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_class_install_property(object_class,
                                  PROP_LOG,
                                  spec);

  g_type_class_add_private(klass, sizeof(ReplayLogWindowPrivate));
}

void export_to_file(ReplayLogWindow *self,
                    GFile *file)
{
  ReplayLogWindowPrivate *priv;
  GFileOutputStream *file_stream;
  GDataOutputStream *data_stream;
  GError *error = NULL;
  gchar *filename, *error_msg;
  GtkTreeIter iter;
  gboolean keep_going;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(self));

  priv = self->priv;

  file_stream = g_file_replace(file, NULL, FALSE,
                               G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error);
  if (!file_stream)
  {
    goto error;
  }

  data_stream = g_data_output_stream_new(G_OUTPUT_STREAM(file_stream));
  if (!data_stream)
  {
    g_object_unref(file_stream);
    goto error;
  }

  for (keep_going = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->log),
                                                  &iter);
       keep_going;
       keep_going = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->log),
                                             &iter))
  {
    GtkTreeIter child;
    ReplayLogLevel level;
    gchar *time_ = NULL;
    gchar *source = NULL;
    gchar *brief = NULL;
    gchar *details = NULL;
    gchar *line;
    gboolean ret;

    gtk_tree_model_get(GTK_TREE_MODEL(priv->log), &iter,
                       REPLAY_LOG_COL_LOG_LEVEL, &level,
                       REPLAY_LOG_COL_TIME, &time_,
                       REPLAY_LOG_COL_SOURCE, &source,
                       REPLAY_LOG_COL_BRIEF_DETAILS, &brief,
                       -1);

    if (gtk_tree_model_iter_children(GTK_TREE_MODEL(priv->log),
                                     &child,
                                     &iter))
    {
      gtk_tree_model_get(GTK_TREE_MODEL(priv->log), &child,
                         REPLAY_LOG_COL_BRIEF_DETAILS, &details);
    }

    if (details)
    {
      line = g_strdup_printf("%s\t%s\t%s\t%s\t%s\n",
                             time_, replay_log_level_to_string(level), source,
                             brief, details);
    }
    else
    {
      line = g_strdup_printf("%s\t%s\t%s\t%s\n",
                             time_, replay_log_level_to_string(level), source,
                             brief);
    }

    g_free(time_);
    g_free(source);
    g_free(brief);
    g_free(details);

    ret = g_data_output_stream_put_string(data_stream, line, NULL, &error);

    if (!ret)
    {
      g_object_unref(data_stream);
      g_object_unref(file_stream);
      goto error;
    }
  }

  g_object_unref(data_stream);
  g_object_unref(file_stream);
  goto out;

  error:
  filename = g_file_get_parse_name(file);
  error_msg = g_strdup_printf("Error exporting event log to file %s\n",
                          filename);
  g_free(filename);

  replay_log_error(priv->log, "Log", error_msg, error ? error->message : NULL);
  g_free(error_msg);

  out:
  return;
}

static void
save_cb(GtkAction *action,
        gpointer data)
{
  ReplayLogWindow *self;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(data));

  self = REPLAY_LOG_WINDOW(data);

  if (self->priv->log)
  {
    GtkWidget *dialog;
    gchar time_str[16];
    time_t t;
    struct tm *tmp;
    gchar *filename = NULL;

    dialog = gtk_file_chooser_dialog_new("Save  Log",
                                         GTK_WINDOW(self),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                   TRUE);

    /* set a sensible filename */
    t = time(NULL);
    tmp = localtime(&t);
    if (tmp && strftime(time_str, sizeof(time_str), "%Y%m%d-%H%M%S", tmp))
    {
      filename = g_strdup_printf("replay_log_%s.txt", time_str);
    }
    else
    {
      filename = g_strdup_printf("replay_log.txt");
    }
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                      filename);
    g_free(filename);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
      export_to_file(self, file);
      g_object_unref(file);
    }
    gtk_widget_destroy(dialog);
  }
}

static void
clear_cb(GtkAction *action,
         gpointer data)
{
  ReplayLogWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(data));

  priv = REPLAY_LOG_WINDOW(data)->priv;

  if (priv->log)
  {
    replay_log_clear(priv->log);
  }
}

GtkWidget *
replay_log_window_new(ReplayLog *log)
{
  return g_object_new(REPLAY_TYPE_LOG_WINDOW,
                      "log", log,
                      NULL);
}

static void
update_sensitivities(ReplayLogWindow *self)
{
  ReplayLogWindowPrivate *priv;
  gboolean sensitive = FALSE;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(self));

  priv = self->priv;

  if (priv->log)
  {
    sensitive = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->log), NULL);
  }

  gtk_action_group_set_sensitive(priv->actions, sensitive);
  gtk_widget_set_sensitive(priv->tree_view, sensitive);
}

static gboolean
row_visible(GtkTreeModel *model,
            GtkTreeIter *iter,
            gpointer data)
{
  ReplayLogWindow *self;
  ReplayLogWindowPrivate *priv;
  ReplayLogLevel log_level;

  g_return_val_if_fail(REPLAY_IS_LOG_WINDOW(data), FALSE);

  self = REPLAY_LOG_WINDOW(data);
  priv = self->priv;

  gtk_tree_model_get(model, iter,
                     REPLAY_LOG_COL_LOG_LEVEL, &log_level,
                     -1);
  return log_level <= priv->log_level;
}

void
replay_log_window_set_log(ReplayLogWindow *self,
                                       ReplayLog *log)
{
  ReplayLogWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_LOG_WINDOW(self));

  priv = self->priv;

  if (priv->log)
  {
    g_object_unref(priv->filter);
    priv->filter = NULL;
    g_object_unref(priv->log);
    priv->log = NULL;
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->tree_view),
                            NULL);
    update_sensitivities(self);
  }
  if (log)
  {
    priv->log = g_object_ref(log);
    priv->filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(log), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(priv->filter),
                                           row_visible,
                                           self,
                                           NULL);
    g_signal_connect_swapped(priv->log, "row-inserted",
                             G_CALLBACK(update_sensitivities), self);
    g_signal_connect_swapped(priv->log, "row-deleted",
                             G_CALLBACK(update_sensitivities), self);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->tree_view),
                            priv->filter);
    update_sensitivities(self);
  }
}
