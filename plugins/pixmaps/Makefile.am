svgappicondir = $(datadir)/icons/hicolor/scalable/apps
svgappicon_DATA = 
appicondir = $(datadir)/icons/hicolor/48x48/apps
appicon_DATA = 

if BUILD_GRAPHVIZ
svgappicon_DATA += dot.svg
appicon_DATA += dot.png
endif

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update_icon_cache
uninstall-hook: update_icon_cache
update_icon_cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = \
	$(svgappicon_DATA) \
	$(appicon_DATA)
