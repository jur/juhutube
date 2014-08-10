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
BUILDROOT_MACHINE = $(shell echo "$(MACHINE)" | grep -e buildroot)
LIBJTDIR = $(LIBJTBASEDIR)/libjt/lib-$(MACHINE)
LIBJT = $(LIBJTDIR)/libjt.a

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration -Wuninitialized
CPPFLAGS += -std=gnu99 -Iinclude
CPPFLAGS += -I$(LIBJTBASEDIR)/libjt/include
LDFLAGS += -L$(LIBJTDIR)
LDLIBS += $(LIBJT)
LDLIBS += -lcurl
ifeq ($(BUILDROOT_MACHINE),)
LDLIBS += -ljson
else
# buldroot uses a different name for the library
LDLIBS += -ljson-c
CPPFLAGS += -DBUILD_WITH_BUILDROOT=1
endif
LDLIBS += -lssl -lcrypto -ldl -lz
