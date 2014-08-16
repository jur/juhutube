#ifndef _LIBJT_H_
#define _LIBJT_H_

/*
 * libjt
 *
 * Access Google YouTube ABI.
 *
 * BSD License
 *
 * Copyright Juergen Urban
 *
 */

#include <curl/curl.h>
#ifdef JSONC
#include <json-c/json.h>

#ifndef is_error
#define is_error(ptr) ((ptr) == NULL)
#endif

#else
#include <json/json.h>
#endif

/*
 * The libjt provides support for accessing youtube accounts and videos.
 *
 * curl_global_init() must be called before using any function.
 * 
 * The library is not multithreading save when you use the same jt_access_token_t
 * in different threads.
 */

/** Success */
#define JT_OK 0
/** Undefined error */
#define JT_ERROR 1
/** jt_update_user_code() wasn't called */
#define JT_ERROR_DEVICE_CODE 2
/** Protocol error returned in HTTP request */
#define JT_PROTOCOL_ERROR 3
/** Some error in curl happened during the transfer (see jt_get_transfer_error()). */
#define JT_TRANSFER_ERROR 4
/** Internal error */
#define JT_JOBJ_NOT_FREE 5
/** Out of memory */
#define JT_NO_MEM 6
/** Bad JSON format in file or HTTP reponse */
#define JT_JASON_PARSE_ERROR 7
/** Code missing in JSON reponse */
#define JT_CODE_MISSING 8
/** Failed to read file */
#define JT_FILE_ERROR 9
/** Client ID is not valid */
#define JT_ERROR_CLIENT_ID 10
/** Client secret is not valid */
#define JT_ERROR_SECRET 11
/** Token file is not valid */
#define JT_NO_TOKEN_FILE 12
/** No authorisation yet */
#define JT_AUTH_PENDING 13
/** Functionc alled to fast */
#define JT_SLOW_DOWN 14
/** User code expired */
#define JT_CODE_EXPIRED 15
/** Refresh token missing (jt_update_user_code() or jt_load_token() wasn't called) */
#define JT_ERROR_REFRESH_TOKEN 16
/** Authorisation error */
#define JT_AUTH_ERROR 17
/** get_token() or jt_load_token() not called */
#define JT_ERROR_ACCESS_TOKEN 18
/** Bad array value in path */
#define JT_PATH_BAD_ARRAY 19
/** Path to long for JSON */
#define JT_PATH_TOO_LONG 20
/** Path to short for JSON */
#define JT_PATH_TOO_SHORT 21
/** Wrong type in JSON path */
#define JT_PATH_WRONG_TYPE 22

/* Flags for jt_alloc(). */

/** When this flag is used the certificate is not checked. This is for testing,
 * using this can be a security problem.
 */
#define JT_FLAG_NO_CERT 1
/** When this flag is used the host is not checked. This is for testing,
 * using this can be a security problem.
 */
#define JT_FLAG_NO_HOST_CHECK 2


/**
 * This is the handle needed to be passed to all functions.
 */
typedef struct jt_access_token_s jt_access_token_t;

/**
 * Get an access token for google youtube.
 *
 * @param logfd File descriptor where debug messages are logged.
 * @param errfd File descriptor where error messages are logged.
 * @param client_id Client ID of the application.
 * @param client_secret Client secret of the application.
 * @param token_file Path to file where token file should be stored for the
 *        current youtube user.
 * @param refresh_token_file Path to file where a refreshed token file should
 *        be stored for the current youtube user.
 * @param flags 0 (with security), JT_FLAG_NO_CERT or JT_FLAG_NO_HOST_CHECK.
 *
 * The string parameters are internally copied and the memory will be freed when
 * jt_free() is called.
 *
 * Get OAuth 2.0 client ID and client secret for the developer here:
 * https://developers.google.com/youtube/registering_an_application
 */
jt_access_token_t *jt_alloc(FILE *logfd, FILE *errfd, const char *client_id,
	const char *client_secret,
	const char *token_file,
	const char *refresh_token_file,
	unsigned int flags);

/**
 * Get an access token for google youtube.
 *
 * @param logfd File descriptor where debug messages are logged.
 * @param errfd File descriptor where error messages are logged.
 * @param secret_file Path to client secret file as downloaded from Googles
 *        Developer Console.
 * @param token_file Path to file where token file should be stored for the
 *        current youtube user.
 * @param refresh_token_file Path to file where a refreshed token file should
 *        be stored for the current youtube user.
 * @param flags 0 (with security), JT_FLAG_NO_CERT or JT_FLAG_NO_HOST_CHECK.
 *
 * The string parameters are internally copied and the memory will be freed when
 * jt_free() is called.
 *
 * Get OAuth 2.0 client ID and client secret for the developer here:
 * https://developers.google.com/youtube/registering_an_application
 */
jt_access_token_t *jt_alloc_by_file(FILE *logfd, FILE *errfd,
	const char *secret_file,
	const char *token_file,
	const char *refresh_token_file,
	unsigned int flags);

/**
 * Free memory which were allocated, including all string returned by any
 * function.
 */
void jt_free(jt_access_token_t *at);

/**
 * Update the user code and the verification URL. The updated values can be
 * read with jt_get_user_code() and jt_get_verification_url().
 *
 * @returns JT_* error code
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 */
int jt_update_user_code(jt_access_token_t *at);

/**
 * Get the access token after the user entered the user code at verification
 * URL.
 *
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_PENDING when the user hasn't entered the user code yet.
 * @return JT_SLOW_DOWN when the function is called to fast needs to be called
 *         with larger intervals (>4 seconds).
 * @return JT_CODE_EXPIRED The user code was not entered in time.
 * @return JT_ERROR_DEVICE_CODE jt_update_user_code() wasn't called.
 */
int jt_get_token(jt_access_token_t *at);

/**
 * Updates the access token using the refresh token.
 * 
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_ERROR When permission denied.
 */
int jt_get_refresh_token(jt_access_token_t *at);

/**
 * Load the latest saved access token from file. This avoids accesing the
 * user again to enter user code again.
 *
 * @return JT_FILE_ERROR when file can't be read.
 * @return JT_JASON_PARSE_ERROR when file is broken.
 */
int jt_load_token(jt_access_token_t *at);

/**
 * Get the playlist for the current user.
 * The function will automatically refresh access tokens when needed.
 *
 * @param pageToken Must be "" for the first page. When this function is
 *        called the next page is returned in the JSON attribute "nextPageToken".
 *
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_ERROR When permission denied.
 * @return JT_ERROR_ACCESS_TOKEN When jt_get_token() or jt_load_token() wasn't
 *         called.
 * @return JT_PROTOCOL_ERROR Protocol error.
 */
int jt_get_my_subscriptions(jt_access_token_t *at, const char *pageToken);

/**
 * Get the channels for the channel id.
 * The function will automatically refresh access tokens when needed.
 *
 * @param channelId The ID of the channel.
 * @param pageToken Must be "" for the first page. When this function is
 *        called the next page is returned in the JSON attribute "nextPageToken".
 *
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_ERROR When permission denied.
 * @return JT_ERROR_ACCESS_TOKEN When jt_get_token() or jt_load_token() wasn't
 *         called.
 * @return JT_PROTOCOL_ERROR Protocol error.
 */
int jt_get_channels(jt_access_token_t *at, const char *channelId, const char *pageToken);

/**
 * Get the channels for the current user.
 * The function will automatically refresh access tokens when needed.
 *
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_ERROR When permission denied.
 * @return JT_ERROR_ACCESS_TOKEN When jt_get_token() or jt_load_token() wasn't
 *         called.
 * @return JT_PROTOCOL_ERROR Protocol error.
 */
int jt_get_my_channels(jt_access_token_t *at, const char *pageToken);

/**
 * Get the playlist for the playlistid.
 * The function will automatically refresh access tokens when needed.
 *
 * @param playlistid The ID of the playlists (e.g. returned by jt_get_channels()
 *        at /items[0]/contentDetails/relatedPlaylists/uploads).
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_ERROR When permission denied.
 * @return JT_ERROR_ACCESS_TOKEN When jt_get_token() or jt_load_token() wasn't
 *         called.
 * @return JT_PROTOCOL_ERROR Protocol error.
 */
int jt_get_playlist(jt_access_token_t *at, const char *playlistid, const char *pageToken);

/**
 * Get the playlist for the current user.
 * The function will automatically refresh access tokens when needed.
 *
 * @param pageToken Must be "" for the first page. When this function is
 *        called the next page is returned in the JSON attribute "nextPageToken".
 *
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_ERROR When permission denied.
 * @return JT_ERROR_ACCESS_TOKEN When jt_get_token() or jt_load_token() wasn't
 *         called.
 * @return JT_PROTOCOL_ERROR Protocol error.
 */
int jt_get_my_playlist(jt_access_token_t *at, const char *pageToken);

/**
 * Get the playlist for the channelid.
 * The function will automatically refresh access tokens when needed.
 *
 * @param channelid The ID of the channel (e.g. returned by jt_get_my_subscriptions()
 *        at /items[0]/contentDetails/relatedPlaylists/uploads).
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_ERROR When permission denied.
 * @return JT_ERROR_ACCESS_TOKEN When jt_get_token() or jt_load_token() wasn't
 *         called.
 * @return JT_PROTOCOL_ERROR Protocol error.
 */
int jt_get_channel_playlists(jt_access_token_t *at, const char *channelid, const char *pageToken);

/**
 * Get the playlist items for the playlistid.
 * The function will automatically refresh access tokens when needed.
 *
 * @param pageToken Must be "" for the first page. When this function is
 *        called the next page is returned in the JSON attribute "nextPageToken".
 *
 * @return JT_OK On success.
 * @return JT_TRANSFER_ERROR When an HTTPS transfer error happened. Wrong
 *         certificate, no DNS and other errors.
 * @return JT_AUTH_ERROR When permission denied.
 * @return JT_ERROR_ACCESS_TOKEN When jt_get_token() or jt_load_token() wasn't
 *         called.
 * @return JT_PROTOCOL_ERROR Protocol error.
 */
int jt_get_playlist_items(jt_access_token_t *at, const char *playlistid, const char *pageToken);

/**
 * Returns the user code. The user needs to enter this code at the verification
 * URL to allow access for the application; i.e. the user needs the web browser
 * on a different device like a computer.
 * The returned pointer is valid until the object at is deleted or
 * jt_update_user_code() is called.
 */
const char *jt_get_user_code(jt_access_token_t *at);

/**
 * Returns the verification URL. The user needs to enter this code at the
 * verification URL to allow access for the application.
 * The returned pointer is valid until the object at is deleted or
 * jt_update_user_code() is called.
 */
const char *jt_get_verification_url(jt_access_token_t *at);

/**
 * Get text returned in the http request for the return code JT_PROTOCOL_ERROR.
 * The returned pointer is valid until the at object is deleted or a new
 * JT_PROTOCOL_ERROR is returned.
 */
const char *jt_get_protocol_error(jt_access_token_t *at);

/**
 * Get text returned in the http request for the return code JT_PROTOCOL_ERROR.
 * The returned pointer is valid until the at object is deleted or a new
 * JT_PROTOCOL_ERROR is returned.
 */
const char *jt_get_error_description(jt_access_token_t *at);

/**
 * @return The last curl error code for JT_TRANSFER_ERROR.
 */
CURLcode jt_get_transfer_error(jt_access_token_t *at);

/**
 * Converts the error code returned by a jt_*() function into text.
 * When you use JT_OK as parameter you get "JT_OK".
 *
 * @param rv Error code which was returned by a jt_*() function.
 * @returns Error code in text format (e.g. "JT_OK").
 */
const char *jt_get_error_code(int rv);

/**
 * Get JSON string from last transfer by path.
 * @param format Printf-like format string for JSON path.
 * @returns Pointer to string of JSON value at path. The pointer is valid until
 *	jt_free_transfer(at) is called.
 * @return NULL on error.
 */
const char *jt_json_get_string_by_path(jt_access_token_t *at, const char *format, ...);

/**
 * Get JSON object from last transfer by path.
 * @param format Printf-like format string for JSON path.
 * @returns Pointer to a JSON object at path. The pointer is valid until
 *	jt_free_transfer(at) is called.
 * @return NULL on error.
 */
json_object *jt_json_get_object_by_path(jt_access_token_t *at, const char *format, ...);

/**
 * Get JSON integer from last transfer by path.
 * @param format Printf-like format string for JSON path.
 * @param value Pointer to returned value.
 * @returns Pointer to string of JSON value at path. The pointer is valid until
 *	jt_free_transfer(at) is called.
 * @return JT_OK On success.
 * @return JT_PATH_BAD_ARRAY Bad array index.
 * @return JT_PATH_TOO_LONG The path is too long.
 * @return JT_PATH_TOO_SHORT The path is too short.
 * @return JT_PATH_WRONG_TYPE The object type is wrong (not integer) 
 */
int jt_json_get_int_by_path(jt_access_token_t *at, int *value, const char *format, ...);

/**
 * Free the JSON objects received by the last transfer.
 * When you don't call this function you get JT_JOBJ_NOT_FREE on the next
 * transfer.
 * @return JT_OK On success.
 * @return JT_ERROR if there was nothing to be freed.
 */
int jt_free_transfer(jt_access_token_t *at);

/**
 * Copy string. Works with NULL pointers. NULL is returned for NULL pointers.
 * @returns Pointer to copied string.
 */
char *jt_strdup(const char *text);

#endif
