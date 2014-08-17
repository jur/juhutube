LIBJTBASEDIR = ..
include $(LIBJTBASEDIR)/libjt.mk

OBJDIR = obj-$(MACHINE)
DEPDIR = dep-$(MACHINE)
MODS = libjt
OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(MODS)))
DEPS = $(addprefix $(DEPDIR)/,$(addsuffix .d,$(MODS)))

# Enable debug
#CPPFLAGS += -DDEBUG
CPPFLAGS += -g

.PHONY: test all install clean

all: $(LIBJT)

install: all
	mkdir -p "$(CHROOTDIR)$(PREFIX)/include/"
	install -D include/libjt.h "$(CHROOTDIR)$(PREFIX)/include/"
	install -D $(LIBJT) "$(CHROOTDIR)$(PREFIX)/lib/"

$(LIBJT): $(OBJS)
	@mkdir -p $(LIBJTDIR)
	$(AR) rcs $@ $^

clean:
	rm -rf $(LIBJTDIR) $(OBJDIR) $(DEPDIR)

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<
	@mkdir -p $(DEPDIR)
	$(CC) -MM -MT $@ $(CPPFLAGS) $(CFLAGS) -o $(DEPDIR)/$*.d $^

-include $(DEPS)
