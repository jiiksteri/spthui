
LIBSPOTIFY_PATH ?= $(HOME)/spotify/libspotify-12.1.51-Linux-x86_64-release

CFLAGS += -g -Wall \
	-I/usr/lib/x86_64-linux-gnu/gtk-2.0/include \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
	-I/usr/include/gtk-2.0 \
	-I/usr/include/atk-1.0 \
	-I/usr/include/cairo \
	-I/usr/include/gdk-pixbuf-2.0 \
	-I/usr/include/pango-1.0 \
	-I/usr/include/gio-unix-2.0/ \
	-I/usr/include/glib-2.0 \
	-I/usr/include/pixman-1 \
	-I/usr/include/freetype2 \
	-I/usr/include/libpng12 \
	-I$(LIBSPOTIFY_PATH)/include \
	-DGTK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGTK_MULTIHEAD_SAFE \
	-DGDK_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DG_DISABLE_DEPRECATED \
	-DGSEAL_ENABLE \


LDFLAGS += -L$(LIBSPOTIFY_PATH)/lib -Wl,--rpath=$(LIBSPOTIFY_PATH)/lib \
	-lspotify \
	-lpthread \
	-lgtk-x11-2.0 \
	-lgdk-x11-2.0 \
	-lgobject-2.0 \
	-lglib-2.0 \
	-lasound \


.PHONY: clean all

all: spthui

spthui: audio.o spthui.o

clean:
	$(RM) spthui *.o
