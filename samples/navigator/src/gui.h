#ifndef _GUI_H_
#define _GUI_H_

struct gui_s;

typedef struct gui_s gui_t;
typedef struct gui_elem_s gui_elem_t;
typedef struct gui_cat_s gui_cat_t;

gui_t *gui_alloc(void);
void gui_free(gui_t *gui);
void gui_loop(gui_t *gui);
gui_cat_t *gui_cat_alloc(gui_t *gui);
gui_elem_t *gui_elem_alloc(gui_t *gui, gui_cat_t *cat, const char *url);
void update_navigator(gui_t *gui);

#endif
