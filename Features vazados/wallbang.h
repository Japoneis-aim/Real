#pragma once
#include <imgui.h>
#include <cmath>
#include <core/context.h>
#include <core/config.h>

namespace Features::Wallbang {
	using namespace Context;

	constexpr float MAX_PENETRATION = 40.0f;
	constexpr float RAY_LENGTH = 8192.0f;
	constexpr float PROBE_STEP = 0.5f;
	constexpr float EPSILON = 0.3f;

	inline Vec3 AngleToDir(float pitch, float yaw) {
		float cp = std::cos(pitch * 0.01745329f);
		float sp = std::sin(pitch * 0.01745329f);
		float cy = std::cos(yaw * 0.01745329f);
		float sy = std::sin(yaw * 0.01745329f);
		return { cp * cy, cp * sy, -sp };
	}

	enum class Surface { Air, Penetrable, Solid };

	struct Result {
		Surface type = Surface::Air;
		float thickness = 0.0f;
	};

	inline Result Check() {
		Result r;
		if (!MapBVHReady.load()) return r;

		Vec3 dir = AngleToDir(LocalAimAngles.x, LocalAimAngles.y);
		Vec3 end = LocalEyePos + dir * RAY_LENGTH;

		auto hit = MapBVH.TraceRay(LocalEyePos, end);
		if (!hit.hit) return r;

		Vec3 probeStart = hit.endPos + dir * EPSILON;

		for (float d = PROBE_STEP; d <= MAX_PENETRATION; d += PROBE_STEP) {
			Vec3 test = probeStart + dir * d;
			auto probe = MapBVH.TraceRay(test, test + dir * 0.1f);
			if (!probe.hit) {
				auto verify = MapBVH.TraceRay(test, test + dir * 2.0f);
				if (!verify.hit) {
					r.type = Surface::Penetrable;
					r.thickness = d;
					return r;
				}
			}
		}

		r.type = Surface::Solid;
		r.thickness = MAX_PENETRATION;
		return r;
	}

	inline void Draw() {
		if (!Config::GameSettings.wallbang) return;
		if (ClientDLL.load() == 0 || !MapBVHReady.load()) return;

		auto res = Check();
		if (res.type == Surface::Air) return;

		auto dl = ImGui::GetBackgroundDrawList();
		float cx = ScreenWidth * 0.5f;
		float cy = ScreenHeight * 0.5f;

		if (res.type == Surface::Penetrable) {
			float t = res.thickness / MAX_PENETRATION;
			uint8_t g = static_cast<uint8_t>(255 * (1.0f - t));
			uint8_t r = static_cast<uint8_t>(255 * t);
			ImU32 col = IM_COL32(r, g, 0, 200);

			dl->AddCircle({ cx, cy }, 12.0f, col, 20, 2.0f);

			float barW = 30.0f;
			float barH = 3.0f;
			float barX = cx - barW * 0.5f;
			float barY = cy + 16.0f;
			dl->AddRectFilled({ barX, barY }, { barX + barW, barY + barH },
				IM_COL32(20, 20, 20, 180));
			dl->AddRectFilled({ barX, barY }, { barX + barW * (1.0f - t), barY + barH }, col);

			char buf[16];
			snprintf(buf, sizeof(buf), "%.0f", res.thickness);
			ImVec2 sz = ImGui::CalcTextSize(buf);
			dl->AddText({ cx - sz.x * 0.5f, barY + 5.0f }, IM_COL32(200, 200, 200, 200), buf);
		} else {
			dl->AddCircle({ cx, cy }, 12.0f, IM_COL32(255, 40, 40, 150), 20, 1.5f);
		}
	}
}
