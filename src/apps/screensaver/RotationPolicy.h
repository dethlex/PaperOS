#pragma once
#include <stdint.h>

namespace paperos {

// True when the lock-screen wallpaper is due for rotation. `last_rot_unix == 0`
// means "never rotated since cold boot" → rotate now. A clock that moved
// backwards (NTP resync) also rotates — harmless, unlike being stuck.
// rotate_min == 0 falls back to 30 (same sanity default as weather refresh).
inline bool rotationDue(uint32_t now_unix, uint32_t last_rot_unix,
                        uint16_t rotate_min) {
    if (rotate_min == 0) rotate_min = 30;
    if (last_rot_unix == 0 || now_unix < last_rot_unix) return true;
    return (now_unix - last_rot_unix) >= static_cast<uint32_t>(rotate_min) * 60U;
}

} // namespace paperos
