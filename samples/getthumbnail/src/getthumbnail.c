/*
 * getthumbnail
 *
 * Sample for using libjt.
 *
 * Get thumbnail of the first favorited YouTube video and display it.
 * This example used SDL.
 *
 * BSD License
 *
 * Copyright Juergen Urban
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include "libjt.h"
#include "clientid.h"

#define TOKEN_FILE ".youtubetoken.json"
#define REFRESH_TOKEN_FILE ".refreshtoken.json"
#define KEY_FILE ".youtubekey"
#define SECRET_FILE ".client_secret.json"

#define DEFAULT_SLEEP 50

#define LOG_ERROR(format, args...) \
	do { \
		fprintf(errfd, __FILE__ ":%u:Error:" format, __LINE__, ##args); \
	} while(0)

#define LOG(format, args...) \
	do { \
		if (logfd != NULL) { \
			fprintf(logfd, format, ##args); \
		} \
	} while(0)

/**
 * States of state machine of GUI.
 */
enum gui_state {
	STATE_ALLOC,
	STATE_LOAD_ACCESS_TOKEN,
	STATE_GET_USER_CODE,
	STATE_GET_TOKEN,
	STATE_LOGGED_IN,
	STATE_GET_PLAYLIST,
	STATE_GET_THUMBNAIL,
	STATE_END,
	STATE_ERROR,
};

/** Needed to load web content via CURL to memory. */
struct mem_s {
	char *memory;
	size_t size;
};

typedef struct mem_s mem_t;

/** File handle for error output messages. */
static FILE *errfd;
/** File handle for log output messages. */
static FILE *logfd;

/** Callback for loading web content via CURL to memory. */
static size_t mem_callback(void *contents, size_t size, size_t nmemb,
	void *userp)
{
	size_t realsize = size * nmemb;
	mem_t *mem = userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
		LOG_ERROR("Not enough memory (realloc returned NULL).\n");
		return 0;
	}
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

/** Linker symbol to YouTube logo in JPEG format. */
extern unsigned char _binary_pictures_yt_powered_jpg_start;
/** Linker symbol to size of YouTube logo in JPEG format. */
extern unsigned char _binary_pictures_yt_powered_jpg_size;

/** Load YouTube logo. */
SDL_Surface *get_youtube_logo(void)
{
	SDL_RWops *rw = SDL_RWFromMem(&_binary_pictures_yt_powered_jpg_start, (long) &_binary_pictures_yt_powered_jpg_size);

	return IMG_Load_RW(rw, 1);
}

/** YouTube logo. */
static SDL_Surface *logo = NULL;
/** Output screen */
static SDL_Surface *screen = NULL;
/** Position of YouTube logo on screen. */
static SDL_Rect logorect = { 0, 0, 0, 0 };
/** Pointer to font used to write something on the screen. */
static TTF_Font *font = NULL;

/* The heigth of the letters in the youtube logo. */
static int mindistance = 34;

/** Clean up of graphic libraries. */
void graphic_cleanup(void)
{
	if (font != NULL) {
		TTF_CloseFont(font);
		font = NULL;
	}
	if (logo != NULL) {
		SDL_FreeSurface(logo);
		logo = NULL;
	}
	if (screen != NULL) {
		SDL_FreeSurface(screen);
		screen = NULL;
	}
	TTF_Quit();
	SDL_Quit();
}

/** Initialize graphic. */
int graphic_init(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Couldn't initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	SDL_ShowCursor(SDL_DISABLE);

	TTF_Init();

	logo = get_youtube_logo();
	if (logo == NULL) {
		LOG_ERROR("Failed to load youtube logo.\n");
		graphic_cleanup();
		return 2;
	}

	font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 36);
	if (font == NULL) {
		graphic_cleanup();
		return 3;
	}

	screen = SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE);
	if (screen == NULL) {
		LOG_ERROR("Couldn't set 640x480x8 video mode: %s\n", SDL_GetError());
		graphic_cleanup();
		return 4;
	}

	logorect.x = screen->w - logo->w - mindistance;
	logorect.y = screen->h - logo->h - mindistance;

	atexit(SDL_Quit);
	atexit(TTF_Quit);
	return 0;
}

/** Get CURL object for transfering web content. */
CURL *prepare_transfer(void)
{
	CURL *curl;

	curl = curl_easy_init();
	if (curl == NULL) {
		LOG_ERROR("Failed to get CURL object. Out of memory?\n");
		return NULL;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_callback);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	return curl;
}

/** Free CURL object after use. */
void cleanup_transfer(CURL *curl)
{
	curl_easy_cleanup(curl);
	curl = NULL;
}

/** Load image via URL from the internet. */
SDL_Surface *load_image(CURL *curl, const char *url)
{
	CURLcode res;
	mem_t chunk;
	SDL_Surface *image = NULL;


	chunk.memory = NULL;
	chunk.size = 0;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

	res = curl_easy_perform(curl);
	if (res == CURLE_OK) {
		if (chunk.memory != NULL) {
			if (chunk.size > 0) {
				SDL_RWops *rw = SDL_RWFromMem(chunk.memory, chunk.size);
				image = IMG_Load_RW(rw, 1);
				rw = NULL;
			}
			free(chunk.memory);
			chunk.memory = NULL;
		}
	} else {
		LOG_ERROR("curl_easy_perform() failed: %d %s url %s\n",
			res, curl_easy_strerror(res), url);
	}

	return image;
}

/**
 * Paint GUI.
 *
 * @param image Pointer to image which will be displayed. The image can also be
 *        an image of a text (created by img_printf).
 * @param headertext Text displayed at the top of the screen.
 */
void gui_paint(SDL_Surface *image, const char *headertext)
{
	SDL_Color clrFg = {255, 255, 255, 0}; /* White */
	SDL_Surface *sText = NULL;

	SDL_FillRect(screen, NULL, 0x000000);

	SDL_BlitSurface(logo, NULL, screen, &logorect);

	if (image != NULL) {
		SDL_Rect rcDest = { 40 /* X pos */, 90 /* Y pos */, 0, 0 };
		SDL_BlitSurface(image, NULL, screen, &rcDest);
	}

	if (headertext != NULL) {
		/* Convert text to an image. */
		sText = TTF_RenderText_Solid(font, headertext, clrFg);
	}
	if (sText != NULL) {
		SDL_Rect rcDest = {40, 40, 0, 0};
		SDL_BlitSurface(sText, NULL, screen, &rcDest);
		SDL_FreeSurface(sText);
		sText = NULL;
	}

	/* Update the screen content. */
	SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);
}

/**
 * Print text to an SDL surface.
 *
 * @param image This object is freed.
 * @param format printf-like format string.
 *
 * @returns SDL surface which should replace the image.
 */
SDL_Surface *img_printf(SDL_Surface *image, const char *format, ...)
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

	return TTF_RenderText_Solid(font, text, clrFg);
}

/**
 * Like strdup, but works with NULL pointers and frees the first parameter.
 * Can be used to replace allocated strings, e.g.
 * text = restrdup(text, "new text");
 * @param text free will be called with this pointer when the pointer is not NULL.
 * @param newtext strdup will be called and the pointer will be returned if it
 *        newtext is not NULL.
 * @returns Pointer to copied string.
 * @return NULL if newtext is NULL or out of memory.
 */
char *restrdup(char *text, const char *newtext)
{
	if (text != NULL) {
		free(text);
		text = NULL;
	}
	if (newtext != NULL) {
		return strdup(newtext);
	} else {
		return NULL;
	}
}

/**
 * Main loop for GUI.
 */
void gui_loop(CURL *curl)
{
	SDL_Surface *image = NULL;
	int done;
	SDL_Event event;
	enum gui_state state;
	enum gui_state nextstate;
	jt_access_token_t *at = NULL;
	unsigned int wakeupcount;
	unsigned int sleeptime = 50 * 3;
	char *url = NULL;
	char *headertext = NULL;
	int rv;
	const char *home;
#ifndef CLIENT_SECRET
	char *secretfile = NULL;
#endif
	int ret;
	char *tokenfile = NULL;
	char *refreshtokenfile = NULL;
	char *keyfile = NULL;

	home = getenv("HOME");
	if (home == NULL) {
		LOG_ERROR("Environment variable HOME is not set.\n");
		return;
	}

	ret = asprintf(&tokenfile, "%s/%s", home, TOKEN_FILE);
	if (ret == -1) {
		LOG_ERROR("Out of memory\n");
		return;
	}

	ret = asprintf(&refreshtokenfile, "%s/%s", home, REFRESH_TOKEN_FILE);
	if (ret == -1) {
		free(tokenfile);
		tokenfile = NULL;
		LOG_ERROR("Out of memory\n");
		return;
	}

	ret = asprintf(&keyfile, "%s/%s", home, KEY_FILE);
	if (ret == -1) {
		free(refreshtokenfile);
		refreshtokenfile = NULL;
		free(tokenfile);
		tokenfile = NULL;
		LOG_ERROR("Out of memory\n");
		return;
	}

#ifndef CLIENT_SECRET
	ret = asprintf(&secretfile, "%s/%s", home, SECRET_FILE);
	if (ret == -1) {
		free(keyfile);
		keyfile = NULL;
		free(refreshtokenfile);
		refreshtokenfile = NULL;
		free(tokenfile);
		tokenfile = NULL;
		LOG_ERROR("Out of memory\n");
		return;
	}
#endif

	done = 0;
	state = STATE_ALLOC;
	wakeupcount = 5;
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
	
						default:
							break;
					}
					break;

				default:
					break;

			}
		}

		/* Paint GUI elements. */
		gui_paint(image, headertext);

		/* wakupcount is used in the state machine to wait some time
		 * until the next state should be processed.
		 */
		if (wakeupcount > 0) {
			wakeupcount--;
		}

		if (!done && (wakeupcount <= 0)) {
			/* The state machine of the GUI. */
			switch(state) {
				case STATE_ALLOC:
					/* First allocate the handle. */
#ifdef CLIENT_SECRET
					at = jt_alloc(logfd, errfd, CLIENT_ID, CLIENT_SECRET, tokenfile, refreshtokenfile, CLIENT_KEY, 0);
#else
					at = jt_alloc_by_file(logfd, errfd, secretfile, tokenfile, refreshtokenfile, keyfile, 0);
#endif
					if (at == NULL) {
						if (image != NULL) {
							SDL_FreeSurface(image);
							image = NULL;
						}
						image = img_printf(image, "Alloc Error");
						wakeupcount = DEFAULT_SLEEP;
					} else {
						state = STATE_LOAD_ACCESS_TOKEN;
					}
					break;

				case STATE_LOAD_ACCESS_TOKEN:
					/* Try to load existing access for YouTube user account. */
					rv = jt_load_token(at);
					if (rv == JT_OK) {
						image = img_printf(image, "Logged in");
						state = STATE_LOGGED_IN;
					} else {
						state = STATE_GET_USER_CODE;
					}
					break;

				case STATE_GET_USER_CODE:
					/* Access user to give access for this application to the YouTube user account. */
					rv = jt_update_user_code(at);
					if (rv == JT_OK) {
						image = img_printf(image, "User Code: %s", jt_get_user_code(at));
						headertext = restrdup(headertext, jt_get_verification_url(at));
						state = STATE_GET_TOKEN;
						wakeupcount = sleeptime;
					} else {
						/* Retry later */
						state = STATE_ERROR;
						nextstate = STATE_GET_USER_CODE;

						/* Remove user code. */
						headertext = restrdup(headertext, NULL);
					}
					break;

				case STATE_GET_TOKEN:
					/* Check whether the user permitted access for this application. */
					rv = jt_get_token(at);
					switch (rv) {
						case JT_OK:
							state = STATE_LOGGED_IN;
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
							image = img_printf(image, "Expired");
							/* Retry */
							wakeupcount = DEFAULT_SLEEP;
							state = STATE_GET_USER_CODE;
							break;
	
						default:
							/* Retry later */
							state = STATE_ERROR;
							nextstate = STATE_GET_USER_CODE;
							break;
					}
					break;

				case STATE_LOGGED_IN:
					/* Now logged into user account. */
					image = img_printf(image, "Logged in");
					state = STATE_GET_PLAYLIST;
					break;

				case STATE_GET_PLAYLIST:
					/* Get playlist of current user (favorites). */
					rv = jt_get_my_playlist(at, "");
					if (rv == JT_OK) {
						int totalResults;

						rv = jt_json_get_int_by_path(at, &totalResults, "/pageInfo/totalResults");
						if (rv != JT_OK) {
							totalResults = 0;
						}
						if (totalResults > 0) {
							headertext = restrdup(headertext, jt_json_get_string_by_path(at, "/items[%d]/snippet/title", 0));
							url = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/snippet/thumbnails/medium/url", 0));
							if (url == NULL) {
								url = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/snippet/thumbnails/default/url", 0));
							}
							if (url != NULL) {
								state = STATE_GET_THUMBNAIL;
							} else {
								image = img_printf(image, "Failed to get URL of thumbnail");

								/* Retry later */
								wakeupcount = DEFAULT_SLEEP;
							}
						}
						rv = jt_free_transfer(at);
						if (rv != JT_OK) {
							LOG("Failed to free the JSON object.\n");
						}
					} else {
						/* Retry later */
						state = STATE_ERROR;
						nextstate = STATE_GET_PLAYLIST;
					}
					break;

				case STATE_GET_THUMBNAIL:
					/* Get thumbnail of the video using CURL. */
					if (url != NULL) {
						/* Load the thumbnail. */
						image = load_image(curl, url);
						if (image != NULL) {
							/* Successfully loaded the thumbnail. */
							state = STATE_END;
							free(url);
							url = NULL;
						} else {
							/* Retry later */
							wakeupcount = DEFAULT_SLEEP;
						}
					} else {
						image = img_printf(image, "URL not valid");

						/* Retry later */
						wakeupcount = DEFAULT_SLEEP;
					}
					break;

				case STATE_END:
					/* Finished. */
					break;

				case STATE_ERROR:  {
					const char *error;

					/* Some error happened, the error code is stored in rv. */

					switch(rv) {
						case JT_PROTOCOL_ERROR:
							error = jt_get_error_description(at);
							if (error != NULL) {
								image = img_printf(image, "%s", error);
							} else {
								error = jt_get_protocol_error(at);
								image = img_printf(image, "Error: %s", error);
							}
							break;

						case JT_TRANSFER_ERROR: {
							CURLcode res;

							res = jt_get_transfer_error(at);
							switch(res) {
								case CURLE_SSL_CACERT:
									image = img_printf(image, "Verification of CA cert failed.");
									break;

								case CURLE_COULDNT_RESOLVE_HOST:
									image = img_printf(image, "DNS failed.");
									break;

								default:
									error = curl_easy_strerror(res);
									image = img_printf(image, "Transfer failed: %s", error);
									break;
							}
							break;
						}

						default:
							error = jt_get_error_code(rv);
							image = img_printf(image, "Error: %s", error);
							break;
					}

					/* Retry */
					wakeupcount = DEFAULT_SLEEP;
					state = nextstate;
					break;
				}
			}
		}
	}

	/* Clean up */
	if (url != NULL) {
		free(url);
		url = NULL;
	}

	if (headertext != NULL) {
		free(headertext);
		headertext = NULL;
	}

	if (at != NULL) {
		jt_free(at);
		at = NULL;
	}

	if (image != NULL) {
		SDL_FreeSurface(image);
		image = NULL;
	}
}

/**
 * Entry point for the application.
 *
 * @param argc Number of application parameters.
 * @param argv Array with application parameters passed in shell.
 *
 * @return 0 on success.
 */
int main(int argc, char *argv[])
{
	int c;
	const char *logfile = NULL;
	CURL *curl = NULL;

	errfd = stderr;

	while((c = getopt (argc, argv, "l:s")) != -1) {
		switch(c) {
			case 'l':
				/* Write log messages to a file. */
				logfile = optarg;
				break;

			case 's':
				/* Write log messages on console. */
				logfd = errfd;
				break;

			default:
				abort();
				break;
		}
	}

	if (logfile != NULL) {
		logfd = fopen(logfile, "wt");
		if (logfd == NULL) {
			LOG_ERROR("Failed to open logfile: %s\n", strerror(errno));
			return -1;
		}
	}
	if (logfd != NULL) {
		errfd = logfd;
	}

	if (graphic_init() != 0) {
		LOG_ERROR("Failed to intialize SDL.\n");
		return -2;
	}

	/* Initialize CURL. This used to transfer the web content. */
	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = prepare_transfer();

	if (curl != NULL) {
		/* Call the GUI main processing loop. */
		gui_loop(curl);

		/* Clean up */
		cleanup_transfer(curl);
		curl = NULL;
	} else {
		LOG_ERROR("Failed to initialize curl. Out of memory?\n");
	}

	curl_global_cleanup();

	graphic_cleanup();

	return 0;
}
