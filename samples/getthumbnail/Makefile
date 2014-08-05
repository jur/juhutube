LIBJTBASEDIR = ../..
include $(LIBJTBASEDIR)/libjt.mk

MACHINE = $(shell $(CC) -dumpmachine)
BUILDROOT_MACHINE = $(shell echo "$(MACHINE)" | grep -e buildroot)
OBJDIR = obj-$(MACHINE)
DEPDIR = dep-$(MACHINE)
BINDIR = bin-$(MACHINE)
TESTDIR = test-$(MACHINE)
PICTURES = yt_powered
MODS = getthumbnail $(PICTURES)
OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(MODS)))
DEPS = $(addprefix $(DEPDIR)/,$(addsuffix .d,$(MODS)))


PROGRAM = $(BINDIR)/getthumbnail

CPPFLAGS += -I../include
CPPFLAGS += -DDEBUG -g
LDLIBS += -lSDL -lSDL_image -lSDL_ttf
BFDTARGET = $(shell LANG=C $(OBJCOPY) --help | grep -e "supported targets:" | sed -e 's-^.*supported targets: *--g' | cut -d ' ' -f 1)
BFDARCH = $(shell LANG=C $(OBJDUMP) --help | grep -e "supported architectures:" | sed -e 's-^.*supported architectures: *--g' | cut -d ' ' -f 1)

.PHONY: test all clean

ifeq ($(BUILDROOT_MACHINE),)
test: all
	mkdir -p $(TESTDIR)
	(cd $(TESTDIR) && ../$(PROGRAM))
endif

all: $(PROGRAM)
	echo DEPS $(DEPS)

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

$(OBJDIR)/%.o: pictures/%.jpg
	@mkdir -p $(OBJDIR)
	$(OBJCOPY) -I binary -O $(BFDTARGET) -B $(BFDARCH) $< $@

-include $(DEPS)
