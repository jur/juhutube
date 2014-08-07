/*
 * navigator
 *
 * Sample for using libjt.
 *
 * Navigate through YouTube.
 * This example use SDL.
 *
 * BSD License
 *
 * Copyright Juergen Urban
 *
 */
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include "log.h"
#include "gui.h"
#include "transfer.h"
#include "libjt.h"
#include "clientid.h"

#define TOKEN_FILE "youtubetoken.json"
#define REFRESH_TOKEN_FILE "refreshtoken.json"

static int login(jt_access_token_t *at)
{
	int rv;

	rv = jt_load_token(at);
	if (rv != JT_OK) {
		LOG("No existing configuration found.\n");

		do {
			/* Need to login. */
			rv = jt_update_user_code(at);
			if (rv == JT_OK) {
				int sleeptime = 3;

				printf("Please enter the code %s at the website %s.\n", jt_get_user_code(at), jt_get_verification_url(at));
			
				do {
					/* Check if we have access. */
					rv = jt_get_token(at);
					switch (rv) {
						case JT_AUTH_PENDING:
							/* Need to wait longer. */
							sleep(sleeptime);
							break;

						case JT_SLOW_DOWN:
							/* Need to poll slower. */
							sleeptime++;
							sleep(sleeptime);
							rv = JT_AUTH_PENDING;
							break;

						default:
							break;
					}
				} while(rv == JT_AUTH_PENDING);

			}
		} while(rv == JT_CODE_EXPIRED);
	}
	return rv;
}

void add_playlist(gui_t *gui, gui_cat_t *cat, jt_access_token_t *at, char *playlistid)
{
	int rv;

	if (playlistid != NULL) {
		int subnr;

		subnr = 0;
		rv = jt_get_playlist_items(at, playlistid, "");
		if (rv == JT_OK) {
			int totalResults = 0;
			int resultsPerPage = 0;
			int i;

			rv = jt_json_get_int_by_path(at, &totalResults, "/pageInfo/totalResults");
			if (rv != JT_OK) {
				totalResults = 0;
			}

			rv = jt_json_get_int_by_path(at, &resultsPerPage, "/pageInfo/resultsPerPage");
			if (rv != JT_OK) {
				resultsPerPage = 0;
			}
			for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
				const char *url;
				
				url = jt_json_get_string_by_path(at, "/items[%d]/snippet/thumbnails/default/url", i);
				//url = jt_json_get_string_by_path(at, "/items[%d]/snippet/thumbnails/medium/url", i);
				if (url != NULL) {
					gui_elem_alloc(gui, cat, url);
				}
				subnr++;
			}
			jt_free_transfer(at);
		}
	}
}

void update_favorites(gui_t *gui, gui_cat_t *cat)
{
	jt_access_token_t *at;
	char *playlistid = NULL;
	int rv;

	at = jt_alloc(logfd, errfd, CLIENT_ID, CLIENT_SECRET, TOKEN_FILE, REFRESH_TOKEN_FILE, 0);

	/* Log into YouTube account. */
	rv = login(at);

	if (rv == JT_OK) {
		rv = jt_get_my_playlist(at, "");
	}

	if (rv == JT_OK) {
		playlistid = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/id", 0));
		jt_free_transfer(at);
		add_playlist(gui, cat, at, playlistid);
	}


	jt_free(at);
	at = NULL;
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
	gui_t *gui;

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
				return 1;
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

	transfer_init();

	gui = gui_alloc();
	if (gui == NULL) {
		LOG_ERROR("Failed to intialize GUI.\n");
		return -2;
	}


	gui_cat_t *cat = gui_cat_alloc(gui); // TBD
	if (cat != NULL) {
		update_favorites(gui, cat);
	}

	/* Call the GUI main processing loop. */
	gui_loop(gui);

	gui_free(gui);
	gui = NULL;

	transfer_cleanup();

	return 0;
}
