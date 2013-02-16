
LIBSPOTIFY_PATH ?= $(HOME)/spotify/libspotify-12.1.51-Linux-x86_64-release

UI ?= gtk2

CFLAGS_gtk_common = \
	-DGTK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGTK_MULTIHEAD_SAFE \
	-DGDK_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DG_DISABLE_DEPRECATED \
	-DGSEAL_ENABLE \
	-pthread \
	-I/usr/include/atk-1.0 \
	-I/usr/include/cairo \
	-I/usr/include/gdk-pixbuf-2.0 \
	-I/usr/include/pango-1.0 \
	-I/usr/include/glib-2.0 \


LDFLAGS_gtk_common = \
	-lgobject-2.0 \
	-lglib-2.0


CFLAGS_gtk2 = $(CFLAGS_gtk_common) \
	-I/usr/include/gtk-2.0 \
	-I/usr/lib/x86_64-linux-gnu/gtk-2.0/include \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \

LDFLAGS_gtk2 = $(LDFLAGS_gtk_common) \
	-lgtk-x11-2.0 \
	-lgdk-x11-2.0


CFLAGS_gtk3 = $(CFLAGS_gtk_common) \
	-I/usr/include/gtk-3.0 \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \

LDFLAGS_gtk3 = $(LDFLAGS_gtk_common) \
	-lgtk-3 \
	-lgdk-3 \

CFLAGS += -g -Wall \
	$(CFLAGS_$(UI)) \
	-I$(LIBSPOTIFY_PATH)/include \


LDFLAGS += -L$(LIBSPOTIFY_PATH)/lib -Wl,--rpath=$(LIBSPOTIFY_PATH)/lib \
	-lspotify \
	-lpthread \
	$(LDFLAGS_$(UI)) \
	-lasound \


.PHONY: clean all

all: spthui

spthui: audio.o item.o popup.o playback_panel.o login_dialog.o spthui.o

clean:
	$(RM) spthui *.o
