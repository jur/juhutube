#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "log.h"
#include "transfer.h"


struct transfer_s {
	CURL *curl;
};

/** Needed to load web content to memory. */
struct transfer_chunk_s {
	char *memory;
	size_t size;
};

typedef struct transfer_chunk_s transfer_chunk_t;

/** Callback for loading web content via CURL to memory. */
static size_t mem_callback(void *contents, size_t size, size_t nmemb,
	void *userp)
{
	size_t realsize = size * nmemb;
	transfer_chunk_t *mem = userp;

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

void transfer_init(void)
{
	/* Initialize CURL. This used to transfer the web content. */
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

void transfer_cleanup(void)
{
	curl_global_cleanup();
}

/** Get CURL object for transfering web content. */
transfer_t *transfer_alloc(void)
{
	transfer_t *transfer;
	const char *capath;

	transfer = malloc(sizeof(*transfer));
	if (transfer == NULL) {
		return NULL;
	}
	memset(transfer, 0, sizeof(*transfer));

	transfer->curl = curl_easy_init();
	if (transfer->curl == NULL) {
		LOG_ERROR("Failed to get CURL object. Out of memory?\n");
		return NULL;
	}
	curl_easy_setopt(transfer->curl, CURLOPT_WRITEFUNCTION, mem_callback);
	curl_easy_setopt(transfer->curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
#ifdef NOVERIFYCERT
	curl_easy_setopt(transfer->curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

	capath = getenv("SSL_CERT_PATH");
	if (capath != NULL) {
		curl_easy_setopt(transfer->curl, CURLOPT_CAPATH, capath);
	}
	capath = getenv("SSL_CERT_FILE");
	if (capath != NULL) {
		curl_easy_setopt(transfer->curl, CURLOPT_CAINFO, capath);
	}

	return transfer;
}

/** Free CURL object after use. */
void transfer_free(transfer_t *transfer)
{
	if (transfer != NULL) {
		if (transfer->curl != NULL) {
			curl_easy_cleanup(transfer->curl);
			transfer->curl = NULL;
		}
		free(transfer);
		transfer = NULL;
	}
}

/** Load binary data via URL from the internet. */
size_t transfer_binary(transfer_t *transfer, const char *url, void **mem)
{
	CURLcode res;
	transfer_chunk_t chunk;

	chunk.memory = NULL;
	chunk.size = 0;

	curl_easy_setopt(transfer->curl, CURLOPT_URL, url);
	curl_easy_setopt(transfer->curl, CURLOPT_WRITEDATA, (void *)&chunk);

	res = curl_easy_perform(transfer->curl);
	if (res != CURLE_OK) {
		LOG_ERROR("curl_easy_perform() failed: %d %s url %s\n",
			res, curl_easy_strerror(res), url);
		chunk.size = 0;
	}

	if (chunk.size <= 0) {
		if (chunk.memory != NULL) {
			free(chunk.memory);
			chunk.memory = NULL;
		}
	} else {
		*mem = chunk.memory;
	}

	return chunk.size;
}

