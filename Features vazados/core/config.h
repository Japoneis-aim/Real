#pragma once
#include <Windows.h>
#include <atomic>
#include <imgui.h>

namespace Config {
	inline constexpr float UNITS_PER_METER = 39.37f;

	inline std::atomic<HWND>& GameWindow() {
		static std::atomic<HWND> hwnd(nullptr);
		return hwnd;
	}

	enum Tab {
		Login,
		Waiting,
		Menu
	};

	namespace detail {
		inline uint32_t TabKey() {
			static uint32_t key = static_cast<uint32_t>(
				reinterpret_cast<uintptr_t>(&key) >> 4) ^ 0xA7B3C1D9u;
			return key;
		}
		inline std::atomic<uint32_t>& RawTab() {
			static std::atomic<uint32_t> v(static_cast<uint32_t>(Tab::Login) ^ TabKey());
			return v;
		}
	}

	struct TabAccessor {
		Tab load() const { return static_cast<Tab>(detail::RawTab().load() ^ detail::TabKey()); }
		void store(Tab t) { detail::RawTab().store(static_cast<uint32_t>(t) ^ detail::TabKey()); }
	};

	inline TabAccessor& CurrentTab() {
		static TabAccessor accessor;
		return accessor;
	}

	inline std::atomic<bool>& MenuVisible() {
		static std::atomic<bool> visible(true);
		return visible;
	}

	inline int MenuKey = VK_INSERT;
	inline std::atomic<bool>& ShutdownRequested() {
		static std::atomic<bool> v(false);
		return v;
	}

	struct Color4 {
		float r, g, b, a;
		float* data() { return &r; }
		ImU32 ToImU32() const {
			return IM_COL32(
				(int)(r * 255), (int)(g * 255),
				(int)(b * 255), (int)(a * 255));
		}
	};

	struct GameSettingsData {
		bool teamCheck = true;
		bool aimbot = false;
		float aimFov = 120.0f;
		float aimSmooth = 50.0f;
		int aimBone = 0;
		int aimKey = VK_XBUTTON2; // Mouse 5 (botao polegar frontal)
		bool aimRequireVisible = true;
		bool aimDynamic = true;
		bool aimHighlight = true;
		bool aimFovCircle = false;
		bool aimHumanize = true;
		bool aimAssistMode = false;
		float aimDistance = 200.0f;

		bool rcs = false;
		float rcsStrength = 70.0f;

		bool triggerbot = false;
		int triggerKey = VK_XBUTTON2;
		int triggerDelay = 50;

		bool esp = true;
		bool espBox = true;
		bool espHealth = true;
		bool espName = true;
		bool espSkeleton = false;
		bool espWeapon = true;
		bool espFlags = true;
		bool espLookDir = false;
		float espDistance = 200.0f;

		bool wallbang = false;
		bool bombTimer = true;
		bool webRadar = false;
		int webRadarPort = 8888;
		bool nadePrediction = true;
		bool streamMode = false;
		bool noFlash = false;
		bool discordRpc = true;
		int fpsLimit = 0;

		Color4 colEnemy        = { 1.0f, 0.3f, 0.3f, 0.9f };
		Color4 colEnemyVisible = { 0.30f, 0.95f, 0.40f, 0.95f };
		Color4 colEnemyHidden  = { 1.00f, 0.30f, 0.30f, 0.85f };
		Color4 colTeam         = { 0.25f, 0.6f, 1.0f, 0.9f };
		Color4 colTarget       = { 1.0f, 0.8f, 0.0f, 1.0f };
		Color4 colDynamic      = { 1.0f, 0.15f, 0.15f, 1.0f }; // osso dinamico — vermelho
		Color4 colSkeleton     = { 1.0f, 1.0f, 1.0f, 0.8f };
		Color4 colLookDir      = { 1.0f, 1.0f, 0.0f, 0.4f };
		Color4 colFovCircle    = { 1.0f, 1.0f, 1.0f, 0.6f };
		Color4 colBox          = { 1.0f, 1.0f, 1.0f, 0.0f }; // alpha=0 -> usa cor dinamica de team/vis
		Color4 colName         = { 0.90f, 0.90f, 0.90f, 1.00f };
		Color4 colWeapon       = { 0.78f, 0.78f, 0.78f, 1.00f };
		Color4 colBomb         = { 1.00f, 0.31f, 0.31f, 1.00f };
		Color4 colNade         = { 1.00f, 1.00f, 0.39f, 1.00f };
	};
	inline GameSettingsData GameSettings;
}
