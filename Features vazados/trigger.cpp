#include <features/trigger.hpp>

namespace Features::Trigger {
	using namespace Context;

	void Tick() {
		if (!Config::GameSettings.triggerbot) return;
		if (ClientDLL.load() == 0 || !Memory->IsAttached()) return;
		// Gate global: nao dispara se o cursor do Windows ta visivel (menu/
		// scoreboard/console/dead) — SendInput moveria desktop cursor.
		if (IsCursorVisible()) return;
		if (!(LI_CACHED(GetAsyncKeyState)(Config::GameSettings.triggerKey) & 0x8000)) return;
		if (std::fabs(LocalVelocity.z) > MAX_Z_VELOCITY) return;

		DWORD now = Sys::TickCount();
		if (now - g_lastTrigger < MIN_TRIGGER_INTERVAL) return;

		uintptr_t pawn = LocalPlayerPawn.load();
		if (!pawn || !Schema.m_iIDEntIndex) return;

		int crosshairIdx = Memory->Read<int>(pawn + Schema.m_iIDEntIndex);
		if (crosshairIdx < 0) return;

		bool isEnemy = false;
		{
			std::lock_guard<std::mutex> lock(EntitiesMutex);
			for (const auto& e : Entities) {
				if (static_cast<int>(e.index) == crosshairIdx && !e.isLocal && e.health > 0 &&
					(!Config::GameSettings.teamCheck || !e.isTeam)) {
					isEnemy = true;
					break;
				}
			}
		}
		if (!isEnemy) return;

		Sys::Sleep(static_cast<DWORD>(Config::GameSettings.triggerDelay));

		AimBot::InitSendInput();
		if (!AimBot::g_pSendInput) return;

		INPUT in{};
		in.type = INPUT_MOUSE;
		in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		AimBot::g_pSendInput(1, &in, sizeof(in));
		Sys::Sleep(10);
		in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		AimBot::g_pSendInput(1, &in, sizeof(in));

		g_lastTrigger = Sys::TickCount();
	}

	void Work() {
		while (true) {
			Sys::Sleep(5);
			if (ClientDLL.load() == 0) return;
			Tick();
		}
	}
}
