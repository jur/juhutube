LIBJTBASEDIR = ../..
include $(LIBJTBASEDIR)/libjt.mk

MACHINE = $(shell $(CC) -dumpmachine)
HOSTMACHINE = $(shell uname -m)
BUILDHOSTMACHINE = $(shell echo "$(MACHINE)" | grep -e "$(HOSTMACHINE)")
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
BFDTARGET = $(shell LANG=C $(OBJCOPY) --help | grep -e "supported targets:" | sed -e 's-^.*supported targets: *--g' | cut -d ' ' -f 1)
BFDARCH = $(shell LANG=C $(OBJDUMP) --help | grep -e "supported architectures:" | sed -e 's-^.*supported architectures: *--g' | cut -d ' ' -f 1)

ifneq ($(shell pkg-config --exists sdl; echo -n $$?),0)
$(error sdl not found)
else
PKGS += sdl
endif
ifneq ($(shell pkg-config --exists SDL_image; echo -n $$?),0)
$(error SDL_image not found)
else
PKGS += SDL_image
endif
ifneq ($(shell pkg-config --exists SDL_ttf; echo -n $$?),0)
#$(error SDL_ttf not found)
LDLIBS += -lSDL_ttf
else
PKGS += SDL_ttf
endif
CPPFLAGS += $(shell pkg-config --cflags $(PKGS))
LDLIBS += $(shell pkg-config --libs $(PKGS))

.PHONY: test all install clean

ifneq ($(BUILDHOSTMACHINE),)
test: all
	mkdir -p $(TESTDIR)
	(cd $(TESTDIR) && ../$(PROGRAM))
endif

all: $(PROGRAM)

install: all
	install -D $(PROGRAM) "$(PREFIX)/bin/"

$(PROGRAM): $(OBJS) $(filter %.a,$(LDLIBS)) $(filter %.o,$(LDLIBS))
	@mkdir -p $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(filter-out %.o,$(filter-out %.a,$(LDLIBS)))

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
