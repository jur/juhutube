.PHONY: all clean

SUBDIRS = getvideolist getthumbnail navigator searchvideo

all install clean:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@ || exit 1; done
