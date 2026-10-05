#include "sprg.h"
#include "sprg_core.h"

int sprg_validate_file(const char* path, uint32_t expected_arch, sprg_image_t* out_image) {
    return sprg_core_validate_file(path, expected_arch, out_image);
}
