.PHONY: all install clean

SUBDIRS = libjt samples

test: all
	#$(MAKE) -C samples/getvideolist $@
	#$(MAKE) -C samples/getthumbnail $@
	$(MAKE) -C samples/navigator $@

all clean install:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@ || exit 1; done
