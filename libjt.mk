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
#CPPFLAGS += -DDEBUG
CPPFLAGS += -I$(LIBJTBASEDIR)/libjt/include
LDFLAGS += -L$(LIBJTDIR)
LDLIBS += $(LIBJT)

PKG_CONFIG ?= pkg-config
PKGS =

ifeq ($(shell $(PKG_CONFIG) --exists json-c; echo -n $$?),0)
JSON = json-c
else
JSON = json
endif

ifneq ($(shell $(PKG_CONFIG) --exists $(JSON); echo -n $$?),0)
$(error $(JSON) not found)
else
PKGS += $(JSON)
endif

ifneq ($(shell $(PKG_CONFIG) --exists libcurl; echo -n $$?),0)
$(error libcurl not found)
else
PKGS += libcurl
endif
ifneq ($(shell $(PKG_CONFIG) --exists openssl; echo -n $$?),0)
#$(error openssl not found)
LDLIBS += -lssl
else
PKGS += openssl
endif

LDLIBS += $(shell $(PKG_CONFIG) --libs $(PKGS))
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags $(PKGS))

CHROOTDIR =
PREFIX = /usr/local
