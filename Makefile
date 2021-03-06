
# If UI=... is not specified we probe for latest gtk.
ifeq ($(UI),)
	UI := $(shell pkg-config gtk+-3.0 && echo "gtk3")
endif
ifeq ($(UI),)
	UI := $(shell pkg-config gtk+-2.0 && echo "gtk2")
endif
ifeq ($(UI),)
	# This most likely won't work.
	UI := gtk2
endif

PKGCONFIG_packages = libspotify alsa $(PKGCONFIG_$(UI))

PKGCONFIG_gtk2 = gtk+-2.0
PKGCONFIG_gtk3 = gtk+-3.0

CFLAGS_gtk2 = $(CFLAGS_gtk_common)
LDFLAGS_gtk2 = $(LDFLAGS_gtk_common)

CFLAGS_gtk3 = $(CFLAGS_gtk_common)
LDFLAGS_gtk3 = $(LDFLAGS_gtk_common)


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

OBJS := \
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
	compat_$(UI).o \
	spthui.o

spthui: $(OBJS)

-include $(OBJS:.o=.d)

clean:
	$(RM) spthui *.o *.d

#
# Dependency generation algorithm lifted from:
#
#  Autodependencies with GNU make
#  http://scottmcpeak.com/autodepend/autodepend.html
#
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	@$(CC) -MM $(CFLAGS) $*.c > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
		sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@$(RM) $*.d.tmp
