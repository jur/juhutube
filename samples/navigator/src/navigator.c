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

	/* Call the GUI main processing loop. */
	gui_loop(gui);

	gui_free(gui);
	gui = NULL;

	transfer_cleanup();

	return 0;
}
