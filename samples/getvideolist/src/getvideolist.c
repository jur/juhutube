/*
 * getvideolist
 *
 * Sample for using libjt.
 *
 * List YoutTube videos of subscriptions.
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
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include "libjt.h"
#include "clientid.h"

#define TOKEN_FILE ".youtubetoken.json"
#define REFRESH_TOKEN_FILE ".refreshtoken.json"
#define SECRET_FILE ".client_secret.json"

#define dprintf(args...) \
	do { \
	} while(0)

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

struct subscriptions_s {
	char *title;
	char *channelId;
};

struct channel_s {
	char *channelId;
	char *playlistId;
};

struct playlist_item_s {
	char *title;
	char *videoId;
	char *thumbnail;
};

typedef struct subscriptions_s subscriptions_t;
typedef struct channel_s channel_t;
typedef struct playlist_item_s playlist_item_t;

static FILE *logfd = NULL;
static FILE *errfd = NULL;

static subscriptions_t *subscriptions;
static int subscription_count;
static channel_t *channels;
static int channel_count;
static playlist_item_t *playlist_items;
static int playlist_item_count;

static void resize_subscriptions(int count)
{
	if (subscription_count != count) {
		int i;

		/* Free memory if it is smaller. */
		for (i = count; i < subscription_count; i++) {
			if (subscriptions[i].title != NULL) {
				free(subscriptions[i].title);
				subscriptions[i].title = NULL;
			}
			if (subscriptions[i].channelId != NULL) {
				free(subscriptions[i].channelId);
				subscriptions[i].channelId = NULL;
			}
		}
		subscriptions = realloc(subscriptions, sizeof(subscriptions[0]) * count);

		/* Initialize new allocated memory. */
		for (i = subscription_count; i < count; i++) {
			memset(&subscriptions[i], 0, sizeof(subscriptions[i]));
		}

		subscription_count = count;
	}
}

static void resize_channels(int count)
{
	if (channel_count != count) {
		int i;

		/* Free memory if it is smaller. */
		for (i = count; i < channel_count; i++) {
			if (channels[i].channelId != NULL) {
				free(channels[i].channelId);
				channels[i].channelId = NULL;
			}
			if (channels[i].playlistId != NULL) {
				free(channels[i].playlistId);
				channels[i].playlistId = NULL;
			}
		}
		channels = realloc(channels, sizeof(channels[0]) * count);

		/* Initialize new allocated memory. */
		for (i = channel_count; i < count; i++) {
			memset(&channels[i], 0, sizeof(channels[i]));
		}

		channel_count = count;
	}
}

static void resize_playlist_items(int count)
{
	if (count > 100) {
		count = 100; // TBD: Load max 100 videos
	}
	if (playlist_item_count != count) {
		int i;

		/* Free memory if it is smaller. */
		for (i = count; i < playlist_item_count; i++) {
			if (playlist_items[i].title != NULL) {
				free(playlist_items[i].title);
				playlist_items[i].title = NULL;
			}
			if (playlist_items[i].videoId != NULL) {
				free(playlist_items[i].videoId);
				playlist_items[i].videoId = NULL;
			}
			if (playlist_items[i].thumbnail != NULL) {
				free(playlist_items[i].thumbnail);
				playlist_items[i].thumbnail = NULL;
			}
		}
		playlist_items = realloc(playlist_items, sizeof(playlist_items[0]) * count);

		/* Initialize new allocated memory. */
		for (i = playlist_item_count; i < count; i++) {
			memset(&playlist_items[i], 0, sizeof(playlist_items[i]));
		}

		playlist_item_count = count;
	}
}

int update_subscriptions(jt_access_token_t *at)
{
	int rv;
	char *nextPageToken;
	int totalResults = 0;
	int resultsPerPage = 0;
	int subnr = 0;

	nextPageToken = strdup("");
	subscription_count = 0;
	subscriptions = NULL;

	do {
		rv = jt_get_my_subscriptions(at, nextPageToken);
		free(nextPageToken);
		nextPageToken = NULL;

		if (rv == JT_OK) {
			int i;

			rv = jt_json_get_int_by_path(at, &totalResults, "/pageInfo/totalResults");
			if (rv != JT_OK) {
				totalResults = 0;
			}
			dprintf("totalResults rv = %d %s value = %d\n", rv, jt_get_error_code(rv), totalResults);

			resize_subscriptions(totalResults);

			rv = jt_json_get_int_by_path(at, &resultsPerPage, "/pageInfo/resultsPerPage");
			if (rv != JT_OK) {
				resultsPerPage = 0;
			}
			dprintf("resultsPerPage rv = %d %s value = %d\n", rv, jt_get_error_code(rv), resultsPerPage);

			nextPageToken = jt_strdup(jt_json_get_string_by_path(at, "nextPageToken"));
			dprintf("nextPageToken %s\n", nextPageToken);

			for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
				if (subscriptions[subnr].title != NULL) {
					free(subscriptions[subnr].title);
					subscriptions[subnr].title = NULL;
				}
				subscriptions[subnr].title = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/snippet/thumbnails/default", 0));
				dprintf("title %s\n", subscriptions[subnr].title);

				if (subscriptions[subnr].channelId != NULL) {
					free(subscriptions[subnr].channelId);
					subscriptions[subnr].channelId = NULL;
				}
				subscriptions[subnr].channelId = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/snippet/resourceId/channelId", i));
				dprintf("channelId %s\n", subscriptions[subnr].channelId);
				subnr++;
			}


			rv = jt_free_transfer(at);
			if (rv != JT_OK) {
				printf("Failed to free the JSON object.\n");
			}
		}
	} while(nextPageToken != NULL);

	return rv;
}

int update_channels(jt_access_token_t *at, const char *channelId)
{
	int rv;
	char *nextPageToken;
	int totalResults = 0;
	int resultsPerPage = 0;
	int subnr = 0;

	nextPageToken = strdup("");
	channel_count = 0;
	channels = NULL;

	do {
		rv = jt_get_channels(at, channelId, nextPageToken);
		free(nextPageToken);
		nextPageToken = NULL;

		if (rv == JT_OK) {
			int i;

			rv = jt_json_get_int_by_path(at, &totalResults, "/pageInfo/totalResults");
			if (rv != JT_OK) {
				totalResults = 0;
			}
			dprintf("totalResults rv = %d %s value = %d\n", rv, jt_get_error_code(rv), totalResults);

			resize_channels(totalResults);

			rv = jt_json_get_int_by_path(at, &resultsPerPage, "/pageInfo/resultsPerPage");
			if (rv != JT_OK) {
				resultsPerPage = 0;
			}
			dprintf("resultsPerPage rv = %d %s value = %d\n", rv, jt_get_error_code(rv), resultsPerPage);

			nextPageToken = jt_strdup(jt_json_get_string_by_path(at, "nextPageToken"));
			dprintf("nextPageToken %s\n", nextPageToken);

			for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
				if (channels[subnr].playlistId != NULL) {
					free(channels[subnr].playlistId);
					channels[subnr].playlistId = NULL;
				}
				channels[subnr].playlistId = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/contentDetails/relatedPlaylists/uploads", i));
				dprintf("playlistId %s\n", channels[subnr].playlistId);

				if (channels[subnr].channelId != NULL) {
					free(channels[subnr].channelId);
					channels[subnr].channelId = NULL;
				}
				channels[subnr].channelId = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/id", i));
				dprintf("channelId %s\n", channels[subnr].channelId);
				subnr++;
			}


			rv = jt_free_transfer(at);
			if (rv != JT_OK) {
				printf("Failed to free the JSON object.\n");
			}
		}
	} while(nextPageToken != NULL);

	return rv;
}

int update_playlist_items(jt_access_token_t *at, const char *playlistId)
{
	int rv;
	char *nextPageToken;
	int totalResults = 0;
	int resultsPerPage = 0;
	int subnr = 0;
	int printed = 0;

	nextPageToken = strdup("");
	playlist_item_count = 0;
	playlist_items = NULL;

	do {
		rv = jt_get_playlist_items(at, playlistId, nextPageToken);
		free(nextPageToken);
		nextPageToken = NULL;

		if (rv == JT_OK) {
			int i;

			rv = jt_json_get_int_by_path(at, &totalResults, "/pageInfo/totalResults");
			if (rv != JT_OK) {
				totalResults = 0;
			}
			dprintf("totalResults rv = %d %s value = %d\n", rv, jt_get_error_code(rv), totalResults);
			if (!printed) {
				printf("Getting %d Videos.\n", totalResults);
				printed = 1;
			}

			resize_playlist_items(totalResults);

			rv = jt_json_get_int_by_path(at, &resultsPerPage, "/pageInfo/resultsPerPage");
			if (rv != JT_OK) {
				resultsPerPage = 0;
			}
			dprintf("resultsPerPage rv = %d %s value = %d\n", rv, jt_get_error_code(rv), resultsPerPage);

			nextPageToken = jt_strdup(jt_json_get_string_by_path(at, "nextPageToken"));
			dprintf("nextPageToken %s\n", nextPageToken);

			for (i = 0; (i < resultsPerPage) && (subnr < totalResults); i++) {
				if (playlist_items[subnr].title != NULL) {
					free(playlist_items[subnr].title);
					playlist_items[subnr].title = NULL;
				}
				playlist_items[subnr].title = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/snippet/title", i));
				dprintf("Title %s\n", playlist_items[subnr].title);

				if (playlist_items[subnr].videoId != NULL) {
					free(playlist_items[subnr].videoId);
					playlist_items[subnr].videoId = NULL;
				}
				playlist_items[subnr].videoId = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/contentDetails/videoId", i));
				dprintf("videoId %s\n", playlist_items[subnr].videoId);

				if (playlist_items[subnr].thumbnail != NULL) {
					free(playlist_items[subnr].thumbnail);
					playlist_items[subnr].thumbnail = NULL;
				}
				playlist_items[subnr].thumbnail = jt_strdup(jt_json_get_string_by_path(at, "/items[%d]/snippet/thumbnails/default/url", i));
				dprintf("thumbnail %s\n", playlist_items[subnr].thumbnail);

				subnr++;
			}


			rv = jt_free_transfer(at);
			if (rv != JT_OK) {
				printf("Failed to free the JSON object.\n");
			}
		}
	} while((nextPageToken != NULL) && (subnr < playlist_item_count));

	return rv;
}

int update_playlists(jt_access_token_t *at, const char *playlistId)
{
	int rv;

	rv = jt_get_playlist(at, playlistId, "");
	if (rv == JT_OK) {
		int totalResults;
		int rv;
		int i;

		printf("kind %s\n", jt_json_get_string_by_path(at, "kind"));
		rv = jt_json_get_int_by_path(at, &totalResults, "/pageInfo/totalResults");
		if (rv != JT_OK) {
			totalResults = 0;
		}
		printf("totalResults rv = %d %s value = %d\n", rv, jt_get_error_code(rv), totalResults);
		for (i = 0; i < totalResults; i++) {
			printf("title %s\n", jt_json_get_string_by_path(at, "/items[%d]/snippet/title", i));
			printf("channelId %s\n", jt_json_get_string_by_path(at, "/items[%d]/snippet/channelId", i));
			printf("thumbnail %s\n", jt_json_get_string_by_path(at, "/items[%d]/snippet/thumbnails/default/url", i));
		}
		rv = jt_free_transfer(at);
		if (rv != JT_OK) {
			printf("Failed to free the JSON object.\n");
		}
	}
	return rv;
}

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

void get_videolist_main(void)
{
	jt_access_token_t *at;
	const char *home;
#ifndef CLIENT_SECRET
	char *secretfile = NULL;
#endif
	int ret;
	char *tokenfile = NULL;
	char *refreshtokenfile = NULL;
	int rv;

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

#ifndef CLIENT_SECRET
	ret = asprintf(&secretfile, "%s/%s", home, SECRET_FILE);
	if (ret == -1) {
		free(refreshtokenfile);
		refreshtokenfile = NULL;
		free(tokenfile);
		tokenfile = NULL;
		LOG_ERROR("Out of memory\n");
		return;
	}
#endif

#ifdef CLIENT_SECRET
	at = jt_alloc(logfd, errfd, CLIENT_ID, CLIENT_SECRET, tokenfile, refreshtokenfile, 0);
#else
	at = jt_alloc_by_file(logfd, errfd, secretfile, tokenfile, refreshtokenfile, 0);
#endif

	/* Log into YouTube account. */
	rv = login(at);

	if (rv == JT_OK) {
		rv = update_subscriptions(at);
	}

	if (rv == JT_OK) {
		int subnr;

		for (subnr = 0; subnr < subscription_count; subnr++) {
			int playnr;
			printf("title %s\n", subscriptions[subnr].title);
			printf("channelId %s\n", subscriptions[subnr].channelId);
			rv = update_channels(at, subscriptions[subnr].channelId);

			for (playnr = 0; playnr < channel_count; playnr++) {
				printf("channelId %s\n", channels[playnr].channelId);
				printf("playlistId %s\n", channels[playnr].playlistId);
				rv = update_playlists(at, channels[playnr].playlistId);
				if (rv == JT_OK) {
					int vidnr;

					rv = update_playlist_items(at, channels[playnr].playlistId);
					for (vidnr = 0; vidnr < playlist_item_count; vidnr++) {
						printf("Title %s\n", playlist_items[vidnr].title);
						printf("videoId %s\n", playlist_items[vidnr].videoId);
						printf("thumbnail %s\n", playlist_items[vidnr].thumbnail);
					}
				}
			}
		}
	}


	/* Clean up */
	resize_subscriptions(0);
	resize_channels(0);
	resize_playlist_items(0);

	switch (rv) {
		case JT_CODE_EXPIRED:
			/* The user was to slow. */
			printf("Authorisation code expired.\n");
			break;

		case JT_PROTOCOL_ERROR:
			/* Some problem in the protocol. */
			printf("Authorisation failed with protocol error: %s\n", jt_get_protocol_error(at));
			break;

		case JT_JASON_PARSE_ERROR:
			/* This can be caused by different HTTP errors like 400 Bad request. */
			printf("There is a problem with the server.\n");
			break;

		case JT_TRANSFER_ERROR: {
			CURLcode res;

			res = jt_get_transfer_error(at);

			switch(res)
			{
				case CURLE_SSL_CACERT:
					printf("Verification of CA certificate failed, please update \"/etc/ssl/certs/\".\n");
					break;

				case CURLE_COULDNT_RESOLVE_HOST:
					printf("DNS failed. Please add server to /etc/resolv.conf.");
					break;

				default:
					printf("Transfer failed: %d %s\n", res,
						curl_easy_strerror(res));
					break;
			}
		}

		case JT_OK:
			printf("Successfully finished.\n");
			break;

		default:
			printf("Failed with error %d %s.\n", rv, jt_get_error_code(rv));
			break;
	}
	jt_free(at);
	at = NULL;
}


int main(int argc, char *argv[])
{
	int c;
	const char *logfile = NULL;

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

	curl_global_init(CURL_GLOBAL_DEFAULT);

	get_videolist_main();

	curl_global_cleanup();
	return 0;
}
