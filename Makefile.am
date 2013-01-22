SUBDIRS = replay plugins ui pixmaps docs data po

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo 'Creating ChangeLog' && \
		                             ( cd "$(top_secdir)" && \
		                               echo '# Generate at dist by Makefile. Do not edit.'; \
			                             echo; \
			                             $(top_srcdir)/missing --run git log --stat ...0.1.0 ) > ChangeLog.tmp \
		                          && mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		                          || ( rm -f ChangeLog.tmp; \
		                               echo 'Failed to generate ChangeLog' >&2 ); \
	else \
		echo 'A git clone is required to generate a ChangeLog' >&2; \
	fi

ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST 	= ChangeLog

CLEANFILES = *~

DISTCHECK_CONFIGURE_FLAGS =	--disable-database-updates \
				--enable-gtk-doc \
				--enable-introspection
