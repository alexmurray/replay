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

#ifdef ENABLE_INTROSPECTION
#include <girepository.h>
#endif

#include <stdio.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <replay/replay.h>
#include <replay/replay-log.h>
#include <replay/replay-window.h>
#include <replay/replay-window-activatable.h>
#include <replay/replay-option-activatable.h>
#include <libpeas/peas.h>

#ifdef ENABLE_INTROSPECTION
static void
dump_introspection_hack(int *argc, char ***argv)
{
  int i = 0;
  while (++i < *argc) {
    if (g_str_has_prefix((*argv)[i], "--introspect-dump"))
    {
      GOptionContext *context = g_option_context_new("HACK!");
      g_option_context_add_group(context, g_irepository_get_option_group());
      g_option_context_parse(context, argc, argv, NULL);
      exit(0);
    }
  }
}
#endif

int main(int argc, char **argv)
{
  ReplayApplication *application;
  gint status;
  gchar *locale_dir;

  g_type_init();

  /* HACK - we want to parse the command line in case we are dumping
   * introspection data for GObjectIntrospection before calling gtk_init() so
   * this can be done during build time */
#ifdef ENABLE_INTROSPECTION
  dump_introspection_hack(&argc, &argv);
#endif

  /* Setup locale/gettext */
  setlocale(LC_ALL, "");

  locale_dir = g_build_filename(DATADIR, "locale", NULL);
  bindtextdomain(GETTEXT_PACKAGE, locale_dir);
  g_free(locale_dir);

  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  gtk_init(&argc, &argv);
  gdk_gl_init(&argc, &argv);
  gtk_gl_init(&argc, &argv);

  application = replay_application_new();
  status = g_application_run(G_APPLICATION(application), argc, argv);

  return status;
}
