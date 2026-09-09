#pragma once
#include <LazyDlls/Lazyimporter.hpp>
#include <imgui.h>
#include <vector>
#include <mutex>
#include <cmath>
#include <math/w2s.h>
#include <core/context.h>
#include <core/config.h>

namespace Features::Nade {
	using namespace Context;

	constexpr float GRAVITY = 800.f;
	constexpr float TICK_INTERVAL = 1.f / 64.f;
	constexpr int MAX_STEPS = 500;
	constexpr float ELASTICITY = 0.45f;
	constexpr float HULL_RADIUS = 2.f;
	constexpr float STOP_SPEED_SQ = 400.f;

	constexpr uint16_t WPN_FLASH = 43, WPN_HE = 44, WPN_SMOKE = 45;
	constexpr uint16_t WPN_MOLOTOV = 46, WPN_DECOY = 47, WPN_INC = 48;

	struct NadePath {
		std::vector<Vec3> pts;
		bool valid = false;
	};

	// g_path eh escrito pelo Nade::Work() thread (Simulate) e lido pelo render
	// thread (Draw). Sem mutex o vector pode realocar enquanto o render itera
	// → crash.
	inline NadePath g_path;
	inline std::mutex g_pathMutex;

	inline bool IsNade(uint16_t id) {
		return id >= WPN_FLASH && id <= WPN_INC;
	}

	inline bool IsMolotov(uint16_t id) {
		return id == WPN_MOLOTOV || id == WPN_INC;
	}

	inline bool ShouldDetonate(uint16_t id, const Vec3& vel, int tick) {
		float elapsed;
		switch (id) {
		case WPN_SMOKE:
		case WPN_DECOY: {
			float sp2d = std::sqrt(vel.x * vel.x + vel.y * vel.y);
			int checkTicks = (int)(0.2f / TICK_INTERVAL);
			return sp2d < (id == WPN_DECOY ? 0.2f : 0.1f) && (tick % checkTicks) == 0;
		}
		case WPN_MOLOTOV:
		case WPN_INC:
			return (float)tick * TICK_INTERVAL > 2.0f;
		case WPN_FLASH:
		case WPN_HE:
			return (float)(tick - 8) * TICK_INTERVAL > 1.5f;
		default: return false;
		}
	}

	inline Vec3 ClipVelocity(const Vec3& vel, const Vec3& normal, float overbounce) {
		float backoff = vel.Dot(normal) * overbounce;
		Vec3 out = { vel.x - normal.x * backoff, vel.y - normal.y * backoff, vel.z - normal.z * backoff };
		if (std::fabs(out.x) < 0.1f) out.x = 0.f;
		if (std::fabs(out.y) < 0.1f) out.y = 0.f;
		if (std::fabs(out.z) < 0.1f) out.z = 0.f;
		return out;
	}

	inline void Simulate() {
		// Early gates BEFORE locking — quando nao tem nade na mao, basicamente
		// custo zero. So depois decide se vai computar / atualizar g_path.
		auto invalidate = []() {
			std::lock_guard<std::mutex> lock(g_pathMutex);
			g_path.valid = false;
		};
		if (!Config::GameSettings.nadePrediction) { invalidate(); return; }
		if (ClientDLL.load() == 0) { invalidate(); return; }

		uint16_t weapId = LocalWeaponId.load();
		if (!IsNade(weapId)) { invalidate(); return; }

		bool lmb = (LI_CACHED(GetAsyncKeyState)(VK_LBUTTON) & 0x8000) != 0;
		bool rmb = (LI_CACHED(GetAsyncKeyState)(VK_RBUTTON) & 0x8000) != 0;
		if (!lmb && !rmb) { invalidate(); return; }

		Vec3 eyePos = LocalEyePos;
		Vec3 angles = LocalAimAngles;

		float pitch = angles.x;
		if (pitch > 90.f) pitch -= 360.f;
		else if (pitch < -90.f) pitch += 360.f;
		pitch -= (90.f - std::fabs(pitch)) * 10.f / 90.f;

		float pr = pitch * 0.01745329f;
		float yr = angles.y * 0.01745329f;
		float cp = std::cos(pr), sp = std::sin(pr);
		float cy = std::cos(yr), sy = std::sin(yr);
		Vec3 forward = { cp * cy, cp * sy, -sp };

		float throwStrength = 1.0f;

		auto base = ClientDLL.load();
		uintptr_t pawn = LocalPlayerPawn.load();
		if (pawn && Schema.m_pWeaponServices && Schema.m_hActiveWeapon) {
			uintptr_t ws = Memory->Read<uintptr_t>(pawn + Schema.m_pWeaponServices);
			if (ws) {
				uint32_t wh = Memory->Read<uint32_t>(ws + Schema.m_hActiveWeapon);
				if (wh) {
					uintptr_t el = Memory->Read<uintptr_t>(base + Offsets.dwEntityList);
					if (el) {
						uintptr_t le = Memory->Read<uintptr_t>(el + ((wh & 0x7FFF) >> 9) * 8 + 16);
						if (le) {
							uintptr_t w = Memory->Read<uintptr_t>(le + (wh & 0x1FF) * 112);
							if (w && Schema.m_flThrowStrength)
								throwStrength = Memory->Read<float>(w + Schema.m_flThrowStrength);
						}
					}
				}
			}
		}
		if (throwStrength < 0.f || throwStrength > 1.f) throwStrength = 1.f;

		Vec3 spawnPos = eyePos;
		spawnPos.z += throwStrength * 12.f - 12.f;

		Vec3 origin = spawnPos + forward * 16.f;
		float throwVel = 750.f * 0.9f;
		float throwSpeed = (throwStrength * 0.7f + 0.3f) * throwVel;

		Vec3 vel = forward * throwSpeed;
		vel = vel + LocalVelocity * 1.25f;

		Vec3 pos = origin;
		NadePath path;
		path.pts.reserve(MAX_STEPS);
		bool hasBVH = MapBVHReady.load();
		int bounceCount = 0;
		int tickTimer = 0;

		for (int tick = 0; tick < MAX_STEPS; tick++) {
			if (tickTimer == 0)
				path.pts.push_back(pos);

			float newVelZ = vel.z - GRAVITY * TICK_INTERVAL;
			Vec3 move = {
				vel.x * TICK_INTERVAL,
				vel.y * TICK_INTERVAL,
				(vel.z + newVelZ) * 0.5f * TICK_INTERVAL
			};
			vel.z = newVelZ;

			Vec3 newPos = pos + move;
			bool hit = false;
			Vec3 hitNormal = {};
			float hitFrac = 1.f;

			if (hasBVH) {
				auto trace = MapBVH.TraceRay(pos, newPos);
				if (trace.hit) {
					hit = true;
					hitNormal = trace.normal;
					hitFrac = trace.fraction;
					newPos = trace.endPos + trace.normal * 0.5f;
				}
			}

			pos = newPos;

			if (hit) {
				bounceCount++;

				if (IsMolotov(weapId) && (hitNormal.z >= 0.7f || vel.LengthSq() < STOP_SPEED_SQ)) {
					path.pts.push_back(pos);
					break;
				}

				Vec3 nv = ClipVelocity(vel, hitNormal, 2.f);
				nv.x *= ELASTICITY; nv.y *= ELASTICITY; nv.z *= ELASTICITY;

				if (hitNormal.z > 0.7f) {
					if (nv.LengthSq() < STOP_SPEED_SQ) {
						path.pts.push_back(pos);
						break;
					}
				}

				vel = nv;
				tickTimer = 0;
			}

			if (ShouldDetonate(weapId, vel, tick) || bounceCount > 20) {
				path.pts.push_back(pos);
				break;
			}

			if (hit || ++tickTimer >= 4)
				tickTimer = 0;
		}

		path.valid = path.pts.size() > 1;
		// Swap atomico — minimiza tempo segurando o lock.
		{
			std::lock_guard<std::mutex> lock(g_pathMutex);
			g_path = std::move(path);
		}
	}

	// Worker dedicado: Sleep(3) constante (~333Hz com timer kernel em 1ms).
	// Spin sem dormir (Sleep(0)) lia LocalEyePos/LocalAimAngles a ~3000Hz
	// enquanto a Data thread escrevia Vec3 nao-atomico em paralelo → leitura
	// rasgada (x novo + y/z velhos) producia forward vector errado e a
	// trajetoria saia pro lugar errado. 3ms entre samples dá tempo da Data
	// terminar a escrita do Vec3 antes do proximo read.
	inline void Work() {
		while (true) {
			if (ClientDLL.load() == 0) { Sys::Sleep(50); continue; }
			Simulate();
			Sys::Sleep(3);
		}
	}

	inline void Draw() {
		if (!Config::GameSettings.nadePrediction) return;

		// Snapshot dos pontos sob mutex pra render iterar sem risco do worker
		// realocar o vector no meio.
		std::vector<Vec3> pts;
		{
			std::lock_guard<std::mutex> lock(g_pathMutex);
			if (!g_path.valid || g_path.pts.size() < 2) return;
			pts = g_path.pts;
		}

		auto dl = ImGui::GetBackgroundDrawList();

		float cx = ScreenWidth * 0.5f;
		float cy = ScreenHeight * 0.5f;

		auto& nadeCol = Config::GameSettings.colNade;
		ImU32 baseCol = nadeCol.ToImU32();
		ImU32 baseRGB = baseCol & 0x00FFFFFF;

		for (size_t i = 0; i < pts.size(); i++) {
			Vec2 sp;
			if (WorldToScreen(ViewMatrix, pts[i], ScreenWidth, ScreenHeight, sp)) {
				if (i == 0) {
					ImU32 head = baseRGB | ((unsigned)(int)(nadeCol.a * 150.f) << 24);
					dl->AddLine({ cx, cy }, { sp.x, sp.y }, head, 1.5f);
				}
				break;
			}
		}

		for (size_t i = 1; i < pts.size(); i++) {
			Vec2 s1, s2;
			if (WorldToScreen(ViewMatrix, pts[i - 1], ScreenWidth, ScreenHeight, s1) &&
				WorldToScreen(ViewMatrix, pts[i], ScreenWidth, ScreenHeight, s2)) {
				float fade = 1.f - (float)i / (float)pts.size();
				int a = (int)(nadeCol.a * (100.f + 155.f * fade));
				if (a > 255) a = 255;
				ImU32 col = baseRGB | ((unsigned)a << 24);
				dl->AddLine({ s1.x, s1.y }, { s2.x, s2.y }, col, 1.5f);
			}
		}

		Vec2 endSp;
		if (WorldToScreen(ViewMatrix, pts.back(), ScreenWidth, ScreenHeight, endSp)) {
			ImU32 impact = baseRGB | ((unsigned)(int)(nadeCol.a * 220.f) << 24);
			dl->AddCircleFilled({ endSp.x, endSp.y }, 6.f, impact, 12);
			dl->AddCircle({ endSp.x, endSp.y }, 6.f, IM_COL32(255, 255, 255, 180), 0, 1.5f);
		}
	}
}
