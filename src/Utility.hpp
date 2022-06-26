#pragma once

#include <Utilities/Utilities.hpp>
#include <Network/Network.hpp>

#define VERIFY(x) \
    if (!(x))     \
        SC_APP_ERROR("ERROR READING/WRITING BUFFER");