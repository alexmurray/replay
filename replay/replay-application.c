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
#include <replay/replay-application.h>
#include <replay/replay-window.h>
#include <replay/replay-application-activatable.h>
#include <replay/replay-option-activatable.h>
#include <libpeas/peas.h>

G_DEFINE_TYPE(ReplayApplication, replay_application, GTK_TYPE_APPLICATION);

/* signals we emit */
enum
{
  FILE_LOADER_ADDED = 0,
  FILE_LOADER_REMOVED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

struct _ReplayApplicationPrivate
{
  /* create a socket listener to listen for network event sources for each
   * ReplayNetworkSource */
  GList *services;
  GList *loaders;

  PeasExtensionSet *set;
};

#define REPLAY_APPLICATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_APPLICATION, ReplayApplicationPrivate))

static void
on_extension_added(PeasExtensionSet *set,
                   PeasPluginInfo *info,
                   PeasExtension *exten,
                   ReplayApplication *application)
{
  if (REPLAY_IS_APPLICATION_ACTIVATABLE(exten))
  {
    replay_application_activatable_activate(REPLAY_APPLICATION_ACTIVATABLE(exten));
  }
  else if (REPLAY_IS_OPTION_ACTIVATABLE(exten))
  {
    replay_option_activatable_activate(REPLAY_OPTION_ACTIVATABLE(exten));
  }
  else
  {
    g_assert_not_reached();
  }
}

static void
on_extension_removed(PeasExtensionSet *set,
                     PeasPluginInfo *info,
                     PeasExtension *exten,
                     ReplayApplication *application)
{
  replay_application_activatable_deactivate(REPLAY_APPLICATION_ACTIVATABLE(exten));
}

static void
action_new(GSimpleAction *action,
              GVariant *parameter,
              gpointer user_data)
{
  GtkWidget *window = replay_window_new(REPLAY_APPLICATION(user_data));
  gtk_widget_show_all(window);
}

static void action_report_bug(GSimpleAction *action,
                              GVariant *parameter,
                              gpointer user_data)
{
  gtk_show_uri(NULL, PACKAGE_BUGREPORT,
               gtk_get_current_event_time(), NULL);
}

gboolean replay_application_display_help(ReplayApplication *self,
                                         const gchar *section,
                                         GError **error)
{
  gboolean ret = FALSE;

  gchar *uri = g_strdup_printf("help:replay");
  if (section) {
      gchar *orig_uri = uri;
      uri = g_strdup_printf("%s?%s", orig_uri, section);
      g_free(orig_uri);
  }
  ret = gtk_show_uri(NULL, uri, gtk_get_current_event_time(), error);
  g_free(uri);
  return ret;
}

static void action_help(GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
        replay_application_display_help(REPLAY_APPLICATION(user_data), NULL, NULL);
}

static const gchar *authors[] = {
  "Alex Murray <murray.alex@gmail.com>",
  NULL
};

static void action_about(GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  gtk_show_about_dialog(NULL,
                        "title", _("About Replay"),
                        "icon-name", PACKAGE,
                        "program-name", "Replay",
                        "version", VERSION,
                        "copyright",
                        "Copyright © 2012 Commonwealth of Australia",
                        "website", PACKAGE_URL,
                        "website-label", _("Replay homepage"),
                        "authors", authors,
                        "documenters", authors,
                        "artists", authors,
                        "logo-icon-name", PACKAGE,
                        "license", _("Replay is licensed under the GNU General Public License version 3 (GPLv3)\n"
                                     "with extensions as allowed under clause 7 of GPLv3 to clarify issues of\n"
                                     "indemnity and liability.\n\n"
                                     "For full details see the COPYING file distributed with Replay or view it online at\n"
                                     "https://github.com/alexmurray/replay/blob/master/COPYING"),
                        NULL);
}

static void
replay_application_activate(GApplication *application)
{
  GtkWidget *window = replay_window_new(REPLAY_APPLICATION(application));
  gtk_widget_show_all(window);
}

static gchar **remaining_args = NULL;

static GOptionEntry entries[] =
{
  /* collects file arguments */
  {
    G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY,
    &remaining_args,
    NULL,
    N_("[FILE]")
  },
  { NULL }
};

static ReplayWindow *
find_free_window(ReplayApplication *self)
{
  ReplayWindow *window = NULL;
  GList *windows;

  /* find a window without an existing event source or create a new one if none
     are free */
  for (windows = gtk_application_get_windows(GTK_APPLICATION(self));
       windows != NULL;
       windows = windows->next)
  {
    ReplayWindow *w = REPLAY_WINDOW(windows->data);
    if (!replay_window_get_event_source(w) &&
        !replay_window_get_event_store(w)) {
      window = w;
      break;
    }
  }
  if (!window) {
    window = REPLAY_WINDOW(replay_window_new(self));
    gtk_widget_show_all(GTK_WIDGET(window));
  }
  gtk_window_present(GTK_WINDOW(window));
  return window;
}

static int
replay_application_command_line(GApplication *application,
                             GApplicationCommandLine *command_line)
{
  GOptionContext *context;
  PeasEngine *engine;
  PeasExtensionSet *set;
  int argc;
  gchar **argv;
  GError *error = NULL;

  argv = g_application_command_line_get_arguments(command_line, &argc);

  /* Setup command line options */
  context = g_option_context_new(_(" - Visualize"));
  g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group(context, gtk_get_option_group(FALSE));

  /* create extension set of all replay commandline plugins */
  engine = peas_engine_get_default();
  set = peas_extension_set_new(engine, REPLAY_TYPE_OPTION_ACTIVATABLE,
                               "context", context, NULL);

  /* activate all activatable extensions in set */
  peas_extension_set_foreach(set,
                             (PeasExtensionSetForeachFunc)on_extension_added,
                             application);

  if (!g_option_context_parse(context, &argc, &argv, &error))
  {
    g_print(_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
            error->message, argv[0]);
    g_error_free(error);
  }

  /* try and load a user-specified files in new windows */
  if (remaining_args != NULL)
  {
    guint i;
    for (i = 0; i < g_strv_length(remaining_args); i++) {
      GFile *file = g_file_new_for_commandline_arg(remaining_args[i]);
      ReplayWindow *window = find_free_window(REPLAY_APPLICATION(application));
      replay_window_load_file(window, file);
      g_object_unref(file);
    }
  }
  /* if this is not a remote invocation we want to open a new blank window as
     the primary window */
  else if (!g_application_command_line_get_is_remote(command_line))
  {
    GtkWidget *window = replay_window_new(REPLAY_APPLICATION(application));
    gtk_widget_show_all(window);
  }
  g_strfreev(remaining_args);
  remaining_args = NULL;
  g_option_context_free(context);
  g_object_unref(set);

  return 0;
}

static void
action_quit(GSimpleAction *action,
            GVariant *parameter,
            gpointer user_data)
{
  GtkApplication *application;
  /* destroy each window so it gets a chance to save state */
  GList *windows;

  application = GTK_APPLICATION(user_data);
  while ((windows = gtk_application_get_windows(application)) != NULL) {
    gtk_application_remove_window(application,
                                  GTK_WINDOW(windows->data));
  }
  g_application_quit(G_APPLICATION(user_data));
}

static GActionEntry app_entries[] = {
  { "new", action_new, NULL, NULL, NULL },
  { "help", action_help, NULL, NULL, NULL },
  { "about", action_about, NULL, NULL, NULL },
  { "report-bug", action_report_bug, NULL, NULL, NULL },
  { "quit", action_quit, NULL, NULL, NULL },
};

static void
replay_application_startup(GApplication *application)
{
  ReplayApplication *self;
  ReplayApplicationPrivate *priv;
  GtkBuilder *builder;
  PeasEngine *engine;
  GSettings *plugin_settings;
  gchar *plugin_dir;
  GError *error = NULL;

  G_APPLICATION_CLASS(replay_application_parent_class)->startup(application);

  self = REPLAY_APPLICATION(application);
  priv = self->priv;

  g_action_map_add_action_entries(G_ACTION_MAP(application), app_entries,
                                  G_N_ELEMENTS(app_entries), application);
  /* TODO: Build and add menus for the application and its window to the
   * application */
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder,
                               "<interface>"
                               "  <menu id='app-menu'>"
                               "    <section>"
                               "      <item>"
                               "        <attribute name='label' translatable='yes'>_New Window</attribute>"
                               "        <attribute name='action'>app.new</attribute>"
                               "        <attribute name='accel'>&lt;Primary&gt;n</attribute>"
                               "      </item>"
                               "    </section>"
                               "    <section>"
                               "      <item>"
                               "        <attribute name='label' translatable='yes'>_Help</attribute>"
                               "        <attribute name='action'>app.help</attribute>"
                               "        <attribute name='accel'>&lt;Primary&gt;q</attribute>"
                               "      </item>"
                               "      <item>"
                               "        <attribute name='label' translatable='yes'>_About Replay</attribute>"
                               "        <attribute name='action'>app.about</attribute>"
                               "      </item>"
                               "      <item>"
                               "        <attribute name='label' translatable='yes'>_Report A Bug</attribute>"
                               "        <attribute name='action'>app.report-bug</attribute>"
                               "      </item>"
                               "    </section>"
                               "    <section>"
                               "      <item>"
                               "        <attribute name='label' translatable='yes'>_Quit</attribute>"
                               "        <attribute name='action'>app.quit</attribute>"
                               "        <attribute name='accel'>&lt;Primary&gt;q</attribute>"
                               "      </item>"
                               "    </section>"
                               "  </menu>", -1, NULL);
  gtk_application_set_app_menu(GTK_APPLICATION(application), G_MENU_MODEL(gtk_builder_get_object(builder, "app-menu")));
  g_object_unref(builder);

  /* ensure we look in the directory we installed the typelib file when
     compiling */
  g_irepository_prepend_search_path(TYPELIBDIR);

  /* setup plugins once only for the entire application */
  if (!g_irepository_require(g_irepository_get_default(), "Peas", "1.0",
                             0, &error))
  {
    g_warning("Could not load Peas repository: %s", error->message);
    g_error_free (error);
    error = NULL;
  }

  if (!g_irepository_require(g_irepository_get_default(), "Replay", "1.0",
                             0, &error))
  {
    g_warning("Could not load Replay repository: %s", error->message);
    g_error_free (error);
    error = NULL;
  }

  engine = peas_engine_get_default();
  /* enable all plugin loaders */
  peas_engine_enable_loader (engine, "gjs");
  peas_engine_enable_loader (engine, "python");
  peas_engine_enable_loader (engine, "seed");

  /* add home dir to search path */
  plugin_dir = g_build_filename(g_get_user_data_dir(), PACKAGE,
                                "plugins", NULL);
  peas_engine_add_search_path(engine, plugin_dir, NULL);
  g_free(plugin_dir);

  /* add system path to search path */
  plugin_dir = g_build_filename(LIBDIR, PACKAGE, "plugins", NULL);
  peas_engine_add_search_path(engine, plugin_dir, NULL);
  g_free(plugin_dir);

  plugin_settings = g_settings_new("au.gov.defence.dsto.parallax.replay.plugin-engine");
  g_settings_bind(plugin_settings, "active-plugins",
                  engine, "loaded-plugins",
                  G_SETTINGS_BIND_DEFAULT);
  priv->set = peas_extension_set_new(engine, REPLAY_TYPE_APPLICATION_ACTIVATABLE,
                                     "application", self, NULL);

  /* activate all activatable extensions */
  peas_extension_set_foreach(priv->set,
                             (PeasExtensionSetForeachFunc)on_extension_added,
                             application);

  /* and make sure to activate any ones which are found in the future */
  g_signal_connect(priv->set, "extension-added",
                   G_CALLBACK(on_extension_added), self);
  g_signal_connect(priv->set, "extension-removed",
                   G_CALLBACK(on_extension_removed), self);

}

static void
replay_application_dispose(GObject *object)
{
  ReplayApplication *self = REPLAY_APPLICATION(object);
  ReplayApplicationPrivate *priv = self->priv;
  GList *list;

  for (list = priv->loaders; list != NULL; list = list->next)
  {
    ReplayFileLoader *file_loader = REPLAY_FILE_LOADER(list->data);
    /* make sure file loading is stopped first */
    replay_file_loader_cancel(file_loader);
    g_object_unref(file_loader);
  }
  g_list_free(priv->loaders);
  priv->loaders = NULL;

  g_list_foreach(priv->services, (GFunc)g_object_unref, NULL);
  g_list_free(priv->services);

  G_OBJECT_CLASS(replay_application_parent_class)->dispose(object);
}

static void
replay_application_finalize(GObject *object)
{
  G_OBJECT_CLASS(replay_application_parent_class)->finalize(object);
}

static void
replay_application_init(ReplayApplication *self)
{
  self->priv = REPLAY_APPLICATION_GET_PRIVATE(self);
}

/* static gboolean */
/* replay_application_local_command_line(GApplication *application, */
/*                                    gchar ***args, */
/*                                    gint *exit_status) */
/* { */
/*   GOptionContext *context; */
/*   gint argc; */

/*   context = g_option_context_new(_(" - Visualize")); */
/*   g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE); */
/*   g_option_context_add_group(context, gtk_get_option_group(FALSE)); */
/*   argc = g_strv_length(*args); */
/*   g_option_context_parse(context, &argc, args, NULL); */
/*   g_option_context_free(context); */

/*   return FALSE; */
/* } */

static gboolean
window_state_event_cb(GtkWidget *widget,
                      GdkEventWindowState *event,
                      ReplayApplication *self)
{
  GList *windows = gtk_application_get_windows(GTK_APPLICATION(self));
  GList *l;

  /* set first window have 100 max fps and the rest to have 10 since they're not
   * the top-most one */
  for (l = windows; l != NULL; l = l->next)
  {
    ReplayWindow *window;
    ReplayGraphView *graph_view;

    window = REPLAY_WINDOW(l->data);
    graph_view = replay_window_get_graph_view(window);
    replay_graph_view_set_max_fps(graph_view,
                               l == windows ? 100.0 : 10.0);
  }
  return FALSE;
}

static void
replay_application_window_added(GtkApplication *application,
                             GtkWindow *window)
{
  GTK_APPLICATION_CLASS(replay_application_parent_class)->window_added(application, window);

  /* track focus of each window so we can make sure the top-most one has most
   * fps */
  g_signal_connect(window, "window-state-event", G_CALLBACK(window_state_event_cb),
                   application);
}

static void
replay_application_window_removed(GtkApplication *application,
                             GtkWindow *window)
{
  /* track focus of each window so we can make sure the top-most one has most
   * fps */
  g_signal_handlers_disconnect_by_func(window, G_CALLBACK(window_state_event_cb),
                                       application);
  GTK_APPLICATION_CLASS(replay_application_parent_class)->window_removed(application, window);
}
static void
replay_application_class_init(ReplayApplicationClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS(class);
  GApplicationClass *app_class = G_APPLICATION_CLASS(class);
  GtkApplicationClass *gtkapp_class = GTK_APPLICATION_CLASS(class);

  obj_class->dispose = replay_application_dispose;
  obj_class->finalize = replay_application_finalize;
  app_class->startup = replay_application_startup;
  app_class->activate = replay_application_activate;
  app_class->command_line = replay_application_command_line;
  /* app_class->local_command_line = replay_application_local_command_line; */
  gtkapp_class->window_added = replay_application_window_added;
  gtkapp_class->window_removed = replay_application_window_removed;

  /* install class signals */
  /**
   * ReplayApplication::file-loader-added:
   * @self: The application emitting this event
   * @loader: The new file loader
   *
   * Emitted by a #ReplayApplication when a new file loader is added
   */
  signals[FILE_LOADER_ADDED] = g_signal_new("file-loader-added",
                                            G_OBJECT_CLASS_TYPE(obj_class),
                                            G_SIGNAL_RUN_LAST,
                                            G_STRUCT_OFFSET(ReplayApplicationClass,
                                                            file_loader_added),
                                            NULL, NULL,
                                            g_cclosure_marshal_VOID__OBJECT,
                                            G_TYPE_NONE,
                                            1,
                                            REPLAY_TYPE_FILE_LOADER);

  /**
   * ReplayApplication::file-loader-removed:
   * @self: The application emitting this event
   * @loader: The new file loader
   *
   * Emitted by a #ReplayApplication when a new file loader is removed
   */
  signals[FILE_LOADER_REMOVED] = g_signal_new("file-loader-removed",
                                            G_OBJECT_CLASS_TYPE(obj_class),
                                            G_SIGNAL_RUN_LAST,
                                            G_STRUCT_OFFSET(ReplayApplicationClass,
                                                            file_loader_removed),
                                            NULL, NULL,
                                            g_cclosure_marshal_VOID__OBJECT,
                                            G_TYPE_NONE,
                                            1,
                                            REPLAY_TYPE_FILE_LOADER);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayApplicationPrivate));
}

ReplayApplication *
replay_application_new()
{
  /* the primary instance handles command-line parsing */
  return g_object_new(replay_application_get_type(),
                      "application-id", "au.gov.defence.dsto.parallax.replay",
                      "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                      NULL);
}

static gboolean incoming_service_cb(GSocketService *service,
                                    GSocketConnection *conn,
                                    GObject *source_object,
                                    gpointer user_data)
{
  ReplayApplication *self;
  ReplayNetworkServer *server;
  ReplayWindow *window;

  g_return_val_if_fail(REPLAY_IS_APPLICATION(user_data), FALSE);

  self = REPLAY_APPLICATION(user_data);
  server = REPLAY_NETWORK_SERVER(source_object);

  window = find_free_window(self);
  replay_window_set_event_source(window, REPLAY_EVENT_SOURCE(server));
  replay_network_server_load_events(server, conn);
  return TRUE;
}

gboolean replay_application_add_network_server(ReplayApplication *self,
                                            ReplayNetworkServer *server,
                                            GError **error)
{
  ReplayApplicationPrivate *priv;
  GSocketService *service;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_APPLICATION(self), FALSE);
  priv = self->priv;

  service = g_socket_service_new();
  ret =  g_socket_listener_add_inet_port(G_SOCKET_LISTENER(service),
                                         replay_network_server_get_port(server),
                                         G_OBJECT(server), error);
  if (!ret)
  {
    g_object_unref(service);
    goto out;
  }

  g_object_set_data(G_OBJECT(service), "replay-network-server", server);
  priv->services = g_list_prepend(priv->services, service);
  g_signal_connect(service, "incoming", G_CALLBACK(incoming_service_cb),
                   self);
  g_socket_service_start(service);

  out:
  return ret;
}

void replay_application_remove_network_server(ReplayApplication *self,
                                      ReplayNetworkServer *server)
{
  ReplayApplicationPrivate *priv;
  GList *l;

  g_return_if_fail(REPLAY_IS_APPLICATION(self));
  priv = self->priv;

  for (l = priv->services; l != NULL; l = l->next) {
    GSocketService *service = G_SOCKET_SERVICE(l->data);
    ReplayNetworkServer *s = g_object_get_data(G_OBJECT(service), "replay-network-server");

    if (s == server) {
      g_socket_service_stop(service);
      g_object_unref(service);
      priv->services = g_list_remove(priv->services, l);
      break;
    }
  }
}

static int compare_file_loaders(gconstpointer a,
                                gconstpointer b)
{
  g_return_val_if_fail(REPLAY_IS_FILE_LOADER(a), -1);
  g_return_val_if_fail(REPLAY_IS_FILE_LOADER(b), -1);

  return g_strcmp0(replay_event_source_get_name(REPLAY_EVENT_SOURCE(a)),
                   replay_event_source_get_name(REPLAY_EVENT_SOURCE(b)));
}

void replay_application_add_file_loader(ReplayApplication *self,
                                        ReplayFileLoader *loader)
{
  g_return_if_fail(REPLAY_IS_APPLICATION(self));

  self->priv->loaders = g_list_insert_sorted(self->priv->loaders,
                                              g_object_ref(loader),
                                              compare_file_loaders);
  g_signal_emit(self, signals[FILE_LOADER_ADDED], 0, loader);
}


void replay_application_remove_file_loader(ReplayApplication *self,
                                           ReplayFileLoader *loader)
{
  GList *list;
  g_return_if_fail(REPLAY_IS_APPLICATION(self));

  if ((list = g_list_find(self->priv->loaders, loader)) != NULL)
  {
    self->priv->loaders = g_list_delete_link(self->priv->loaders,
                                             list);
    g_signal_emit(self, signals[FILE_LOADER_REMOVED], 0, loader);
    g_object_unref(loader);
  }
}

/**
 * replay_application_get_file_loaders:
 * @self: A #ReplayApplication
 *
 * Returns: (element-type ReplayFileLoader) (transfer none): A list of the #ReplayFileLoader available to this
 * application. This list is owned by the application and should not be
 * modified or freed by the caller.
 */
const GList *
replay_application_get_file_loaders(ReplayApplication *self)
{
  g_return_val_if_fail(REPLAY_IS_APPLICATION(self), NULL);

  return self->priv->loaders;
}
