#include "core/error/error_list.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstring>

// ===========================================================================
// Error enum
// ===========================================================================

TEST_CASE("Error: OK is zero")
{
    CHECK(OK == 0);
}

TEST_CASE("Error: enum values are sequential")
{
    CHECK(FAILED == 1);
    CHECK(ERR_MAX == 2);
}

// ===========================================================================
// error_names table
// ===========================================================================

TEST_CASE("error_names: entries up to ERR_MAX are accessible")
{
    for (int i = 0; i < ERR_MAX; ++i)
    {
        CHECK(error_names[i] != nullptr);
    }
}

TEST_CASE("error_names: strings are correct")
{
    CHECK(std::strcmp(error_names[OK], "OK") == 0);
    CHECK(std::strcmp(error_names[FAILED], "Failed") == 0);
}
