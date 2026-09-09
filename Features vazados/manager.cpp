#include <features/manager.hpp>

namespace Features {

	void Start() {
		bool exp = false;
		if (!g_started.compare_exchange_strong(exp, true)) return;

		Sys::CreateDetachedThread([](void*) { Trigger::Work(); });
		Sys::CreateDetachedThread([](void*) { AimBot::Work(); });
		Sys::CreateDetachedThread([](void*) { BombTimer::Work(); });
	}
}
