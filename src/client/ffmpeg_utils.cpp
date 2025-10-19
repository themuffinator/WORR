#include "ffmpeg_utils.h"

#ifdef USE_AVCODEC

#include <array>

extern "C" {
#include <libavutil/error.h>
}

std::string AvErrorString(int err)
{
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buf{};
    return av_make_error_string(buf.data(), buf.size(), err);
}

#endif // USE_AVCODEC
