#pragma once

#include <stdint.h>

namespace RidenDongle {

struct FirmwareUpdateState {
    bool started = false;
    bool finished = false;
    bool failed = false;
    bool success = false;
    bool restart_pending = false;
    uint32_t received_bytes = 0;
    uint32_t written_bytes = 0;
    uint32_t restart_requested_ms = 0;
};

inline FirmwareUpdateState &firmware_update_state()
{
    static FirmwareUpdateState state;
    return state;
}

} // namespace RidenDongle
