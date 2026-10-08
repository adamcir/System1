#ifndef SYSTEM_SMOD_H
#define SYSTEM_SMOD_H

#include "types.h"

/* Native System Module (SMOD) revision 1; not an ELF container. */
#define SMOD_FORMAT_VERSION 1u
#define SMOD_API_VERSION 1u
#define SMOD_ARCH_I386 1u
#define SMOD_ARCH_X86_64 2u
#define SMOD_FLAG_EXECUTABLE 1u
#define SMOD_HEADER_SIZE 32u
#define SMOD_IMAGE_LIMIT 4096u

/* Public, versioned API passed to each module's entrypoint.
 * Future revisions can add callbacks without importing kernel symbols.
 */
typedef struct {
    uint32_t abi_version;
    void (*report_ready)(void);
} smod_api_v1_t;

typedef int (*smod_entry_fn_t)(const smod_api_v1_t* api);

#endif
