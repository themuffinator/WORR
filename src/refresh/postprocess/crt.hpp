#pragma once

#include "../gl.hpp"

[[nodiscard]] bool R_CRTEnabled() noexcept;

glStateBits_t R_CRTPrepare(glStateBits_t bits, int viewportWidth, int viewportHeight);
