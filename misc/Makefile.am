## Process this file with automake to create Makefile.in.

SUBDIRS = ext.d macros.d skins syntax

LIBFILES_OUT = mc.ext

noinst_DATA = xterm.ad

pkgdata_DATA = \
	mc.lib
if CHARSET
pkgdata_DATA += mc.charsets
endif

SCRIPTS_IN = \
	mc.menu.in \
	mcedit.menu.in

SCRIPTS_OUT = \
	mc.menu \
	mcedit.menu

LIBFILES_CONST = \
	filehighlight.ini \
	mc.default.keymap \
	mc.emacs.keymap

LIBFILES_SCRIPT = \
	edit.indent.rc

cfgdir = $(sysconfdir)/@PACKAGE@
cfg_DATA = \
	$(LIBFILES_CONST) \
	$(SCRIPTS_OUT) \
	$(LIBFILES_OUT)

cfg_SCRIPTS = \
	$(LIBFILES_SCRIPT)

iconsdir = $(datadir)/icons/hicolor/scalable/apps
icons_DATA = \
	mc.svg

desktopdir = $(datadir)/applications
desktop_DATA = \
	mc.desktop \
	mcedit.desktop
dist_noinst_DATA = \
	mc.desktop.in \
	mcedit.desktop.in

# We're not using the INTLTOOL_DESKTOP_RULE macro because it expects us
# to run intltoolize on the source tree and it breaks when builddir != srcdir.
# (https://mail.gnome.org/archives/commits-list/2010-October/msg05148.html)
%.desktop: %.desktop.in
	$(MKDIR_P) $(dir $@)
	LC_ALL=C $(INTLTOOL_MERGE) --desktop-style --utf8 --cache $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(LIBFILES_SCRIPT) \
	$(SCRIPTS_IN) \
	$(noinst_DATA) \
	$(pkgdata_DATA) \
	$(icons_DATA) \
	$(desktop_DATA)
#if CHARSET is defined, mc.charsets is already here
if !CHARSET
EXTRA_DIST += mc.charsets
endif

CLEANFILES = \
	$(SCRIPTS_OUT) \
	$(desktop_in_files) \
	$(desktop_DATA)

install-data-hook:
	cd $(DESTDIR)$(cfgdir) && rm -f mc.keymap && $(LN_S) mc.default.keymap mc.keymap

uninstall-hook:
	cd $(DESTDIR)$(cfgdir) && rm -f mc.keymap
