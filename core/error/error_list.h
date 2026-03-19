#pragma once

enum Error
{
    OK = 0, // (0)
    FAILED, // Generic failure
    // Add more error codes here...
    ERR_MAX, // not being used , represent the maximum error code
};

extern const char* error_names[];
