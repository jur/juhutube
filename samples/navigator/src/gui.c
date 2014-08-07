#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include "log.h"
#include "gui.h"
#include "transfer.h"
#include "pictures.h"


struct gui_elem_s {
	SDL_Surface *image;
	const char *url;
	gui_elem_t *prev;
	gui_elem_t *next;
};

struct gui_cat_s {
	gui_elem_t *elem;
	gui_elem_t *current;
	gui_cat_t *prev;
	gui_cat_t *next;
};

struct gui_s {
	/** YouTube logo. */
	SDL_Surface *logo;
	/** Output screen */
	SDL_Surface *screen;
	/** Position of YouTube logo on screen. */
	SDL_Rect logorect;
	/** Pointer to font used to write something on the screen. */
	TTF_Font *font;

	/** The heigth of the letters in the youtube logo. */
	int mindistance;

	/** Handle for transfering web content. */
	transfer_t *transfer;

	/** Categories chown in GUI. */
	gui_cat_t *categories;
	gui_cat_t *current;
};

/** Load YouTube logo. */
static SDL_Surface *get_youtube_logo(void)
{
	SDL_RWops *rw = SDL_RWFromMem(&_binary_pictures_yt_powered_jpg_start, (long) &_binary_pictures_yt_powered_jpg_size);

	return IMG_Load_RW(rw, 1);
}

/** Initialize graphic. */
gui_t *gui_alloc(void)
{
	gui_t *gui;

	gui = malloc(sizeof(*gui));
	if (gui == NULL) {
		LOG_ERROR("Out of memory.\n");
		return NULL;
	}
	memset(gui, 0 , sizeof(*gui));
	gui->mindistance = 34;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Couldn't initialize SDL: %s\n", SDL_GetError());
		return NULL;
	}

	SDL_ShowCursor(SDL_DISABLE);

	TTF_Init();

	gui->logo = get_youtube_logo();
	if (gui->logo == NULL) {
		LOG_ERROR("Failed to load youtube logo.\n");
		gui_free(gui);
		return NULL;
	}

	gui->font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 36);
	if (gui->font == NULL) {
		gui_free(gui);
		return NULL;
	}

	gui->screen = SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE);
	if (gui->screen == NULL) {
		LOG_ERROR("Couldn't set 640x480x8 video mode: %s\n", SDL_GetError());
		gui_free(gui);
		return NULL;
	}

	gui->logorect.x = gui->screen->w - gui->logo->w - gui->mindistance;
	gui->logorect.y = gui->screen->h - gui->logo->h - gui->mindistance;

	atexit(SDL_Quit);
	atexit(TTF_Quit);

	gui->transfer = transfer_alloc();
	if (gui->transfer == NULL) {
		LOG_ERROR("Out of memory\n");
		gui_free(gui);
		return NULL;
	}

	return gui;
}

/** Clean up of graphic libraries. */
void gui_free(gui_t *gui)
{
	if (gui != NULL) {
		if (gui->transfer != NULL) {
			transfer_free(gui->transfer);
			gui->transfer = NULL;
		}
		if (gui->font != NULL) {
			TTF_CloseFont(gui->font);
			gui->font = NULL;
		}
		if (gui->logo != NULL) {
			SDL_FreeSurface(gui->logo);
			gui->logo = NULL;
		}
		if (gui->screen != NULL) {
			SDL_FreeSurface(gui->screen);
			gui->screen = NULL;
		}
		TTF_Quit();
		SDL_Quit();
		free(gui);
		gui = NULL;
	}
}

/**
 * Paint GUI.
 */
void gui_paint(gui_t *gui)
{
	SDL_Surface *sText = NULL;
	SDL_Rect rcDest = { 40 /* X pos */, 90 /* Y pos */, 0, 0 };
	gui_cat_t *cat;

	SDL_FillRect(gui->screen, NULL, 0x000000);

	SDL_BlitSurface(gui->logo, NULL, gui->screen, &gui->logorect);

	cat = gui->current;
	while (cat != NULL) {
		gui_elem_t *current;
		int maxHeight = 0;

		rcDest.x = 40;

		current = cat->current;
		while (current != NULL) {
			if (maxHeight < current->image->h) {
				maxHeight = current->image->h;
			}
			SDL_BlitSurface(current->image, NULL, gui->screen, &rcDest);

			rcDest.x += current->image->w + 40;
			if (rcDest.x >= gui->screen->w) {
				break;
			}

			current = current->next;
			if (current == cat->current) {
				/* Stop when it repeats. */
				break;
			}
		}
		rcDest.y += maxHeight + 40;
		if (rcDest.y >= gui->screen->h) {
			break;
		}
		cat = cat->next;
		if (cat == gui->current) {
			/* Stop when it repeats. */
			break;
		}
	}

	if (sText != NULL) {
		SDL_Rect rcDest = {40, 40, 0, 0};
		SDL_BlitSurface(sText, NULL, gui->screen, &rcDest);
		SDL_FreeSurface(sText);
		sText = NULL;
	}

	/* Update the screen content. */
	SDL_UpdateRect(gui->screen, 0, 0, gui->screen->w, gui->screen->h);
}

/**
 * Print text to an SDL surface.
 *
 * @param image This object is freed.
 * @param format printf-like format string.
 *
 * @returns SDL surface which should replace the image.
 */
SDL_Surface *gui_printf(gui_t *gui, SDL_Surface *image, const char *format, ...)
{
	va_list ap;
	SDL_Color clrFg = {255, 255, 255, 0}; /* White */
	char *text = NULL;
	int ret;

	if (image != NULL) {
		SDL_FreeSurface(image);
		image = NULL;
	}

	va_start(ap, format);
	ret = vasprintf(&text, format, ap);
	va_end(ap);
	if (ret == -1) {
		text = NULL;
		return NULL;
	}

	return TTF_RenderText_Solid(gui->font, text, clrFg);
}

void gui_inc_cat(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;
	if (cat != NULL) {
		gui->current = cat->next;
	}
}

void gui_dec_cat(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;
	if (cat != NULL) {
		gui->current = cat->prev;
	}
}

void gui_inc_elem(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;
	if (cat != NULL) {
		if (cat->current != NULL) {
			cat->current = cat->current->next;
		}
	}
}

void gui_dec_elem(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;
	if (cat != NULL) {
		if (cat->current != NULL) {
			cat->current = cat->current->prev;
		}
	}
}

/**
 * Main loop for GUI.
 */
void gui_loop(gui_t *gui)
{
	int done;
	SDL_Event event;

	done = 0;
	while(!done) {
		/* Check for events */
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_KEYDOWN:
					/* Key pressed on keyboard. */
					switch(event.key.keysym.sym) {
						case SDLK_ESCAPE:
						case SDLK_SPACE:
						case SDLK_RETURN:
						case SDLK_q:
							/* Quit */
							done = 1;
							break;

						case SDLK_LEFT:
							gui_dec_elem(gui);
							break;

						case SDLK_RIGHT:
							gui_inc_elem(gui);
							break;

						case SDLK_UP:
							gui_dec_cat(gui);
							break;

						case SDLK_DOWN:
							gui_inc_cat(gui);
							break;
	
						default:
							break;
					}
					break;

				default:
					break;

			}
		}

		/* Paint GUI elements. */
		gui_paint(gui);
	}
}

SDL_Surface *gui_load_image(transfer_t *transfer, const char *url)
{
	SDL_Surface *image = NULL;
	void *mem = NULL;
	int size;

	size = transfer_binary(transfer, url, &mem);
	if ((size > 0) && (mem != NULL)) {
		SDL_RWops *rw = SDL_RWFromMem(mem, size);
		image = IMG_Load_RW(rw, 1);
		rw = NULL;
		free(mem);
		mem = NULL;
	}
	return image;
}

gui_cat_t *gui_cat_alloc(gui_t *gui)
{
	gui_cat_t *rv;
	gui_cat_t *last;

	rv = malloc(sizeof(*rv));
	if (rv == NULL) {
		return NULL;
	}
	memset(rv, 0, sizeof(*rv));

	if (gui->current == NULL) {
		gui->current = rv;
	}
	if (gui->categories == NULL) {
		rv->next = rv;
		rv->prev = rv;
		gui->categories = rv;
	} else {
		last = gui->categories->prev;

		rv->next = last->next;
		rv->prev = last;

		last->next = rv;
		gui->categories->prev = rv;
	}

	return rv;
}

gui_elem_t *gui_elem_alloc(gui_t *gui, gui_cat_t *cat, const char *url)
{
	gui_elem_t *rv;
	gui_elem_t *last;

	rv = malloc(sizeof(*rv));
	if (rv == NULL) {
		return NULL;
	}
	memset(rv, 0, sizeof(*rv));
	rv->url = strdup(url); // TBD: free

	if (cat->current == NULL) {
		cat->current = rv;
	}
	if (cat->elem == NULL) {
		rv->next = rv;
		rv->prev = rv;
		cat->elem = rv;
	} else {
		last = cat->elem->prev;

		rv->next = last->next;
		rv->prev = last;

		last->next = rv;
		cat->elem->prev = rv;
	}
	rv->image = gui_load_image(gui->transfer, rv->url); // TBD: Delayed load.

	return rv;
}


