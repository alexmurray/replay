# gsettings schema
gsettings_SCHEMAS	= au.gov.defence.dsto.parallax.replay.gschema.xml
@GSETTINGS_RULES@

# pkg-config pc file for libreplay
pkgconfigdir      = $(libdir)/pkgconfig
pkgconfig_DATA    = replay.pc

# desktop file for menu entry
desktopdir        = $(datadir)/applications
desktop_in_files  = replay.desktop.in
desktop_DATA      = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

install-data-hook: update_desktop_database
uninstall-hook: update_desktop_database

update_desktop_database:
if DATABASE_UPDATES
	@-if test -n "$(UPDATE_DESKTOP_DATABASE)" && test -z "$(DESTDIR)"; then \
		echo "Updating desktop database."; \
		$(UPDATE_DESKTOP_DATABASE); \
	fi
else
	@echo "*** desktop databse not updated.  After (un)install, run this:";
	@echo "***   $(UPDATE_DESKTOP_DATABASE)";
endif

EXTRA_DIST = \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(gsettings_SCHEMAS) \
	replay.pc.in \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA) \
	$(schemas_DATA) \
	$(pkgconfig_DATA) \
	$(NULL)
