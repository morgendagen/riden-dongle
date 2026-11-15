#pragma once

#include <stdint.h>

namespace RidenDongle {

struct WebDiagnostics {
    uint32_t status_read_failures = 0;
    uint32_t toggle_requests = 0;
    uint32_t toggle_state_read_failures = 0;
    uint32_t toggle_write_ack_failures = 0;
    uint32_t toggle_reconciled_writes = 0;
    uint32_t toggle_unconfirmed_writes = 0;
};

inline WebDiagnostics &web_diagnostics()
{
    static WebDiagnostics diagnostics;
    return diagnostics;
}

} // namespace RidenDongle
