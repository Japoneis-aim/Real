#pragma once
#include <string>
#include <atomic>
#include <chrono>

namespace UI::Profile {
    inline std::string Username;
    inline std::string Role;
    inline std::string ExpiryText;
    inline std::atomic<bool> Authenticated{ false };
    // Watchdog state — preenchido apos auth bem sucedido, lido em loop pelo
    // WatchdogTick pra forcar logout/shutdown se a licenca expirar.
    inline std::chrono::system_clock::time_point ExpiresAt{};
    inline std::atomic<bool> HasExpiry{ false };
}
