#ifndef SYSTEM1_X86_64_SPRG_H
#define SYSTEM1_X86_64_SPRG_H

#include "sprg_core.h"

int sprg_validate_file(const char* path, uint32_t expected_arch, sprg_image_t* out_image);

#endif
