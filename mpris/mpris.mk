
TOP ?= mpris

ifneq ($(MPRIS),no)
	ifeq ($(shell pkg-config --exists dbus-1 && echo "yes"),yes)
		MPRIS := yes
	else
		MPRIS := no
	endif
endif

ifeq ($(MPRIS),yes)
	CFLAGS += -DMPRIS=$(MPRIS) $(shell pkg-config --cflags dbus-1)
	LDFLAGS += $(shell pkg-config --libs dbus-1)
	OBJS += $(TOP)/mpris.o $(TOP)/symbol.o $(TOP)/debug.o $(TOP)/symtab.o \
		$(TOP)/introspect.o $(TOP)/connect.o $(TOP)/properties.o \
		$(TOP)/message.o $(TOP)/mediaplayer2.o
endif
