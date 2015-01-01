# 
# libjt
# 
# Access Google YouTube ABI.
# 
# BSD License
# 
# Copyright Juergen Urban
# 
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
RANLIB=$(CROSS_COMPILE)ranlib
AS=$(CROSS_COMPILE)as
OBJDUMP=$(CROSS_COMPILE)objdump
OBJCOPY=$(CROSS_COMPILE)objcopy
STRIP=$(CROSS_COMPILE)strip

MACHINE = $(shell $(CC) -dumpmachine)
LIBJTDIR = $(LIBJTBASEDIR)/libjt/lib-$(MACHINE)
LIBJT = $(LIBJTDIR)/libjt.a

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration
CPPFLAGS += -std=gnu99 -Iinclude
CPPFLAGS += -I$(LIBJTBASEDIR)/libjt/include
LDFLAGS += -L$(LIBJTDIR)
LDLIBS += $(LIBJT)
LDLIBS += $(shell pkg-config --libs json openssl libcurl)
CPPFLAGS += $(shell pkg-config --cflags json openssl libcurl)

ifneq ($(shell pkg-config --exists json; echo -n $$?),0)
$(error json not found)
endif
ifneq ($(shell pkg-config --exists libcurl; echo -n $$?),0)
$(error libcurl not found)
endif

CHROOTDIR =
PREFIX = /usr/local
