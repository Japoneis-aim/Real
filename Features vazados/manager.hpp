#pragma once
#include <atomic>
#include <nt/wrap.h>
#include <features/trigger.hpp>
#include <features/aimbot.hpp>
#include <features/bombtimer.hpp>

namespace Features {

	inline std::atomic<bool> g_started = false;

	void Start();

	inline void Stop() {
		g_started.store(false);
	}
}
