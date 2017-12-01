localedir = $(datadir)/locale

lib_LTLIBRARIES = libgemanx_core.la

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/view \
	$(GUI_CFLAGS) \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\"

if USE_PROXY
PROXY_SOURCES = \
	proxy.c proxy.h
endif

libgemanx_core_la_LIBADD = $(GUI_LIBS)
libgemanx_core_la_LDFLAGS = \
	-version-info @LIBTOOL_VERSION_INFO@ \
	-export-dynamic \
	@LIBTOOL_EXPORT_OPTIONS@ \
	-rpath $(libdir)
libgemanx_core_la_SOURCES = \
	$(PROXY_SOURCES) \
	gemanx_utils.h \
	caret.cpp caret.h \
	site.cpp site.h \
	termdata.cpp termdata.h \
	view.cpp view.h \
	font.cpp font.h \
	stringutil.cpp stringutil.h \
	widget.cpp widget.h \
	termview.cpp termview.h \
	termsel.cpp termsel.h \
	fileutil.c fileutil.h \
	pkgver.c \
	encoding.cpp encoding.h
