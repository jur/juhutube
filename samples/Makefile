.PHONY: all clean

SUBDIRS = getvideolist getthumbnail navigator

all clean:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@ || exit 1; done
