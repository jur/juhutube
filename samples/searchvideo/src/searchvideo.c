/*
 * getvideolist
 *
 * Sample for using libjt.
 *
 * Search YouTube videos.
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
#define KEY_FILE ".youtubekey"
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

static FILE *logfd = NULL;
static FILE *errfd = NULL;

int search_video(jt_access_token_t *at, const char *searchterm)
{
	int rv;
	char pageToken[80] = "";
	int totalResults;
	int found = 0;

	do {
		rv = jt_search_video(at, searchterm, pageToken);
		if (rv == JT_OK) {
			int resultsPerPage;
			int rv;
			int i;
			const char *nextPageToken;

			printf("kind %s\n", jt_json_get_string_by_path(at, "kind"));
			nextPageToken = jt_json_get_string_by_path(at, "nextPageToken");
			if (nextPageToken != NULL) {
				strncpy(pageToken, nextPageToken, sizeof(pageToken));
				pageToken[sizeof(pageToken) - 1] = 0;
			}
			rv = jt_json_get_int_by_path(at, &totalResults, "/pageInfo/totalResults");
			if (rv != JT_OK) {
				totalResults = 0;
			}
			rv = jt_json_get_int_by_path(at, &resultsPerPage, "/pageInfo/resultsPerPage");
			if (rv != JT_OK) {
				resultsPerPage = 0;
			}
			printf("totalResults = %d, resultsPerPage = %d\n", totalResults, resultsPerPage);
			for (i = 0; i < resultsPerPage; i++) {
				printf("video id %s\n", jt_json_get_string_by_path(at, "/items[%d]/id/videoId", i));
				printf("title %s\n", jt_json_get_string_by_path(at, "/items[%d]/snippet/title", i));
				printf("channelId %s\n", jt_json_get_string_by_path(at, "/items[%d]/snippet/channelId", i));
				printf("thumbnail %s\n", jt_json_get_string_by_path(at, "/items[%d]/snippet/thumbnails/default/url", i));
			}
			rv = jt_free_transfer(at);
			if (rv != JT_OK) {
				printf("Failed to free the JSON object.\n");
			}
		}
	} while (found < totalResults);
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

void search_video_main(int authorize)
{
	jt_access_token_t *at;
	const char *home;
#ifndef CLIENT_SECRET
	char *secretfile = NULL;
#endif
	int ret;
	char *tokenfile = NULL;
	char *refreshtokenfile = NULL;
	char *keyfile = NULL;
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

#ifdef CLIENT_SECRET
	at = jt_alloc(logfd, errfd, CLIENT_ID, CLIENT_SECRET, tokenfile, refreshtokenfile, CLIENT_KEY, 0);
#else
	at = jt_alloc_by_file(logfd, errfd, secretfile, tokenfile, refreshtokenfile, keyfile, 0);
#endif

	if (authorize) {
		/* Log into YouTube account. */
		rv = login(at);

		if (rv == JT_OK) {
			printf("Login failed, trying without login.\n");
		}
	}
	rv = search_video(at, "ps2+linux");

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
	int authorize = 0;

	errfd = stderr;

	while((c = getopt (argc, argv, "l:sa")) != -1) {
		switch(c) {
			case 'l':
				/* Write log messages to a file. */
				logfile = optarg;
				break;

			case 's':
				/* Write log messages on console. */
				logfd = errfd;
				break;

			case 'a':
				authorize = 1;
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

	search_video_main(authorize);

	curl_global_cleanup();
	return 0;
}
