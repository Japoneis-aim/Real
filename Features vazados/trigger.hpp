#pragma once
#include <Windows.h>
#include <cmath>
#include <core/context.h>
#include <core/config.h>
#include <nt/wrap.h>
#include <features/aimbot.hpp>

namespace Features::Trigger {
	using namespace Context;

	constexpr float MAX_Z_VELOCITY = 18.0f;
	constexpr DWORD MIN_TRIGGER_INTERVAL = 50;

	inline DWORD g_lastTrigger = 0;

	void Tick();
	void Work();
}
