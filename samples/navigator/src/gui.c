#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include "log.h"
#include "gui.h"
#include "transfer.h"
#include "pictures.h"
#include "clientid.h"
#include "libjt.h"

#define TOKEN_FILE "youtubetoken.json"
#define REFRESH_TOKEN_FILE "refreshtoken.json"

/** Maximum images loaded while painting. */
#define MAX_LOAD_WHILE_PAINT 1
/** Maximum retry count for images. */
#define IMG_LOAD_RETRY 5
/** Special value for image loaded successfully. */
#define IMG_LOADED 10
#define DEFAULT_SLEEP (4 * 50)

#define BORDER_X 40
#define BORDER_Y 40
#define DIST_FORCED_LINE_BREAK 10

/** Maximum value for scroll counter, slows down the scrolling of text. */
#ifdef __mipsel__
#define SCROLL_COUNT_MAX 7
#else
#define SCROLL_COUNT_MAX 20
#endif
/** Start value for counting frames until text is scrolled by one pixel. */
#ifdef __mipsel__
#define SCROLL_COUNT_START 6
#else
#define SCROLL_COUNT_START 16
#endif
/** Same as SCROLL_COUNT_START but when the end of the text is reached and it
 * should scroll into the other direction. This should be a smaller value, so
 * the scrolling shortly stops. So the user has time to read the text completely.
 * Otherwise the outsider letters would only be seen for a very short time. */
#define SCROLL_COUNT_END 0

#define CASESTATE(state) \
	case state: \
		return #state;

/** States for GUI state machine. */
enum gui_state {
	GUI_STATE_SLEEP,
	GUI_STATE_STARTUP,
	GUI_STATE_LOAD_ACCESS_TOKEN,
	GUI_STATE_GET_USER_CODE,
	GUI_STATE_GET_TOKEN,
	GUI_STATE_LOGGED_IN,
	GUI_STATE_ERROR,
	GUI_RESET_STATE,
	GUI_STATE_INIT,
	GUI_STATE_GET_FAVORITES,
	GUI_STATE_GET_PLAYLIST,
	GUI_STATE_GET_SUBSCRIPTIONS,
	GUI_STATE_GET_CHANNELS,
	GUI_STATE_RUNNING,
};

const char *get_state_text(enum gui_state state)
{
	switch(state) {
		CASESTATE(GUI_STATE_SLEEP)
		CASESTATE(GUI_STATE_STARTUP)
		CASESTATE(GUI_STATE_LOAD_ACCESS_TOKEN)
		CASESTATE(GUI_STATE_GET_USER_CODE)
		CASESTATE(GUI_STATE_GET_TOKEN)
		CASESTATE(GUI_STATE_LOGGED_IN)
		CASESTATE(GUI_STATE_ERROR)
		CASESTATE(GUI_RESET_STATE)
		CASESTATE(GUI_STATE_INIT)
		CASESTATE(GUI_STATE_GET_FAVORITES)
		CASESTATE(GUI_STATE_GET_PLAYLIST)
		CASESTATE(GUI_STATE_GET_SUBSCRIPTIONS)
		CASESTATE(GUI_STATE_GET_CHANNELS)
		CASESTATE(GUI_STATE_RUNNING)
	}
	return "unknown";
}

/** The structure describes a YouTube video (playlist item). */
struct gui_elem_s {
	/** Small thumbnail of video. */
	SDL_Surface *image;
	/** Medium size thumbnail of video. */
	SDL_Surface *imagemedium;
	/** Load counter for small thumbnail of image. */
	int loaded;
	/** Load counter for medium size thumbnail of image. */
	int loadedmedium;
	/** URL to small thumbnail. */
	const char *url;
	/** URL to medium size thumbnail. */
	const char *urlmedium;
	/** YouTube video ID. */
	const char *videoid;
	/** Pointer to next video in list. */
	gui_elem_t *prev;
	/** Pointer to previous video in list. */
	gui_elem_t *next;
	/** Video title */
	char *title;
	/** Video nr in playlist. */
	int subnr;
	/** Token to get the next elements via the YouTube API. */
	char *nextPageToken;
	/** Token to get the previous elements via the YouTube API. */
	char *prevPageToken;
	/** Scroll position of text. */
	int textScrollPos;
	/** Scroll direction. */
	int textScrollDir;
	/** Counter to delay text scrolling. */
	int textScrollCounter;
};

/** The structure describes a category (e.g. favorites or subscribed channel). */
struct gui_cat_s {
	/** First element of playlist items (YouTube videos). */
	gui_elem_t *elem;
	/** Currently selected YouTube video. */
	gui_elem_t *current;
	/** YouTube channel ID. */
	char *channelid;
	/** YouTube playlist ID. */
	char *playlistid;
	/** Pointer to previous category in list. */
	gui_cat_t *prev;
	/** Pointer to next category in list. */
	gui_cat_t *next;
	/** Subscription/Channel/Playlist title. */
	char *title;
	/** Number of the channel in the list. */
	int channelNr;
	/** Token to get the next channel via the YouTube API. */
	char *channelNextPageToken;
	/** Number of the favorite. */
	int favnr;
	/** Token to get the next favorite via the YouTube API. */
	char *favoritesNextPageToken;
	/** Number of the subscription. */
	int subnr;
	/** Next page to get more subscriptions. */
	char *subscriptionNextPageToken;
	/** Previous page to get more subscriptions. */
	char *subscriptionPrevPageToken;
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
	/** Pointer to small font used to write something on the screen. */
	TTF_Font *smallfont;

	/** The height of the letters in the youtube logo. */
	int mindistance;

	/** Handle for transfering web content. */
	transfer_t *transfer;

	/** Categories chown in GUI. Pointer to first element.
	 * NULL if empty. categories->prev points to last element.
	 */
	gui_cat_t *categories;
	/** Current selected category. */
	gui_cat_t *current;

	/** YouTube access. */
	jt_access_token_t *at;

	/** Status */
	char *statusmsg;

	/** Category currently updated. */
	gui_cat_t *cur_cat;
};

/**
 * Like asprintf, but frees buffer if it is not NULL.
 */
static char *buf_printf(char *buffer, const char *format, ...)
{
	va_list ap;
	int ret;

	if (buffer != NULL) {
		free(buffer);
		buffer = NULL;
	}

	va_start(ap, format);
	ret = vasprintf(&buffer, format, ap);
	va_end(ap);
	if (ret == -1) {
		buffer = NULL;
		return NULL;
	}

	return buffer;
}

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

	gui->smallfont = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 16);
	if (gui->smallfont == NULL) {
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

	gui->at = jt_alloc(logfd, errfd, CLIENT_ID, CLIENT_SECRET, TOKEN_FILE, REFRESH_TOKEN_FILE, 0);
	if (gui->at == NULL) {
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
		if (gui->statusmsg != NULL) {
			free(gui->statusmsg);
			gui->statusmsg = NULL;
		}
		if (gui->at != NULL) {
			jt_free(gui->at);
			gui->at =  NULL;
		}
		if (gui->transfer != NULL) {
			transfer_free(gui->transfer);
			gui->transfer = NULL;
		}
		if (gui->smallfont != NULL) {
			TTF_CloseFont(gui->smallfont);
			gui->smallfont = NULL;
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


void gui_paint_cat_view(gui_t *gui)
{
	SDL_Rect rcDest = { BORDER_X /* X pos */, 90 /* Y pos */, 0, 0 };
	gui_cat_t *cat;
	int load_counter;
	SDL_Color clrFg = {255, 255, 255, 0}; /* White */

	load_counter = 0;
	cat = gui->current;
	if (cat != NULL) {
		SDL_Surface *sText = NULL;

		if (cat->title != NULL) {
			/* Convert text to an image. */
			sText = TTF_RenderUTF8_Solid(gui->font, cat->title, clrFg);
		}
		if (sText != NULL) {
			SDL_Rect headerDest = {40, 40, 0, 0};
			SDL_BlitSurface(sText, NULL, gui->screen, &headerDest);
			SDL_FreeSurface(sText);
			sText = NULL;
		}
	}
	while (cat != NULL) {
		gui_elem_t *current;
		int maxHeight = 0;

		rcDest.x = BORDER_X;

		current = cat->current;
		while (current != NULL) {
			SDL_Surface *image;

			if ((cat == gui->current) && (current->urlmedium != NULL)) {
				/* Show medium image size. */
				if ((current->loadedmedium < IMG_LOAD_RETRY) && (load_counter < MAX_LOAD_WHILE_PAINT)) {
					load_counter++;
					current->loadedmedium++;
					/* Delayed load. */
					image = gui_load_image(gui->transfer, current->urlmedium);
					if (image != NULL) {
						current->loadedmedium = IMG_LOADED;
						if (current->imagemedium != NULL) {
							SDL_FreeSurface(current->imagemedium);
							current->imagemedium = NULL;
						}
						current->imagemedium = image;
					}
				}
				if (current->imagemedium == NULL) {
					current->imagemedium = TTF_RenderUTF8_Solid(gui->font, "No Thumbnail", clrFg);
				}
				if ((current->loadedmedium != IMG_LOADED) && (current->loaded == IMG_LOADED)) {
					/* Use small image when medium image is not yet available. */
					image = current->image;
				} else {
					/* Use medium image. */
					image = current->imagemedium;
				}
			} else {
				/* Show medium image size. */
				if ((current->loaded < IMG_LOAD_RETRY) && (load_counter < MAX_LOAD_WHILE_PAINT)) {
					load_counter++;
					current->loaded++;
					/* Delayed load. */
					image = gui_load_image(gui->transfer, current->url);
					if (image != NULL) {
						current->loaded = IMG_LOADED;
						if (current->image != NULL) {
							SDL_FreeSurface(current->image);
							current->image = NULL;
						}
						current->image = image;
					}
				}
				if (current->image == NULL) {
					current->image = TTF_RenderUTF8_Solid(gui->font, "No Thumbnail", clrFg);
				}
				image = current->image;
			}
			if (image != NULL) {
				int overlap_x = 0;
				int overlap_y = 0;

				if (maxHeight < image->h) {
					maxHeight = image->h;
				}
				if (((gui->logorect.x - gui->mindistance) < (rcDest.x + image->w))
					&& ((gui->logorect.x + gui->logo->w + gui->mindistance) > rcDest.x)) {
					overlap_x = 1;
				}
				if (((gui->logorect.y - gui->mindistance) < (rcDest.y + image->h))
					&& ((gui->logorect.y + gui->logo->h + gui->mindistance) > rcDest.y)) {
					overlap_y = 1;
				}
				if (!overlap_x || !overlap_y) {
					/* Show video title for first/selected video. */
					SDL_Surface *sText = NULL;

					SDL_BlitSurface(image, NULL, gui->screen, &rcDest);

					/* Print video title under the image. */
					if (cat->title != NULL) {
						/* Convert text to an image. */
						sText = TTF_RenderUTF8_Solid(gui->smallfont, current->title, clrFg);
					}
					if (sText != NULL) {
						SDL_Rect headerDest = rcDest;
						SDL_Rect headerSrc;
						headerDest.y += image->h + 10;
						if (current->textScrollPos > (sText->w - image->w)) {
							/* This can happen, because there are small and medium size thumbnails.
							 * When the user navigates, the thumbnails can be larger or smaller.
							 */
							current->textScrollPos = 0;
						}
						if ((cat != gui->current) || (current != cat->current)) {
							/* When it is not the selected video, don't scroll the text, because this is
							 * irritating.
							 */
							current->textScrollPos = 0;
						}
						headerSrc.x = current->textScrollPos;
						headerSrc.y = 0;
						if (sText->w > image->w) {
							/* The title is too long, we need to scroll the text. */
							headerSrc.w = image->w;
							current->textScrollCounter++;
							if (current->textScrollCounter >= SCROLL_COUNT_MAX) {
								current->textScrollCounter = SCROLL_COUNT_START;
								if (current->textScrollDir == 0) {
									if (current->textScrollPos >= (sText->w - image->w)) {
										current->textScrollDir = 1;
										current->textScrollCounter = SCROLL_COUNT_END;
									} else {
										current->textScrollPos++;
									}
								} else {
									if (current->textScrollPos == 0) {
										current->textScrollDir = 0;
										current->textScrollCounter = SCROLL_COUNT_END;
									} else {
										current->textScrollPos--;
									}
								}
							}
						} else {
							headerSrc.w = image->w;
						}
						headerSrc.h = image->h;
						SDL_BlitSurface(sText, &headerSrc, gui->screen, &headerDest);
						SDL_FreeSurface(sText);
						sText = NULL;
					}
				}

				rcDest.x += image->w + BORDER_X;
				if (rcDest.x >= gui->screen->w) {
					break;
				}

				current = current->next;
				if (current == cat->elem) {
					/* Don't draw stuff after the last video. */
					break;
				}

				if (current == cat->current) {
					/* Stop when it repeats. */
					break;
				}
			}
		}
		rcDest.y += maxHeight + BORDER_Y;
		if (rcDest.y >= gui->screen->h) {
			break;
		}
		cat = cat->next;
		if (cat == gui->categories) {
			/* Don't draw stuff after the last category. */
			break;
		}

		if (cat == gui->current) {
			/* Stop when it repeats. */
			break;
		}
	}
}

void gui_paint_status(gui_t *gui)
{
	SDL_Color clrFg = {255, 255, 255, 0}; /* White */
	const char *text;
	char t[2];
	SDL_Rect rcDest = {BORDER_X, BORDER_Y, 0, 0};
	int maxHeight = 0;

	text = gui->statusmsg;
	t[0] = 0;
	t[1] = 0;

	while(*text != 0) {
		t[0] = *text;
		if (*text == '\n') {
			rcDest.x = BORDER_X;
			rcDest.y += maxHeight + BORDER_Y;
			maxHeight = 0;
		} else {
			SDL_Surface *sText = NULL;

			sText = TTF_RenderUTF8_Solid(gui->font, t, clrFg);
			if (sText != NULL) {
				if ((rcDest.x + sText->w) >= (gui->screen->w - BORDER_X)) {
					rcDest.x = BORDER_X;
					rcDest.y += maxHeight + DIST_FORCED_LINE_BREAK;
					maxHeight = 0;
				}
				SDL_BlitSurface(sText, NULL, gui->screen, &rcDest);
				rcDest.x += sText->w;
				if (sText->h > maxHeight) {
					maxHeight = sText->h;
				}
				SDL_FreeSurface(sText);
				sText = NULL;
			}
		}
		text++;
	}
}
/**
 * Paint GUI.
 */
void gui_paint(gui_t *gui)
{
	SDL_FillRect(gui->screen, NULL, 0x000000);

	SDL_BlitSurface(gui->logo, NULL, gui->screen, &gui->logorect);

	if (gui->statusmsg != NULL) {
		gui_paint_status(gui);
	} else {
		gui_paint_cat_view(gui);
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

	return TTF_RenderUTF8_Solid(gui->font, text, clrFg);
}

void gui_inc_cat(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;

	if (cat != NULL) {
		gui->current = cat->next;

		/* Free thumbnail which are currently not shown. */
		cat = gui->current;
		if (cat != NULL) {
			cat = cat->prev;
			if ((cat != NULL) && (cat != gui->categories->prev)) {
				cat = cat->prev;
				if ((cat != NULL) && (cat != gui->categories->prev)) {
					gui_elem_t *elem;

					elem = cat->elem;
					while(elem != NULL) {
						elem = elem->next;
						if (elem->loadedmedium == IMG_LOADED) {
							elem->loadedmedium = 0;
						}
						if (elem->imagemedium != NULL) {
							SDL_FreeSurface(elem->imagemedium);
							elem->imagemedium = NULL;
						}
						if (elem == cat->elem) {
							break;
						}
					}
					cat = cat->prev;
					if ((cat != NULL) && (cat != gui->categories->prev)) {
						cat = cat->prev;
						if ((cat != NULL) && (cat != gui->categories->prev)) {

							elem = cat->elem;
							while(elem != NULL) {
								elem = elem->next;
								if (elem->loaded == IMG_LOADED) {
									elem->loaded = 0;
								}
								if (elem->image != NULL) {
									SDL_FreeSurface(elem->image);
									elem->image = NULL;
								}
								if (elem == cat->elem) {
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}

void gui_dec_cat(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;
	if (cat != NULL) {
		gui->current = cat->prev;

		/* Free thumbnail which are currently not shown. */
		cat = gui->current;
		if (cat != NULL) {
			cat = cat->next;
			if ((cat != NULL) && (cat != gui->categories)) {
				cat = cat->next;
				if ((cat != NULL) && (cat != gui->categories)) {
					gui_elem_t *elem;

					elem = cat->elem;
					while(elem != NULL) {
						elem = elem->next;
						if (elem->loadedmedium == IMG_LOADED) {
							elem->loadedmedium = 0;
						}
						if (elem->imagemedium != NULL) {
							SDL_FreeSurface(elem->imagemedium);
							elem->imagemedium = NULL;
						}
						if (elem == cat->elem) {
							break;
						}
					}
					cat = cat->next;
					if ((cat != NULL) && (cat != gui->categories)) {
						if ((cat != NULL) && (cat != gui->categories)) {
							elem = cat->elem;
							while(elem != NULL) {
								elem = elem->next;
								if (elem->loaded == IMG_LOADED) {
									elem->loaded = 0;
								}
								if (elem->image != NULL) {
									SDL_FreeSurface(elem->image);
									elem->image = NULL;
								}
								if (elem == cat->elem) {
									break;
								}
							}
						}
					}
				}
			}
		}
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

int update_playlist(gui_t *gui, gui_cat_t *cat)
{
	int rv;
	int subnr;
	const char *nextPageToken;
	gui_elem_t *last;

	if (cat->elem != NULL) {
		last = cat->elem->prev;

		/* Get the number of the last element in the list. */
		subnr = last->subnr;
		nextPageToken = last->nextPageToken;
		subnr++;
	} else {
		last = NULL;

		/* This is a new list start with number 0. */
		subnr = 0;
		nextPageToken = "";
	}

	rv = jt_get_playlist_items(gui->at, gui->cur_cat->playlistid, nextPageToken);
	nextPageToken = NULL;

	if (rv == JT_OK) {
		int totalResults = 0;
		int resultsPerPage = 0;
		int i;

		rv = jt_json_get_int_by_path(gui->at, &totalResults, "/pageInfo/totalResults");
		if (rv != JT_OK) {
			totalResults = 0;
		}

		rv = jt_json_get_int_by_path(gui->at, &resultsPerPage, "/pageInfo/resultsPerPage");
		if (rv != JT_OK) {
			resultsPerPage = 0;
		}
		for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
			const char *url;
			
			url = jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/thumbnails/default/url", i);
			if (url != NULL) {
				gui_elem_t *elem;

				elem = gui_elem_alloc(gui, cat, url);
				if (elem != NULL) {
					last = elem;
					if (i == 0) {
						elem->prevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
					}
			 		elem->title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
					elem->urlmedium = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/thumbnails/medium/url", i));
					elem->videoid = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/resourceId/videoId", i));
					elem->subnr = subnr;
				}
			}
			subnr++;
		}

		if (last != NULL) {
			if (last->nextPageToken != NULL) {
				free(last->nextPageToken);
				last->nextPageToken = NULL;
			}
			last->nextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
		}
		jt_free_transfer(gui->at);
	}
	return rv;
}


int update_favorites(gui_t *gui, gui_cat_t *selected_cat)
{
	int rv;
	const char *nextPageToken;
	int favnr;

	if (selected_cat != NULL) {
		nextPageToken = selected_cat->favoritesNextPageToken;
		favnr = selected_cat->favnr;
	} else {
		nextPageToken = "";
		favnr = 0;
	}

	rv = jt_get_my_playlist(gui->at, nextPageToken);

	if (rv == JT_OK) {
		int totalResults = 0;
		int resultsPerPage = 0;
		int i;

		rv = jt_json_get_int_by_path(gui->at, &totalResults, "/pageInfo/totalResults");
		if (rv != JT_OK) {
			totalResults = 0;
		}

		rv = jt_json_get_int_by_path(gui->at, &resultsPerPage, "/pageInfo/resultsPerPage");
		if (rv != JT_OK) {
			resultsPerPage = 0;
		}
		for (i = 0; (i < resultsPerPage) && (favnr < totalResults); i++) {
			gui_cat_t *cat;

			cat = gui_cat_alloc(gui);
			if (cat != NULL) {
				// TBD: free playlistid and other stuff
				cat->playlistid = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/id", i));
				cat->channelid = jt_strdup(jt_json_get_string_by_path(gui->at, "/snippet[%d]/channelId", i));
		 		cat->title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
				cat->favoritesNextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
				cat->favnr = favnr;
				if (i == 0) {
					gui->cur_cat = cat;
				}
			}
			favnr++;
		}
		jt_free_transfer(gui->at);
	}
	return rv;
}

int update_subscriptions(gui_t *gui, gui_cat_t *selected_cat)
{
	int rv;
	int subnr;
	const char *nextPageToken;

	if (selected_cat != NULL) {
		subnr = selected_cat->subnr;
		nextPageToken = selected_cat->subscriptionNextPageToken;
	} else {
		subnr = 0;
		nextPageToken = "";
	}

	rv = jt_get_my_subscriptions(gui->at, nextPageToken);
	if (rv == JT_OK) {
		gui_cat_t *last;
		int totalResults = 0;
		int resultsPerPage = 0;
		int i;

		last = selected_cat;

		rv = jt_json_get_int_by_path(gui->at, &totalResults, "/pageInfo/totalResults");
		if (rv != JT_OK) {
			totalResults = 0;
		}

		rv = jt_json_get_int_by_path(gui->at, &resultsPerPage, "/pageInfo/resultsPerPage");
		if (rv != JT_OK) {
			resultsPerPage = 0;
		}
		for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
			const char *channelid;
			
			channelid = jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/resourceId/channelId", i);
			if (channelid != NULL) {
				gui_cat_t *cat;

				cat = gui_cat_alloc(gui);
				if (cat != NULL) {
					last = cat;
					cat->channelid = strdup(channelid);
			 		cat->title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
					if (i == 0) {
						cat->subscriptionPrevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
						gui->cur_cat = cat;
					}
				}
			}
			subnr++;
		}
		if (last != NULL) {
			if (last->subscriptionNextPageToken != NULL) {
				free(last->subscriptionNextPageToken);
				last->subscriptionNextPageToken = NULL;
			}

			last->subscriptionNextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
		}
		jt_free_transfer(gui->at);
	}
	return rv;
}

int update_channels(gui_t *gui, gui_cat_t *selected_cat, gui_cat_t **l)
{
	int rv;
	int channelNr;
	const char *nextPageToken;

	if (selected_cat == NULL) {
		/* Get first page. */
		channelNr = 0;
		nextPageToken = "";
	} else {
		/* Get next page. */
		channelNr = selected_cat->channelNr;
		nextPageToken = selected_cat->channelNextPageToken;
		if (nextPageToken == NULL) {
			nextPageToken = "";
		}
	}
	rv = jt_get_channels(gui->at, selected_cat->channelid, nextPageToken);
	if (rv == JT_OK) {
		int totalResults = 0;
		int resultsPerPage = 0;
		int i;
		gui_cat_t *last;

		last = selected_cat;

		rv = jt_json_get_int_by_path(gui->at, &totalResults, "/pageInfo/totalResults");
		if (rv != JT_OK) {
			totalResults = 0;
		}

		rv = jt_json_get_int_by_path(gui->at, &resultsPerPage, "/pageInfo/resultsPerPage");
		if (rv != JT_OK) {
			resultsPerPage = 0;
		}
		for (i = 0; (i < resultsPerPage) && (channelNr < totalResults); i++) {
			const char *playlistid;
			
			playlistid = jt_json_get_string_by_path(gui->at, "/items[%d]/contentDetails/relatedPlaylists/uploads", i);
			if (playlistid != NULL) {
				gui_cat_t *cat = selected_cat;

				if (cat->playlistid != NULL) {
					/* Need to add new cat for playlist. */
					cat = gui_cat_alloc(gui);
				}

				if (cat != NULL) {
					char *title;

					last = cat;

					cat->channelNr = channelNr;
					cat->channelid = selected_cat->channelid;
					cat->playlistid = strdup(playlistid);
			 		title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
					if (title != NULL) {
						if (cat->title != NULL) {
							free(cat->title);
							cat->title = NULL;
						}
						cat->title = title;
					}
				}
			}
			selected_cat->channelNr++;
		}
		if (last != NULL) {
			if (last->channelNextPageToken != NULL) {
				free(last->channelNextPageToken);
				last->channelNextPageToken = NULL;
			}
			last->channelNextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
		}
		jt_free_transfer(gui->at);
		*l = last;
	}
	return rv;
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

void playVideo(gui_elem_t *elem, int format, int buffersize)
{
	char *cmd = NULL;
	int ret;

	printf("Playing %s (%s)\n", elem->title, elem->videoid);

	if (format == 0) {
		ret = asprintf(&cmd, "wget --user-agent=\"$(youtube-dl --dump-user-agent)\" -o /dev/null -O - --load-cookies /tmp/ytcookie-%s.txt - \"$(youtube-dl -g --cookies=/tmp/ytcookie-%s.txt 'http://www.youtube.com/watch?v=%s')\" | mplayer -cache %d -", elem->videoid, elem->videoid, elem->videoid, buffersize);
	} else {
		ret = asprintf(&cmd, "wget --user-agent=\"$(youtube-dl --dump-user-agent)\" -o /dev/null -O - --load-cookies /tmp/ytcookie-%s.txt - \"$(youtube-dl -g -f %d --cookies=/tmp/ytcookie-%s.txt 'http://www.youtube.com/watch?v=%s')\" | mplayer -cache %d -", elem->videoid, format, elem->videoid, elem->videoid, buffersize);
	}
	if (ret != -1) {
		printf("%s\n", cmd);
		system(cmd);
		free(cmd);
		cmd = NULL;
		ret = asprintf(&cmd, "/tmp/ytcookie-%s.txt", elem->videoid);
		if (ret != -1) {
			unlink(cmd);
			free(cmd);
			cmd = NULL;
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
	enum gui_state state;
	enum gui_state prevstate;
	enum gui_state nextstate;
	enum gui_state afterplayliststate;
	unsigned int wakeupcount;
	unsigned int sleeptime = 50 * 3;
	int rv;

	done = 0;
	wakeupcount = 5;
	rv = JT_OK;
	state = GUI_STATE_STARTUP;
	afterplayliststate = GUI_STATE_RUNNING;
	prevstate = state;
	while(!done) {
		enum gui_state curstate;

		if (state != prevstate) {
			//printf("Enter new state %d %s\n", state, get_state_text(state));
			prevstate = state;
		}

		curstate = (wakeupcount <= 0) ? state : GUI_STATE_SLEEP;

		if (curstate == GUI_STATE_RUNNING) {
			/* Don't display error message. */
			if (gui->statusmsg != NULL) {
				free(gui->statusmsg);
				gui->statusmsg = NULL;
			}
		}

		/* Check for events */
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_KEYDOWN:
					/* Key pressed on keyboard. */
					switch(event.key.keysym.sym) {
						case SDLK_SPACE:
						case SDLK_RETURN: {
							gui_cat_t *cat;

							cat = gui->current;
							if (cat) {
								gui_elem_t *elem;

								elem = cat->current;
								if (elem != NULL) {
									if (elem->videoid != NULL) {
										if (event.key.keysym.sym == SDLK_RETURN) {
											playVideo(elem, 5, 1024);
										} else {
											playVideo(elem, 0, 4096);
										}
									}
								}
							}
							break;
						}

						case SDLK_ESCAPE:
						case SDLK_q:
							/* Quit */
							done = 1;
							break;

						case SDLK_LEFT:
							if (gui->statusmsg == NULL) {
								gui_cat_t *cat;

								cat = gui->current;
								if ((cat != NULL) && (cat->current != NULL) && (cat->elem != NULL) && (cat->current->prev == cat->elem->prev)) {
								} else {
									if (cat->elem != NULL) {
										gui_dec_elem(gui);
									}
								}
							}
							break;

						case SDLK_RIGHT:
							if (gui->statusmsg == NULL) {
								gui_cat_t *cat;

								cat = gui->current;
								if ((cat != NULL) && (cat->current != NULL) && (cat->elem != NULL) && (cat->current->next == cat->elem)) {
									gui_elem_t *last;

									last = cat->elem->prev;

									if ((last->nextPageToken != NULL) && (curstate == GUI_STATE_RUNNING)) {
										afterplayliststate = curstate;
										state = GUI_STATE_GET_PLAYLIST;
										gui->cur_cat = cat;
									}
								} else {
									if (cat->elem != NULL) {
										gui_inc_elem(gui);
									}
								}
							}
							break;

						case SDLK_HOME:
							if (gui->statusmsg == NULL) {
								gui_cat_t *cat;

								cat = gui->current;
								if ((cat != NULL) && (cat->elem != NULL)) {
									while (!((cat->current != NULL) && (cat->current->prev == cat->elem->prev))) {
										gui_dec_elem(gui);
									}
								}
							}
							break;

						case SDLK_END:
							if (gui->statusmsg == NULL) {
								gui_cat_t *cat;

								cat = gui->current;
								if ((cat != NULL) && (cat->elem != NULL)) {
									while (!((cat->current != NULL) && (cat->current->next == cat->elem))) {
										gui_inc_elem(gui);
									}
								}
							}
							break;

						case SDLK_UP:
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->categories != NULL) {
									gui_cat_t *cat;
									gui_cat_t *last;

									cat = gui->current;
									last = gui->categories->prev;
									if ((cat != NULL) && (cat->prev == last)) {
									} else {
										gui_dec_cat(gui);
									}
								}
							}
							break;

						case SDLK_DOWN:
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->categories != NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									if ((cat != NULL) && (cat->next == gui->categories)) {
										if (cat->subscriptionNextPageToken != NULL) {
											/* Get more subscriptions. */
											state = GUI_STATE_GET_SUBSCRIPTIONS;
											gui->cur_cat = cat;
										}
									} else {
										if ((cat != NULL) && (cat->next != NULL) && (cat->next->next == gui->categories)) {
											if (cat->next->subscriptionNextPageToken != NULL) {
												/* Get more subscriptions. */
												state = GUI_STATE_GET_SUBSCRIPTIONS;
												gui->cur_cat = cat->next;
											}
										}
										gui_inc_cat(gui);
									}
								}
							}
							break;

						case SDLK_PAGEUP:
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->categories != NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									while (!((cat != NULL) && (cat->prev == gui->categories->prev))) {
										gui_dec_cat(gui);
										cat = gui->current;
									}
								}
							}
							break;

						case SDLK_PAGEDOWN:
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->categories != NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									while (!((cat != NULL) && (cat->next == gui->categories))) {
										gui_inc_cat(gui);
										cat = gui->current;
									}
									if (cat->subscriptionNextPageToken != NULL) {
										/* Get more subscriptions. */
										state = GUI_STATE_GET_SUBSCRIPTIONS;
										gui->cur_cat = cat;
									}
								}
							}
							break;
	
						default:
							break;
					}
					break;

				default:
					break;

			}
		}
		if (state != prevstate) {
			//printf("Enter new state %d %s (triggered by user).\n", state, get_state_text(state));
			prevstate = state;
		}

		if (wakeupcount > 0) {
			wakeupcount--;
		}

		/* GUI state machine. */
		curstate = (wakeupcount <= 0) ? state : GUI_STATE_SLEEP;
		switch(curstate) {
			case GUI_STATE_SLEEP:
				break;

			case GUI_STATE_STARTUP:
				state = GUI_STATE_LOAD_ACCESS_TOKEN;
				break;

			case GUI_STATE_LOAD_ACCESS_TOKEN:
				/* Try to load existing access for YouTube user account. */
				rv = jt_load_token(gui->at);
				if (rv == JT_OK) {
					state = GUI_STATE_LOGGED_IN;
				} else {
					state = GUI_STATE_GET_USER_CODE;
				}
				break;

			case GUI_STATE_GET_USER_CODE:
				/* Access user to give access for this application to the YouTube user account. */
				rv = jt_update_user_code(gui->at);
				if (rv == JT_OK) {
					gui->statusmsg = buf_printf(gui->statusmsg,
						"Please enter the User Code:\n"
						"%s\n"
						"in a webbrowser at:\n"
						"%s",
						jt_get_user_code(gui->at),
						jt_get_verification_url(gui->at));
					state = GUI_STATE_GET_TOKEN;
					wakeupcount = sleeptime;
				} else {
					/* Retry later */
					state = GUI_STATE_ERROR;
					nextstate = GUI_STATE_GET_USER_CODE;

					/* Remove user code. */
					if (gui->statusmsg != NULL) {
						free(gui->statusmsg);
						gui->statusmsg = NULL;
					}
				}
				break;

			case GUI_STATE_GET_TOKEN:
				/* Check whether the user permitted access for this application. */
				rv = jt_get_token(gui->at);
				switch (rv) {
					case JT_OK:
						state = GUI_STATE_LOGGED_IN;
						break;

					case JT_AUTH_PENDING:
						/* Need to wait longer. */
						wakeupcount = sleeptime;
						break;
	
					case JT_SLOW_DOWN:
						/* Need to poll slower. */
						sleeptime += 25;
						wakeupcount = sleeptime;
						break;

					case JT_CODE_EXPIRED:
						gui->statusmsg = buf_printf(gui->statusmsg, "Expired");
						/* Retry */
						wakeupcount = DEFAULT_SLEEP;
						state = GUI_STATE_GET_USER_CODE;
						break;
	
					default:
						/* Retry later */
						state = GUI_STATE_ERROR;
						nextstate = GUI_STATE_GET_USER_CODE;
						break;
				}
				break;

			case GUI_STATE_LOGGED_IN:
				/* Now logged into user account. */
				gui->statusmsg = buf_printf(gui->statusmsg, "Logged in");
				state = GUI_STATE_INIT;
				break;

			case GUI_RESET_STATE:
				state = nextstate;
				if (gui->statusmsg != NULL) {
					free(gui->statusmsg);
					gui->statusmsg = NULL;
				}
				break;

			case GUI_STATE_INIT: {
				state = GUI_STATE_GET_FAVORITES;
				break;
			}

			case GUI_STATE_GET_FAVORITES:
				rv = update_favorites(gui, gui->cur_cat);
				if (rv != JT_OK) {
					state = GUI_STATE_ERROR;
					wakeupcount = DEFAULT_SLEEP;
					nextstate = GUI_STATE_GET_SUBSCRIPTIONS;
					gui->cur_cat = NULL;
				} else {
					state = GUI_STATE_GET_PLAYLIST;
					afterplayliststate = GUI_STATE_GET_SUBSCRIPTIONS;
				}
				break;

			case GUI_STATE_GET_PLAYLIST:
				if (gui->cur_cat != NULL) {
					if (gui->cur_cat->playlistid != NULL) {
						rv = update_playlist(gui, gui->cur_cat);
						if (rv == JT_OK) {
							state = afterplayliststate;
							if (gui->statusmsg != NULL) {
								free(gui->statusmsg);
								gui->statusmsg = NULL;
							}
						} else {
							state = GUI_STATE_ERROR;
							wakeupcount = DEFAULT_SLEEP;
							nextstate = afterplayliststate;
						}
					} else {
						gui->statusmsg = buf_printf(gui->statusmsg, "No playlist id");
						wakeupcount = DEFAULT_SLEEP;
						state = afterplayliststate;
					}
				} else {
					gui->statusmsg = buf_printf(gui->statusmsg, "No categories allocated");
					wakeupcount = DEFAULT_SLEEP;
					state = afterplayliststate;
				}
				gui->cur_cat = NULL;
				break;

			case GUI_STATE_GET_SUBSCRIPTIONS: {
				rv = update_subscriptions(gui, gui->cur_cat);
				if (rv == JT_OK) {
					state = GUI_STATE_GET_CHANNELS;
				} else {
					state = GUI_STATE_ERROR;
					wakeupcount = DEFAULT_SLEEP;
					nextstate = GUI_STATE_RUNNING;
				}
				break;
			}

			case GUI_STATE_GET_CHANNELS:
				if (gui->cur_cat != NULL) {
					gui_cat_t *last = NULL;
					rv = update_channels(gui, gui->cur_cat, &last);
					if (rv == JT_OK) {
						if ((last == NULL) || (last->channelNextPageToken == NULL)) {
							state = GUI_STATE_GET_PLAYLIST;
							afterplayliststate = GUI_STATE_RUNNING;
						}
					} else {
						state = GUI_STATE_ERROR;
						wakeupcount = DEFAULT_SLEEP;
						nextstate = GUI_STATE_RUNNING;
					}
				} else {
					gui->statusmsg = buf_printf(gui->statusmsg, "No category allocated for channels");
					wakeupcount = DEFAULT_SLEEP;
					state = GUI_STATE_RUNNING;
				}
				break;

			case GUI_STATE_RUNNING:
				if (gui->statusmsg != NULL) {
					free(gui->statusmsg);
					gui->statusmsg = NULL;
				}
				break;

			case GUI_STATE_ERROR:  {
				const char *error;

				/* Some error happened, the error code is stored in rv. */

				switch(rv) {
					case JT_PROTOCOL_ERROR:
						error = jt_get_error_description(gui->at);
						if (error != NULL) {
							gui->statusmsg = buf_printf(gui->statusmsg, "%s", error);
						} else {
							error = jt_get_protocol_error(gui->at);
							gui->statusmsg = buf_printf(gui->statusmsg, "Error: %s", error);
						}
						break;

					case JT_TRANSFER_ERROR: {
						CURLcode res;

						res = jt_get_transfer_error(gui->at);
						switch(res) {
							case CURLE_SSL_CACERT:
								gui->statusmsg = buf_printf(gui->statusmsg, "Verification of CA cert failed.");
								break;

							case CURLE_COULDNT_RESOLVE_HOST:
								gui->statusmsg = buf_printf(gui->statusmsg, "DNS failed.");
								break;

							default:
								error = curl_easy_strerror(res);
								gui->statusmsg = buf_printf(gui->statusmsg, "Transfer failed: %s", error);
								break;
						}
						break;
					}

					default:
						error = jt_get_error_code(rv);
						gui->statusmsg = buf_printf(gui->statusmsg, "Error: %s", error);
						break;
				}

				/* Retry */
				wakeupcount = DEFAULT_SLEEP;
				state = GUI_RESET_STATE;
				break;
			}
		}

		/* Paint GUI elements. */
		gui_paint(gui);
	}
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

	(void) gui;

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

	return rv;
}

