LIBJTBASEDIR = ../..
include $(LIBJTBASEDIR)/libjt.mk

MACHINE = $(shell $(CC) -dumpmachine)
HOSTMACHINE = $(shell uname -m | sed -e 's-armv7l-arm-g')
BUILDHOSTMACHINE = $(shell echo "$(MACHINE)" | grep -e "$(HOSTMACHINE)")
OBJDIR = obj-$(MACHINE)
DEPDIR = dep-$(MACHINE)
BINDIR = bin-$(MACHINE)
TESTDIR = test-$(MACHINE)
PICTURES = yt_powered
MODS = navigator log transfer gui
OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(MODS)))
DEPS = $(addprefix $(DEPDIR)/,$(addsuffix .d,$(MODS)))


PROGRAM = $(BINDIR)/ytnavigator

CPPFLAGS += -I../include
CPPFLAGS += -DDEBUG -g
BFDTARGET = $(shell LANG=C $(OBJCOPY) --help | grep -e "supported targets:" | sed -e 's-^.*supported targets: *--g' | cut -d ' ' -f 1)
BFDARCH = $(shell LANG=C $(OBJDUMP) --help | grep -e "supported architectures:" | sed -e 's-^.*supported architectures: *--g' | cut -d ' ' -f 1)
VERIFYCERT = yes

ifneq ($(VERIFYCERT),yes)
CPPFLAGS += -DNOVERIFYCERT
endif

ifneq ($(shell $(PKG_CONFIG) --exists sdl; echo -n $$?),0)
$(error sdl not found)
else
PKGS += sdl
endif
ifneq ($(shell $(PKG_CONFIG) --exists SDL_image; echo -n $$?),0)
$(error SDL_image not found)
else
PKGS += SDL_image
endif
ifneq ($(shell $(PKG_CONFIG) --exists SDL_ttf; echo -n $$?),0)
#$(error SDL_ttf not found)
LDLIBS += -lSDL_ttf
else
PKGS += SDL_ttf
endif
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags $(PKGS))
LDLIBS += $(shell $(PKG_CONFIG) --libs $(PKGS))

.PHONY: test all clean

ifneq ($(BUILDHOSTMACHINE),)
test: all
	echo $(BUILDHOSTMACHINE)
	mkdir -p $(TESTDIR)
	(cd $(TESTDIR) && bash -x ../youtubeplayer.sh ../$(PROGRAM) ../pictures)
endif

all: $(PROGRAM)

install: all
	install -D $(PROGRAM) "$(CHROOTDIR)$(PREFIX)/bin/"
	sed -i youtubeplayer.sh -e "s#DEFAULTPREFIX=.*\$$#DEFAULTPREFIX=\"$(PREFIX)\"#g"
	install -D youtubeplayer.sh "$(CHROOTDIR)$(PREFIX)/bin/"
	mkdir -p "$(CHROOTDIR)$(PREFIX)/share/ytnavigator"
	install -D pictures/*.jpg "$(CHROOTDIR)$(PREFIX)/share/ytnavigator/"

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
