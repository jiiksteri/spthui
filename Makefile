
UI ?= gtk2

PKGCONFIG_packages = libspotify alsa $(PKGCONFIG_$(UI))

PKGCONFIG_gtk2 = gtk+-2.0
PKGCONFIG_gtk3 = gtk+-3.0

CFLAGS_gtk2 = $(CFLAGS_gtk_common)
LDFLAGS_gtk2 = $(LDFLAGS_gtk_common)

CFLAGS_gtk3 = $(CFLAGS_gtk_common)
LDFLAGS_gtk3 = $(LDFLAGS_gtk_common)

OBJS += \
	audio.o \
	titles.o \
	item.o \
	view.o \
	popup.o \
	playback_panel.o \
	login_dialog.o \
	tabs.o \
	search.o \
	image.o \
	spthui.o

CFLAGS_gtk_common = \
	-DGTK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGTK_MULTIHEAD_SAFE \
	-DGDK_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DG_DISABLE_DEPRECATED \
	-DGSEAL_ENABLE \



CFLAGS := $(CFLAGS) -g -Wall \
	$(shell pkg-config --cflags $(PKGCONFIG_packages)) \
	$(CFLAGS_$(UI)) \

LDFLAGS := $(LDFLAGS) $(shell pkg-config --libs $(PKGCONFIG_packages)) \
	-lpthread \
	$(LDFLAGS_$(UI)) \


.PHONY: clean all

all: spthui

spthui: $(OBJS)

clean:
	$(RM) spthui $(OBJS)
