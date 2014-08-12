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
JSONC = $(shell LANG=C $(CC) -E -v - </dev/null 2>&1 1>/dev/null | sed -e "1,/\#include <...>/d" -e "/End of search list/,\$$d" | while read dir; do if [ -d $$dir/json-c ]; then echo yes; fi; done)
LIBJTDIR = $(LIBJTBASEDIR)/libjt/lib-$(MACHINE)
LIBJT = $(LIBJTDIR)/libjt.a

CPPFLAGS += -W -Wall -Werror-implicit-function-declaration -Wuninitialized
CPPFLAGS += -std=gnu99 -Iinclude
CPPFLAGS += -I$(LIBJTBASEDIR)/libjt/include
LDFLAGS += -L$(LIBJTDIR)
LDLIBS += $(LIBJT)
LDLIBS += -lcurl
ifeq ($(JSONC),)
LDLIBS += -ljson
else
# buldroot uses a different name for the library
LDLIBS += -ljson-c
CPPFLAGS += -DJSONC=1
endif
LDLIBS += -lssl -lcrypto -ldl -lz
