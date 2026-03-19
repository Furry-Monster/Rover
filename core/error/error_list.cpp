#include "error_list.h"

#include "core/typedefs.h"

const char* error_names[] = {
    "OK",
    "Failed",
};

static_assert(std_size(error_names) == Error::ERR_MAX);
