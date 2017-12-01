if USE_SCRIPT
SCRIPT_FILES = \
	script/orz.py \
	script/eliza.py \
	script/PCManXlib.py
endif

if USE_NANCY
NANCY_FILES = \
	nancy_bot/default.conf \
	nancy_bot/default_msg.data \
	nancy_bot/default_usages.data \
	nancy_bot/example.conf \
	nancy_bot/example_msg.data \
	nancy_bot/example_usages.data
endif

gemanx_DATA = \
	sitelist \
	emoticons

gemanxdir = $(datadir)/gemanx

gemanx_script_DATA = $(SCRIPT_FILES)
gemanx_scriptdir = $(datadir)/gemanx/script

gemanx_nancy_DATA = $(NANCY_FILES)
gemanx_nancydir = $(datadir)/gemanx/nancy_bot

EXTRA_DIST = \
	$(gemanx_DATA) \
	$(gemanx_script_DATA) \
	$(gemanx_nancy_DATA) \
	gemanx.png
