#pragma once

#include "../core/logger.hpp"

namespace app {

class SessionSleepGuard final {
public:
    explicit SessionSleepGuard(core::Logger& logger);
    ~SessionSleepGuard();

    SessionSleepGuard(const SessionSleepGuard&) = delete;
    SessionSleepGuard& operator=(const SessionSleepGuard&) = delete;

private:
    bool active_ = false;
};

// This is an intentional persistent setup policy. It disables hibernation and,
// as a consequence, Windows Fast Startup. Normal sleep remains available.
bool disableHibernationAndFastStartup(core::Logger& logger);

} // namespace app
