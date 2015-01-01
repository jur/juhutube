LIBJTBASEDIR = ../..
include $(LIBJTBASEDIR)/libjt.mk

MACHINE = $(shell $(CC) -dumpmachine)
HOSTMACHINE = $(shell uname -m)
BUILDHOSTMACHINE = $(shell echo "$(MACHINE)" | grep -e "$(HOSTMACHINE)")
OBJDIR = obj-$(MACHINE)
DEPDIR = dep-$(MACHINE)
BINDIR = bin-$(MACHINE)
TESTDIR = test-$(MACHINE)
MODS = getvideolist
OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(MODS)))


PROGRAM = $(BINDIR)/getvideolist

CPPFLAGS += -I../include
CPPFLAGS += -DDEBUG -g

.PHONY: test all install clean

ifneq ($(BUILDHOSTMACHINE),)
test: all
	mkdir -p $(TESTDIR)
	(cd $(TESTDIR) && ../$(PROGRAM))
endif

all: $(PROGRAM)

install: all
	install -D $(PROGRAM) "$(CHROOTDIR)$(PREFIX)/bin/"

$(PROGRAM): $(OBJS) $(filter %.a,$(LDLIBS))
	@mkdir -p $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(filter-out %.a,$(LDLIBS))

clean:
	rm -rf $(BINDIR) $(OBJDIR) $(DEPDIR)

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<
	@mkdir -p $(DEPDIR)
	$(CC) -MM -MT $@ $(CPPFLAGS) $(CFLAGS) -o $(DEPDIR)/$*.d $^

-include $(DEPS)
