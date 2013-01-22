svgappicondir = $(datadir)/icons/hicolor/scalable/apps
svgappicon_DATA = replay.svg replay-plugin.svg
appicondir = $(datadir)/icons/hicolor/48x48/apps
appicon_DATA = replay.png replay-plugin.png
texturesdir = $(datadir)/$(PACKAGE)/textures
textures_DATA = active-node-glow.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

xdg_icon_resource = xdg-icon-resource install --context mimetypes --size 48 replay-log-icon.png application-x-replay-event-log

install-data-hook: update_icon_cache
uninstall-hook: update_icon_cache
update_icon_cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
		echo "Installing document icon."; \
		$(xdg_icon_resource); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = \
	$(svgappicon_DATA) \
	$(appicon_DATA) \
	$(textures_DATA) \
	replay-log-icon.png
