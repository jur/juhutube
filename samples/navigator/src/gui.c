#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include "log.h"
#include "gui.h"
#include "transfer.h"
#include "pictures.h"
#include "clientid.h"
#include "libjt.h"

#define TOKEN_FILE ".youtubetoken"
#define REFRESH_TOKEN_FILE ".refreshtoken"
#define KEY_FILE ".youtubekey"
#define SECRET_FILE ".client_secret.json"
#define TITLE_FILE ".accounttitle"
#define MENU_STATE_FILE ".menustate"
#ifndef __arm__

/* Buttons for PS2 and normal Linux. */
#define BTN_SQUARE SDLK_s
#define BTN_CROSS SDLK_SPACE
#define BTN_CIRCLE SDLK_ESCAPE
#define BTN_TRIANGLE SDLK_r
#define BTN_START SDLK_RETURN
#define BTN_SELECT SDLK_a
#define BTN_L1 SDLK_HOME
#define BTN_R1 SDLK_END
#define BTN_L2 SDLK_PAGEUP
#define BTN_R2 SDLK_PAGEDOWN
#define BTN_UNUSED SDLK_l
#else

/* Buttons for Open Pandora. */
#define BTN_SQUARE SDLK_HOME
#define BTN_CROSS SDLK_PAGEDOWN
#define BTN_CIRCLE SDLK_END
#define BTN_TRIANGLE SDLK_PAGEUP
#define BTN_START SDLK_LALT
#define BTN_SELECT SDLK_LCTRL
#define BTN_L1 SDLK_RSHIFT
#define BTN_R1 SDLK_RCTRL
#define BTN_L2 SDLK_i
#define BTN_R2 SDLK_u
#define BTN_UNUSED SDLK_l
#endif
/** Maximum images loaded while painting. */
#define MAX_LOAD_WHILE_PAINT 1
/** Maximum retry count for images. */
#define IMG_LOAD_RETRY 5
/** Special value for image loaded successfully. */
#define IMG_LOADED 10
/** Maximum categories shown. */
#define MAX_SHOW 3
/** Maximum videos per cat shown. */
#define MAX_VIDS 5
/** Maximum number of Youtube accounts supported. */
#define MAX_ACCOUNTS 10
/** Chunk size for loadinf files. */
#define CHUNK_SIZE 256
/** Size of cache for small text. */
#define TITLE_CACHE_ENTRIES 30

#define BUFFER_SIZE 4096

/* Cache size, defined in the distance of the categories. */
/** How many categories should preserve the large thumbnails.
 * Foward and backward from the current selected category.
 */
#define PRESERVE_MEDIUM_IMAGES 1
/** How many categories should preserve the small thumbnails.
 * Foward and backward from the current selected category.
 */
#define PRESERVE_SMALL_IMAGES 4
/** How many categories should be stored in memory.
 * Foward and backward from the current selected category.
 */
#define PRESERVE_CAT 10

/** How many videos are preserved in each category 2 * PRESERVE_ELEM * results
 * per page (YouTube API)
 */
#define PRESERVE_ELEM 3

/** Default wait time for a message to display in frames. */
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

/** Return string if string parameter is not NULL, otherwise return "(null)". */
#define CHECKSTR(stringptr) (((stringptr) != NULL) ? (stringptr) : "(null)")

#define MENU_VERSION 1

#define CASESTATE(state) \
	case state: \
		return #state;

/** States for GUI state machine. */
enum gui_state {
	GUI_STATE_SLEEP,
	GUI_STATE_STARTUP,
	GUI_STATE_LOAD_ACCESS_TOKEN,
	GUI_STATE_NEW_ACCESS_TOKEN,
	GUI_STATE_GET_USER_CODE,
	GUI_STATE_GET_TOKEN,
	GUI_STATE_LOGGED_IN,
	GUI_STATE_ERROR,
	GUI_RESET_STATE,
	GUI_STATE_INIT,
	GUI_STATE_GET_FAVORITES,
	GUI_STATE_GET_PREV_FAVORITES,
	GUI_STATE_GET_PLAYLIST,
	GUI_STATE_GET_PREV_PLAYLIST,
	GUI_STATE_GET_SUBSCRIPTIONS,
	GUI_STATE_GET_PREV_SUBSCRIPTIONS,
	GUI_STATE_GET_MY_CHANNELS,
	GUI_STATE_GET_MY_PREV_CHANNELS,
	GUI_STATE_GET_CHANNELS,
	GUI_STATE_GET_PREV_CHANNELS,
	GUI_STATE_GET_CHANNEL_PLAYLIST,
	GUI_STATE_GET_PREV_CHANNEL_PLAYLIST,
	GUI_STATE_RUNNING,
	GUI_STATE_PLAY_VIDEO,
	GUI_STATE_PLAY_PREV_VIDEO,
	GUI_STATE_WAIT_FOR_CONTINUE,
	GUI_STATE_MAIN_MENU,
	GUI_STATE_POWER_OFF,
	GUI_STATE_MENU_PLAYLIST,
	GUI_STATE_TIMEOUT,
	GUI_STATE_SEARCH,
	GUI_STATE_SEARCH_PLAYLIST,
	GUI_STATE_SEARCH_PREV_PLAYLIST,
	GUI_STATE_SEARCH_ENTER,
	GUI_STATE_QUIT,
	GUI_STATE_UPDATE,
};

const char *get_state_text(enum gui_state state)
{
	switch(state) {
		CASESTATE(GUI_STATE_SLEEP)
		CASESTATE(GUI_STATE_STARTUP)
		CASESTATE(GUI_STATE_LOAD_ACCESS_TOKEN)
		CASESTATE(GUI_STATE_NEW_ACCESS_TOKEN)
		CASESTATE(GUI_STATE_GET_USER_CODE)
		CASESTATE(GUI_STATE_GET_TOKEN)
		CASESTATE(GUI_STATE_LOGGED_IN)
		CASESTATE(GUI_STATE_ERROR)
		CASESTATE(GUI_RESET_STATE)
		CASESTATE(GUI_STATE_INIT)
		CASESTATE(GUI_STATE_GET_FAVORITES)
		CASESTATE(GUI_STATE_GET_PREV_FAVORITES)
		CASESTATE(GUI_STATE_GET_PLAYLIST)
		CASESTATE(GUI_STATE_GET_PREV_PLAYLIST)
		CASESTATE(GUI_STATE_GET_SUBSCRIPTIONS)
		CASESTATE(GUI_STATE_GET_PREV_SUBSCRIPTIONS)
		CASESTATE(GUI_STATE_GET_CHANNELS)
		CASESTATE(GUI_STATE_GET_PREV_CHANNELS)
		CASESTATE(GUI_STATE_GET_MY_CHANNELS)
		CASESTATE(GUI_STATE_GET_MY_PREV_CHANNELS)
		CASESTATE(GUI_STATE_GET_CHANNEL_PLAYLIST)
		CASESTATE(GUI_STATE_GET_PREV_CHANNEL_PLAYLIST)
		CASESTATE(GUI_STATE_RUNNING)
		CASESTATE(GUI_STATE_PLAY_VIDEO)
		CASESTATE(GUI_STATE_PLAY_PREV_VIDEO)
		CASESTATE(GUI_STATE_WAIT_FOR_CONTINUE)
		CASESTATE(GUI_STATE_MAIN_MENU)
		CASESTATE(GUI_STATE_POWER_OFF)
		CASESTATE(GUI_STATE_MENU_PLAYLIST)
		CASESTATE(GUI_STATE_TIMEOUT)
		CASESTATE(GUI_STATE_SEARCH)
		CASESTATE(GUI_STATE_SEARCH_PLAYLIST)
		CASESTATE(GUI_STATE_SEARCH_PREV_PLAYLIST)
		CASESTATE(GUI_STATE_SEARCH_ENTER)
		CASESTATE(GUI_STATE_QUIT)
		CASESTATE(GUI_STATE_UPDATE)
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
	char *url;
	/** URL to medium size thumbnail. */
	char *urlmedium;
	/** YouTube video ID. */
	char *videoid;
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
	/** YouTube channel ID. */
	char *channelid;
};

/** The structure describes a category (e.g. favorites or subscribed channel). */
struct gui_cat_s {
	/** First element of playlist items (YouTube videos). */
	gui_elem_t *elem;
	/** Currently selected YouTube video. */
	gui_elem_t *current;
	/** Where to add this item in gui->categories. */
	gui_cat_t *where;
	/** YouTube channel ID. */
	char *channelid;
	/** YouTube playlist ID. */
	char *playlistid;
	/** YouTube searchterm. */
	char *searchterm;
	/** Pointer to previous category in list. */
	gui_cat_t *prev;
	/** Pointer to next category in list. */
	gui_cat_t *next;
	/** Subscription/Channel/Playlist title. */
	char *title;
	/** Number of the channel in the list. */
	int channelNr;
	/** Number of the channel in the list for the first of the page. */
	int channelStart;
	/** Token to get the next channel via the YouTube API. */
	char *channelNextPageToken;
	/** Token to get the previous channel via the YouTube API. */
	char *channelPrevPageToken;
	/** Number of the favorite. */
	int favnr;
	/** Token to get the next favorite via the YouTube API. */
	char *favoritesNextPageToken;
	/** Token to get the previous favorite via the YouTube API. */
	char *favoritesPrevPageToken;
	/** Number of the subscription. */
	int subnr;
	/** Next page to get more subscriptions. */
	char *subscriptionNextPageToken;
	/** Previous page to get more subscriptions. */
	char *subscriptionPrevPageToken;
	/** State in state machine to get next page. */
	enum gui_state nextPageState;
	/** State in state machine to get previous page. */
	enum gui_state prevPageState;
	/** First page to load. */
	const char *videopagetoken;
	/** First page valid for the following playlist. */
	const char *expected_playlistid;
	/** Videonumber of the first video of the page specified by videopagetoken. */
	int vidnr;
	/** Scroll direction of title. */
	int scrolldir;
	/** Scroll position of title. */
	int scrollpos;
};

typedef struct gui_menu_entry_s gui_menu_entry_t;

/** Menu entry fro main menu. */
struct gui_menu_entry_s {
	/** Displayed title for menu. */
	char *title;
	/** Image containing the text shown. */
	SDL_Surface *textimg;
	/** Selecting the menu entry causes the following state. */
	enum gui_state state;
	/** Selecting the menu entry causes the following state on init. */
	enum gui_state initstate;
	/** Menu entry number. */
	int nr;
	/** Token number for account. */
	int tokennr;
	/** Playlist ID of this entry. */
	char *playlistid;
	/** Channel ID of this entry. */
	char *channelid;
	/** Searchterm */
	char *searchterm;
	/** Scroll position. */
	int pos;
	/** Scroll direction. */
	int dir;

	/** Next menu entry. */
	gui_menu_entry_t *next;
	/** Previous menu entry. */
	gui_menu_entry_t *prev;
};

/** Entry for caching images of text. */
typedef struct {
	/** Text pointer of cached title image. */
	const char *title;
	/** The cached title image. */
	SDL_Surface *sText;
	/** Hit counter. */
	int count;
} title_cache_t;

struct gui_s {
	/** Path to resources like images. */
	const char *sharedir;
	/** YouTube logo. */
	SDL_Surface *logo;
	SDL_Surface *cross;
	SDL_Surface *cross_text;
	SDL_Surface *circle;
	SDL_Surface *circle_text;
	SDL_Surface *triangle;
	SDL_Surface *triangle_text;
	SDL_Surface *square;
	SDL_Surface *square_text;
	int description_status;
	/** Output screen */
	SDL_Surface *screen;
	/** Position of YouTube logo on screen. */
	SDL_Rect logorect;
	/** Position of description. */
	int description_pos;
	/** Pointer to font used to write something on the screen. */
	TTF_Font *font;
	/** Pointer to small font used to write something on the screen. */
	TTF_Font *smallfont;
	/** Pointer to font used to write description of buttons. */
	TTF_Font *descfont;

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

	/** Categories where playlist needs to be loaded. */
	gui_cat_t *get_playlist_cat;

	/** Categories where channels needs to be loaded. */
	gui_cat_t *get_channel_cat;

	/** True if in fullscreen mode. */
	int fullscreenmode;

	/** Used to return from channel playlist. */
	gui_cat_t *prev_cat;

	/** SDL Joystick handle. */
	SDL_Joystick *joystick;

	/** Main menu. */
	gui_menu_entry_t *mainmenu;
	/** Selected menu entry. */
	gui_menu_entry_t *selectedmenu;

	/** True, if account is allocated. */
	int account_allocated[MAX_ACCOUNTS];

	/** Cache for images of small text. */
	title_cache_t title_cache_table[TITLE_CACHE_ENTRIES];
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
static SDL_Surface *gui_get_image(gui_t *gui, const char *file)
{
	int ret;
	char *filename = NULL;
	SDL_Surface *rv;

	if (file == NULL) {
		return NULL;
	}

	ret = asprintf(&filename, "%s/%s", gui->sharedir, file);
	if (ret == -1) {
		return NULL;
	}

	rv = IMG_Load(filename);

	if (rv == NULL) {
		LOG_ERROR("Failed to load \"%s\".\n", filename);
	}
	free(filename);
	filename = NULL;

	return rv;
}

static void add_cached_title(gui_t *gui, SDL_Surface *sText, const char *title)
{
	int i;
	int min;
	int idx = 0;

	min = 100000;
	for (i = 0; i < TITLE_CACHE_ENTRIES; i++) {
		gui->title_cache_table[i].count--;
		if (gui->title_cache_table[i].count < min) {
			min = gui->title_cache_table[i].count;
			idx = i;
		}
	}
	if (gui->title_cache_table[idx].sText != NULL) {
		SDL_FreeSurface(gui->title_cache_table[idx].sText);
	}
	gui->title_cache_table[idx].sText = sText;
	gui->title_cache_table[idx].count = 1000;
	gui->title_cache_table[idx].title = title;
}

static void cache_title_free(gui_t *gui)
{
	int i;

	for (i = 0; i < TITLE_CACHE_ENTRIES; i++) {
		if (gui->title_cache_table[i].sText != NULL) {
			SDL_FreeSurface(gui->title_cache_table[i].sText);
			gui->title_cache_table[i].sText = NULL;
			gui->title_cache_table[i].title = NULL;
			gui->title_cache_table[i].count = 0;
		}
	}
}

static gui_cat_t *gui_cat_alloc(gui_t *gui, gui_cat_t **listhead, gui_cat_t *where)
{
	gui_cat_t *rv;

	rv = malloc(sizeof(*rv));
	if (rv == NULL) {
		return NULL;
	}
	memset(rv, 0, sizeof(*rv));
	rv->scrolldir = 1;

	if (listhead == &gui->categories) {
		if (gui->current == NULL) {
			gui->current = rv;
		}
	}
	if ((where == NULL) || (listhead != &gui->categories)) {
		if (*listhead == NULL) {
			/* First one inserted in list. */
			rv->next = rv;
			rv->prev = rv;
			*listhead = rv;
		} else {
			gui_cat_t *last;

			/* Insert after last in list. */
			last = (*listhead)->prev;
	
			rv->next = last->next;
			rv->prev = last;

			rv->next->prev = rv;
			last->next = rv;
		}
		rv->where = where;
	} else {
		/* Insert after where. */
		rv->next = where->next;
		rv->prev = where;

		rv->next->prev = rv;
		where->next = rv;
	}
	return rv;
}

static gui_elem_t *gui_elem_alloc(gui_t *gui, gui_cat_t *cat, gui_elem_t *where, const char *url)
{
	gui_elem_t *rv;

	(void) gui;

	rv = malloc(sizeof(*rv));
	if (rv == NULL) {
		return NULL;
	}
	memset(rv, 0, sizeof(*rv));
	rv->url = jt_strdup(url);

	if (cat->current == NULL) {
		cat->current = rv;
	}
	if (cat->elem == NULL) {
		rv->next = rv;
		rv->prev = rv;
		cat->elem = rv;
	} else if (where == NULL) {
		rv->next = cat->elem;
		rv->prev = cat->elem->prev;

		cat->elem->prev->next = rv;
		rv->next->prev = rv;

		cat->elem = rv;
	} else {
		rv->next = where->next;
		rv->prev = where;

		where->next = rv;
		rv->next->prev = rv;
	}

	return rv;
}

static void gui_elem_free(gui_elem_t *elem)
{
	if (elem != NULL) {
		/* Remove from list. */
		if (elem->next != NULL) {
			elem->next->prev = elem->prev;
		}
		if (elem->prev != NULL) {
			elem->prev->next = elem->next;
		}
		elem->prev = NULL;
		elem->next = NULL;
		if (elem->image != NULL) {
			SDL_FreeSurface(elem->image);
			elem->image = NULL;
		}
		if (elem->imagemedium != NULL) {
			SDL_FreeSurface(elem->imagemedium);
			elem->imagemedium = NULL;
		}
		if (elem->url != NULL) {
			free(elem->url);
			elem->url = NULL;
		}
		if (elem->urlmedium != NULL) {
			free(elem->urlmedium);
			elem->urlmedium = NULL;
		}
		if (elem->videoid != NULL) {
			free(elem->videoid);
			elem->videoid = NULL;
		}
		if (elem->title != NULL) {
			free(elem->title);
			elem->title = NULL;
		}
		if (elem->nextPageToken != NULL) {
			free(elem->nextPageToken);
			elem->nextPageToken = NULL;
		}
		if (elem->prevPageToken != NULL) {
			free(elem->prevPageToken);
			elem->prevPageToken = NULL;
		}
		if (elem->channelid != NULL) {
			free(elem->channelid);
			elem->channelid = NULL;
		}
		free(elem);
		elem = NULL;
	}
}

static void gui_cat_free(gui_t *gui, gui_cat_t *cat)
{
	if ((cat != NULL) && (gui->prev_cat != cat)) {
		gui_elem_t *elem;
		gui_elem_t *next;

		if (cat->next == cat) {
			cat->next = NULL;
		}
		if (cat->prev == cat) {
			cat->prev = NULL;
		}

		/* Shouldn't be important. */
		cat->where = NULL;

		/* Check if cat is at the top of the list. */
		if (gui->categories == cat) {
			gui->categories = cat->next;
		}

		/* Check if cat is at the top of the list. */
		if (gui->current == cat) {
			gui->current = cat->next;
		}

		/* Element can't be further used. */
		if (gui->cur_cat == cat) {
			gui->cur_cat = NULL;
		}

		/* Remove from list. */
		if (cat->next != NULL) {
			cat->next->prev = cat->prev;
		}
		if (cat->prev != NULL) {
			cat->prev->next = cat->next;
		}
		cat->prev = NULL;
		cat->next = NULL;

		/* Was allocated as part of the list cat->elem. */
		cat->current = NULL;

		elem = cat->elem;
		cat->elem = NULL;
		while(elem != NULL) {
			next = elem->next;
			if (elem == elem->prev) {
				/* This is the last one, there is no next. */
				next = NULL;
			}
			gui_elem_free(elem);
			elem = NULL;
			elem = next;
		}

		if (cat->channelid != NULL) {
			free(cat->channelid);
			cat->channelid = NULL;
		}
		if (cat->playlistid != NULL) {
			free(cat->playlistid);
			cat->playlistid = NULL;
		}
		if (cat->searchterm != NULL) {
			free(cat->searchterm);
			cat->searchterm = NULL;
		}
		if (cat->title != NULL) {
			free(cat->title);
			cat->title = NULL;
		}
		if (cat->channelNextPageToken != NULL) {
			free(cat->channelNextPageToken);
			cat->channelNextPageToken = NULL;
		}
		if (cat->channelPrevPageToken != NULL) {
			free(cat->channelPrevPageToken);
			cat->channelPrevPageToken = NULL;
		}
		if (cat->favoritesNextPageToken != NULL) {
			free(cat->favoritesNextPageToken);
			cat->favoritesNextPageToken = NULL;
		}
		if (cat->favoritesPrevPageToken != NULL) {
			free(cat->favoritesPrevPageToken);
			cat->favoritesPrevPageToken = NULL;
		}
		if (cat->subscriptionNextPageToken != NULL) {
			free(cat->subscriptionNextPageToken);
			cat->subscriptionNextPageToken = NULL;
		}
		if (cat->subscriptionPrevPageToken != NULL) {
			free(cat->subscriptionPrevPageToken);
			cat->subscriptionPrevPageToken = NULL;
		}

		/* not allocated */
		cat->videopagetoken = NULL;
		cat->vidnr = 0;

		free(cat);
		cat = NULL;
	}
}

static void renumber_menu_entries(gui_menu_entry_t *listhead)
{
	gui_menu_entry_t *cur;
	int nr = 0;

	cur = listhead;
	nr = 0;
	while (cur != NULL) {
		cur->nr = nr;
		cur = cur->next;
		if (cur == listhead) {
			break;
		}
		nr++;
	}
}

static gui_menu_entry_t *gui_menu_entry_alloc(gui_t *gui, gui_menu_entry_t **listhead, gui_menu_entry_t *where, const char *title, enum gui_state state, enum gui_state initstate)
{
	gui_menu_entry_t *rv;

	rv = malloc(sizeof(*rv));
	if (rv == NULL) {
		return NULL;
	}
	memset(rv, 0, sizeof(*rv));

	rv->dir = 1;
	rv->pos = 0;

	if (listhead == &gui->mainmenu) {
		if (gui->selectedmenu == NULL) {
			gui->selectedmenu = rv;
		}
	}
	if ((where == NULL) || (listhead != &gui->mainmenu)) {
		if (*listhead == NULL) {
			/* First one inserted in list. */
			rv->next = rv;
			rv->prev = rv;
			*listhead = rv;
		} else {
			gui_menu_entry_t *last;

			/* Insert after last in list. */
			last = (*listhead)->prev;
	
			rv->next = last->next;
			rv->prev = last;

			rv->next->prev = rv;
			last->next = rv;
		}
	} else {
		/* Insert after where. */
		rv->next = where->next;
		rv->prev = where;

		rv->next->prev = rv;
		where->next = rv;
	}
	if (title != NULL) {
		rv->title = jt_strdup(title);
	}
	rv->state = state;
	rv->initstate = initstate;
	renumber_menu_entries(*listhead);
	return rv;
}

static void gui_menu_entry_free(gui_t *gui, gui_menu_entry_t *entry)
{
	if (entry != NULL) {
		if (entry->next == entry) {
			entry->next = NULL;
		}
		if (entry->prev == entry) {
			entry->prev = NULL;
		}

		/* Check if entry is at the top of the list. */
		if (gui->mainmenu == entry) {
			gui->mainmenu = entry->next;
		}

		/* Check if entry is at the top of the list. */
		if (gui->selectedmenu == entry) {
			gui->selectedmenu = entry->next;
		}

		/* Remove from list. */
		if (entry->next != NULL) {
			entry->next->prev = entry->prev;
		}
		if (entry->prev != NULL) {
			entry->prev->next = entry->next;
		}
		entry->prev = NULL;
		entry->next = NULL;

		if (entry->title != NULL) {
			free(entry->title);
			entry->title = NULL;
		}
		if (entry->textimg != NULL) {
			SDL_FreeSurface(entry->textimg);
			entry->textimg = NULL;
		}
		if ((entry->state == GUI_STATE_LOAD_ACCESS_TOKEN) || (entry->state == GUI_STATE_NEW_ACCESS_TOKEN)) {
			gui->account_allocated[entry->tokennr] = 0;
		}
		if (entry->playlistid != NULL) {
			free(entry->playlistid);
			entry->playlistid = NULL;
		}
		if (entry->channelid != NULL) {
			free(entry->channelid);
			entry->channelid = NULL;
		}
		if (entry->searchterm != NULL) {
			free(entry->searchterm);
			entry->searchterm = NULL;
		}

		free(entry);
		entry = NULL;

		renumber_menu_entries(gui->mainmenu);
	}
}

static void save_file(const char *filename, const void *mem, size_t size)
{
	FILE *fout;

	LOG("%s() filename %s\n", __FUNCTION__, filename);

	fout = fopen(filename, "wb");
	if (fout != NULL) {
		if (fwrite(mem, size, 1, fout) != 1) {
			LOG_ERROR("Failed to write file: %s\n", strerror(errno));
		}
		fclose(fout);
		fout = NULL;
	}
}


static void *load_file(const char *filename)
{
	FILE *fin;
	int pos = 0;
	void *mem = NULL;
	int eof = 0;

	LOG("%s() file %s\n", __FUNCTION__, filename);

	fin = fopen(filename, "rb");
	if (fin != NULL) {
		int rv;

		mem = malloc(CHUNK_SIZE);
		if (mem == NULL) {
			fclose(fin);
			fin = NULL;
			return NULL;
		}
		memset(mem, 0, CHUNK_SIZE);
		do {
			rv = fread(mem + pos, 1, CHUNK_SIZE - 1, fin);
			if (rv < 0) {
				LOG_ERROR("Failed to read file: %s\n", strerror(errno));
				free(mem);
				mem = NULL;
				return (void *) -1;
			} else {
				eof = feof(fin);
				pos += rv;
				if (!eof) {
					mem = realloc(mem, pos + CHUNK_SIZE);
					if (mem == NULL) {
						LOG_ERROR("out of memory\n");
						break;
					}
					memset(mem + pos, 0, CHUNK_SIZE);
				}
			}
		} while (!eof);
		fclose(fin);
		fin = NULL;

		return mem;
	} else {
		return (void *) -1;
	}
}

static char *check_token(int nr)
{
	const char *home;
	char *tokenfile = NULL;
	int ret;
	void *mem;
	char *accountname = NULL;
	char *titlefile = NULL;

	home = getenv("HOME");
	if (home == NULL) {
		LOG_ERROR("Environment variable HOME is not set.\n");
		return NULL;
	}

	ret = asprintf(&tokenfile, "%s/%s%03d.json", home, TOKEN_FILE, nr);
	if (ret == -1) {
		LOG_ERROR("Out of memory\n");
		return NULL;
	}

	mem = load_file(tokenfile);

	free(tokenfile);
	tokenfile = NULL;

	if (mem == NULL) {
		/* Out of memory */
		return strdup("Account");
	}
	if (mem == ((void *) -1)) {
		return NULL;
	}
	free(mem);
	mem = NULL;

	ret = asprintf(&titlefile, "%s/%s%03d.txt", home, TITLE_FILE, nr);
	if (ret == -1) {
		LOG_ERROR("Out of memory\n");
		mem = NULL;
	} else {
		mem = load_file(titlefile);
		free(titlefile);
		if (mem == ((void *) -1)) {
			mem = NULL;
		}
	}
	titlefile = NULL;

	if (mem != NULL) {
		return mem;
	} else {
		ret = asprintf(&accountname, "Account %03d", nr);
		if (ret == -1) {
			LOG_ERROR("Out of memory\n");
			return strdup("Account");
		}
		return accountname;
	}
}

static void load_menu_state(gui_t *gui)
{
	const char *home;
	FILE *fin;
	char *filename;
	int ret;
	int menuver = 0;

	home = getenv("HOME");
	if (home == NULL) {
		LOG_ERROR("Environment variable HOME is not set.\n");
		return;
	}

	ret = asprintf(&filename, "%s/%s.cfg", home, MENU_STATE_FILE);
	if (ret == -1) {
		LOG_ERROR("Out of memory\n");
		return;
	}

	fin = fopen(filename, "rb");

	free(filename);
	filename = NULL;

	if (fin != NULL) {
		char *buffer;

		buffer = malloc(BUFFER_SIZE);
		if (buffer != NULL) {
			int error = 0;

			if (fgets(buffer, BUFFER_SIZE, fin) != NULL) {
				buffer[strlen(buffer) - 1] = 0;
				if (strncmp(buffer, "MenuFormat", 10) == 0) {
					LOG("Detected MenuFormat\n");
					if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
						LOG_ERROR("Failed to read last menu state.\n");
						error = 1;
					} else {
						menuver = strtol(buffer, NULL, 0);
						LOG("Detected MenuFormat Version %d\n", menuver);
						if (menuver > MENU_VERSION) {
							error = 1;
							LOG_ERROR("Unsupported MenuFormat %d\n", menuver);
						}
					}
				} else {
					/* Reset to start of file, because version not detected. */
					fseek(fin, 0, SEEK_SET);
				}
			}

			while(!error && !feof(fin)) {
				gui_menu_entry_t *entry = NULL;
				enum gui_state state = GUI_STATE_MENU_PLAYLIST;
				enum gui_state initstate = GUI_STATE_GET_PLAYLIST;
				int addit = 0;
				int tokennr;
				char *title = NULL;
				char *playllistid = NULL;
				char *channelid = NULL;
				char *searchterm = NULL;

				if (menuver >= 1) {
					if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
						error = 1;
						break;
					}
					/* Get state and initstate. */
					buffer[strlen(buffer) - 1] = 0;
					if (buffer[0] == 0) {
						error = 1;
						break;
					}
					state = strtol(buffer, NULL, 0);
					LOG("Menu entry has state %d %s (from %s)\n", state, get_state_text(state), buffer);

					if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
						LOG_ERROR("Failed reading menu.\n");
						error = 1;
						break;
					}
					buffer[strlen(buffer) - 1] = 0;
					if (buffer[0] == 0) {
						error = 1;
						break;
					}
					initstate = strtol(buffer, NULL, 0);
					LOG("Menu entry has initstate %d %s (from %s)\n", initstate, get_state_text(initstate), buffer);
				}
				LOG("Load menu entry for state %d %s\n", state, get_state_text(state));
				switch(state) {
					case GUI_STATE_SEARCH:
						if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						buffer[strlen(buffer) - 1] = 0;
						if (buffer[0] == 0) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						tokennr = strtol(buffer, NULL, 0);

						if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						buffer[strlen(buffer) - 1] = 0;
						if (buffer[0] == 0) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						title = strdup(buffer);
						LOG("Menu entry %s\n", title);

						if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						buffer[strlen(buffer) - 1] = 0;
						if (buffer[0] == 0) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						searchterm = strdup(buffer);
						LOG("searchterm %s\n", buffer);

						addit = 1;

						/* Empty line */
						fgets(buffer, BUFFER_SIZE, fin);
						buffer[strlen(buffer) - 1] = 0;
						break;

					case GUI_STATE_MENU_PLAYLIST:
						if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						buffer[strlen(buffer) - 1] = 0;
						if (buffer[0] == 0) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						tokennr = strtol(buffer, NULL, 0);

						if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						buffer[strlen(buffer) - 1] = 0;
						if (buffer[0] == 0) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						title = strdup(buffer);
						LOG("Menu entry %s\n", buffer);

						if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						buffer[strlen(buffer) - 1] = 0;
						if (buffer[0] == 0) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						playllistid = strdup(buffer);
						LOG("playllistid %s\n", buffer);

						if (fgets(buffer, BUFFER_SIZE, fin) == NULL) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						buffer[strlen(buffer) - 1] = 0;
						if (buffer[0] == 0) {
							LOG_ERROR("Failed reading menu.\n");
							error = 1;
							break;
						}
						channelid = strdup(buffer);
						LOG("channelid %s\n", buffer);

						addit = 1;

						/* Empty line */
						fgets(buffer, BUFFER_SIZE, fin);
						buffer[strlen(buffer) - 1] = 0;
						break;
					default:
						LOG("Load menu entry for state %d %s not supported\n", state, get_state_text(state));
						break;
				}

				if (addit && !error) {
					entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, title, state, initstate);
				}
				if (entry != NULL) {
					entry->tokennr = tokennr;
					entry->playlistid = playllistid;
					playllistid = NULL;
					entry->channelid = channelid;
					channelid = NULL;
					entry->searchterm = searchterm;
				} else {
					if (playllistid != NULL) {
						free(playllistid);
						playllistid = NULL;
					}
					if (channelid != NULL) {
						free(channelid);
						channelid = NULL;
					}
					if (searchterm != NULL) {
						free(searchterm);
						searchterm = NULL;
					}
				}

				if (title != NULL) {
					free(title);
					title = NULL;
				}
			}

			free(buffer);
			buffer = NULL;
		}
		fclose(fin);
		fin = NULL;
	}
}


/** Initialize graphic. */
gui_t *gui_alloc(const char *sharedir, int fullscreen, const char *searchterm)
{
	gui_t *gui;
	const SDL_VideoInfo *info;
	int i;
	int found;
	Uint32 flags;
	char *filename = NULL;
	int ret;
	gui_menu_entry_t *entry;

	flags = 0;
	if (fullscreen) {
		flags |= SDL_FULLSCREEN;
	}

	gui = malloc(sizeof(*gui));
	if (gui == NULL) {
		LOG_ERROR("Out of memory.\n");
		return NULL;
	}
	memset(gui, 0 , sizeof(*gui));
	gui->mindistance = 34;
	gui->sharedir = sharedir;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
		LOG_ERROR("Couldn't initialize SDL: %s\n", SDL_GetError());
		return NULL;
	}

	SDL_ShowCursor(SDL_DISABLE);

	TTF_Init();

	gui->logo = gui_get_image(gui, "yt_powered.jpg");
	if (gui->logo == NULL) {
		LOG_ERROR("Failed to load youtube logo.\n");
		gui_free(gui);
		return NULL;
	}

	ret = asprintf(&filename, "%s/FreeSansBold.ttf", gui->sharedir);
	if (ret == -1) {
		return NULL;
	}
	gui->font = TTF_OpenFont(filename, 36);
	free(filename);
	filename = NULL;

	if (gui->font == NULL) {
		gui->font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 36);
	}
	if (gui->font == NULL) {
		LOG("Failed to load /usr/share/fonts/truetype/freefont/FreeSansBold.ttf.\n");
		gui->font = TTF_OpenFont("/usr/share/fonts/truetype/DejaVuSans-Bold.ttf", 36);
	}
	if (gui->font == NULL) {
		LOG_ERROR("Failed to load font.\n");
		gui_free(gui);
		return NULL;
	}

	ret = asprintf(&filename, "%s/FreeSansBold.ttf", gui->sharedir);
	if (ret == -1) {
		return NULL;
	}
	gui->smallfont = TTF_OpenFont(filename, 36);
	free(filename);
	filename = NULL;
	if (gui->smallfont == NULL) {
		gui->smallfont = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 16);
	}
	if (gui->smallfont == NULL) {
		LOG("Failed to load /usr/share/fonts/truetype/freefont/FreeSansBold.ttf.\n");
		gui->smallfont = TTF_OpenFont("/usr/share/fonts/truetype/DejaVuSans-Bold.ttf", 16);
	}
	if (gui->smallfont == NULL) {
		LOG_ERROR("Failed to load font.\n");
		gui_free(gui);
		return NULL;
	}

	info = SDL_GetVideoInfo();
	if (info != NULL)
		gui->screen = SDL_SetVideoMode(info->current_w, info->current_h, 16, SDL_SWSURFACE | SDL_ANYFORMAT | flags);
	if (gui->screen == NULL)
		gui->screen = SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE | SDL_ANYFORMAT | flags);
	if (gui->screen == NULL)
		gui->screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE | SDL_ANYFORMAT | flags);
	if (gui->screen == NULL) {
		LOG_ERROR("Couldn't set 640x480x16 video mode: %s\n", SDL_GetError());
		gui_free(gui);
		return NULL;
	}
	gui->fullscreenmode = 1;

	gui->logorect.x = gui->screen->w - gui->logo->w - gui->mindistance;
	gui->logorect.y = gui->screen->h - gui->logo->h - gui->mindistance;
	/* Put description of buttons to the same y position as the logo. */
	gui->description_pos = gui->logorect.y;

	if (gui->screen->w > 800) {
		gui->cross = gui_get_image(gui, "cross.jpg");
		gui->circle = gui_get_image(gui, "circle.jpg");
		gui->triangle = gui_get_image(gui, "triangle.jpg");
		gui->square = gui_get_image(gui, "square.jpg");
		gui->descfont = gui->font;
	} else {
		gui->cross = gui_get_image(gui, "cross_small.jpg");
		gui->circle = gui_get_image(gui, "circle_small.jpg");
		gui->triangle = gui_get_image(gui, "triangle_small.jpg");
		gui->square = gui_get_image(gui, "square_small.jpg");
		gui->descfont = gui->smallfont;
	}


	atexit(SDL_Quit);
	atexit(TTF_Quit);

	gui->transfer = transfer_alloc();
	if (gui->transfer == NULL) {
		LOG_ERROR("Out of memory\n");
		gui_free(gui);
		return NULL;
	}


	if (SDL_NumJoysticks() > 0) {
		gui->joystick = SDL_JoystickOpen(0);
		SDL_JoystickEventState(SDL_ENABLE);
	}

	gui->description_status = 0;

	entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, "Login", GUI_STATE_NEW_ACCESS_TOKEN, GUI_STATE_GET_MY_CHANNELS);
	if (entry == NULL) {
		LOG_ERROR("Out of memory\n");
	}
	entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, "Power Off", GUI_STATE_POWER_OFF, GUI_STATE_GET_MY_CHANNELS);
	if (entry == NULL) {
		LOG_ERROR("Out of memory\n");
	}
	entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, "Quit", GUI_STATE_QUIT, GUI_STATE_GET_MY_CHANNELS);
	if (entry == NULL) {
		LOG_ERROR("Out of memory\n");
	}
	entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, "Update", GUI_STATE_UPDATE, GUI_STATE_GET_MY_CHANNELS);
	if (entry == NULL) {
		LOG_ERROR("Out of memory\n");
	}
	entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, "Search", GUI_STATE_SEARCH_ENTER, GUI_STATE_SEARCH_PLAYLIST);
	if (entry == NULL) {
		LOG_ERROR("Out of memory\n");
	} else {
		if (searchterm != NULL) {
			entry->searchterm = strdup(searchterm);
		}
	}

	found = 0;
	for (i = 0; i < MAX_ACCOUNTS; i++) {
		char *accountname;

		accountname = check_token(i);
		if (accountname != NULL) {
			gui_menu_entry_t *entry;

			entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, accountname, GUI_STATE_LOAD_ACCESS_TOKEN, GUI_STATE_GET_MY_CHANNELS);
			if (entry != NULL) {
				entry->tokennr = i;
				gui->account_allocated[i] = 1;
				if (!found) {
					/* Select first account as default. */
					gui->selectedmenu = entry;
				}
				found = 1;
			}
			free(accountname);
		}
	}
	load_menu_state(gui);

	return gui;
}

static int get_next_tokennr(gui_t *gui)
{
	int i;
	
	for (i = 0; i < MAX_ACCOUNTS; i++) {
		if (!gui->account_allocated[i]) {
			gui->account_allocated[i] = 1;
			return i;
		}
	}
	return -1;
}

static jt_access_token_t *alloc_token(int nr)
{
	const char *home;
	jt_access_token_t *at;
	char *tokenfile = NULL;
	char *refreshtokenfile = NULL;
	char *keyfile = NULL;
#ifndef CLIENT_SECRET
	char *secretfile = NULL;
#endif
	int ret;
	int flags = 0;

	home = getenv("HOME");
	if (home == NULL) {
		LOG_ERROR("Environment variable HOME is not set.\n");
		return NULL;
	}
	
	ret = asprintf(&tokenfile, "%s/%s%03d.json", home, TOKEN_FILE, nr);
	if (ret == -1) {
		LOG_ERROR("Out of memory\n");
		return NULL;
	}

	ret = asprintf(&refreshtokenfile, "%s/%s%03d.json", home, REFRESH_TOKEN_FILE, nr);
	if (ret == -1) {
		free(tokenfile);
		tokenfile = NULL;
		LOG_ERROR("Out of memory\n");
		return NULL;
	}

	ret = asprintf(&keyfile, "%s/%s", home, KEY_FILE);
	if (ret == -1) {
		free(refreshtokenfile);
		refreshtokenfile = NULL;
		free(tokenfile);
		tokenfile = NULL;
		LOG_ERROR("Out of memory\n");
		return NULL;
	}

#ifndef CLIENT_SECRET
	ret = asprintf(&secretfile, "%s/%s", home, SECRET_FILE);
	if (ret == -1) {
		free(refreshtokenfile);
		refreshtokenfile = NULL;
		free(tokenfile);
		tokenfile = NULL;
		LOG_ERROR("Out of memory\n");
		return NULL;
	}
#endif

#ifdef NOVERIFYCERT
	flags |= JT_FLAG_NO_CERT;
#endif
#ifdef CLIENT_SECRET
	at = jt_alloc(logfd, errfd, CLIENT_ID, CLIENT_SECRET, tokenfile, refreshtokenfile, CLIENT_KEY, flags);
#else
	at = jt_alloc_by_file(logfd, errfd, secretfile, tokenfile, refreshtokenfile, keyfile, flags);
#endif
	free(tokenfile);
	tokenfile = NULL;
	free(refreshtokenfile);
	refreshtokenfile = NULL;
	free(keyfile);
	keyfile = NULL;
	free(secretfile);
	secretfile = NULL;

	if (at == NULL) {
		LOG_ERROR("Out of memory\n");
		return NULL;
	}
	return at;
}

static void delete_token(int nr)
{
	const char *home;
	char *tokenfile = NULL;
	char *refreshtokenfile = NULL;
	char *titlefile = NULL;
	int ret;

	home = getenv("HOME");
	if (home == NULL) {
		LOG_ERROR("Environment variable HOME is not set.\n");
		return;
	}

	ret = asprintf(&tokenfile, "%s/%s%03d.json", home, TOKEN_FILE, nr);
	if (ret == -1) {
		LOG_ERROR("Out of memory\n");
		return;
	}
	unlink(tokenfile);
	free(tokenfile);
	tokenfile = NULL;

	ret = asprintf(&refreshtokenfile, "%s/%s%03d.json", home, REFRESH_TOKEN_FILE, nr);
	if (ret == -1) {
		LOG_ERROR("Out of memory\n");
		return;
	}
	unlink(refreshtokenfile);
	free(refreshtokenfile);
	refreshtokenfile = NULL;

	ret = asprintf(&titlefile, "%s/%s%03d.txt", home, TITLE_FILE, nr);
	if (ret == -1) {
		free(tokenfile);
		tokenfile = NULL;
		LOG_ERROR("Out of memory\n");
		return;
	}
	unlink(titlefile);
	free(titlefile);
	titlefile = NULL;
}

void gui_free_categories(gui_t *gui)
{
	if (gui != NULL) {
		gui_cat_t *cat;
		gui_cat_t *next;

		/* Was allocated as part of the list gui->categories. */
		gui->current = NULL;
		gui->cur_cat = NULL;
		gui->prev_cat = NULL;


		cat = gui->categories;
		gui->categories = NULL;
		while(cat != NULL) {
			next = cat->next;
			if (cat == cat->prev) {
				/* This is the last one, there is no next. */
				next = NULL;
			}
			gui_cat_free(gui, cat);
			cat = NULL;
			cat = next;
		}

		cat = gui->get_playlist_cat;
		gui->get_playlist_cat = NULL;
		while(cat != NULL) {
			next = cat->next;
			if (cat == cat->prev) {
				/* This is the last one, there is no next. */
				next = NULL;
			}
			gui_cat_free(gui, cat);
			cat = NULL;
			cat = next;
		}

		cat = gui->get_channel_cat;
		gui->get_channel_cat = NULL;
		while(cat != NULL) {
			next = cat->next;
			if (cat == cat->prev) {
				/* This is the last one, there is no next. */
				next = NULL;
			}
			gui_cat_free(gui, cat);
			cat = NULL;
			cat = next;
		}
	}
}

/** Clean up of graphic libraries. */
void gui_free(gui_t *gui)
{
	if (gui != NULL) {
		gui_menu_entry_t *menu;
		gui_menu_entry_t *mnext;

		gui_free_categories(gui);

		menu = gui->mainmenu;
		gui->mainmenu = NULL;
		gui->selectedmenu = NULL;
		while(menu != NULL) {
			mnext = menu->next;
			if (menu == menu->prev) {
				/* This is the last one, there is no next. */
				mnext = NULL;
			}
			gui_menu_entry_free(gui, menu);
			menu = NULL;
			menu = mnext;
		}

		if (gui->joystick != NULL) {
			SDL_JoystickClose(gui->joystick);
			gui->joystick = NULL;
		}

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
		gui->descfont = NULL;
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
		if (gui->cross != NULL) {
			SDL_FreeSurface(gui->cross);
			gui->cross = NULL;
		}
		if (gui->circle != NULL) {
			SDL_FreeSurface(gui->circle);
			gui->circle = NULL;
		}
		if (gui->square != NULL) {
			SDL_FreeSurface(gui->square);
			gui->square = NULL;
		}
		if (gui->triangle != NULL) {
			SDL_FreeSurface(gui->triangle);
			gui->triangle = NULL;
		}
		if (gui->screen != NULL) {
			SDL_FreeSurface(gui->screen);
			gui->screen = NULL;
		}
		if (gui->cross_text != NULL) {
			SDL_FreeSurface(gui->cross_text);
			gui->cross_text = NULL;
		}
		if (gui->circle_text != NULL) {
			SDL_FreeSurface(gui->circle_text);
			gui->circle_text = NULL;
		}
		if (gui->square_text != NULL) {
			SDL_FreeSurface(gui->square_text);
			gui->square_text = NULL;
		}
		if (gui->triangle_text != NULL) {
			SDL_FreeSurface(gui->triangle_text);
			gui->triangle_text = NULL;
		}
		if (gui->screen != NULL) {
			SDL_FreeSurface(gui->screen);
			gui->screen = NULL;
		}

		cache_title_free(gui);

		TTF_Quit();
		SDL_VideoQuit();
		SDL_Quit();
		free(gui);
		gui = NULL;
	}
}

static SDL_Surface *gui_load_image(transfer_t *transfer, const char *url)
{
	SDL_Surface *image = NULL;
	void *mem = NULL;
	int size;

	if (url == NULL) {
		return NULL;
	}

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

/**
 * Print text to an SDL surface.
 *
 * @param image This object is freed.
 * @param format printf-like format string.
 *
 * @returns SDL surface which should replace the image.
 */
static SDL_Surface *gui_printf(TTF_Font *font, SDL_Surface *image, const char *format, ...)
{
	va_list ap;
	SDL_Color clrFg = {255, 255, 255, 0}; /* White */
	char *text = NULL;
	int ret;
	SDL_Surface *rv;

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

	rv = TTF_RenderUTF8_Solid(font, text, clrFg);

	free(text);
	text = NULL;

	return rv;
}

static SDL_Surface *cached_title(gui_t *gui, const char *title)
{
	int i;

	for (i = 0; i < TITLE_CACHE_ENTRIES; i++) {
		if (gui->title_cache_table[i].title == title) {
			gui->title_cache_table[i].count++;
			return gui->title_cache_table[i].sText;
		}
	}
	//printf("Cache miss for: %s\n", title);
	return NULL;
}

static void gui_paint_cat_view(gui_t *gui)
{
	SDL_Rect rcDest = { BORDER_X /* X pos */, 90 /* Y pos */, 0, 0 };
	gui_cat_t *cat;
	int load_counter;
	int i;

	i = 0;

	load_counter = 0;
	cat = gui->current;
	if (cat != NULL) {
		SDL_Surface *sText = NULL;

		if (cat->title != NULL) {
			int nr;

			nr = cat->subnr;
			if (cat->subnr == 0) {
				nr = cat->channelNr;
			}
			/* Convert text to an image. */
			if (cat->title != NULL) {
				sText = gui_printf(gui->font, sText, "%03d %s", nr + 1, cat->title);
			} else {
				sText = gui_printf(gui->font, sText, "%03d No Title", nr + 1);
			}
		}
		if (sText != NULL) {
			SDL_Rect headerSrc = { 0, 0, 0, 0};
			SDL_Rect headerDest = {40, 40, 0, 0};

			headerSrc.w = sText->w;
			headerSrc.h = sText->h;
			if (sText->w > (gui->screen->w - 2 * headerDest.x)) {
				headerSrc.w = gui->screen->w - 2 * headerDest.x;
				cat->scrollpos += cat->scrolldir;
				headerSrc.x = cat->scrollpos;
				if (cat->scrollpos <= -headerDest.x) {
					cat->scrolldir = 1;
				}
				if (cat->scrollpos <= 0) {
					/* Stop some time at position 0. */
					headerSrc.x = 0;
				}
				if ((sText->w - cat->scrollpos) < (headerSrc.w - headerDest.x)) {
					cat->scrolldir = -1;
				}
				if ((sText->w - cat->scrollpos) < headerSrc.w) {
					/* Stop some time at the end. */
					headerSrc.x = sText->w - headerSrc.w;
				}
			}

			SDL_BlitSurface(sText, &headerSrc, gui->screen, &headerDest);
			SDL_FreeSurface(sText);
			sText = NULL;
		}
	} else {
		SDL_Surface *sText = NULL;

		sText = gui_printf(gui->font, sText, "Nothing loaded");
		if (sText != NULL) {
			SDL_Rect headerDest = {40, 40, 0, 0};
			rcDest.x = BORDER_X;

			SDL_BlitSurface(sText, NULL, gui->screen, &headerDest);
			SDL_FreeSurface(sText);
			sText = NULL;
		}
	}
	while ((cat != NULL) && (i < MAX_SHOW)) {
		gui_elem_t *current;
		int maxHeight = 0;
		int j;

		rcDest.x = BORDER_X;

		current = cat->current;
		if (current == NULL) {
			SDL_Surface *sText = NULL;

			sText = gui_printf(gui->font, sText, "No video in playlist");
			if (sText != NULL) {
				if ((gui->description_pos - gui->mindistance) >= (rcDest.y + sText->h)) {
					SDL_BlitSurface(sText, NULL, gui->screen, &rcDest);
				}
				if (maxHeight < sText->h) {
					maxHeight = sText->h;
				}
				SDL_FreeSurface(sText);
				sText = NULL;
			}
		}
		j = 0;
		while ((current != NULL) && (j < MAX_VIDS)) {
			SDL_Surface *image;

			if ((cat == gui->current) && (current == cat->current) && (current->urlmedium != NULL)) {
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
					current->imagemedium = gui_printf(gui->font, current->imagemedium, "No Thumbnail");
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
					current->image = gui_printf(gui->font, current->image, "No Thumbnail");
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
				if ((gui->description_pos - gui->mindistance) < (rcDest.y + image->h)) {
					overlap_y = 1;
					overlap_x = 1;
				}
				if (!overlap_x || !overlap_y) {
					/* Show video title for first/selected video. */
					SDL_Surface *sText = NULL;

					SDL_BlitSurface(image, NULL, gui->screen, &rcDest);

					/* Print video title under the image. */
					if (cat->title != NULL) {
						sText = cached_title(gui, current->title);

						if (sText == NULL) {
							/* Convert text to an image. */
							sText = gui_printf(gui->smallfont, sText, "[%d] %s", current->subnr + 1, current->title);
							if (sText != NULL) {
								add_cached_title(gui, sText, current->title);
							}
						}
					}
					if (sText != NULL) {
						SDL_Rect headerDest = rcDest;
						SDL_Rect headerSrc;
						headerDest.y += image->h + 10;
						if (current->textScrollPos > (sText->w - image->w + 10)) {
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
										current->textScrollPos += 3;
									}
								} else {
									if (current->textScrollPos == 0) {
										current->textScrollDir = 0;
										current->textScrollCounter = SCROLL_COUNT_END;
									} else {
										current->textScrollPos -= 3;
									}
								}
							}
						} else {
							headerSrc.w = image->w;
						}
						headerSrc.h = image->h;
						SDL_BlitSurface(sText, &headerSrc, gui->screen, &headerDest);
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
			j++;
		}
		rcDest.y += maxHeight + BORDER_Y;
		if (rcDest.y >= gui->screen->h) {
			break;
		}
		if (gui->prev_cat != NULL) {
			if ((cat->next != NULL) && (cat->nextPageState != cat->next->nextPageState)) {
				/* Don't show anything outside selected playlist. */
				break;
			}
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
		i++;
	}
}

static void gui_paint_main_view(gui_t *gui)
{
	SDL_Rect rcDest = { BORDER_X /* X pos */, 90 /* Y pos */, 0, 0 };
	gui_menu_entry_t *entry;

	entry = gui->selectedmenu;
	while(entry != NULL) {
		SDL_Rect headerSrc = { 0, 0, 0, 0 };

		if ((entry->textimg == NULL) && (entry->title != NULL)) {
			entry->textimg = gui_printf(gui->font, entry->textimg, entry->title);
		}
		if (entry->textimg != NULL) {
			int maxHeight;
			SDL_Surface *image;
			int scroll = 0;

			image = entry->textimg;
			maxHeight = image->h;

			if ((gui->description_pos - gui->mindistance) >= (rcDest.y + image->h)) {
				headerSrc.h = image->h;
				headerSrc.w = image->w;
				if ((headerSrc.w + BORDER_X) > (gui->screen->w - BORDER_X)) {
					headerSrc.w = gui->screen->w - 2 * BORDER_X;
					scroll = 1;
				}

				if (entry == gui->selectedmenu) {
					if (scroll) {
						headerSrc.x = entry->pos;
						entry->pos += entry->dir;
						if ((image->w - entry->pos) < (headerSrc.w - BORDER_X)) {
							entry->dir = -1;
						}
						if ((image->w - entry->pos) < headerSrc.w) {
							/* Stop some time at the end. */
							headerSrc.x = image->w - headerSrc.w;
						}
						if (entry->pos <= -BORDER_X) {
							entry->dir = 1;
						}
						if (entry->pos <= 0) {
							/* Stop some time at position 0. */
							headerSrc.x = 0;
						}
					} else {
						rcDest.x = BORDER_X;
						entry->pos += entry->dir;
						if (entry->pos >= 28) {
							entry->dir = -1;
						}
						if (entry->pos == 0) {
							entry->dir = 1;
						}
					}
				}
				if ((entry != gui->selectedmenu) || scroll || (entry->pos <= 14)) {
					SDL_BlitSurface(image, &headerSrc, gui->screen, &rcDest);
				}
				rcDest.x = BORDER_X;
				rcDest.y += maxHeight + BORDER_Y;
			}
		}

		/* Get next menu entry. */
		entry = entry->next;
		if (entry == gui->mainmenu) {
			break;
		}
	}
}

static void gui_paint_status(gui_t *gui)
{
	const char *text;
	char t[5];
	SDL_Rect rcDest = {BORDER_X, BORDER_Y, 0, 0};
	int maxHeight = 0;

	text = gui->statusmsg;

	while(*text != 0) {
		int i;

		t[0] = 0;
		t[1] = 0;
		t[2] = 0;
		t[3] = 0;
		t[4] = 0;
		for (i = 0; i < 4; i++) {
			t[i] = *text;
			if ((t[i] & 0x80) == 0x00) {
				break;
			}
			text++;
		}
		if (*text == '\n') {
			rcDest.x = BORDER_X;
			rcDest.y += maxHeight + BORDER_Y;
			maxHeight = 0;
		} else {
			SDL_Surface *sText = NULL;

			sText = gui_printf(gui->font, sText, t);
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

static void gui_paint_pic_text(gui_t *gui, SDL_Rect *rect, SDL_Surface *image, SDL_Surface *text)
{
	if ((image != NULL) && (text != NULL)) {
		if ((gui->logorect.y + gui->logo->h + gui->mindistance) > rect->y) {
			if ((gui->logorect.x - gui->mindistance) < (rect->x + image->w + 10 + text->w)) {
				rect->y += text->h + 10;
				rect->x = gui->mindistance;
			}
		}
		rect->y += (text->h - image->h) / 2;
		SDL_BlitSurface(image, NULL, gui->screen, rect);
		rect->y -= (text->h - image->h) / 2;
		rect->x += image->w + 10;
		SDL_BlitSurface(text, NULL, gui->screen, rect);
		rect->x += text->w + gui->mindistance;
	}
}

static void gui_paint_nav(gui_t *gui)
{
	SDL_Rect rect;

	memset(&rect, 0, sizeof(rect));

	rect.x = gui->mindistance;
	rect.y = gui->description_pos;

	gui_paint_pic_text(gui, &rect, gui->cross, gui->cross_text);
	gui_paint_pic_text(gui, &rect, gui->circle, gui->circle_text);
	gui_paint_pic_text(gui, &rect, gui->triangle, gui->triangle_text);
	gui_paint_pic_text(gui, &rect, gui->square, gui->square_text);
}

/**
 * Paint GUI.
 */
static void gui_paint(gui_t *gui, enum gui_state state)
{
	SDL_FillRect(gui->screen, NULL, 0x000000);

	SDL_BlitSurface(gui->logo, NULL, gui->screen, &gui->logorect);
	gui_paint_nav(gui);

	if (gui->statusmsg != NULL) {
		gui_paint_status(gui);
	} else {
		if (state == GUI_STATE_MAIN_MENU) {
			gui_paint_main_view(gui);
		} else {
			gui_paint_cat_view(gui);
		}
	}

	/* Update the screen content. */
	SDL_UpdateRect(gui->screen, 0, 0, gui->screen->w, gui->screen->h);
}

/** Free large thumbnails to get more memory for new thumbnails. */
static void gui_cat_large_free(gui_cat_t *cat)
{
	gui_elem_t *elem;

	elem = cat->elem;
	while(elem != NULL) {
		elem = elem->next;

		if (elem->loadedmedium == IMG_LOADED) {
			/* The image was successfully loaded, loading it again
			 * should work again.
			 */
			elem->loadedmedium = 0;
		}
		if (elem->imagemedium != NULL) {
			/* Free some memory, image can be reloaded later when
			 * needed.
			 */
			SDL_FreeSurface(elem->imagemedium);
			elem->imagemedium = NULL;
		}
		if (elem == cat->elem) {
			/* Last element in list. */
			break;
		}
	}
}

/** Free smaller thumbnails to get more memory for new thumbnails. */
static void gui_cat_small_free(gui_cat_t *cat)
{
	gui_elem_t *elem;

	elem = cat->elem;
	while(elem != NULL) {
		elem = elem->next;
		if (elem->loaded == IMG_LOADED) {
			/* The image was successfully loaded, loading it again
			 * should work again.
			 */
			elem->loaded = 0;
		}
		if (elem->image != NULL) {
			/* Free some memory, image can be reloaded later when
			 * needed.
			 */
			SDL_FreeSurface(elem->image);
			elem->image = NULL;
		}
		if (elem == cat->elem) {
			/* Last element in list. */
			break;
		}
	}
}

/** Get the token for the previous page. */
static char *gui_get_prevPageToken(gui_cat_t *cat)
{
	if (cat != NULL) {
		char *prevPageToken;

		switch(cat->prevPageState)
		{
			case GUI_STATE_GET_PREV_FAVORITES:
				prevPageToken = cat->favoritesPrevPageToken;
				break;

			case GUI_STATE_GET_PREV_SUBSCRIPTIONS:
				prevPageToken = cat->subscriptionPrevPageToken;
				break;

			case GUI_STATE_GET_PREV_CHANNEL_PLAYLIST:
				prevPageToken = cat->channelPrevPageToken;
				break;

			case GUI_STATE_GET_MY_PREV_CHANNELS:
#if 0
				prevPageToken = cat->channelPrevPageToken;
#else
				/* TBD: No support in navigator for unloading this side. */
				prevPageToken = NULL;
#endif
				break;

			case GUI_STATE_MENU_PLAYLIST:
				/* Not supported. */
				prevPageToken = NULL;
				break;

			case GUI_STATE_SEARCH:
				/* Not supported. */
				prevPageToken = NULL;
				break;

			default:
				LOG_ERROR(__FILE__ ":%d: %s not supported in category %s.\n", __LINE__,
					get_state_text(cat->prevPageState), cat->title);
				prevPageToken = NULL;
				break;
		}
		return prevPageToken;
	} else {
		return NULL;
	}
}

/** Get the token for the next page. */
static char *gui_get_nextPageToken(gui_cat_t *cat)
{
	if (cat != NULL) {
		char *nextPageToken;

		switch(cat->nextPageState)
		{
			case GUI_STATE_GET_FAVORITES:
				nextPageToken = cat->favoritesNextPageToken;
				break;

			case GUI_STATE_GET_SUBSCRIPTIONS:
				nextPageToken = cat->subscriptionNextPageToken;
				break;

			case GUI_STATE_GET_CHANNEL_PLAYLIST:
				nextPageToken = cat->channelNextPageToken;
				break;

			case GUI_STATE_GET_MY_CHANNELS:
#if 0
				nextPageToken = cat->channelNextPageToken;
#else
				/* TBD: No support in navigator for unloading this side. */
				nextPageToken = NULL;
#endif
				break;

			case GUI_STATE_MENU_PLAYLIST:
				/* Not supported. */
				nextPageToken = NULL;
				break;

			case GUI_STATE_SEARCH:
				/* Not supported. */
				nextPageToken = NULL;
				break;

			default:
				LOG_ERROR(__FILE__ ":%d: %s not supported in category %s.\n", __LINE__,
					get_state_text(cat->nextPageState), cat->title);
				nextPageToken = NULL;
				break;
		}
		return nextPageToken;
	} else {
		return NULL;
	}
}

/** Select next category in list and free "older" stuff. */
static int gui_inc_cat(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;

	if (cat != NULL) {
		int n;

		if ((gui->prev_cat != NULL) && (cat->next != NULL) && (cat->nextPageState != cat->next->nextPageState)) {
			/* Don't go outside selected playlist. */
			return 1;
		}
		gui->current = cat->next;

		/* Free thumbnail which are currently not shown. */
		n = 0;
		while((cat != NULL) && (cat != gui->categories->prev)) {
			if (n == PRESERVE_MEDIUM_IMAGES) {
				/* Free medium images. */
				gui_cat_large_free(cat);
			} else if (n == PRESERVE_SMALL_IMAGES) {
				/* Free small images. */
				gui_cat_small_free(cat);
			} else if (n == PRESERVE_CAT) {
				if ((cat->prevPageState == cat->next->prevPageState)) {
					char *prevPageToken;

					prevPageToken = gui_get_prevPageToken(cat->next);

					if (prevPageToken != NULL) {
						/* Temporary remove list from memory.
						 * List can be loaded later again.
						 */
						/* Must be same type to be restorable. */
						gui_cat_free(gui, cat);
						cat = NULL;
						break;
					}
				}
			}
			if (gui->prev_cat != NULL) {
				/* Playlist was selected. Don't delete stuff outside the playlist. */
				if ((cat->prev != NULL) && (cat->prevPageState != cat->prev->prevPageState)) {
					break;
				}
			}
			cat = cat->prev;
			n++;
		}
	}
	return 0;
}

/** Select previous category in list and free "older" stuff. */
static int gui_dec_cat(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;
	if (cat != NULL) {
		int n;

		if ((gui->prev_cat != NULL) && (cat->prev != NULL) && (cat->prevPageState != cat->prev->prevPageState)) {
			/* Don't go outside selected playlist. */
			return 1;
		}
		gui->current = cat->prev;

		/* Free thumbnail which are currently not shown. */
		n = 0;
		while((cat != NULL) && (cat != gui->categories)) {
			if (n == PRESERVE_MEDIUM_IMAGES) {
				/* Free medium images. */
				gui_cat_large_free(cat);
			} else if (n == PRESERVE_SMALL_IMAGES) {
				/* Free small images. */
				gui_cat_small_free(cat);
			} else if (n == PRESERVE_CAT) {
				if ((cat->nextPageState == cat->prev->nextPageState)) {
					char *nextPageToken;

					nextPageToken = gui_get_nextPageToken(cat->prev);

					if (nextPageToken != NULL) {
						/* Temporary remove list from memory.
						 * List can be loaded later again.
						 */
						/* Must be same type to be restorable. */
						gui_cat_free(gui, cat);
						cat = NULL;
						break;
					}
				}
				break;
			}
			if (gui->prev_cat != NULL) {
				/* Playlist was selected. Don't delete stuff outside the playlist. */
				if ((cat->next != NULL) && (cat->nextPageState != cat->next->nextPageState)) {
					break;
				}
			}
			cat = cat->next;
			n++;
		}
	}
	return 0;
}

/** Select next element in list. */
static void gui_inc_elem(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;
	if (cat != NULL) {
		gui_elem_t *elem;

		elem = cat->current;
		if (elem != NULL) {
			int n;

			cat->current = elem->next;

			/* Free thumbnail which are currently not shown. */
			n = 0;
			while((elem != NULL) && (elem != cat->elem->prev)) {
				char *prevPageToken;

				prevPageToken = elem->prevPageToken;
				if (n >= PRESERVE_ELEM) {
					gui_elem_t *n;

					/* Temporary remove list from memory.
					 * List can be loaded later again.
					 */
					/* Must be same type to be restorable. */
					if (cat->current == elem) {
						cat->current = elem->next;
					}
					if (cat->elem == elem) {
						cat->elem = elem->next;
					}
					n = elem->next;
					gui_elem_free(elem);
					elem = NULL;
					elem = n;
				}
				if (prevPageToken != NULL) {
					n++;
				}
				elem = elem->prev;
			}
		}
	}
}

/** Select previous element in list. */
static void gui_dec_elem(gui_t *gui)
{
	gui_cat_t *cat;

	cat = gui->current;
	if (cat != NULL) {
		gui_elem_t *elem;

		elem = cat->current;

		if (elem != NULL) {
			int n;
			cat->current = elem->prev;

			/* Free thumbnail which are currently not shown. */
			n = 0;
			while((elem != NULL) && (elem != cat->elem)) {
				char *nextPageToken;

				nextPageToken = elem->nextPageToken;
				if (n >= PRESERVE_ELEM) {
					gui_elem_t *n;

					/* Temporary remove list from memory.
					 * List can be loaded later again.
					 */
					/* Must be same type to be restorable. */
					if (cat->current == elem) {
						cat->current = elem->prev;
					}
					if (cat->elem == elem) {
						cat->elem = elem->prev;
					}
					n = elem->prev;
					gui_elem_free(elem);
					elem = NULL;
					elem = n;
				}
				if (nextPageToken != NULL) {
					n++;
				}
				elem = elem->next;
			}
		}
	}
}


static int update_playlist(gui_t *gui, gui_cat_t *cat, int reverse)
{
	int rv;
	int subnr;
	const char *pageToken;
	gui_elem_t *last;

	if (cat->elem != NULL) {
		/* Get the number of the last element in the list. */
		if (reverse) {
			gui_elem_t *first;

			first = cat->elem;
			last = NULL;

			pageToken = first->prevPageToken;
			subnr = first->subnr;
		} else {
			last = cat->elem->prev;
			pageToken = last->nextPageToken;
			subnr = last->subnr;
			subnr++;
		}
		if (pageToken == NULL) {
			LOG_ERROR("%s: Bad pageToken in cat %s.\n", __FUNCTION__, cat->title);
			return JT_ERROR;
		}
	} else {
		last = NULL;

		/* This is a new list start with number 0. */
		if (cat->videopagetoken != NULL) {
			pageToken = cat->videopagetoken;
			subnr = cat->vidnr;
		} else {
			subnr = 0;
			pageToken = "";
		}
	}

	rv = jt_get_playlist_items(gui->at, cat->playlistid, pageToken);
	pageToken = NULL;

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
		if (reverse) {
			subnr -= resultsPerPage;
		}
		for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
			const char *url;
			gui_elem_t *elem;
			int val;

			val = subnr;
			rv = jt_json_get_int_by_path(gui->at, &val, "/items[%d]/snippet/position", i);
			if (rv == JT_OK) {
				subnr = val;
			} else {
				rv = JT_OK;
			}
			
			url = jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/thumbnails/default/url", i);

			elem = gui_elem_alloc(gui, cat, last, url);
			if (elem != NULL) {
				last = elem;
				if (i == 0) {
					elem->prevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
					if (!reverse) {
						cat->current = elem;
					}
				}
			 	elem->title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
				elem->urlmedium = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/thumbnails/medium/url", i));
				elem->videoid = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/resourceId/videoId", i));
				elem->subnr = subnr;
			}
			subnr++;
		}

		if (last != NULL) {
			if (last->nextPageToken != NULL) {
				free(last->nextPageToken);
				last->nextPageToken = NULL;
			}
			last->nextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
			if (reverse) {
				cat->current = last;
			}
		}
		jt_free_transfer(gui->at);
	} else {
		if (rv == JT_PROTOCOL_ERROR) {
			const char *error;

			error = jt_get_protocol_error(gui->at);
			LOG_ERROR("Playlist %s protocol error '%s' in cat %s.\n", cat->playlistid, error, cat->title);
			if (error != NULL) {
				if (strcmp(error, "playlistNotFound") == 0) {
					LOG_ERROR("Playlist %s not found n cat %s.\n", cat->playlistid, cat->title);
					rv = JT_OK;
				}
			}
		}
	}
	return rv;
}

/** Get channelid where the selected video comes from. */
static int update_channelid_of_video(gui_t *gui, gui_elem_t *selected_video)
{
	int rv = JT_ERROR;

	if (selected_video != NULL) {
		if ((selected_video->videoid != NULL) && (selected_video->channelid == NULL)) {
			rv = jt_get_video(gui->at, selected_video->videoid);
			if (rv == JT_OK) {
				selected_video->channelid = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/channelId", 0));
				jt_free_transfer(gui->at);
			}
		}
	}
	return rv;
}


static int update_favorites(gui_t *gui, gui_cat_t *selected_cat, int reverse)
{
	int rv;
	const char *pageToken;
	int favnr;

	if (selected_cat != NULL) {
		favnr = selected_cat->favnr;
		if (reverse) {
			pageToken = selected_cat->favoritesPrevPageToken;
		} else {
			pageToken = selected_cat->favoritesNextPageToken;
			favnr++;
		}
		if (pageToken == NULL) {
			LOG_ERROR("%s: Bad pageToken in cat %s.\n", __FUNCTION__, selected_cat->title);
			return JT_ERROR;
		}
	} else {
		pageToken = "";
		favnr = 0;
	}

	rv = jt_get_my_playlist(gui->at, pageToken);

	if (rv == JT_OK) {
		gui_cat_t *last;
		int totalResults = 0;
		int resultsPerPage = 0;
		int i;

		if (reverse) {
			if (selected_cat != NULL) {
				last = selected_cat->prev;
			}
		} else {
			last = selected_cat;
		}

		rv = jt_json_get_int_by_path(gui->at, &totalResults, "/pageInfo/totalResults");
		if (rv != JT_OK) {
			totalResults = 0;
		}

		rv = jt_json_get_int_by_path(gui->at, &resultsPerPage, "/pageInfo/resultsPerPage");
		if (rv != JT_OK) {
			resultsPerPage = 0;
		}

		if (reverse) {
			favnr -= resultsPerPage;
		}
		for (i = 0; (i < resultsPerPage) && (favnr < totalResults); i++) {
			gui_cat_t *cat;

			cat = gui_cat_alloc(gui, &gui->get_playlist_cat, last);
			if (cat != NULL) {
				last = cat;
				cat->nextPageState = GUI_STATE_GET_FAVORITES;
				cat->prevPageState = GUI_STATE_GET_PREV_FAVORITES;
				cat->channelid = jt_strdup(jt_json_get_string_by_path(gui->at, "/snippet[%d]/channelId", i));
				cat->playlistid = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/id", i));
		 		cat->title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
				cat->favnr = favnr;
				if (i == 0) {
					cat->favoritesPrevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
				}
			}
			favnr++;
		}
		if (last != NULL) {
			if (last->nextPageState == GUI_STATE_GET_FAVORITES) {
				if (last->favoritesNextPageToken != NULL) {
					free(last->favoritesNextPageToken);
					last->favoritesNextPageToken = NULL;
				}
				last->favoritesNextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
			}
		}
		jt_free_transfer(gui->at);
	}
	return rv;
}

static int update_subscriptions(gui_t *gui, gui_cat_t *selected_cat, int reverse, const char *catpagetoken, int catnr, const char *selected_playlistid, const char *videopagetoken, int vidnr)
{
	int rv;
	int subnr;
	const char *pageToken;

	if (selected_cat != NULL) {
		subnr = selected_cat->subnr;
		if (reverse) {
			pageToken = selected_cat->subscriptionPrevPageToken;
		} else {
			pageToken = selected_cat->subscriptionNextPageToken;
			subnr++;
		}
		if (pageToken == NULL) {
			LOG_ERROR("%s: Bad pageToken in cat %s.\n", __FUNCTION__, selected_cat->title);
			return JT_ERROR;
		}
	} else {
		subnr = 0;
		pageToken = "";
	}
	if (catpagetoken != NULL) {
		pageToken = catpagetoken;
		subnr = catnr;
	}

	rv = jt_get_my_subscriptions(gui->at, pageToken);
	if (rv == JT_OK) {
		gui_cat_t *last;
		int totalResults = 0;
		int resultsPerPage = 0;
		int i;

		if (reverse) {
			if (selected_cat != NULL) {
				last = selected_cat->prev;
			}
		} else {
			last = selected_cat;
		}

		rv = jt_json_get_int_by_path(gui->at, &totalResults, "/pageInfo/totalResults");
		if (rv != JT_OK) {
			totalResults = 0;
		}

		rv = jt_json_get_int_by_path(gui->at, &resultsPerPage, "/pageInfo/resultsPerPage");
		if (rv != JT_OK) {
			resultsPerPage = 0;
		}
		if (reverse) {
			subnr -= resultsPerPage;
		}
		for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
			const char *channelid;
			
			channelid = jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/resourceId/channelId", i);
			if (channelid != NULL) {
				gui_cat_t *cat;

				cat = gui_cat_alloc(gui, &gui->get_channel_cat, last);
				if (cat != NULL) {
					last = cat;
					cat->nextPageState = GUI_STATE_GET_SUBSCRIPTIONS;
					cat->prevPageState = GUI_STATE_GET_PREV_SUBSCRIPTIONS;
					cat->channelid = strdup(channelid);
					cat->subnr = subnr;
			 		cat->title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
					/* Check if this playlist should be selected. */
					if (selected_playlistid != NULL) {
						/* Select same video page as selected before. */
						cat->videopagetoken = videopagetoken;
						cat->expected_playlistid = selected_playlistid;
					}
					cat->vidnr = vidnr;
					if (i == 0) {
						cat->subscriptionPrevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
						if ((cat->subscriptionPrevPageToken == NULL) && (resultsPerPage != 0)) {
							int page;

							page = jt_get_page_number(pageToken);
							page -= resultsPerPage;

							if (page > 0) {
								last->subscriptionPrevPageToken = jt_get_page_token(page);
							}
						}
					}
				}
			}
			subnr++;
		}
		if (last != NULL) {
			if (last->nextPageState == GUI_STATE_GET_SUBSCRIPTIONS) {
				if (last->subscriptionNextPageToken != NULL) {
					free(last->subscriptionNextPageToken);
					last->subscriptionNextPageToken = NULL;
				}

				last->subscriptionNextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
				if ((last->subscriptionNextPageToken == NULL) && (resultsPerPage != 0)) {
					int page;

					page = jt_get_page_number(pageToken);
					page += resultsPerPage;

					if (page < totalResults) {
						last->subscriptionNextPageToken = jt_get_page_token(page);
					}
				}
			}
		}
		jt_free_transfer(gui->at);
	}
	return rv;
}

static int update_channels(gui_t *gui, gui_cat_t *selected_cat, gui_cat_t **l)
{
	int rv;
	int channelNr;
	int channelStart;
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
		} else {
			channelNr++;
		}
	}
	channelStart = channelNr;
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

				if ((cat == NULL) || (cat->playlistid != NULL)) {
					/* Need to add new cat for playlist. */
					cat = gui_cat_alloc(gui, &gui->get_playlist_cat, last);
					LOG_ERROR("Call gui_cat_alloc\n");
				}

				if (cat != NULL) {
					char *title;
					const char *channelid;

					last = cat;

					if (selected_cat != NULL) {
						cat->nextPageState = selected_cat->nextPageState;
						cat->prevPageState = selected_cat->prevPageState;
					} else {
						cat->nextPageState = GUI_STATE_GET_CHANNELS;
						cat->prevPageState = GUI_STATE_GET_PREV_CHANNELS;
					}
					cat->channelNr = channelNr;
					cat->channelStart = channelStart;
					channelid = jt_json_get_string_by_path(gui->at, "/items[%d]/id", i);

					if (channelid != NULL) {
						if (cat->channelid != NULL) {
							free(cat->channelid);
							cat->channelid = NULL;
						}
						cat->channelid = strdup(channelid);
					}
					cat->playlistid = strdup(playlistid);
			 		title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
					if (title != NULL) {
						if (cat->title != NULL) {
							free(cat->title);
							cat->title = NULL;
						}
						cat->title = title;
					}
					if (cat->expected_playlistid != NULL) {
						if (strcmp(playlistid, cat->expected_playlistid) != 0) {
							/* Page token is not valid for current playllist. */
							cat->videopagetoken = NULL;
							cat->expected_playlistid = NULL;
						}
					}
					if (i == 0) {
						cat->channelPrevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
					}
				}
			}
			channelNr++;
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

static int update_my_channels(gui_t *gui, gui_cat_t *selected_cat, const char *selected_playlistid, const char *videopagetoken)
{
	int rv;
	int channelNr;
	int channelStart;
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
		} else {
			channelNr++;
		}
		if (nextPageToken == NULL) {
			LOG_ERROR("%s: Bad pageToken in cat %s.\n", __FUNCTION__, selected_cat->title);
			return JT_ERROR;
		}
	}
	channelStart = channelNr;
	rv = jt_get_my_channels(gui->at, nextPageToken);
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
			json_object *jobj;
			
			jobj = jt_json_get_object_by_path(gui->at, "/items[%d]/contentDetails/relatedPlaylists", i);
			json_object_object_foreach(jobj, key, val) {
				if (json_object_get_type(val) == json_type_string) {
					const char *playlistid = json_object_get_string(val);

					if (playlistid != NULL) {
						gui_cat_t *cat = selected_cat;

						if ((cat == NULL) || (cat->playlistid != NULL)) {
							/* Need to add new cat for playlist. */
							cat = gui_cat_alloc(gui, &gui->get_playlist_cat, last);
						}

						if (cat != NULL) {
							const char *title;
							char *t = NULL;
							int ret;

							last = cat;

							cat->nextPageState = GUI_STATE_GET_MY_CHANNELS;
							cat->prevPageState = GUI_STATE_GET_MY_PREV_CHANNELS;
							cat->channelNr = channelNr;
							cat->channelStart = channelStart;
							cat->channelid = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/id", i));
							cat->playlistid = strdup(playlistid);
							/* Check if this playlist should be selected. */
							if ((selected_playlistid != NULL) && (strcmp(selected_playlistid, playlistid) == 0)) {
								/* Select same video page as selected before. */
								cat->videopagetoken = videopagetoken;
								cat->expected_playlistid = selected_playlistid;
							}
					 		title = jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i);
							if (title != NULL) {
								char *k = NULL;

								if ((title[0] != 0) && (gui->selectedmenu != NULL)) {
									const char *home;

									if (gui->selectedmenu->title != NULL) {
										free(gui->selectedmenu->title);
										gui->selectedmenu->title = NULL;
										if (gui->selectedmenu->textimg != NULL) {
											SDL_FreeSurface(gui->selectedmenu->textimg);
											gui->selectedmenu->textimg = NULL;
										}
									}
									gui->selectedmenu->title = strdup(title);
									/* Save account title in file. */
									home = getenv("HOME");
									if (home != NULL) {
										char *titlefile = NULL;

										ret = asprintf(&titlefile, "%s/%s%03d.txt", home, TITLE_FILE, gui->selectedmenu->tokennr);
										if (ret != -1) {
											save_file(titlefile, title, strlen(title));
											free(titlefile);
										}
										titlefile = NULL;
									}
								}

								k = strdup(key);
								if (k != NULL) {
									if (k[0] != 0) {
										/* Use capital case. */
										k[0] = toupper(k[0]);
									}
									ret = asprintf(&t, "%s - %s", title, k);
									if (ret == -1) {
										t = NULL;
									}
									free(k);
									k = NULL;
								} else {
									ret = asprintf(&t, "%s - %s", title, key);
									if (ret == -1) {
										t = NULL;
									}
								}
							} else {
								ret = asprintf(&t, "%s", key);
								if (ret == -1) {
									t = NULL;
								}
							}
							if (ret != -1) {
								if (cat->title != NULL) {
									free(cat->title);
									cat->title = NULL;
								}
								cat->title = t;
							}
							if (cat->title == NULL) {
								cat->title = strdup("Unknown");
							}
							if (i == 0) {
								cat->channelPrevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
							}
						}
					}
					channelNr++;
				}
			}
		}
		if (last != NULL) {
			if (last->channelNextPageToken != NULL) {
				free(last->channelNextPageToken);
				last->channelNextPageToken = NULL;
			}
			last->channelNextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
		}
		jt_free_transfer(gui->at);
	}

	return rv;
}

static int update_channel_playlists(gui_t *gui, gui_cat_t *selected_cat, int reverse, const char *channelid, const char *catpagetoken, int subnr, const char *selected_playlistid, const char *videopagetoken, int vidnr)
{
	int rv;
	int channelNr;
	int channelStart;
	const char *pageToken;

	if ((selected_cat == NULL) || (selected_cat->nextPageState != GUI_STATE_GET_CHANNEL_PLAYLIST)) {
		/* Get first page. */
		channelNr = 0;
		pageToken = "";
	} else {
		/* Get next page. */
		channelNr = selected_cat->channelNr;
		pageToken = selected_cat->channelNextPageToken;
		if (reverse) {
			pageToken = selected_cat->channelPrevPageToken;
		} else {
			pageToken = selected_cat->channelNextPageToken;
			channelNr++;
		}
	}
	if (catpagetoken != NULL) {
		pageToken = catpagetoken;
		channelNr = subnr;
	} else {
		if (channelid == NULL) {
			if (selected_cat != NULL) {
				channelid = selected_cat->channelid;
				if (selected_cat->current != NULL) {
					if (selected_cat->current->channelid != NULL) {
						channelid = selected_cat->current->channelid;
					}
				}
			}
		}
	}
	rv = jt_get_channel_playlists(gui->at, channelid, pageToken);
	if (rv == JT_OK) {
		int totalResults = 0;
		int resultsPerPage = 0;
		int i;
		gui_cat_t *last;

		if (reverse) {
			if (selected_cat != NULL) {
				last = selected_cat->prev;
			}
		} else {
			if ((selected_cat != NULL) && (selected_cat->nextPageState == GUI_STATE_GET_CHANNEL_PLAYLIST)) {
				last = selected_cat;
			} else {
				last = NULL;
			}
		}

		rv = jt_json_get_int_by_path(gui->at, &totalResults, "/pageInfo/totalResults");
		if (rv != JT_OK) {
			totalResults = 0;
		}

		rv = jt_json_get_int_by_path(gui->at, &resultsPerPage, "/pageInfo/resultsPerPage");
		if (rv != JT_OK) {
			resultsPerPage = 0;
		}
		if (reverse) {
			channelNr -= resultsPerPage;
		}
		channelStart = channelNr;
		for (i = 0; (i < resultsPerPage) && (channelNr < totalResults); i++) {
			const char *playlistid;

			playlistid = jt_json_get_string_by_path(gui->at, "/items[%d]/id", i);
			if (playlistid != NULL) {
				gui_cat_t *cat = selected_cat;

				if ((cat == NULL) || (cat->playlistid != NULL) || (cat->nextPageState != GUI_STATE_GET_CHANNEL_PLAYLIST)) {
					/* Need to add new cat for playlist. */
					cat = gui_cat_alloc(gui, &gui->get_playlist_cat, last);
					if (cat == NULL) {
						LOG_ERROR("Out of memory.\n");
					}
				}

				if (cat != NULL) {
					char *title;
					const char *channelid;

					last = cat;

					cat->nextPageState = GUI_STATE_GET_CHANNEL_PLAYLIST;
					cat->prevPageState = GUI_STATE_GET_PREV_CHANNEL_PLAYLIST;
					cat->channelNr = channelNr;
					cat->channelStart = channelStart;
					channelid = jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/channelId", i);

					if (channelid != NULL) {
						if (cat->channelid != NULL) {
							free(cat->channelid);
							cat->channelid = NULL;
						}
						cat->channelid = strdup(channelid);
					}
					cat->playlistid = strdup(playlistid);
			 		title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
					if (title != NULL) {
						if (cat->title != NULL) {
							free(cat->title);
							cat->title = NULL;
						}
						cat->title = title;
					}
					/* Check if this playlist should be selected. */
					if ((selected_playlistid != NULL) && (strcmp(selected_playlistid, playlistid) == 0)) {
						/* Select same video page as selected before. */
						cat->videopagetoken = videopagetoken;
						cat->expected_playlistid = selected_playlistid;
					}
					cat->vidnr = vidnr;
					if (i == 0) {
						cat->channelPrevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
						/* If this is new, automatically select the playlist. */
						if ((selected_cat == NULL) || (selected_cat->nextPageState != cat->nextPageState)) {
							if (!reverse) {
								gui->current = cat;
							}
						}
					}
				}
			}
			channelNr++;
		}
		if (last != NULL) {
			if (last->channelNextPageToken != NULL) {
				free(last->channelNextPageToken);
				last->channelNextPageToken = NULL;
			}
			last->channelNextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
		}
		jt_free_transfer(gui->at);
	}
	return rv;
}

static int update_search_playlists(gui_t *gui, gui_cat_t *cat, int reverse)
{
	int rv;
	int subnr;
	const char *pageToken;
	gui_elem_t *last;

	if (cat->elem != NULL) {
		/* Get the number of the last element in the list. */
		if (reverse) {
			gui_elem_t *first;

			first = cat->elem;
			last = NULL;

			pageToken = first->prevPageToken;
			subnr = first->subnr;
		} else {
			last = cat->elem->prev;
			pageToken = last->nextPageToken;
			subnr = last->subnr;
			subnr++;
		}
		if (pageToken == NULL) {
			LOG_ERROR("%s: Bad pageToken in cat %s.\n", __FUNCTION__, cat->title);
			return JT_ERROR;
		}
	} else {
		last = NULL;

		/* This is a new list start with number 0. */
		if (cat->videopagetoken != NULL) {
			pageToken = cat->videopagetoken;
			subnr = cat->vidnr;
		} else {
			subnr = 0;
			pageToken = "";
		}
	}

	rv = jt_search_video(gui->at, cat->searchterm, pageToken);
	pageToken = NULL;

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
		if (reverse) {
			subnr -= resultsPerPage;
		}
		for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
			const char *url;
			gui_elem_t *elem;
			int val;

			val = subnr;
			rv = jt_json_get_int_by_path(gui->at, &val, "/items[%d]/snippet/position", i);
			if (rv == JT_OK) {
				subnr = val;
			} else {
				rv = JT_OK;
			}
			
			url = jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/thumbnails/default/url", i);

			elem = gui_elem_alloc(gui, cat, last, url);
			if (elem != NULL) {
				last = elem;
				if (i == 0) {
					elem->prevPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "prevPageToken"));
					if (!reverse) {
						cat->current = elem;
					}
				}
			 	elem->title = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/title", i));
				elem->urlmedium = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/thumbnails/medium/url", i));
				elem->videoid = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/id/videoId", i));
				elem->channelid = jt_strdup(jt_json_get_string_by_path(gui->at, "/items[%d]/snippet/channelId", i));
				elem->subnr = subnr;
			}
			subnr++;
		}

		if (last != NULL) {
			if (last->nextPageToken != NULL) {
				free(last->nextPageToken);
				last->nextPageToken = NULL;
			}
			last->nextPageToken = jt_strdup(jt_json_get_string_by_path(gui->at, "nextPageToken"));
			if (reverse) {
				cat->current = last;
			}
		}
		jt_free_transfer(gui->at);
	} else {
		if (rv == JT_PROTOCOL_ERROR) {
			const char *error;

			error = jt_get_protocol_error(gui->at);
			LOG_ERROR("Search %s protocol error '%s' in cat %s.\n", cat->playlistid, error, cat->title);
		}
	}
	return rv;
}

/** Find page token for cat. */
static const char *find_cat_page_token(gui_t *gui, gui_cat_t *cat)
{
	if (cat != NULL) {
		gui_cat_t *cur;
		const char *rv = NULL;

		cur = cat->next;
		while ((cur != NULL) && (cur != gui->categories) && (cat->prevPageState == cur->prevPageState)) {
			rv = gui_get_prevPageToken(cur);
			if (rv != NULL) {
				return rv;
			}
			cur = cur->next;
		}
		cur = cat->prev;
		while ((cur != NULL) && (cur != gui->categories->prev) && (cat->nextPageState == cur->nextPageState)) {
			rv = gui_get_nextPageToken(cur);
			if (rv != NULL) {
				return rv;
			}
			cur = cur->prev;
		}
	}
	return NULL;
}

static int playVideo(gui_t *gui, const char *videofile, gui_cat_t *cat, gui_elem_t *elem, int format, int buffersize, const char *lastcatpagetoken)
{
	if (videofile == NULL) {
		char *cmd = NULL;
		int ret;

		if (gui->fullscreenmode) {
			/* Disable fullscreen, so that mplayer can get it for playing the video. */
			SDL_WM_ToggleFullScreen(gui->screen);
		}

		printf("Playing %s (%s)\n", CHECKSTR(elem->title), CHECKSTR(elem->videoid));
		if (elem->videoid == NULL) {
			LOG_ERROR("Video has no videoid.\n");
			return 1;
		}

		if (format == 0) {
			ret = asprintf(&cmd, "wget --user-agent=\"$(youtube-dl --dump-user-agent)\" -o /dev/null -O - --load-cookies /tmp/ytcookie-%s.txt - \"$(youtube-dl -g --cookies=/tmp/ytcookie-%s.txt 'http://www.youtube.com/watch?v=%s')\" | mplayer -cache %d -fs -", elem->videoid, elem->videoid, elem->videoid, buffersize);
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
		} else {
			cmd = NULL;
		}
		if (gui->fullscreenmode) {
			/* Enable fullscreen again after video playback. */
			SDL_WM_ToggleFullScreen(gui->screen);
		}
		return 0;
	} else {
		FILE *fout;

		printf("Writing config to %s (%s)\n", videofile, elem->videoid);

		fout = fopen(videofile, "wb");
		if (fout != NULL) {
			gui_elem_t *p;
			int vidnr = 0;
			const char *catPageToken;

			if (gui->selectedmenu != NULL) {
				fprintf(fout, "SELECTEDMENU=\"%d\"\n", gui->selectedmenu->nr);
			} else {
				fprintf(fout, "SELECTEDMENU=\"0\"\n");
			}
			fprintf(fout, "VIDEOID=\"%s\"\n", elem->videoid);
			if (cat->playlistid != NULL) {
				fprintf(fout, "PLAYLISTID=\"%s\"\n", cat->playlistid);
			} else {
				fprintf(fout, "PLAYLISTID=\"\"\n");
			}
			if (cat->channelid != NULL) {
				fprintf(fout, "CHANNELID=\"%s\"\n", cat->channelid);
			} else {
				fprintf(fout, "CHANNELID=\"\"\n");
			}
			if (cat->searchterm != NULL) {
				fprintf(fout, "SEARCHTERM=\"%s\"\n", cat->searchterm);
			} else {
				fprintf(fout, "SEARCHTERM=\"\"\n");
			}
			catPageToken = find_cat_page_token(gui, cat);
			if (catPageToken == NULL) {
				catPageToken = lastcatpagetoken;
			}
			if (catPageToken != NULL) {
				fprintf(fout, "CATPAGETOKEN=\"%s\"\n", catPageToken);
			} else {
				fprintf(fout, "CATPAGETOKEN=\"\"\n");
			}

			p = elem;
			while (p != NULL) {
				if (p->prev == cat->elem->prev) {
					break;
				}
				p = p->prev;
				if (p->nextPageToken != NULL) {
					break;
				}
			}
			if ((p != NULL) && (p != elem) && (p->nextPageToken != NULL)) {
				fprintf(fout, "VIDPAGETOKEN=\"%s\"\n", p->nextPageToken);
				vidnr = p->subnr + 1;
			} else {
				p = elem;
				while (p != NULL) {
					if (p->next == cat->elem) {
						break;
					}
					p = p->next;
					if (p->prevPageToken != NULL) {
						break;
					}
				}
				if ((p != NULL) && (p != elem) && (p->prevPageToken != NULL)) {
					fprintf(fout, "VIDPAGETOKEN=\"%s\"\n", p->prevPageToken);
					/* Found prevPageToken, but no nextPageToken. This means that the first
					 * video contains the vidnr when prevPageToken is used.
					 */
					vidnr = cat->elem->subnr;
				} else if (cat->videopagetoken != NULL) {
					/* No further playlist items loaded after restart, must be the same page token and vidnr. */
					fprintf(fout, "VIDPAGETOKEN=\"%s\"\n", cat->videopagetoken);
					vidnr = cat->vidnr;
				} else {
					/* Must be the first page: */
					fprintf(fout, "VIDPAGETOKEN=\"\"\n");
					/* First video has number 0. */
					vidnr = 0;
				}
			}
			fprintf(fout, "CATNR=\"%d\"\n", cat->subnr);
			fprintf(fout, "CHANNELNR=\"%d\"\n", cat->channelNr);
			fprintf(fout, "CHANNELSTART=\"%d\"\n", cat->channelStart);
			/* Number of the first video when using VIDPAGETOKEN. */
			fprintf(fout, "VIDNR=\"%d\"\n", vidnr);
			fprintf(fout, "STATE=\"%d\"\n", cat->nextPageState);
			if (elem->title != NULL) {
				char *title;

				title = strdup(elem->title);

				if (title != NULL) {
					int i;

					i = 0;
					while(title[i] != 0) {
						if (title[i] == '\'') {
							/* Remove stuff which can cause quoting problems in shell. */
							title[i] = '`';
						}
						i++;
					}
					fprintf(fout, "VIDEOTITLE='%s'\n", title);
					free(title);
					title = NULL;
				} else {
					fprintf(fout, "VIDEOTITLE=''\n");
				}
			} else {
				fprintf(fout, "VIDEOTITLE=''\n");
			}
			fclose(fout);
			return 0;
		} else {
			LOG_ERROR("Failed to write output file %s. %s\n", videofile, strerror(errno));
			return 1;
		}
	}
}

void print_debug_cat(gui_t *gui, gui_cat_t *cat)
{
	(void) gui;

	if (gui->prev_cat != NULL) {
		printf("0x %p Prev Category is subnr %d: channelNr: %d channelid %s: playlistid %s: nextPage %s:%s prevPage %s:%s: %s\n",
			gui->prev_cat,
			gui->prev_cat->subnr,
			gui->prev_cat->channelNr,
			CHECKSTR(gui->prev_cat->channelid),
			CHECKSTR(gui->prev_cat->playlistid),
			get_state_text(gui->prev_cat->nextPageState), CHECKSTR(gui_get_nextPageToken(gui->prev_cat)),
			get_state_text(gui->prev_cat->prevPageState), CHECKSTR(gui_get_prevPageToken(gui->prev_cat)), CHECKSTR(gui->prev_cat->title));
	}

	if (cat != NULL) {
		gui_elem_t *p;

		printf("0x%p Category is subnr %d: channelNr: %d channelid %s: playlistid %s: nextPage %s:%s prevPage %s:%s: %s\n",
			cat,
			cat->subnr,
			cat->channelNr,
			CHECKSTR(cat->channelid),
			CHECKSTR(cat->playlistid),
			get_state_text(cat->nextPageState), CHECKSTR(gui_get_nextPageToken(cat)),
			get_state_text(cat->prevPageState), CHECKSTR(gui_get_prevPageToken(cat)), CHECKSTR(cat->title));

		p = cat->elem;
		while (p != NULL) {
			printf("%c subnr %d channelid %s videoid %s p->prevPageToken %s title %s\n",
				(p == cat->current) ? '+' : '-', p->subnr,
				CHECKSTR(p->channelid),
				CHECKSTR(p->videoid),
				CHECKSTR(p->prevPageToken), CHECKSTR(p->title));
			printf("%c subnr %d channelid %s videoid %s p->nextPageToken %s title %s\n",
				(p == cat->current) ? '+' : '-', p->subnr,
				CHECKSTR(p->channelid),
				CHECKSTR(p->videoid),
				CHECKSTR(p->nextPageToken), CHECKSTR(p->title));
			if (p->next == cat->elem) {
				break;
			}
			p = p->next;
		}
	} else {
		printf("Category is NULL.\n");
	}
}

static void set_description_for_subscriptions(gui_t *gui)
{
	if (gui->description_status != 1) {
		gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Play playlist");
		gui->circle_text = gui_printf(gui->descfont, gui->circle_text, "Main menu");
		gui->square_text = gui_printf(gui->descfont, gui->square_text, "Show playlist");
		gui->triangle_text = gui_printf(gui->descfont, gui->triangle_text, "Play playlist backwards");

		gui->description_status = 1;
	}
}

static void set_description_for_playlist(gui_t *gui)
{
	if (gui->description_status != 2) {
		gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Play playlist");
		gui->circle_text = gui_printf(gui->descfont, gui->circle_text, "Back");
		gui->triangle_text = gui_printf(gui->descfont, gui->triangle_text, "Play playlist backwards");
		gui->description_status = 2;
	}
}

static void set_no_description(gui_t *gui)
{
	if (gui->cross_text != NULL) {
		SDL_FreeSurface(gui->cross_text);
		gui->cross_text = NULL;
	}
	if (gui->circle_text != NULL) {
		SDL_FreeSurface(gui->circle_text);
		gui->circle_text = NULL;
	}
	if (gui->square_text != NULL) {
		SDL_FreeSurface(gui->square_text);
		gui->square_text = NULL;
	}
	if (gui->triangle_text != NULL) {
		SDL_FreeSurface(gui->triangle_text);
		gui->triangle_text = NULL;
	}
	gui->description_status = 0;
}

static void set_description_continue(gui_t *gui)
{
	if (gui->description_status != 3) {
		set_no_description(gui);
		gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Continue");
		gui->circle_text = gui_printf(gui->descfont, gui->circle_text, "Power Off");
		gui->description_status = 3;
	}
}

static void set_description_select(gui_t *gui)
{
	gui_menu_entry_t *entry;

	if (gui->description_status != 3) {
		set_no_description(gui);
		gui->square_text = gui_printf(gui->descfont, gui->square_text, "Power Off");
	}
	entry = gui->selectedmenu;
	if (entry != NULL) {
		switch (entry->state) {
			case GUI_STATE_NEW_ACCESS_TOKEN:
				gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "New account");
				break;

			case GUI_STATE_LOAD_ACCESS_TOKEN:
				gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Show account");
				break;

			case GUI_STATE_POWER_OFF:
				gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Power Off");
				break;

			case GUI_STATE_QUIT:
				gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Quit");
				break;

			case GUI_STATE_MENU_PLAYLIST:
				gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Show playlist");
				break;

			case GUI_STATE_SEARCH:
				gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Search");
				break;

			case GUI_STATE_UPDATE:
				gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Update youtube-dl");
				break;

			default:
				gui->cross_text = gui_printf(gui->descfont, gui->cross_text, "Select");
				break;
		}
		switch (entry->state) {
			case GUI_STATE_LOAD_ACCESS_TOKEN:
				gui->triangle_text = gui_printf(gui->descfont, gui->triangle_text, "Remove account");
				break;

			case GUI_STATE_MENU_PLAYLIST:
				gui->triangle_text = gui_printf(gui->descfont, gui->triangle_text, "Remove playlist");
				break;

			case GUI_STATE_SEARCH:
				gui->triangle_text = gui_printf(gui->descfont, gui->triangle_text, "Remove search");
				break;

			default:
				if (gui->triangle_text != NULL) {
					SDL_FreeSurface(gui->triangle_text);
					gui->triangle_text = NULL;
				}
				break;
		}
	} else {
		set_no_description(gui);
		gui->square_text = gui_printf(gui->descfont, gui->square_text, "Power Off");
	}
	gui->description_status = 3;
}

static void set_description_cancel(gui_t *gui)
{
	if (gui->description_status != 3) {
		set_no_description(gui);
		gui->circle_text = gui_printf(gui->descfont, gui->circle_text, "Cancel");
		gui->description_status = 3;
	}
}

static void save_menu_state(gui_t *gui)
{
	const char *home;
	FILE *fout;
	char *filename;
	int error = 0;
	int ret;

	home = getenv("HOME");
	if (home == NULL) {
		LOG_ERROR("Environment variable HOME is not set.\n");
		return;
	}

	ret = asprintf(&filename, "%s/%s.cfg", home, MENU_STATE_FILE);
	if (ret == -1) {
		LOG_ERROR("Out of memory\n");
		return;
	}

	fout = fopen(filename, "wb");

	free(filename);
	filename = NULL;

	if (fout != NULL) {
		gui_menu_entry_t *entry;

		if (fprintf(fout, "MenuFormat\n") < 0) {
			error = 1;
		}
		if (fprintf(fout, "%d\n", MENU_VERSION) < 0) {
			error = 1;
		}

		entry = gui->mainmenu;
		while(!error && (entry != NULL)) {
			switch (entry->state) {
				case GUI_STATE_SEARCH:
					if (entry->searchterm == NULL) {
						break;
					}
					if (entry->searchterm[0] == 0) {
						break;
					}
					if (fprintf(fout, "%d\n", entry->state) < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "%d\n", entry->initstate) < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "%d\n", entry->tokennr) < 0) {
						error = 1;
						break;
					}
					if (fprintf(fout, "%s\n", (entry->title != NULL) ? entry->title : "No title") < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "%s\n", (entry->searchterm != NULL) ? entry->searchterm : "") < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "\n") < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					break;

				case GUI_STATE_MENU_PLAYLIST:
					if (fprintf(fout, "%d\n", entry->state) < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "%d\n", entry->initstate) < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "%d\n", entry->tokennr) < 0) {
						error = 1;
						break;
					}
					if (fprintf(fout, "%s\n", (entry->title != NULL) ? entry->title : "No title") < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "%s\n", (entry->playlistid != NULL) ? entry->playlistid : "") < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "%s\n", (entry->channelid != NULL) ? entry->channelid : "") < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					if (fprintf(fout, "\n") < 0) {
						LOG_ERROR("Failed to write file: %s\n", strerror(errno));
						error = 1;
						break;
					}
					break;

				default:
					break;
			}
			entry = entry->next;
			if (entry == gui->mainmenu) {
				break;
			}
		}
		fclose(fout);
		fout = NULL;
	} else {
		error = 1;
	}
	if (error) {
		/* On error delete broken file. */
		unlink(filename);
	}
}

/**
 * Main loop for GUI.
 */
int gui_loop(gui_t *gui, int retval, int origgetstate, const char *videofile, const char *channelid, const char *searchterm, const char *playlistid, const char *catpagetoken, const char *videoid, int catnr, int channelnr, const char *videopagetoken, int vidnr, int menunr, int timer)
{
	int done;
	SDL_Event event;
	enum gui_state state;
	enum gui_state getstate;
	enum gui_state prevstate;
	enum gui_state restorestate;
	enum gui_state nextstate;
	enum gui_state afterplayliststate;
	enum gui_state aftersearchplayliststate;
	unsigned int wakeupcount;
	unsigned int sleeptime = 50 * 3;
	int rv;
	int foundvid = 0;
	const char *lastcatpagetoken;
	int timeoutcounter = 0;
	char *oldstatusmsg = NULL;
	char searchstring[80];
	unsigned int searchpos;

	searchstring[0] = 0;
	searchpos = 0;
	if (searchterm != NULL) {
		strncpy(searchstring, searchterm, sizeof(searchstring));
		searchstring[sizeof(searchstring) - 1] = 0;
		searchpos = strlen(searchstring);
	}

	getstate = origgetstate;
	LOG("videopagetoken %s at startup\n", videopagetoken);

	lastcatpagetoken = catpagetoken;
	done = 0;
	wakeupcount = 5;
	rv = JT_OK;
	state = GUI_STATE_STARTUP;
	afterplayliststate = GUI_STATE_RUNNING;
	aftersearchplayliststate = GUI_STATE_RUNNING;
	prevstate = state;
	if (getstate == 0) {
		gui->statusmsg = buf_printf(gui->statusmsg, "Juhutube\n© Jürgen Urban");
		wakeupcount = DEFAULT_SLEEP / 2;
	}
	while(!done) {
		enum gui_state curstate;

		if (state != prevstate) {
			LOG("Enter new state %d %s\n", state, get_state_text(state));
			prevstate = state;
		}

		curstate = (wakeupcount <= 0) ? state : GUI_STATE_SLEEP;

		if (curstate == GUI_STATE_RUNNING) {
			/* No operation is pending. */
			gui->cur_cat = NULL;

			/* Don't display error message. */
			if (gui->statusmsg != NULL) {
				free(gui->statusmsg);
				gui->statusmsg = NULL;
			}
		}

		/* Check for events */
		while(SDL_PollEvent(&event))
		{
			SDLKey key;

			key = event.key.keysym.sym;
			switch(event.type)
			{
				case SDL_JOYAXISMOTION:
					if (event.jaxis.axis == 0) {
						if (event.jaxis.value > 3000) {
							event.type = SDL_KEYDOWN;
							key = SDLK_RIGHT;
						} else if (event.jaxis.value < -3000) {
							event.type = SDL_KEYDOWN;
							key = SDLK_LEFT;
						}
					} else if (event.jaxis.axis == 1) {
						if (event.jaxis.value > 3000) {
							event.type = SDL_KEYDOWN;
							key = SDLK_DOWN;
						} else if (event.jaxis.value < -3000) {
							event.type = SDL_KEYDOWN;
							key = SDLK_UP;
						}
					}
					break;
			}

			/* Disable any automatic when a key is pressed. */
			switch(event.type)
			{
				case SDL_JOYBUTTONDOWN:
					switch(event.jbutton.button) {
						case 0: /* Square */
							key = BTN_SQUARE;
							break;
						case 9: /* Start */
							key = BTN_START;
							break;
						case 1: /* Cross */
							/* Play playlist */
							key = BTN_CROSS;
							break;
						case 2: /* Triangle */
							/* Play playlist backwards */
							key = BTN_TRIANGLE;
							break;
						case 8: /* Select */
							key = BTN_SELECT;
							break;
						case 3: /* Circle */
							/* Select playllist */
							key = BTN_CIRCLE;
							break;
						case 4: /* L1 */
							key = BTN_L1;
							break;
						case 5: /* R1 */
							key = BTN_R1;
							break;
						case 6: /* L2 */
							key = BTN_L2;
							break;
						case 7: /* R2 */
							key = BTN_R2;
							break;
						default:
							key = SDLK_l;
							LOG("SDL_JOYBUTTONDOWN: %d\n", event.jbutton.button);
						break;
					}

				case SDL_KEYDOWN:
					retval = 0;
					/* Reset timeout on key press. */
					timeoutcounter = 0;
					if (state == GUI_STATE_TIMEOUT) {
						/* restore previous state. */
						state = restorestate;
						if (gui->statusmsg != NULL) {
							free(gui->statusmsg);
							gui->statusmsg = NULL;
						}
						gui->statusmsg = oldstatusmsg;
						wakeupcount = 0;
					}

					/* Don't wait when a key is pressed. */
					wakeupcount = 0;
					/* Key pressed on keyboard. */
					switch(key) {
						case BTN_SELECT:
							if (curstate == GUI_STATE_RUNNING) {
								gui_cat_t *cat;

								cat = gui->current;
								if (cat != NULL) {
									if (cat->playlistid != NULL) {
										gui_menu_entry_t *entry;

										entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, cat->title, GUI_STATE_MENU_PLAYLIST, GUI_STATE_GET_PLAYLIST);
										entry->playlistid = jt_strdup(cat->playlistid);
										entry->channelid = jt_strdup(cat->channelid);
										entry->tokennr = gui->selectedmenu->tokennr;
									}
									if (cat->searchterm != NULL) {
										gui_menu_entry_t *entry;

										entry = gui_menu_entry_alloc(gui, &gui->mainmenu, NULL, cat->title, GUI_STATE_SEARCH, GUI_STATE_SEARCH_PLAYLIST);
										entry->searchterm = jt_strdup(cat->searchterm);
										entry->tokennr = gui->selectedmenu->tokennr;
									}
								}
							}
							break;
						case BTN_CROSS:
							if (state == GUI_STATE_WAIT_FOR_CONTINUE) {
								state = GUI_RESET_STATE;
								break;
							}
							if (curstate == GUI_STATE_MAIN_MENU) {
								gui_menu_entry_t *entry;

								entry = gui->selectedmenu;
								if (entry != NULL) {
									set_no_description(gui);
									state = entry->state;
								}
								break;
							}
							if (state == GUI_STATE_SEARCH_ENTER) {
								state = GUI_STATE_SEARCH;
								if (gui->selectedmenu->searchterm != NULL) {
									free(gui->selectedmenu->searchterm);
								}
								gui->selectedmenu->searchterm = jt_strdup(searchstring);
								break;
							}

						case BTN_TRIANGLE:
							if (curstate == GUI_STATE_MAIN_MENU) {
								gui_menu_entry_t *entry;

								/* Delete entry. */
								entry = gui->selectedmenu;
								if (entry != NULL) {
									if (entry->state == GUI_STATE_LOAD_ACCESS_TOKEN) {
										/* Remove account. */
										delete_token(entry->tokennr);
										gui_menu_entry_free(gui, entry);
										entry = NULL;
									} else if (entry->state == GUI_STATE_MENU_PLAYLIST) {
										/* Remove playlist. */
										gui_menu_entry_free(gui, entry);
										entry = NULL;
									} else if (entry->state == GUI_STATE_SEARCH) {
										/* Remove search. */
										gui_menu_entry_free(gui, entry);
										entry = NULL;
									}
								}
								break;
							}
						case BTN_START: {
							if (curstate == GUI_STATE_RUNNING) {
								gui_cat_t *cat;

								cat = gui->current;
								if (cat) {
									gui_elem_t *elem;

									elem = cat->current;
									if (elem != NULL) {
										if (elem->videoid != NULL) {
											int ret = 1;

											set_no_description(gui);

											ret = playVideo(gui, videofile, cat, elem, 0, 4096, lastcatpagetoken);
											if ((videofile != NULL) && (ret == 0)) {
												/* Terminate program, another program needs to use the videofile
												 * to play the video.
												 */
												done = 1;
											}
											if (ret == 0) {
												if (key == BTN_START) {
													/* Play current video only. */
													retval = 1;
												} else if (key == BTN_CROSS) {
													/* Play playlist. */
													retval = 2;
												} else {
													/* Play playlist backward. */
													retval = 3;
												}
											} else {
												retval = 0;
											}
										}
									}
								}
							}
							break;
						}

						case BTN_SQUARE: {
							if (state == GUI_STATE_MAIN_MENU) {
								state = GUI_STATE_POWER_OFF;
								break;
							}
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->statusmsg == NULL) {
									if (gui->current != NULL) {
										lastcatpagetoken = NULL;
										set_no_description(gui);
										if (gui->prev_cat == NULL) {
											/* Show playlists of channel. */
											gui->cur_cat = gui->current;
											gui->prev_cat = gui->current;
											state = GUI_STATE_GET_CHANNEL_PLAYLIST;
											switch (gui->cur_cat->nextPageState) {
												case GUI_STATE_GET_MY_CHANNELS:
												case GUI_STATE_GET_FAVORITES:
													update_channelid_of_video(gui, gui->cur_cat->current);
													break;
												default:
													break;
											}
										}
									}
								}
							}
							break;
						}

						case BTN_CIRCLE: {
							if (state == GUI_STATE_WAIT_FOR_CONTINUE) {
								state = GUI_STATE_POWER_OFF;
								break;
							}
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->statusmsg == NULL) {
									lastcatpagetoken = NULL;
									set_no_description(gui);
									if ((gui->current != NULL) && (gui->prev_cat != NULL)) {
										/* Back to previous view from playlist view. */
										gui_cat_t *cat;
										enum gui_state nextPageState;

										nextPageState = gui->current->nextPageState;

										gui->current = gui->prev_cat;
										gui->prev_cat = NULL;

										/* Delete playlist when returning. */
										cat = gui->categories;
										while (cat != NULL) {
											gui_cat_t *next;
											next = cat->next;
											if (cat->nextPageState == nextPageState) {
												gui_cat_free(gui, cat);
												cat = NULL;
											}
											cat = next;
											if ((gui->categories == NULL) || (cat == gui->categories)) {
												break;
											}
										}
									} else {
										gui_free_categories(gui);

										/* Back to main menu. */
										nextstate = GUI_STATE_MAIN_MENU;
										state = GUI_RESET_STATE;
									}
								}
							}
							if (state == GUI_STATE_GET_TOKEN) {
								gui_menu_entry_t *entry;

								entry = gui->selectedmenu;
								if (entry != NULL) {
									if ((entry->state == GUI_STATE_LOAD_ACCESS_TOKEN) || (entry->state == GUI_STATE_NEW_ACCESS_TOKEN)) {
										/* Remove account. */
										delete_token(entry->tokennr);
										gui_menu_entry_free(gui, entry);
										entry = NULL;
									} else if (entry->state == GUI_STATE_MENU_PLAYLIST) {
										/* Remove playlist. */
										gui_menu_entry_free(gui, entry);
										entry = NULL;
									}
								}
								if (state == GUI_STATE_GET_TOKEN) {
									set_no_description(gui);
									/* Back to main menu. */
									nextstate = GUI_STATE_MAIN_MENU;
									state = GUI_RESET_STATE;
									wakeupcount = 0;
									gui_free_categories(gui);
								}
								break;
							}
							break;
						}

						case SDLK_q:
							if (state == GUI_STATE_SEARCH_ENTER) {
								break;
							}
							set_no_description(gui);
							/* Quit */
							done = 1;
							retval = 0;
							break;

						case SDLK_POWER:
							set_no_description(gui);
							/* Power Off */
							done = 4;
							retval = 0;
							break;

						case SDLK_LEFT:
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->statusmsg == NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									if ((cat != NULL) && (cat->current != NULL) && (cat->elem != NULL) && (cat->current->prev == cat->elem->prev)) {
										gui_elem_t *first;

										first = cat->elem;

										if ((first->prevPageToken != NULL) && (curstate == GUI_STATE_RUNNING)) {
											if (cat->nextPageState == GUI_STATE_SEARCH) {
												aftersearchplayliststate = curstate;
												state = GUI_STATE_SEARCH_PREV_PLAYLIST;
											} else {
												afterplayliststate = curstate;
												state = GUI_STATE_GET_PREV_PLAYLIST;
											}
											gui->cur_cat = cat;
										}
									} else {
										if (cat->elem != NULL) {
											gui_dec_elem(gui);
										}
									}
								}
							}
							break;

						case SDLK_RIGHT:
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->statusmsg == NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									if ((cat != NULL) && (cat->current != NULL) && (cat->elem != NULL) && (cat->current->next == cat->elem)) {
										gui_elem_t *last;

										last = cat->elem->prev;

										if ((last->nextPageToken != NULL) && (curstate == GUI_STATE_RUNNING)) {
											if (cat->nextPageState == GUI_STATE_SEARCH) {
												aftersearchplayliststate = curstate;
												state = GUI_STATE_SEARCH_PLAYLIST;
											} else {
												afterplayliststate = curstate;
												state = GUI_STATE_GET_PLAYLIST;
											}
											gui->cur_cat = cat;
										}
									} else {
										if (cat->elem != NULL) {
											gui_inc_elem(gui);
										}
									}
								}
							}
							break;

						case BTN_L1:
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

						case BTN_R1:
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
							if (curstate == GUI_STATE_MAIN_MENU) {
								if (gui->selectedmenu != NULL) {
									gui->selectedmenu = gui->selectedmenu->prev;
								}
							}
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->categories != NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									if (cat != NULL) {
										if ((cat == gui->categories) || (cat->prev == NULL) || (cat->prevPageState != cat->prev->prevPageState)) {
											char *prevPageToken;

											prevPageToken = gui_get_prevPageToken(cat);

											if (prevPageToken != NULL) {
												/* Load previous page. */
												state = cat->prevPageState;
												gui->cur_cat = cat;
												lastcatpagetoken = NULL;
											}
										}
										if (state != GUI_STATE_RUNNING) {
											/* Loading page... */
										} else if (cat == gui->categories) {
											/* This is already the first in the list. */
										} else {
											gui_dec_cat(gui);
											cat = NULL;
										}

									}
								}
							}
							break;

						case SDLK_DOWN:
							if (curstate == GUI_STATE_MAIN_MENU) {
								if (gui->selectedmenu != NULL) {
									gui->selectedmenu = gui->selectedmenu->next;
								}
							}
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->categories != NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									if (cat != NULL) {
										if ((cat->next == NULL) || (cat->next == gui->categories) || (cat->nextPageState != cat->next->nextPageState)) {
											char *nextPageToken;
	
											nextPageToken = gui_get_nextPageToken(cat);

											if (nextPageToken != NULL) {
												/* Load previous page. */
												state = cat->nextPageState;
												gui->cur_cat = cat;
												lastcatpagetoken = NULL;
											}
										}
										if (state != GUI_STATE_RUNNING) {
											/* Loading page... */
										} else if (cat->next == gui->categories) {
											/* This is already the last in the list. */
										} else {
											gui_inc_cat(gui);
											cat = NULL;
										}
										cat = gui->current;
										/* Check if next page can be preloaded: */
										if ((state == GUI_STATE_RUNNING) && (cat != NULL) && ((cat->next == NULL) || (cat->next == gui->categories) || (cat->nextPageState != cat->next->nextPageState))) {
											char *nextPageToken;
	
											nextPageToken = gui_get_nextPageToken(cat);

											if (nextPageToken != NULL) {
												/* Load previous page. */
												state = cat->nextPageState;
												gui->cur_cat = cat;
												lastcatpagetoken = NULL;
											}
										}

										cat = gui->current;
										/* Check if previous page can be preloaded: */
										if ((state == GUI_STATE_RUNNING) && (cat != NULL) && ((cat == gui->categories) || (cat->prev == NULL) || (cat->prevPageState != cat->prev->prevPageState))) {
											char *prevPageToken;

											prevPageToken = gui_get_prevPageToken(cat);

											if (prevPageToken != NULL) {
												/* Load previous page. */
												state = cat->prevPageState;
												gui->cur_cat = cat;
												lastcatpagetoken = NULL;
											}
										}
									}
								}
							}
							break;

						case BTN_L2:
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->categories != NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									while (!((cat != NULL) && (cat->prev != NULL) && (cat->prev == gui->categories->prev))) {
										if (gui_dec_cat(gui)) {
											break;
										}
										cat = gui->current;
									}
								}
							}
							break;

						case BTN_R2:
							if (curstate == GUI_STATE_RUNNING) {
								if (gui->categories != NULL) {
									gui_cat_t *cat;

									cat = gui->current;
									while (!((cat != NULL) && (cat->next != NULL) && (cat->next == gui->categories))) {
										if (gui_inc_cat(gui)) {
											break;
										}
										cat = gui->current;
									}
									if (gui_get_nextPageToken(cat) != NULL) {
										/* Get more subscriptions. */
										state = cat->nextPageState;
										gui->cur_cat = cat;
										lastcatpagetoken = NULL;
									}
								}
							}
							break;

						case SDLK_f:
							if (state == GUI_STATE_SEARCH_ENTER) {
								break;
							}
						case SDLK_F1:
							gui->fullscreenmode ^= 1;
							SDL_WM_ToggleFullScreen(gui->screen);
							break;
						case SDLK_d:
							if (state == GUI_STATE_SEARCH_ENTER) {
								break;
							}
							print_debug_cat(gui, gui->current);
							break;
	
						default:
							break;
					}
					if (state == GUI_STATE_SEARCH_ENTER) {
						int k = 0;
						switch(key) {
							case SDLK_a:
								k = 'a';
								break;
							case SDLK_b:
								k = 'b';
								break;
							case SDLK_c:
								k = 'c';
								break;
							case SDLK_d:
								k = 'd';
								break;
							case SDLK_e:
								k = 'e';
								break;
							case SDLK_f:
								k = 'f';
								break;
							case SDLK_g:
								k = 'g';
								break;
							case SDLK_h:
								k = 'h';
								break;
							case SDLK_i:
								k = 'i';
								break;
							case SDLK_j:
								k = 'j';
								break;
							case SDLK_k:
								k = 'k';
								break;
							case SDLK_l:
								k = 'l';
								break;
							case SDLK_m:
								k = 'm';
								break;
							case SDLK_n:
								k = 'n';
								break;
							case SDLK_o:
								k = 'o';
								break;
							case SDLK_p:
								k = 'p';
								break;
							case SDLK_q:
								k = 'q';
								break;
							case SDLK_r:
								k = 'r';
								break;
							case SDLK_s:
								k = 's';
								break;
							case SDLK_t:
								k = 't';
								break;
							case SDLK_u:
								k = 'u';
								break;
							case SDLK_v:
								k = 'v';
								break;
							case SDLK_w:
								k = 'w';
								break;
							case SDLK_x:
								k = 'x';
								break;
							case SDLK_y:
								k = 'y';
								break;
							case SDLK_z:
								k = 'z';
								break;
							case SDLK_PLUS:
								k = '+';
								break;
							case SDLK_MINUS:
								k = '-';
								break;
							case SDLK_COLON:
								k = '|';
								break;
							case SDLK_0:
								k = '0';
								break;
							case SDLK_1:
								k = '1';
								break;
							case SDLK_2:
								k = '2';
								break;
							case SDLK_3:
								k = '3';
								break;
							case SDLK_4:
								k = '4';
								break;
							case SDLK_5:
								k = '5';
								break;
							case SDLK_6:
								k = '6';
								break;
							case SDLK_7:
								k = '7';
								break;
							case SDLK_8:
								k = '8';
								break;
							case SDLK_9:
								k = '9';
								break;
							case SDLK_BACKSPACE:
								if (searchpos > 0) {
									searchpos--;
									searchstring[searchpos] = 0;
								}
								break;
							case SDLK_RETURN:
								state = GUI_STATE_SEARCH;
								if (gui->selectedmenu->searchterm != NULL) {
									free(gui->selectedmenu->searchterm);
								}
								gui->selectedmenu->searchterm = jt_strdup(searchstring);
								break;

							default:
								break;
						}
						if (searchpos < (sizeof(searchstring) - 1)) {
							if (k != 0) {
								searchstring[searchpos] = k;
								searchpos++;
								searchstring[searchpos] = 0;
							}
						}
						break;
					}
					break;

				default:
					break;

			}
		}

		if (done) {
			break;
		}
		if (state != prevstate) {
			LOG("Enter new state %d %s (triggered by user).\n", state, get_state_text(state));
			prevstate = state;
		}

		if (wakeupcount > 0) {
			wakeupcount--;
		}

		/* GUI state machine. */
		curstate = (wakeupcount <= 0) ? state : GUI_STATE_SLEEP;
		switch(curstate) {
			case GUI_STATE_SLEEP:
				if (state != GUI_STATE_GET_TOKEN) {
					set_no_description(gui);
				}
				break;

			case GUI_STATE_WAIT_FOR_CONTINUE:
				set_description_continue(gui);
				break;

			case GUI_STATE_STARTUP:
				state = GUI_STATE_MAIN_MENU;
				if (gui->mainmenu != NULL) {
					if (menunr != 0) {
						gui_menu_entry_t *entry;

						entry = gui->mainmenu;
						do {
							if (entry->nr == menunr) {
								break;
							}
							entry = entry->next;
						} while (entry != gui->mainmenu);
						if (entry->nr == menunr) {
							gui->selectedmenu = entry;
							state = gui->selectedmenu->state;
							if (state == GUI_STATE_SEARCH_ENTER) {
								state = GUI_STATE_SEARCH;
							}
						}
						menunr = 0;
					}
				}
				break;

			case GUI_STATE_MAIN_MENU:
				if (gui->statusmsg != NULL) {
					free(gui->statusmsg);
					gui->statusmsg = NULL;
				}
				set_description_select(gui);
				if (gui->at != NULL) {
					jt_free(gui->at);
					gui->at = NULL;
					gui_free_categories(gui);
				}
				break;

			case GUI_STATE_NEW_ACCESS_TOKEN: {
				gui_menu_entry_t *entry;
				int nr;
				char *accountname;
				int ret;
				
				nr = get_next_tokennr(gui);
				if (nr < 0) {
					gui->statusmsg = buf_printf(gui->statusmsg, "Too many YouTube accounts. Please remove one first.");
					/* Retry */
					wakeupcount = DEFAULT_SLEEP;
					state = GUI_STATE_MAIN_MENU;
					break;
				}
				entry = gui->mainmenu;
				while(entry != NULL) {
					gui_menu_entry_t *next;

					next = entry->next;
					if (next == gui->mainmenu) {
						break;
					}
					if (next != NULL) {
						if (next->tokennr > nr) {
							/* Sort by tokennr. */
							/* Insert before larger tokennr. */
							break;
						}
					}
					entry = entry->next;
				}

				ret = asprintf(&accountname, "Account %03d", nr);
				if (ret == -1) {
					LOG_ERROR("Out of memory\n");
					entry = NULL;
				} else {
					entry = gui_menu_entry_alloc(gui, &gui->mainmenu, entry, accountname, GUI_STATE_LOAD_ACCESS_TOKEN, GUI_STATE_GET_MY_CHANNELS);
					free(accountname);
					accountname = NULL;
				}
				if (entry == NULL) {
					gui->statusmsg = buf_printf(gui->statusmsg, "Failed to allocate menu entry.");
					/* Retry */
					wakeupcount = DEFAULT_SLEEP;
					nextstate = GUI_STATE_MAIN_MENU;
					state = GUI_RESET_STATE;
					break;
				}
				entry->tokennr = nr;
				gui->selectedmenu = entry;
				delete_token(entry->tokennr);
			}

			case GUI_STATE_LOAD_ACCESS_TOKEN:
				if (gui->at != NULL) {
					jt_free(gui->at);
					gui->at = NULL;
					gui_free_categories(gui);
				}
				gui->at = alloc_token(gui->selectedmenu->tokennr);
				if (gui->at == NULL) {
					gui->statusmsg = buf_printf(gui->statusmsg, "Failed to allocate token.");
					/* Retry */
					wakeupcount = DEFAULT_SLEEP;
					nextstate = GUI_STATE_MAIN_MENU;
					state = GUI_RESET_STATE;
					break;
				}
				/* Try to load existing access for YouTube user account. */
				rv = jt_load_token(gui->at);
				if (rv == JT_OK) {
					state = GUI_STATE_LOGGED_IN;
				} else {
					if (gui->selectedmenu->initstate == GUI_STATE_GET_MY_CHANNELS) {
						/* Showing my channels needs login. */
						state = GUI_STATE_GET_USER_CODE;
					} else {
						/* Search doesn't need login. */
						state = GUI_STATE_INIT;
					}
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
				set_description_cancel(gui);

				/* Check whether the user permitted access for this application. */
				rv = jt_get_token(gui->at);
				switch (rv) {
					case JT_OK:
						set_no_description(gui);
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
				gui->statusmsg = buf_printf(gui->statusmsg, "Account found");
				state = GUI_STATE_INIT;
				break;

			case GUI_RESET_STATE:
				set_no_description(gui);
				state = nextstate;
				if (gui->statusmsg != NULL) {
					free(gui->statusmsg);
					gui->statusmsg = NULL;
				}
				break;

			case GUI_STATE_INIT: {
#if 0 /* GUI_STATE_GET_FAVORITES is already included in GUI_STATE_GET_MY_CHANNELS. */
				state = GUI_STATE_GET_FAVORITES;
#else
				if ((gui->selectedmenu != NULL)) {
					state = gui->selectedmenu->initstate;
				} else {
					state = GUI_STATE_GET_MY_CHANNELS;
				}
#endif
				break;
			}

			case GUI_STATE_GET_FAVORITES:
				rv = update_favorites(gui, gui->cur_cat, 0);
				if (rv != JT_OK) {
					state = GUI_STATE_ERROR;
					if ((gui->categories == NULL) || (gui->categories->prev == NULL) || (gui->categories->prev->nextPageState == GUI_STATE_GET_FAVORITES)) {
						/* First time running, need also to get the subscriptions. */
						if (gui->get_playlist_cat != NULL) {
							nextstate = GUI_STATE_GET_PLAYLIST;
							afterplayliststate = GUI_STATE_GET_MY_CHANNELS;
						} else {
							nextstate = GUI_STATE_GET_MY_CHANNELS;
						}
					} else {
						nextstate = GUI_STATE_GET_MY_CHANNELS;
					}
				} else {
					if ((gui->categories == NULL) || (gui->categories->prev == NULL) || (gui->categories->prev->nextPageState == GUI_STATE_GET_FAVORITES)) {
						/* First time running, need also to get the subscriptions. */
						state = GUI_STATE_GET_PLAYLIST;
						afterplayliststate = GUI_STATE_GET_MY_CHANNELS;
					} else {
						state = GUI_STATE_RUNNING;
					}
				}
				gui->cur_cat = NULL;
				break;

			case GUI_STATE_GET_MY_CHANNELS:
				rv = update_my_channels(gui, gui->cur_cat, playlistid, videopagetoken);
				if (rv != JT_OK) {
					state = GUI_STATE_ERROR;
					if ((gui->categories == NULL) || (gui->categories->prev == NULL) || (gui->categories->prev->nextPageState == GUI_STATE_GET_FAVORITES)) {
						/* First time running, need also to get the subscriptions. */
						if (gui->get_playlist_cat != NULL) {
							nextstate = GUI_STATE_GET_PLAYLIST;
							afterplayliststate = GUI_STATE_GET_SUBSCRIPTIONS;
						} else {
							nextstate = GUI_STATE_GET_SUBSCRIPTIONS;
						}
					} else {
						nextstate = GUI_STATE_GET_SUBSCRIPTIONS;
					}
				} else {
					state = GUI_STATE_GET_PLAYLIST;
					afterplayliststate = GUI_STATE_GET_SUBSCRIPTIONS;
				}
				gui->cur_cat = NULL;
				break;

			case GUI_STATE_GET_PREV_FAVORITES:
				rv = update_favorites(gui, gui->cur_cat, 1);
				if (rv != JT_OK) {
					state = GUI_STATE_ERROR;
					nextstate = GUI_STATE_RUNNING;
				} else {
					state = GUI_STATE_GET_PLAYLIST;
					afterplayliststate = GUI_STATE_GET_SUBSCRIPTIONS;
				}
				gui->cur_cat = NULL;
				break;

			case GUI_STATE_GET_PLAYLIST:
				if ((gui->cur_cat != NULL) || (gui->get_playlist_cat != NULL)) {
					if (gui->cur_cat == NULL) {
						gui->cur_cat = gui->get_playlist_cat;
						if (gui->get_playlist_cat->next != gui->get_playlist_cat) {
							/* Remove it from the gui->get_playlist_cat list. */
							gui->get_playlist_cat = gui->get_playlist_cat->next;
							gui->get_playlist_cat->prev = gui->get_playlist_cat->prev->prev;
							gui->get_playlist_cat->prev->next = gui->get_playlist_cat;
						} else {
							/* List is now empty. */
							gui->get_playlist_cat = NULL;
						}
						if (gui->cur_cat->where != NULL) {
							/* Insert after where. */
							gui->cur_cat->next = gui->cur_cat->where->next;
							gui->cur_cat->prev = gui->cur_cat->where;

							gui->cur_cat->where->next->prev = gui->cur_cat;
							gui->cur_cat->where->next = gui->cur_cat;
						} else {
							if (gui->categories == NULL) {
								/* First element added. */
								gui->categories = gui->cur_cat;
								gui->cur_cat->next = gui->cur_cat;
								gui->cur_cat->prev = gui->cur_cat;
							} else {
								gui_cat_t *l;

								/* Add at the end of the list. */
								l = gui->categories->prev;

								/* Add as new last: */
								gui->cur_cat->next = gui->categories;
								gui->cur_cat->prev = l;

								gui->categories->prev = gui->cur_cat;
								l->next = gui->cur_cat;
							}
							if (gui->current == NULL) {
								gui->current = gui->cur_cat;
							}
						}
					}
					if (gui->cur_cat->playlistid != NULL) {
						rv = update_playlist(gui, gui->cur_cat, 0);
						if (rv == JT_OK) {
							gui_cat_t *cat;

							if (!foundvid && (videoid != NULL)) {
								LOG("Search for playlist %s\n", playlistid);
								cat = gui->categories;
								while(cat != NULL) {
									LOG("in playlist %s %s\n", cat->playlistid, cat->title);
									if ((cat->nextPageState == ((enum gui_state) getstate)) && (cat->playlistid != NULL)
										&& (playlistid != NULL) && (strcmp(cat->playlistid, playlistid) == 0)) {
										/* Select current category which was specified by the parameter catnr. */
										gui->current = cat;
										LOG("Found playlist %s %s\n", cat->playlistid, cat->title);
										break;
									}
									cat = cat->next;
									if (cat == gui->categories) {
										break;
									}
								}
								if ((cat != NULL) && (cat->nextPageState == ((enum gui_state) getstate)) && (cat->playlistid != NULL)
									&& (playlistid != NULL) && (strcmp(cat->playlistid, playlistid) == 0)) {
									/* Try to find video selected by parameter videoid. */
									gui_elem_t *elem;

									LOG("Search for videoid %s in %s\n", videoid, cat->title);

									elem = cat->elem;
									while(elem != NULL) {
										LOG("in video id %s\n", elem->videoid);
										if ((elem->videoid != NULL) && (strcmp(elem->videoid, videoid) == 0)) {
											/* Found video, so select it. */
											cat->current = elem;
											/* Next video can be selected if user selected to play playlist. */
											LOG("Found for videoid %s in %s\n", videoid, cat->title);
											foundvid = 1;
											break;
										}
										elem = elem->next;
										if (elem == cat->elem) {
											break;
										}
									}
									LOG("reset playlistid %s because playlist was found.\n", playlistid);
									playlistid = NULL;
									searchterm = NULL;
									videoid = NULL;
								}
							}
							if (gui->get_playlist_cat == NULL) {
								/* Go to next state if there is nothing to load in this state. */
								state = afterplayliststate;
							}
							if (gui->statusmsg != NULL) {
								free(gui->statusmsg);
								gui->statusmsg = NULL;
							}
						} else {
							nextstate = state;
							state = GUI_STATE_ERROR;
							if (gui->get_playlist_cat == NULL) {
								nextstate = afterplayliststate;
							}
						}
					} else {
						gui->statusmsg = buf_printf(gui->statusmsg, "No playlist id");
						LOG_ERROR("GUI_STATE_GET_PLAYLIST: No playlist id in cat %s.\n", CHECKSTR(gui->cur_cat->title));
						wakeupcount = DEFAULT_SLEEP;
						if (gui->get_playlist_cat == NULL) {
							state = afterplayliststate;
						}
					}
				} else {
					LOG_ERROR("GUI_STATE_GET_PLAYLIST: No categories allocated.\n");
					gui->statusmsg = buf_printf(gui->statusmsg, "No categories allocated");
					wakeupcount = DEFAULT_SLEEP;
					state = afterplayliststate;
				}
				gui->cur_cat = NULL;
				break;

			case GUI_STATE_GET_PREV_PLAYLIST:
				if (gui->cur_cat != NULL) {
					if (gui->cur_cat->playlistid != NULL) {
						rv = update_playlist(gui, gui->cur_cat, 1);
					} else {
						gui->statusmsg = buf_printf(gui->statusmsg, "No playlist id");
						LOG_ERROR("GUI_STATE_GET_PREV_PLAYLIST: No playlist id in cat %s.\n", gui->cur_cat->title);
						wakeupcount = DEFAULT_SLEEP;
					}
				} else {
					LOG_ERROR("GUI_STATE_GET_PREV_PLAYLIST: No categories allocated.\n");
					gui->statusmsg = buf_printf(gui->statusmsg, "No categories allocated");
					wakeupcount = DEFAULT_SLEEP;
				}
				state = afterplayliststate;
				gui->cur_cat = NULL;
				break;

			case GUI_STATE_GET_SUBSCRIPTIONS: {
				if (((enum gui_state) getstate) == state) {
					rv = update_subscriptions(gui, gui->cur_cat, 0, catpagetoken, catnr, playlistid, videopagetoken, vidnr);
					catpagetoken = NULL;
				} else {
					rv = update_subscriptions(gui, gui->cur_cat, 0, NULL, 0, NULL, NULL, 0);
				}
				if (rv == JT_OK) {
					state = GUI_STATE_GET_CHANNELS;
				} else {
					state = GUI_STATE_ERROR;
					nextstate = GUI_STATE_RUNNING;
				}
				gui->cur_cat = NULL;
				break;
			}

			case GUI_STATE_GET_PREV_SUBSCRIPTIONS: {
				rv = update_subscriptions(gui, gui->cur_cat, 1, NULL, 0, NULL, NULL, 0);
				if (rv == JT_OK) {
					state = GUI_STATE_GET_CHANNELS;
				} else {
					state = GUI_STATE_ERROR;
					nextstate = GUI_STATE_RUNNING;
				}
				gui->cur_cat = NULL;
				break;
			}

			case GUI_STATE_GET_CHANNELS:
				if ((gui->cur_cat != NULL) || (gui->get_channel_cat != NULL)) {
					gui_cat_t *last = NULL;
					if (gui->cur_cat == NULL) {
						gui->cur_cat = gui->get_channel_cat;
						if (gui->get_channel_cat->next != gui->get_channel_cat) {
							/* Remove it from the gui->get_channel_cat list. */
							gui->get_channel_cat = gui->get_channel_cat->next;
							gui->get_channel_cat->prev = gui->get_channel_cat->prev->prev;
							gui->get_channel_cat->prev->next = gui->get_channel_cat;
						} else {
							/* List is now empty. */
							gui->get_channel_cat = NULL;
						}

						/* Insert in list for GUI_STATE_GET_PLAYLIST: */
						if (gui->get_playlist_cat == NULL) {
							/* First element added. */
							gui->get_playlist_cat = gui->cur_cat;
							gui->cur_cat->next = gui->cur_cat;
							gui->cur_cat->prev = gui->cur_cat;
						} else {
							gui_cat_t *l;

							/* Add at the end of the list. */
							l = gui->get_playlist_cat->prev;

							/* Add as new last: */
							gui->cur_cat->next = gui->get_playlist_cat;
							gui->cur_cat->prev = l;

							gui->get_playlist_cat->prev = gui->cur_cat;
							l->next = gui->cur_cat;
						}
					}
					rv = update_channels(gui, gui->cur_cat, &last);
					if (rv == JT_OK) {
						if ((last == NULL) || (last->channelNextPageToken == NULL)) {
							if (gui->get_channel_cat == NULL) {
								if (gui->get_playlist_cat == NULL) {
									state = GUI_STATE_RUNNING;
									wakeupcount = DEFAULT_SLEEP;
									gui->statusmsg = buf_printf(gui->statusmsg, "Failed update_channels() for cat %s.", (gui->cur_cat != NULL) ? CHECKSTR(gui->cur_cat->title) : "(null)");
									LOG_ERROR("Failed update_channels() for cat %s.\n", (gui->cur_cat != NULL) ? gui->cur_cat->title : "(null)");
								} else {
									state = GUI_STATE_GET_PLAYLIST;
									if (catpagetoken != NULL) {
										switch(getstate) {
											case GUI_STATE_GET_CHANNEL_PLAYLIST:
												/* Load selected category (was selected before restart). */
												afterplayliststate = getstate;
												/* Use current to return from playlist. */
												gui->prev_cat = gui->current;
												break;
											default:
												afterplayliststate = GUI_STATE_RUNNING;
												break;
										}
									} else {
										afterplayliststate = GUI_STATE_RUNNING;
									}
								}
							}
						}
					} else {
						nextstate = state;
						state = GUI_STATE_ERROR;
						if (gui->get_channel_cat == NULL) {
							nextstate = GUI_STATE_RUNNING;
						}
					}
				} else {
					gui->statusmsg = buf_printf(gui->statusmsg, "No category allocated for channels");
					LOG_ERROR("GUI_STATE_GET_CHANNELS: No category allocated for channels.\n");
					wakeupcount = DEFAULT_SLEEP;
					state = GUI_STATE_RUNNING;
				}
				gui->cur_cat = NULL;
				break;

			case GUI_STATE_GET_CHANNEL_PLAYLIST:
			case GUI_STATE_GET_PREV_CHANNEL_PLAYLIST: {
				int reverse;

				if (state == GUI_STATE_GET_CHANNEL_PLAYLIST) {
					reverse = 0;
				} else if (state == GUI_STATE_GET_PREV_CHANNEL_PLAYLIST) {
					reverse = 1;
				} else {
					reverse = 0;
					LOG_ERROR("Unsupported state: %d %s.\n", state, get_state_text(state));
				}
				if (gui->cur_cat == NULL) {
					LOG("Update channel for channelid %s catpagetoken %s channelnr %d playlistid %s videopagetoken %s vidnr %d\n",
						channelid, catpagetoken, channelnr, playlistid, videopagetoken, vidnr);
					rv = update_channel_playlists(gui, gui->cur_cat, reverse, channelid, catpagetoken, channelnr, playlistid, videopagetoken, vidnr);
					if (videopagetoken != NULL) {
						LOG("reset videopagetoken %s in  GUI_STATE_GET_CHANNEL_PLAYLIST\n", videopagetoken);
					}
					catpagetoken = NULL;
					videopagetoken = NULL;
					channelid = NULL;
				} else {
					LOG("Update channel %s for channelid %s playlistid %s\n", gui->cur_cat->title, gui->cur_cat->channelid, gui->cur_cat->playlistid);
					rv = update_channel_playlists(gui, gui->cur_cat, reverse, NULL, NULL, 0, NULL, NULL, 0);
				}
				if (rv == JT_OK) {
					if (gui->get_playlist_cat == NULL) {
						state = GUI_STATE_RUNNING;
						wakeupcount = DEFAULT_SLEEP;
						gui->statusmsg = buf_printf(gui->statusmsg, "No playlists for cat %s.", (gui->cur_cat != NULL) ? CHECKSTR(gui->cur_cat->title) : "(null)");
						LOG_ERROR("No playlists for %s.\n", (gui->cur_cat != NULL) ? CHECKSTR(gui->cur_cat->title) : "(null)");
					} else {
						state = GUI_STATE_GET_PLAYLIST;
						afterplayliststate = GUI_STATE_RUNNING;
					}
				} else {
					nextstate = GUI_STATE_RUNNING;
					state = GUI_STATE_ERROR;
				}
				if (gui->prev_cat != NULL) {
					if ((gui->current == NULL) || (gui->current->nextPageState != GUI_STATE_GET_CHANNEL_PLAYLIST)) {
						/* Error while loading playlist, no item selected from the playlist, restore normal state. */
						gui->prev_cat = NULL;
					}
				}
				gui->cur_cat = NULL;
				break;
			}

			case GUI_STATE_SEARCH_PLAYLIST:
			case GUI_STATE_SEARCH_PREV_PLAYLIST: {
				int reverse;

				if (state == GUI_STATE_SEARCH_PLAYLIST) {
					reverse = 0;
				} else if (state == GUI_STATE_SEARCH_PREV_PLAYLIST) {
					reverse = 1;
				} else {
					reverse = 0;
					LOG_ERROR("Unsupported state: %d %s.\n", state, get_state_text(state));
				}
				if (gui->cur_cat == NULL) {
					LOG_ERROR("No categoriy created.\n");
				} else {
					rv = update_search_playlists(gui, gui->cur_cat, reverse);
				}
				if (rv == JT_OK) {
					gui_cat_t *cat = gui->cur_cat;
					if (!foundvid && (videoid != NULL) && (cat != NULL) &&
						(searchterm != NULL) && (cat->searchterm != NULL) &&
						(strcmp(searchterm, cat->searchterm) == 0)) {
						LOG("Found cat for searchterm %s\n", searchterm);
						if (cat->nextPageState == ((enum gui_state) getstate)) {
							/* Try to find video selected by parameter videoid. */
							gui_elem_t *elem;

							LOG("Search for videoid %s in %s\n", videoid, cat->title);

							elem = cat->elem;
							while(elem != NULL) {
								LOG("in video id %s\n", elem->videoid);
								if ((elem->videoid != NULL) && (strcmp(elem->videoid, videoid) == 0)) {
									/* Found video, so select it. */
									cat->current = elem;
									/* Next video can be selected if user selected to play playlist. */
									LOG("Found for videoid %s in %s\n", videoid, cat->title);
									foundvid = 1;
									break;
								}
								elem = elem->next;
								if (elem == cat->elem) {
									break;
								}
							}
							LOG("reset searchterm %s because category was found.\n", searchterm);
							searchterm = NULL;
							playlistid = NULL;
							videoid = NULL;
						}
						/* Go to next state if there is nothing to load in this state. */
						state = aftersearchplayliststate;
						if (gui->statusmsg != NULL) {
							free(gui->statusmsg);
							gui->statusmsg = NULL;
						}
					}
					state = GUI_STATE_RUNNING;
				} else {
					nextstate = GUI_STATE_RUNNING;
					state = GUI_STATE_ERROR;
					searchterm = NULL;
					playlistid = NULL;
					videoid = NULL;
				}
				if (gui->prev_cat != NULL) {
					if ((gui->current == NULL) || (gui->current->nextPageState != GUI_STATE_SEARCH_PLAYLIST)) {
						/* Error while loading playlist, no item selected from the playlist, restore normal state. */
						gui->prev_cat = NULL;
					}
				}
				gui->cur_cat = NULL;
				break;
			}

			case GUI_STATE_RUNNING:
				/* Reset startup values. */
				catpagetoken = NULL;
				if (videopagetoken != NULL) {
					LOG("reset videopagetoken %s in  GUI_STATE_RUNNING\n", videopagetoken);
				}
				videopagetoken = NULL;
				playlistid = NULL;

				/* No operation is pending. */
				gui->cur_cat = NULL;
				if (gui->statusmsg != NULL) {
					free(gui->statusmsg);
					gui->statusmsg = NULL;
				}
				if (gui->current != NULL) {
					gui_cat_t *cat;

					cat = gui->current;

					/* Check if previous page can be preloaded when nothing is to do. */
					if ((state == GUI_STATE_RUNNING) && (cat != NULL) && ((cat == gui->categories) || (cat->prev == NULL) || (cat->prevPageState != cat->prev->prevPageState))) {
						char *prevPageToken;

						prevPageToken = gui_get_prevPageToken(cat);

						if (prevPageToken != NULL) {
							/* Load previous page. */
							state = cat->prevPageState;
							gui->cur_cat = cat;
						}
					}
					cat = gui->current;
					/* Check if next page can be preloaded when nothing is to do: */
					if ((state == GUI_STATE_RUNNING) && (cat != NULL) && ((cat->next == NULL) || (cat->next == gui->categories) || (cat->nextPageState != cat->next->nextPageState))) {
						char *nextPageToken;
	
						nextPageToken = gui_get_nextPageToken(cat);

						if (nextPageToken != NULL) {
							/* Load previous page. */
							state = cat->nextPageState;
							gui->cur_cat = cat;
						}
					}

					/* Play next video in playlist. */
					cat = gui->current;
					if ((state == GUI_STATE_RUNNING) && (cat != NULL) && foundvid && (retval == 2)) {
						if ((cat->current != NULL) && (cat->elem != NULL) && (cat->current->next == cat->elem)) {
							gui_elem_t *last;

							last = cat->elem->prev;

							if ((last->nextPageToken != NULL) && (curstate == GUI_STATE_RUNNING)) {
								if (cat->searchterm != NULL) {
									aftersearchplayliststate = GUI_STATE_PLAY_VIDEO;
									state = GUI_STATE_SEARCH_PLAYLIST;
								} else {
									afterplayliststate = GUI_STATE_PLAY_VIDEO;
									state = GUI_STATE_GET_PLAYLIST;
								}
								gui->cur_cat = cat;
							}
						} else {
							gui_elem_t *elem;

							elem = cat->current;
							if (elem != NULL) {
								gui_inc_elem(gui);
							}
							if (elem != cat->current) {
								wakeupcount = DEFAULT_SLEEP;
								state = GUI_STATE_PLAY_VIDEO;
							}
						}
					}

					/* Play previous video in playlist. */
					cat = gui->current;
					if ((state == GUI_STATE_RUNNING) && (cat != NULL) && foundvid && (retval == 3)) {
						if ((cat->current != NULL) && (cat->elem != NULL) && (cat->current->prev == cat->elem->prev)) {
							gui_elem_t *first;

							first = cat->elem;

							if ((first->prevPageToken != NULL) && (curstate == GUI_STATE_RUNNING)) {
								if (cat->searchterm != NULL) {
									aftersearchplayliststate = GUI_STATE_PLAY_PREV_VIDEO;
									state = GUI_STATE_SEARCH_PREV_PLAYLIST;
								} else {
									afterplayliststate = GUI_STATE_PLAY_PREV_VIDEO;
									state = GUI_STATE_GET_PREV_PLAYLIST;
								}
								gui->cur_cat = cat;
							}
						} else {
							gui_elem_t *elem;

							elem = cat->current;

							if (elem != NULL) {
								gui_dec_elem(gui);
							}
							if (elem != cat->current) {
								wakeupcount = DEFAULT_SLEEP;
								state = GUI_STATE_PLAY_PREV_VIDEO;
							}
						}
					}
				}
				break;

			case GUI_STATE_PLAY_VIDEO:
			case GUI_STATE_PLAY_PREV_VIDEO:
				if ((gui->current != NULL) && (retval != 0)) {
					gui_cat_t *cat;
					gui_elem_t *elem;

					cat = gui->current;
					elem = cat->current;

					if (elem != NULL) {
						int ret;

						/* Play next video. */
						ret = playVideo(gui, videofile, cat, elem, 0, 4096, lastcatpagetoken);
						if ((videofile != NULL) && (ret == 0)) {
							/* Terminate program, another program needs to use the videofile
							 * to play the video.
							 */
							done = 1;
						} else {
							/* No restart, selecting next video will always work. */
							foundvid = 1;
						}
						if (ret == 0) {
							if (state == GUI_STATE_PLAY_VIDEO) {
								retval = 2;
							} else if (state == GUI_STATE_PLAY_PREV_VIDEO) {
								retval = 3;
							} else {
								LOG_ERROR("Unsupport play state: %d %s.\n", state, get_state_text(state));
							}
						} else {
							retval = 0;
						}
					}
				}
				state = GUI_STATE_RUNNING;
				break;

			case GUI_STATE_ERROR:  {
				const char *error;

				/* Some error happened, the error code is stored in rv. */

				switch(rv) {
					case JT_PROTOCOL_ERROR:
						error = jt_get_error_description(gui->at);
						if (error != NULL) {
							gui->statusmsg = buf_printf(gui->statusmsg, "%s", error);
							LOG_ERROR("%s\n", error);
						} else {
							error = jt_get_protocol_error(gui->at);
							if (error != NULL) {
								gui->statusmsg = buf_printf(gui->statusmsg, "Error: %s", error);
								LOG_ERROR("Error: %s\n", error);
							} else {
								gui->statusmsg = buf_printf(gui->statusmsg, "Unknown protocol error");
								LOG_ERROR("Error: Unkown protocol error\n");
							}
						}
						break;

					case JT_TRANSFER_ERROR: {
						CURLcode res;

						res = jt_get_transfer_error(gui->at);
						switch(res) {
							case CURLE_SSL_CACERT:
								gui->statusmsg = buf_printf(gui->statusmsg, "Verification of CA cert failed.");
								LOG_ERROR("Verification of CA cert failed.\n");
								break;

							case CURLE_COULDNT_RESOLVE_HOST:
								gui->statusmsg = buf_printf(gui->statusmsg, "DNS failed.");
								LOG_ERROR("DNS failed.\n");
								break;

							default:
								error = curl_easy_strerror(res);
								gui->statusmsg = buf_printf(gui->statusmsg, "Transfer failed: %s", CHECKSTR(error));
								LOG_ERROR("Transfer failed: %s\n", CHECKSTR(error));
								break;
						}
						break;

					case JT_ERROR_CLIENT_ID:
#ifndef CLIENT_SECRET
						gui->statusmsg = buf_printf(gui->statusmsg, "Please download the client id and secret as JSON file from https://developers.google.com/youtube/v3/ and store it in your home directory at %s.", SECRET_FILE);
						LOG_ERROR("Please download the client id and secret as JSON file from https://developers.google.com/youtube/v3/ and store it in your home directory at %s.\n", SECRET_FILE);
#else
						gui->statusmsg = buf_printf(gui->statusmsg, "Please get the client id and secret and fix client.h in the source code of this program.");
						LOG_ERROR("Please get the client id and secret and fix client.h in the source code of this program.\n");
#endif
						break;
					}

					default:
						error = jt_get_error_code(rv);
						gui->statusmsg = buf_printf(gui->statusmsg, "Error: %s", CHECKSTR(error));
						LOG_ERROR("Error: %s\n", CHECKSTR(error));
						break;
				}
				state = GUI_STATE_WAIT_FOR_CONTINUE;
				break;
			}

			case GUI_STATE_GET_MY_PREV_CHANNELS:
				gui->statusmsg = buf_printf(gui->statusmsg, "GUI_STATE_GET_MY_PREV_CHANNELS shouldn't be entered.");
				/* Just go on: */
				wakeupcount = DEFAULT_SLEEP;
				state = GUI_STATE_RUNNING;
				break;

			case GUI_STATE_GET_PREV_CHANNELS:
				gui->statusmsg = buf_printf(gui->statusmsg, "GUI_STATE_GET_PREV_CHANNELS shouldn't be entered.");
				/* Just go on: */
				wakeupcount = DEFAULT_SLEEP;
				state = GUI_STATE_RUNNING;
				break;

			case GUI_STATE_POWER_OFF:
			case GUI_STATE_TIMEOUT:
				done = 1;
				retval = 4;
				break;

			case GUI_STATE_QUIT:
				done = 1;
				retval = 0;
				break;

			case GUI_STATE_UPDATE:
				done = 1;
				retval = 5;
				break;

			case GUI_STATE_MENU_PLAYLIST: {
				/* Show playlist which was selected from menu. */
				gui_cat_t *cat;

				cat = gui_cat_alloc(gui, &gui->categories, NULL);
				cat->nextPageState = GUI_STATE_MENU_PLAYLIST;
				cat->prevPageState = GUI_STATE_MENU_PLAYLIST;
				cat->playlistid = jt_strdup(gui->selectedmenu->playlistid);
				cat->channelid = jt_strdup(gui->selectedmenu->channelid);
		 		cat->title = jt_strdup(gui->selectedmenu->title);
				if (((enum gui_state) getstate) == state) {
					cat->videopagetoken = videopagetoken;
					videopagetoken = NULL;
					afterplayliststate = GUI_STATE_RUNNING;
				}
				if (catpagetoken != NULL) {
					switch(getstate) {
					case GUI_STATE_GET_CHANNEL_PLAYLIST:
						/* Load selected category (was selected before restart). */
						afterplayliststate = getstate;
						/* Use current to return from playlist. */
						gui->prev_cat = gui->current;
						break;
					default:
						afterplayliststate = GUI_STATE_RUNNING;
						break;
					}
				}
				gui->cur_cat = cat;
				state = GUI_STATE_LOAD_ACCESS_TOKEN;
				break;
			}

			case GUI_STATE_SEARCH_ENTER: {
				gui->statusmsg = buf_printf(gui->statusmsg, "Please enter search string:\n%s", searchstring);
				break;
			}

			case GUI_STATE_SEARCH: {
				/* Show playlist which was selected from menu. */
				gui_cat_t *cat;

				cat = gui_cat_alloc(gui, &gui->categories, NULL);
				cat->nextPageState = GUI_STATE_SEARCH;
				cat->prevPageState = GUI_STATE_SEARCH;
				cat->searchterm = jt_strdup(gui->selectedmenu->searchterm);
		 		cat->title = jt_strdup(gui->selectedmenu->searchterm);
				if (((enum gui_state) getstate) == state) {
					cat->videopagetoken = videopagetoken;
					videopagetoken = NULL;
					aftersearchplayliststate = GUI_STATE_RUNNING;
				}
				if (catpagetoken != NULL) {
					switch(getstate) {
					case GUI_STATE_SEARCH_PLAYLIST:
						/* Load selected category (was selected before restart). */
						aftersearchplayliststate = getstate;
						/* Use current to return from playlist. */
						gui->prev_cat = gui->current;
						break;
					default:
						aftersearchplayliststate = GUI_STATE_RUNNING;
						break;
					}
				}
				gui->cur_cat = cat;
				state = GUI_STATE_LOAD_ACCESS_TOKEN;
				break;
			}

		}

		if (state == GUI_STATE_RUNNING) {
			if (gui->prev_cat != NULL) {
				set_description_for_playlist(gui);
			} else {
				set_description_for_subscriptions(gui);
			}
		} else {
			if (retval != 0) {
				set_no_description(gui);
			}
		}

		/* Paint GUI elements. */
		gui_paint(gui, state);

		/* Check timer for power off. */
		timeoutcounter++;
		if (!done && (timer > 0)) {
			if (timeoutcounter > timer) {
				switch (state) {
				case GUI_STATE_RUNNING:
				case GUI_STATE_MAIN_MENU:
					prevstate = state;
					restorestate = state;
					state = GUI_STATE_TIMEOUT;
					LOG("Enter new state %d %s (triggered by timeout).\n", state, get_state_text(state));

					oldstatusmsg = gui->statusmsg;
					gui->statusmsg = buf_printf(NULL, "Timeout powering off.");
					wakeupcount = 3 * DEFAULT_SLEEP;
					break;

				case GUI_STATE_POWER_OFF:
				case GUI_STATE_TIMEOUT:
					/* Already powering off. */
					break;

				default:
					LOG_ERROR("Force power off by timeout in state %d %s.\n", state, get_state_text(state));
					/* Power off. */
					done = 1;
					retval = 4;
					break;
				}
			}
		}
	}
	if (oldstatusmsg != NULL) {
		free(oldstatusmsg);
		oldstatusmsg = NULL;
	}
	save_menu_state(gui);
	return retval;
}
