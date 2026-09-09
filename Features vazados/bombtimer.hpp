#pragma once
#include <imgui.h>
#include <cmath>
#include <core/context.h>
#include <core/config.h>
#include <nt/wrap.h>

namespace Features::BombTimer {
	using namespace Context;

	struct BombState {
		bool planted = false;
		bool defused = false;
		bool defusing = false;
		float timeLeft = 0.0f;
		float defuseLeft = 0.0f;
		int site = 0;
	};

	inline BombState g_bomb{};

	void Tick();
	void Draw();
	void Work();
}
