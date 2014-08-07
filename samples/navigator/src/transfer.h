#ifndef _TRANSFER_H_
#define _TRANSFER_H_

struct transfer_s;

typedef struct transfer_s transfer_t;

void transfer_init(void);
void transfer_cleanup(void);
transfer_t *transfer_alloc(void);
void transfer_free(transfer_t *transfer);
size_t transfer_binary(transfer_t *transfer, const char *url, void **mem);

#endif
