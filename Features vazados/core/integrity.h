#pragma once
#include <atomic>
#include <nt/wrap.h>
#include <vmp.h>

namespace Integrity {

    inline std::atomic<uint32_t> g_crcHeartbeat = 0;
    inline std::atomic<uint32_t> g_mainHeartbeat = 0;
    inline std::atomic<bool> g_alive = true;

    inline void Die() {
        return;
    }

    inline void CrcThread(void*) {
        VMP_BEGIN_ULTRA("crc_loop");
        while (g_alive.load()) {
            Sys::Sleep(3000 + (g_crcHeartbeat.load() % 2000));

#ifdef VMP_ENABLED
            if (!VMProtectIsValidImageCRC()) Die();
            if (VMProtectIsDebuggerPresent(true)) Die();
#endif

            g_crcHeartbeat.fetch_add(1);

            uint32_t mainBeat = g_mainHeartbeat.load();
            static uint32_t lastMainBeat = 0;
            static int mainStaleCount = 0;
            if (mainBeat == lastMainBeat)
                mainStaleCount++;
            else
                mainStaleCount = 0;
            lastMainBeat = mainBeat;

            if (mainStaleCount > 5) Die();
        }
        VMP_END;
    }

    inline void CheckCrcAlive() {
        static uint32_t lastBeat = 0;
        static int staleCount = 0;

        uint32_t beat = g_crcHeartbeat.load();
        if (beat == lastBeat)
            staleCount++;
        else
            staleCount = 0;
        lastBeat = beat;

        if (staleCount > 10) Die();
    }

    inline void TickMain() {
        g_mainHeartbeat.fetch_add(1);
    }

    inline void Start() {
    }
}
