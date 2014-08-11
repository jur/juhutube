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
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <curl/curl.h>
#ifdef BUILD_WITH_BUILDROOT
#include <json-c/json.h>
#else
#include <json/json.h>
#endif

#include "libjt.h"

#define CHUNK_SIZE 256

#ifdef DEBUG
#define JT_JSON_DEBUG
#endif

/**
 * Write error message, similar to printf().
 */
#define LOG_ERROR(format, args...) \
	do { \
		fprintf(at->errfd, __FILE__ ":%u:Error:" format, __LINE__, ##args); \
		fflush(at->errfd); \
	} while(0)

/**
 * Write log message, similar to printf().
 */
#define LOG(format, args...) \
	do { \
		if (at->logfd != NULL) { \
			fprintf(at->logfd, __FILE__ ":%u:" format, __LINE__, ##args); \
			if (at->errfd == stderr) { \
				fflush(at->errfd); \
			} \
		} \
	} while(0)

/**
 * Convert switch case value into text format.
 */
#define CONVCASETOTEXT(code) \
	case code: \
		return #code;


typedef struct jt_transfer_s jt_transfer_t;
typedef struct jt_mem_s jt_mem_t;

struct jt_mem_s {
	jt_access_token_t *at;
	char *memory;
	size_t size;
};

struct jt_transfer_s {
	CURL *curl;
	CURLcode res;
	json_object *jobj;
	jt_mem_t chunk;
};

struct jt_access_token_s {
	FILE *logfd;
	FILE *errfd;

	/* App keys. */
	char *client_id;
	char *client_secret;

	/* User authorisation. */
	char *device_code;
	char *user_code;
	char *verification_url;

	/* Access token */
	char *access_token;
	char *token_type;
	char *refresh_token;

	/* Token storage */
	char *token_file;
	char *refresh_token_file;

	/* HTTP Transfer */
	jt_transfer_t transfer;

	/* Protocol error */
	char *protocol_error;
	char *error_description;
};

char *jt_strdup(const char *text)
{
	if (text != NULL) {
		return strdup(text);
	} else {
		return NULL;
	}
}

static size_t jt_mem_callback(void *contents, size_t size, size_t nmemb,
	void *userp)
{
	size_t realsize = size * nmemb;
	jt_mem_t *mem = userp;
	jt_access_token_t *at = mem->at;

	if (mem->memory == NULL) {
		mem->memory = malloc(mem->size + realsize + 1);
	} else {
		mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	}
	if(mem->memory == NULL) {
		LOG_ERROR("Not enough memory (realloc returned NULL).\n");
		return 0;
	}
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}


jt_access_token_t *jt_alloc(FILE *logfd, FILE *errfd, const char *client_id,
	const char *client_secret, const char *token_file,
	const char *refresh_token_file, unsigned int flags)
{
	jt_access_token_t *at;

	at = malloc(sizeof(*at));
	if (at == NULL) {
		return NULL;
	}
	memset(at, 0, sizeof(*at));

	at->logfd = logfd;
	at->errfd = errfd;

	LOG("%s()\n", __FUNCTION__);

	at->client_id = jt_strdup(client_id);
	at->client_secret = jt_strdup(client_secret);
	at->token_file = jt_strdup(token_file);
	at->refresh_token_file = jt_strdup(refresh_token_file);

	at->transfer.curl = curl_easy_init();

	if (at->transfer.curl == NULL) {
		free(at);
		at = NULL;
		return NULL;
	}

	curl_easy_setopt(at->transfer.curl, CURLOPT_WRITEFUNCTION, jt_mem_callback);
	curl_easy_setopt(at->transfer.curl, CURLOPT_WRITEDATA, (void *)&at->transfer.chunk);
	curl_easy_setopt(at->transfer.curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	/* Don't include headers in response. */
	curl_easy_setopt(at->transfer.curl, CURLOPT_HEADER, 0);

	if (flags & JT_FLAG_NO_CERT) {
		curl_easy_setopt(at->transfer.curl, CURLOPT_SSL_VERIFYPEER, 0L);
	}
	if (flags & JT_FLAG_NO_HOST_CHECK) {
		curl_easy_setopt(at->transfer.curl, CURLOPT_SSL_VERIFYHOST, 0L);
	}


	return at;
}

void jt_free(jt_access_token_t *at)
{
	LOG("%s()\n", __FUNCTION__);
	if (at->client_id != NULL) {
		free(at->client_id);
		at->client_id = NULL;
	}
	if (at->client_secret != NULL) {
		free(at->client_secret);
		at->client_secret = NULL;
	}
	if (at->token_file != NULL) {
		free(at->token_file);
		at->token_file = NULL;
	}
	if (at->refresh_token_file != NULL) {
		free(at->refresh_token_file);
		at->refresh_token_file = NULL;
	}
	if (at->device_code != NULL) {
		free(at->device_code);
		at->device_code = NULL;
	}
	if (at->user_code != NULL) {
		free(at->user_code);
		at->user_code = NULL;
	}
	if (at->verification_url != NULL) {
		free(at->verification_url);
		at->verification_url = NULL;
	}
	if (at->access_token != NULL) {
		free(at->access_token);
		at->access_token = NULL;
	}
	if (at->token_type != NULL) {
		free(at->token_type);
		at->token_type = NULL;
	}
	if (at->refresh_token != NULL) {
		free(at->refresh_token);
		at->refresh_token = NULL;
	}
	if (at->protocol_error != NULL) {
		free(at->protocol_error);
		at->protocol_error = NULL;
	}
	if (at->error_description != NULL) {
		free(at->error_description);
		at->error_description = NULL;
	}

	curl_easy_cleanup(at->transfer.curl);
	at->transfer.curl = NULL;

	free(at);
	at = NULL;
}

static void jt_save_file(jt_access_token_t *at, const char *filename, void *mem, size_t size)
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

static void *jt_load_file(jt_access_token_t *at, const char *filename)
{
	FILE *fin;
	int pos = 0;
	void *mem = NULL;
	int eof = 0;

	LOG("%s()\n", __FUNCTION__);

	fin = fopen(filename, "rb");
	if (fin != NULL) {
		int rv;

		mem = malloc(CHUNK_SIZE);
		if (mem == NULL) {
			fclose(fin);
			fin = NULL;
			return NULL;
		}
		do {
			rv = fread(mem + pos, 1, CHUNK_SIZE, fin);
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

static const char *jt_json_sub_get_string(json_object *jobj, const char *jsonkey) {
	json_object_object_foreach(jobj, key, val) {
		enum json_type type;

		type = json_object_get_type(val);
#if 0
		printf("type: %d key %s\n", type, key);
#endif

		switch (type) {
			case json_type_string:
				if (strcmp(key, jsonkey) == 0) {
					return json_object_get_string(val);
				}
				break;

			default:
				break;
		}
	}
	return NULL;
}

static const char *jt_json_get_string(jt_access_token_t *at, const char *jsonkey) {
	const char *rv;
	LOG("%s() jsonkey %s jobj %p\n", __FUNCTION__, jsonkey, at->transfer.jobj);
	if (at->transfer.jobj == NULL) {
		LOG_ERROR("Called jt_json_get_string with invalid jobj.\n");
		return NULL;
	}

	if (is_error(at->transfer.jobj)) {
		return NULL;
	}

	rv = jt_json_sub_get_string(at->transfer.jobj, jsonkey);

	LOG("%s() jsonkey %s = %s\n", __FUNCTION__, jsonkey, rv);

	return rv;
}

static const char *jt_json_sub_get_string_by_path(jt_access_token_t *at, json_object *jobj, char *path, ...) {
	char *jsonkey;
	char *end;
	char *term = NULL;
	long array = -1;

	LOG("%s(): path %s\n", __FUNCTION__, path);

	/* Removing leading slashes from path. */
	while(path[0] == '/') {
		path++;
	}
	jsonkey = path;
	LOG("%s(): jsonkey %s\n", __FUNCTION__, jsonkey);

	/* Find end of current component (ends with slash or string termination). */
	end = path;
	while((*end != 0) && (*end != '/')) {
		if (*end == '[') {
			LOG("%s(): detected array in path %s\n", __FUNCTION__, end);
			array = strtol(end + 1, NULL, 0);
			term = end;
			if (array < 0) {
				LOG_ERROR("%s(): path %s has a bad array index.\n", __FUNCTION__, path);
				return NULL;
			}
		}
		end++;
	}

	/* Set path to next component or 0 if this is already the last component. */
	path = end;
	if (*end != 0) {
		path++;
	}
	/* Terminate jsonkey string. */
	*end = 0;
	/* Get jsonkey from array, remove '[' from string. */
	LOG("%s(): term %p\n", __FUNCTION__, term);
	if (term != NULL) {
		*term = 0;
	}
	LOG("%s(): jsonkey '%s' *path 0x%02x\n", __FUNCTION__, jsonkey, *path);

	json_object_object_foreach(jobj, key, val) {
		enum json_type type;
#if 0
		const char *typedscription = NULL;
#endif

		type = json_object_get_type(val);
#if 0
		switch (type) {
			case json_type_string:
				typedscription = "String";
				break;
			case json_type_object:
				typedscription = "Object";
				break;
			case json_type_array:
				typedscription = "Array";
				break;
			case json_type_null:
				typedscription = "Null";
				break;
			case json_type_boolean:
				typedscription = "Boolean";
				break;
			case json_type_double:
				typedscription = "Double";
				break;
			case json_type_int:
				typedscription = "Int";
				break;
		}
		printf("type: %d %s key %s\n", type, typedscription, key);
#endif

		if (strcmp(key, jsonkey) == 0) {
			switch (type) {
				case json_type_string:
					if ((*path == 0) && (array < 0)) {
						LOG("%s(): found string\n", __FUNCTION__);
						return json_object_get_string(val);
					} else {
						LOG("%s(): reject string\n", __FUNCTION__);
						return NULL;
					}
					break;
				case json_type_object:
					if (*path != 0) {
						json_object *sub;
	
						LOG("%s(): found object\n", __FUNCTION__);
						sub = json_object_object_get(jobj, key);
	
						return jt_json_sub_get_string_by_path(at, sub, path);
					} else {
						LOG("%s(): reject object\n", __FUNCTION__);
						return NULL;
					}
					break;

				case json_type_array:
					if (array >= 0) {
						json_object *sub;
	
						sub = json_object_object_get(jobj, key);
	
						/* Only check first element in array. */
						sub = json_object_array_get_idx(sub, array);
						if ((sub == NULL) || is_error(sub)) {
							LOG_ERROR("%s(): Array index %ld out of range.\n", __FUNCTION__, array);
							return NULL;
						}
						if (*path == 0) {
							if (json_object_get_type(sub) == json_type_string) {
								LOG("%s(): found string in array\n", __FUNCTION__);
								return json_object_get_string(sub);
							} else {
								LOG("%s(): reject array, path too short\n", __FUNCTION__);
								return NULL;
							}
						} else {
							LOG("%s(): found array\n", __FUNCTION__);
							return jt_json_sub_get_string_by_path(at, sub, path);
						}
					} else {
						LOG("%s(): reject array\n", __FUNCTION__);
						return NULL;
					}
					break;

				default:
					/* Unexpected type */
					LOG("%s(): wrong path\n", __FUNCTION__);
					return NULL;
			}
		}
	}
	return NULL;
}


const char *jt_json_get_string_by_path(jt_access_token_t *at, const char *format, ...) {
	va_list ap;
	const char *rv;
	char *path = NULL;
	int ret;

	if (at->transfer.jobj == NULL) {
		LOG_ERROR("Called jt_json_get_string with invalid jobj.\n");
		return NULL;
	}

	if (is_error(at->transfer.jobj)) {
		return NULL;
	}

	va_start(ap, format);
	ret = vasprintf(&path, format, ap);
	va_end(ap);
	if (ret == -1) {
		path = NULL;
		return NULL;
	}
	LOG("%s() path %s jobj %p\n", __FUNCTION__, path, at->transfer.jobj);
	if (path == NULL) {
		return NULL;
	}
	rv = jt_json_sub_get_string_by_path(at, at->transfer.jobj, path);
	free(path);
	path = NULL;

	LOG("%s() path %s = %s\n", __FUNCTION__, path, rv);

	return rv;
}

static int jt_json_sub_get_int_by_path(jt_access_token_t *at, json_object *jobj, char *path, int *value) {
	char *jsonkey;
	char *end;
	char *term = NULL;
	long array = -1;

	LOG("%s(): path %s\n", __FUNCTION__, path);

	/* Removing leading slashes from path. */
	while(path[0] == '/') {
		path++;
	}
	jsonkey = path;
	LOG("%s(): jsonkey %s\n", __FUNCTION__, jsonkey);

	/* Find end of current component (ends with slash or string termination). */
	end = path;
	while((*end != 0) && (*end != '/')) {
		if (*end == '[') {
			LOG("%s(): detected array in path %s\n", __FUNCTION__, end);
			term = end;
			array = strtol(end + 1, NULL, 0);
			if (array < 0) {
				LOG_ERROR("%s(): path %s has a bad array index.\n", __FUNCTION__, path);
				return JT_PATH_BAD_ARRAY;
			}
		}
		end++;
	}

	/* Set path to next component or 0 if this is already the last component. */
	path = end;
	if (*end != 0) {
		path++;
	}
	/* Terminate jsonkey string. */
	*end = 0;
	/* Get jsonkey from array, remove '[' from string. */
	LOG("%s(): term %p\n", __FUNCTION__, term);
	if (term != NULL) {
		*term = 0;
	}
	LOG("%s(): jsonkey '%s' *path 0x%02x\n", __FUNCTION__, jsonkey, *path);

	json_object_object_foreach(jobj, key, val) {
		enum json_type type;
#if 0
		const char *typedscription = NULL;
#endif

		type = json_object_get_type(val);
#if 0
		switch (type) {
			case json_type_string:
				typedscription = "String";
				break;
			case json_type_object:
				typedscription = "Object";
				break;
			case json_type_array:
				typedscription = "Array";
				break;
			case json_type_null:
				typedscription = "Null";
				break;
			case json_type_boolean:
				typedscription = "Boolean";
				break;
			case json_type_double:
				typedscription = "Double";
				break;
			case json_type_int:
				typedscription = "Int";
				break;
		}
		printf("type: %d %s key %s\n", type, typedscription, key);
#endif

		if (strcmp(key, jsonkey) == 0) {
			switch (type) {
				case json_type_int:
					if ((*path == 0) && (array < 0)) {
						LOG("%s(): found it\n", __FUNCTION__);
						*value = json_object_get_int(val);
						return JT_OK;
					} else {
						LOG("%s(): reject int\n", __FUNCTION__);
						return JT_PATH_TOO_LONG;
					}
					break;
				case json_type_object:
					if (*path != 0) {
						json_object *sub;
	
						LOG("%s(): found object\n", __FUNCTION__);
						sub = json_object_object_get(jobj, key);
	
						return jt_json_sub_get_int_by_path(at, sub, path, value);
					} else {
						LOG("%s(): reject object\n", __FUNCTION__);
						return JT_PATH_TOO_SHORT;
					}
					break;

				case json_type_array:
					if (array >= 0) {
						json_object *sub;
	
						sub = json_object_object_get(jobj, key);
	
						/* Only check first element in array. */
						sub = json_object_array_get_idx(sub, array);
						if ((sub == NULL) || is_error(sub)) {
							LOG_ERROR("%s(): Array index %ld out of range.\n", __FUNCTION__, array);
							return JT_PATH_BAD_ARRAY;
						}
						if (*path == 0) {
							if (json_object_get_type(sub) == json_type_int) {
								LOG("%s(): found int in array\n", __FUNCTION__);
								*value = json_object_get_int(sub);
								return JT_OK;
							} else {
								LOG("%s(): reject array, path too short\n", __FUNCTION__);
								return JT_PATH_TOO_SHORT;
							}
						} else {
							LOG("%s(): found array\n", __FUNCTION__);
							return jt_json_sub_get_int_by_path(at, sub, path, value);
						}
					} else {
						LOG("%s(): reject array\n", __FUNCTION__);
						return JT_PATH_BAD_ARRAY;
					}
					break;

				default:
					/* Unexpected type */
					LOG("%s(): wrong path\n", __FUNCTION__);
					return JT_PATH_WRONG_TYPE;
			}
		}
	}
	return JT_ERROR;
}

int jt_json_get_int_by_path(jt_access_token_t *at, int *value, const char *format, ...) {
	va_list ap;
	int rv;
	char *path = NULL;
	int ret;

	if (at->transfer.jobj == NULL) {
		LOG_ERROR("Called %s() with invalid jobj.\n", __FUNCTION__);
		return JT_ERROR;
	}

	if (is_error(at->transfer.jobj)) {
		return JT_ERROR;
	}

	va_start(ap, format);
	ret = vasprintf(&path, format, ap);
	va_end(ap);
	if (ret == -1) {
		path = NULL;
		return JT_NO_MEM;
	}
	LOG("%s() path %s jobj %p\n", __FUNCTION__, path, at->transfer.jobj);
	rv = jt_json_sub_get_int_by_path(at, at->transfer.jobj, path, value);
	free(path);
	path = NULL;

	LOG("%s() path %s = %d\n", __FUNCTION__, path, rv);

	return rv;
}

static char *jt_json_get_strdup(jt_access_token_t *at, const char *jsonkey) {
	return jt_strdup(jt_json_get_string(at, jsonkey));
}

static size_t print_httppost_callback(void *arg, const char *buf, size_t len)
{
	FILE *fout = arg;

	fwrite(buf, len, 1, fout);

	return len;
}

static void jt_print_response(jt_access_token_t *at, const char *url, const char *webpage, struct curl_httppost *formpost)
{
	if (at->transfer.jobj == NULL) {
		LOG("URL: %s\n", url);
		LOG("%s\n", webpage);
	} else {
		const char *error;
		const char *description;

		error = jt_json_get_string(at, "error");
		description = jt_json_get_string(at, "error_description");
		if (error != NULL) {
			if (description != NULL) {
				LOG("Response of URL %s is:\n", url);
				LOG("Error: %s, %s\n", error, description);
				if (at->errfd != NULL) {
					curl_formget(formpost, at->errfd, print_httppost_callback);
				}
			} else {
				if ((strcmp(error, "slow_down") != 0) &&
					(strcmp(error, "authorization_pending") != 0)) {
					LOG("Response of URL %s is:\n", url);
					LOG("Error: %s, %s\n", error, description);
					if (at->errfd != NULL) {
						curl_formget(formpost, at->errfd, print_httppost_callback);
					}
					LOG("Error: %s\n", error);
				}
			}
		} else {
			LOG("Response of URL %s is:\n", url);
			LOG("%s\n", webpage);
		}
	}
}

/**
 * Load a JSON object from the given URL.
 * @param at Access token object.
 * @param url Get data from this URL.
 * @param headers HTTP headers to use or NULL.
 * @param formpost Form to post or NULL.
 * @param savefile Save response in binary/text format or NULL.
 *
 * The JSON object is stored at at->transfer.jobj when the return good is JT_OK.
 * @return JT_OK when JSON object was successfully received.
 * @return JT_PROTOCOL_ERROR when an error was detected the error is saved in
 *         at->protocol_error and at->error_description if available.
 * @return JT_TRANSFER_ERROR when an error was detect by CURL see at->transfer.res.
 * @return JT_JASON_PARSE_ERROR if received data was not in JSON format.
 * @return JT_NO_MEM on out of memory.
 */
static int jt_load_json(jt_access_token_t *at, const char *url, struct curl_slist *headers,
	struct curl_httppost *formpost, const char *savefilename)
{
	LOG("%s(): URL %s: savefilename %s\n", __FUNCTION__, url, savefilename);

	if (at->transfer.jobj != NULL) {
		LOG_ERROR("jt_load_json called with jobj.\n");
#ifdef JT_JSON_DEBUG
		json_object_to_file("debug.json", at->transfer.jobj);
#endif
		return JT_JOBJ_NOT_FREE;
	}
	at->transfer.jobj = NULL;

	at->transfer.chunk.memory = NULL;
	at->transfer.chunk.size = 0;
	at->transfer.chunk.at = at;

	if (at->transfer.curl != NULL) {

		curl_easy_setopt(at->transfer.curl, CURLOPT_URL, url);

		if (headers != NULL) {
			curl_easy_setopt(at->transfer.curl, CURLOPT_HTTPHEADER, headers);
			LOG("with header\n");
		} else {
			curl_easy_setopt(at->transfer.curl, CURLOPT_HTTPHEADER, NULL);
			LOG("without header\n");
		}
		if (formpost != NULL) {
			LOG("with formpost\n");
			curl_easy_setopt(at->transfer.curl, CURLOPT_HTTPPOST, formpost);
		} else {
			LOG("without formpost\n");
			curl_easy_setopt(at->transfer.curl, CURLOPT_HTTPPOST, NULL);
			curl_easy_setopt(at->transfer.curl, CURLOPT_POST, 0);
			curl_easy_setopt(at->transfer.curl, CURLOPT_NOBODY, 0);
		}

		/* Transfer data via HTTP or HTTPS. */
		at->transfer.res = curl_easy_perform(at->transfer.curl);
		if (at->transfer.res != CURLE_OK) {
			LOG_ERROR("curl_easy_perform() failed: %s\n",
				curl_easy_strerror(at->transfer.res));
		}

		if (at->transfer.chunk.memory != NULL) {
			char *error;

			/* Save file if filename was given. */
			if (savefilename != NULL) {
				if (at->transfer.chunk.size > 0) {
					jt_save_file(at, savefilename, at->transfer.chunk.memory, at->transfer.chunk.size);
				}
			}
			at->transfer.jobj = json_tokener_parse(at->transfer.chunk.memory);
			if (at->logfd != NULL) {
				jt_print_response(at, url, at->transfer.chunk.memory, formpost);
			}
			free(at->transfer.chunk.memory);
			at->transfer.chunk.memory = NULL;
	
			/* Check if an error happened. */
			error = jt_json_get_strdup(at, "error");
			if (error != NULL) {
				if (at->protocol_error != NULL) {
					free(at->protocol_error);
					at->protocol_error = NULL;
				}
				if (at->error_description != NULL) {
					free(at->error_description);
					at->error_description = NULL;
				}
				at->protocol_error = error;
				at->error_description = jt_json_get_strdup(at, "error_description");
				error = NULL;
				json_object_put(at->transfer.jobj);
				at->transfer.jobj = NULL;
				return JT_PROTOCOL_ERROR;
			}

		}
		if (at->transfer.res != CURLE_OK) {
			at->transfer.jobj = NULL;
			return JT_TRANSFER_ERROR;
		} else if (at->transfer.jobj == NULL) {
			return JT_JASON_PARSE_ERROR;
		} else if (is_error(at->transfer.jobj)) {
			at->transfer.jobj = NULL;
			return JT_JASON_PARSE_ERROR;
		} else {
			return JT_OK;
		}
	}
	return JT_NO_MEM;
}

int jt_update_user_code(jt_access_token_t *at)
{
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	int rv;

	LOG("%s()\n", __FUNCTION__);

	if (at->client_id == NULL) {
		return JT_ERROR_CLIENT_ID;
	}

	if (at->client_secret == NULL) {
		return JT_ERROR_SECRET;
	}

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "client_id", CURLFORM_COPYCONTENTS, at->client_id, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "scope", CURLFORM_COPYCONTENTS, "https://gdata.youtube.com", CURLFORM_END);
	rv = jt_load_json(at, "https://accounts.google.com/o/oauth2/device/code", NULL, formpost, NULL);
	curl_formfree(formpost);
	formpost = NULL;
	if (rv == JT_OK) {
		if (at->device_code != NULL) {
			free(at->device_code);
			at->device_code = NULL;
		}
		at->device_code = jt_json_get_strdup(at, "device_code");

		if (at->verification_url != NULL) {
			free(at->verification_url);
			at->verification_url = NULL;
		}
		at->verification_url = jt_json_get_strdup(at, "verification_url");

		if (at->user_code != NULL) {
			free(at->user_code);
			at->user_code = NULL;
		}
		at->user_code = jt_json_get_strdup(at, "user_code");

		json_object_put(at->transfer.jobj);
		at->transfer.jobj = NULL;

		if (at->device_code == NULL) {
			return JT_CODE_MISSING;
		}
		if (at->verification_url == NULL) {
			return JT_CODE_MISSING;
		}
		if (at->user_code == NULL) {
			return JT_CODE_MISSING;
		}
	}
	return rv;
}

int jt_get_token(jt_access_token_t *at)
{
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	int rv = JT_ERROR;

	LOG("%s()\n", __FUNCTION__);

	if (at->device_code == NULL) {
		return JT_ERROR_DEVICE_CODE;
	}
	if (at->client_id == NULL) {
		return JT_ERROR_CLIENT_ID;
	}
	if (at->client_secret == NULL) {
		return JT_ERROR_SECRET;
	}

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "client_id", CURLFORM_COPYCONTENTS, at->client_id, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "client_secret", CURLFORM_COPYCONTENTS, at->client_secret, CURLFORM_END);
	/* device_code */
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "code", CURLFORM_COPYCONTENTS, at->device_code, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "grant_type", CURLFORM_COPYCONTENTS, "http://oauth.net/grant_type/device/1.0", CURLFORM_END);
	rv = jt_load_json(at, "https://accounts.google.com/o/oauth2/token", NULL, formpost, at->token_file);
	curl_formfree(formpost);
	formpost = NULL;
	lastptr = NULL;
	if (rv == JT_OK) {
		char *access_token = NULL;
		char *token_type = NULL;
		char *refresh_token = NULL;

		access_token = jt_json_get_strdup(at, "access_token");
		if (access_token != NULL) {
			if (at->access_token != NULL) {
				free(at->access_token);
				at->access_token = NULL;
			}
			at->access_token = access_token;
		}
		token_type = jt_json_get_strdup(at, "token_type");
		if (token_type != NULL) {
			if (at->token_type != NULL) {
				free(at->token_type);
				at->token_type = NULL;
			}
			at->token_type = token_type;
		}
		refresh_token = jt_json_get_strdup(at, "refresh_token");
		if (refresh_token != NULL) {
			if (at->refresh_token != NULL) {
				free(at->refresh_token);
				at->refresh_token = NULL;
			}
			at->refresh_token = refresh_token;
		}
		json_object_put(at->transfer.jobj);
		at->transfer.jobj = NULL;
		if (access_token != NULL) {
			/* Got a got that should work. */
			return JT_OK;
		}
		/* There was no access token in the response. */
		return JT_CODE_MISSING;
	} else if (rv == JT_PROTOCOL_ERROR) {
		if (strcmp(at->protocol_error, "authorization_pending") == 0) {
			return JT_AUTH_PENDING;
		} else if (strcmp(at->protocol_error, "slow_down") == 0) {
			return JT_SLOW_DOWN;
		} else if (strcmp(at->protocol_error, "verification_code_expired") == 0) {
			return JT_CODE_EXPIRED;
		}
	}

	return rv;
}

int jt_get_refresh_token(jt_access_token_t *at)
{
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	int rv;

	LOG("%s()\n", __FUNCTION__);

	if (at->client_id == NULL) {
		return JT_ERROR_CLIENT_ID;
	}

	if (at->client_secret == NULL) {
		return JT_ERROR_SECRET;
	}

	if (at->refresh_token == NULL) {
		return JT_ERROR_REFRESH_TOKEN;
	}

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "client_id", CURLFORM_COPYCONTENTS, at->client_id, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "client_secret", CURLFORM_COPYCONTENTS, at->client_secret, CURLFORM_END);
	/* device_code */
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "refresh_token", CURLFORM_COPYCONTENTS, at->refresh_token, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "grant_type", CURLFORM_COPYCONTENTS, "refresh_token", CURLFORM_END);
	rv = jt_load_json(at, "https://accounts.google.com/o/oauth2/token", NULL, formpost, at->refresh_token_file);
	curl_formfree(formpost);
	formpost = NULL;
	if (rv == JT_OK) {
		char *access_token = NULL;
		char *token_type = NULL;

		access_token = jt_json_get_strdup(at, "access_token");
		if (access_token != NULL) {
			if (at->access_token != NULL) {
				free(at->access_token);
				at->access_token = NULL;
			}
			at->access_token = access_token;
		} else {
			rv = JT_AUTH_ERROR;
		}
		token_type = jt_json_get_strdup(at, "token_type");
		if (token_type != NULL) {
			if (at->token_type != NULL) {
				free(at->token_type);
				at->token_type = NULL;
			}
			at->token_type = token_type;
		}
	
		json_object_put(at->transfer.jobj);
		at->transfer.jobj = NULL;
	}
	return rv;
}

static int jt_load_json_refreshing(jt_access_token_t *at,
	struct curl_httppost *formpost, const char *savefilename,
	const char *format, ...)
{
	int rv;
	int retry;
	int ret;
	char *url = NULL;
	va_list ap;

	if (at->access_token == NULL) {
		return JT_ERROR_ACCESS_TOKEN;
	}
	if (at->token_type == NULL) {
		return JT_ERROR_ACCESS_TOKEN;
	}
	if (at->transfer.jobj != NULL) {
		LOG_ERROR("jt_load_json called with jobj.\n");
#ifdef JT_JSON_DEBUG
		json_object_to_file("debug.json", at->transfer.jobj);
#endif
		return JT_JOBJ_NOT_FREE;
	}

	va_start(ap, format);
	ret = vasprintf(&url, format, ap);
	va_end(ap);
	if (ret == -1) {
		url = NULL;
		return JT_NO_MEM;
	}
	LOG("%s() URL: %s\n", __FUNCTION__, url);

	retry = 0;
	do {
		char *token = NULL;
		struct curl_slist *headers = NULL;

		if (at->transfer.jobj != NULL) {
			json_object_put(at->transfer.jobj);
			at->transfer.jobj = NULL;
		}

		ret = asprintf(&token, "Authorization: %s %s", at->token_type, at->access_token);
		if (ret == -1) {
			token = NULL;
			free(url);
			url = NULL;
			return JT_NO_MEM;
		}
		headers = curl_slist_append(headers, token);

		rv = jt_load_json(at, url, headers, formpost, savefilename);

		curl_slist_free_all(headers);
		headers = NULL;
		free(token);
		token = NULL;

		if (rv == JT_OK) {
			const char *error;

			error = jt_json_get_string_by_path(at, "/error/errors[0]/reason");

			if (error != NULL) {
				LOG_ERROR("%s() URL: %s\n", __FUNCTION__, url);
				LOG_ERROR("%s\n", error);
				if (strcmp(error, "authError") == 0) {
					error = NULL;
					json_object_put(at->transfer.jobj);
					at->transfer.jobj = NULL;

					LOG_ERROR("Refreshing token\n");

					rv = jt_get_refresh_token(at);
					if (rv == JT_OK) {
						rv = JT_AUTH_ERROR;
					}
				} else {
					if (at->protocol_error != NULL) {
						free(at->protocol_error);
						at->protocol_error = NULL;
					}
					at->protocol_error = jt_strdup(error);
					rv = JT_PROTOCOL_ERROR;
				}
				error = NULL;
			}
		}
		retry++;
	} while ((rv == JT_AUTH_ERROR) && (retry < 3));

	if (rv != JT_OK) {
		/* Clean up on error. */
		if (at->transfer.jobj != NULL) {
			json_object_put(at->transfer.jobj);
			at->transfer.jobj = NULL;
		}
	}
	free(url);
	url = NULL;
	return rv;
}

int jt_get_my_subscriptions(jt_access_token_t *at, const char *pageToken)
{
	int rv;

	LOG("%s()\n", __FUNCTION__);

	rv = jt_load_json_refreshing(at, NULL, "subscriptions.json",
		"https://www.googleapis.com/youtube/v3/subscriptions?part=snippet&mine=true&maxResults=1&pageToken=%s",
		pageToken);

	return rv;
}

int jt_get_channels(jt_access_token_t *at, const char *channelId, const char *pageToken)
{
	int rv;

	LOG("%s()\n", __FUNCTION__);

	rv = jt_load_json_refreshing(at, NULL, "channels.json",
		"https://www.googleapis.com/youtube/v3/channels?part=snippet%%2CcontentDetails&id=%s&pageToken=%s",
		channelId,
		pageToken);

	return rv;
}

int jt_get_my_channels(jt_access_token_t *at, const char *pageToken)
{
	int rv;

	LOG("%s()\n", __FUNCTION__);

	rv = jt_load_json_refreshing(at, NULL, "mychannels.json",
		"https://www.googleapis.com/youtube/v3/channels?part=snippet%%2CcontentDetails&mine=true&pageToken=%s",
		pageToken);

	return rv;
}

int jt_get_playlist(jt_access_token_t *at, const char *playlistid, const char *pageToken)
{
	int rv;

	LOG("%s()\n", __FUNCTION__);

	rv = jt_load_json_refreshing(at, NULL, "playlist.json",
		"https://www.googleapis.com/youtube/v3/playlists?part=snippet&id=%s&maxResults=50&pageToken=%s",
		playlistid,
		pageToken);

	return rv;
}

int jt_get_my_playlist(jt_access_token_t *at, const char *pageToken)
{
	int rv;

	LOG("%s()\n", __FUNCTION__);

	rv = jt_load_json_refreshing(at, NULL, "myplaylist.json",
		"https://www.googleapis.com/youtube/v3/playlists?part=snippet&mine=true&maxResults=1&pageToken=%s",
		pageToken);

	return rv;
}

int jt_get_playlist_items(jt_access_token_t *at, const char *playlistid, const char *pageToken)
{
	int rv;

	LOG("%s()\n", __FUNCTION__);

	rv = jt_load_json_refreshing(at, NULL, "playlistitem.json",
		"https://www.googleapis.com/youtube/v3/playlistItems?part=snippet%%2CcontentDetails&playlistId=%s&maxResults=20&pageToken=%s",
		playlistid,
		pageToken);

	return rv;
}

static int jt_load_token_file(jt_access_token_t *at, const char *filename)
{
	void *mem;
	char *access_token = NULL;
	char *token_type = NULL;
	char *refresh_token = NULL;

	LOG("%s() filename %s\n", __FUNCTION__, filename);

	if (at->transfer.jobj != NULL) {
		LOG_ERROR("jt_load_token_file called with jobj.\n");
#ifdef JT_JSON_DEBUG
		json_object_to_file("debug.json", at->transfer.jobj);
#endif
		return JT_JOBJ_NOT_FREE;
	}

	mem = jt_load_file(at, filename);
	if (mem == NULL) {
		return JT_NO_MEM;
	}
	if (mem == ((void *) -1)) {
		return JT_FILE_ERROR;
	}
	at->transfer.jobj = json_tokener_parse(mem);
	if (at->transfer.jobj == NULL) {
		return JT_JASON_PARSE_ERROR;
	} else if (is_error(at->transfer.jobj)) {
		at->transfer.jobj = NULL;
		return JT_JASON_PARSE_ERROR;
	}
	access_token = jt_json_get_strdup(at, "access_token");
	if (access_token != NULL) {
		if (at->access_token != NULL) {
			free(at->access_token);
			at->access_token = NULL;
		}
		at->access_token = access_token;
	}
	token_type = jt_json_get_strdup(at, "token_type");
	if (token_type != NULL) {
		if (at->token_type != NULL) {
			free(at->token_type);
			at->token_type = NULL;
		}
		at->token_type = token_type;
	}
	refresh_token = jt_json_get_strdup(at, "refresh_token");
	if (refresh_token != NULL) {
		if (at->refresh_token != NULL) {
			free(at->refresh_token);
			at->refresh_token = NULL;
		}
		at->refresh_token = refresh_token;
	}
	json_object_put(at->transfer.jobj);
	at->transfer.jobj = NULL;
	free(mem);
	mem = NULL;
	return JT_OK;
}

int jt_load_token(jt_access_token_t *at)
{
	int rv = JT_NO_TOKEN_FILE;

	LOG("%s()\n", __FUNCTION__);

	if (at->token_file != NULL) {
		/* Load saved token. */
		rv = jt_load_token_file(at, at->token_file);
	}

	if (rv == JT_OK) {
		if (at->refresh_token_file != NULL) {
			/* Check if there is an updated access token. */
			rv = jt_load_token_file(at, at->refresh_token_file);
		}
		if (at->access_token == NULL) {
			rv = JT_CODE_MISSING;
		} else {
			rv = JT_OK;
		}
	}
	return rv;
}

const char *jt_get_user_code(jt_access_token_t *at)
{
	return at->user_code;
}

const char *jt_get_verification_url(jt_access_token_t *at)
{
	return at->verification_url;
}

const char *jt_get_protocol_error(jt_access_token_t *at)
{
	return at->protocol_error;
}

const char *jt_get_error_description(jt_access_token_t *at)
{
	return at->error_description;
}

CURLcode jt_get_transfer_error(jt_access_token_t *at)
{
	return at->transfer.res;
}

const char *jt_get_error_code(int rv)
{
	switch (rv) {
		CONVCASETOTEXT(JT_OK)
		CONVCASETOTEXT(JT_ERROR)
		CONVCASETOTEXT(JT_ERROR_DEVICE_CODE)
		CONVCASETOTEXT(JT_PROTOCOL_ERROR)
		CONVCASETOTEXT(JT_TRANSFER_ERROR)
		CONVCASETOTEXT(JT_JOBJ_NOT_FREE)
		CONVCASETOTEXT(JT_NO_MEM)
		CONVCASETOTEXT(JT_JASON_PARSE_ERROR)
		CONVCASETOTEXT(JT_CODE_MISSING)
		CONVCASETOTEXT(JT_FILE_ERROR)
		CONVCASETOTEXT(JT_ERROR_CLIENT_ID)
		CONVCASETOTEXT(JT_ERROR_SECRET)
		CONVCASETOTEXT(JT_NO_TOKEN_FILE)
		CONVCASETOTEXT(JT_AUTH_PENDING)
		CONVCASETOTEXT(JT_SLOW_DOWN)
		CONVCASETOTEXT(JT_CODE_EXPIRED)
		CONVCASETOTEXT(JT_ERROR_REFRESH_TOKEN)
		CONVCASETOTEXT(JT_AUTH_ERROR)
		CONVCASETOTEXT(JT_ERROR_ACCESS_TOKEN)
		CONVCASETOTEXT(JT_PATH_BAD_ARRAY)
		CONVCASETOTEXT(JT_PATH_TOO_LONG)
		CONVCASETOTEXT(JT_PATH_TOO_SHORT)
		CONVCASETOTEXT(JT_PATH_WRONG_TYPE)
		default:
			return "unknown error code";
	}
}

int jt_free_transfer(jt_access_token_t *at)
{
	if (at->transfer.jobj != NULL) {
		json_object_put(at->transfer.jobj);
		at->transfer.jobj = NULL;
		return JT_OK;
	} else {
		return JT_ERROR;
	}
}
