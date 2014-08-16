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
#include <stdlib.h>

#include "log.h"
#include "gui.h"
#include "transfer.h"

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
	const char *videofile = NULL;
	const char *catpagetoken = NULL;
	int catnr = 0;
	int vidnr = 0;
	const char *videoid = NULL;
	const char *playlistid = NULL;
	const char *videopagetoken = NULL;
	int state = 0;
	int retval = 0;

	errfd = stderr;

	while((c = getopt (argc, argv, "l:sv:c:i:n:m:t:u:p:r:")) != -1) {
		switch(c) {
			case 'l':
				/* Write log messages to a file. */
				logfile = optarg;
				break;

			case 'c':
				/* Write log messages to a file. */
				catpagetoken = optarg;
				break;

			case 's':
				/* Write log messages on console. */
				logfd = errfd;
				break;

			case 'v':
				videofile = optarg;
				break;

			case 'i':
				videoid = optarg;
				break;

			case 'n':
				catnr = strtol(optarg, NULL, 0);
				break;

			case 'm':
				state = strtol(optarg, NULL, 0);
				break;

			case 't':
				videopagetoken = optarg;
				break;

			case 'u':
				vidnr = strtol(optarg, NULL, 0);
				break;

			case 'p':
				playlistid = optarg;
				break;

			case 'r':
				retval = strtol(optarg, NULL, 0);
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

	/* Call the GUI main processing loop. */
	retval = gui_loop(gui, retval, state, videofile, playlistid, catpagetoken, videoid, catnr, videopagetoken, vidnr);

	gui_free(gui);
	gui = NULL;

	transfer_cleanup();

	return retval;
}
