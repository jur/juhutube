#ifndef _GUI_H_
#define _GUI_H_

struct gui_s;

typedef struct gui_s gui_t;
typedef struct gui_elem_s gui_elem_t;
typedef struct gui_cat_s gui_cat_t;

/**
 * Allocate GUI.
 * @param videofile Videofile for storing information about selected video.
 */
gui_t *gui_alloc(const char *sharedir, int fullscreen, const char *searchterm);
void gui_free(gui_t *gui);
int gui_loop(gui_t *gui, int retval, int origgetstate, const char *videofile, const char *channelid, const char *searchterm, const char *playlistid, const char *catpagetoken, const char *videoid, int catnr, int channelnr, const char *videopagetoken, int vidnr, int menunr, int timer);

#endif
