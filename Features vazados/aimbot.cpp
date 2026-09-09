#include <features/aimbot.hpp>
#include <LazyDlls/Lazyimporter.hpp>
#include <xorstr.h>

namespace Features::AimBot {
	using namespace Context;

	void TryBone(const Entity& e, Bone b, float fovPx, float cx, float cy, Target& best) {
		Vec3 pos = e.bones[static_cast<size_t>(b)];
		if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) return;
		if (Config::GameSettings.aimRequireVisible && !e.IsBoneVisible(b)) return;

		Vec2 sp;
		if (!WorldToScreen(ViewMatrix, pos, ScreenWidth, ScreenHeight, sp)) return;

		float dx = sp.x - cx;
		float dy = sp.y - cy;
		float dist = std::sqrt(dx * dx + dy * dy);

		if (dist < fovPx && dist < best.angleDist) {
			best.worldPos = pos;
			best.screenDx = dx;
			best.screenDy = dy;
			best.angleDist = dist;
			best.index = e.index;
			best.bone = b;
			best.found = true;
		}
	}

	Target FindClosest(float fovPx) {
		float cx = ScreenWidth * 0.5f;
		float cy = ScreenHeight * 0.5f;

		Target best;
		best.angleDist = fovPx;

		std::lock_guard<std::mutex> lock(EntitiesMutex);
		for (const auto& e : Entities) {
			if (e.isLocal || e.health <= 0) continue;
			if (Config::GameSettings.teamCheck && e.isTeam) continue;
			if (e.distance > Config::GameSettings.aimDistance * Config::UNITS_PER_METER) continue;
			if (Config::GameSettings.aimDynamic) {
				for (Bone b : DynamicBones)
					TryBone(e, b, fovPx, cx, cy, best);
			} else {
				TryBone(e, BoneFromConfig(), fovPx, cx, cy, best);
			}
		}
		return best;
	}

	void InitSendInput() {
		if (g_pSendInput) return;
		HMODULE hMod = LI_CACHED(GetModuleHandleA)(xorstr_("win32u.dll"));
		if (!hMod) hMod = LI_CACHED(LoadLibraryA)(xorstr_("win32u.dll"));
		if (hMod) g_pSendInput = reinterpret_cast<NtUserSendInput_fn>(
			LI_CACHED(GetProcAddress)(hMod, xorstr_("NtUserSendInput")));
		if (!g_pSendInput) g_pSendInput = reinterpret_cast<NtUserSendInput_fn>(
			LI_CACHED(GetProcAddress)(LI_CACHED(GetModuleHandleA)(xorstr_("user32.dll")), xorstr_("SendInput")));
	}

	void MoveMouse(int dx, int dy) {
		if (dx > MAX_MOVE_PX) dx = MAX_MOVE_PX;
		if (dx < -MAX_MOVE_PX) dx = -MAX_MOVE_PX;
		if (dy > MAX_MOVE_PX) dy = MAX_MOVE_PX;
		if (dy < -MAX_MOVE_PX) dy = -MAX_MOVE_PX;
		if (dx == 0 && dy == 0) return;

		InitSendInput();
		if (!g_pSendInput) return;

		INPUT in{};
		in.type = INPUT_MOUSE;
		in.mi.dx = dx;
		in.mi.dy = dy;
		in.mi.dwFlags = MOUSEEVENTF_MOVE;
		g_pSendInput(1, &in, sizeof(in));
	}

	void GetRCSPixels(int& rx, int& ry) {
		rx = 0; ry = 0;
		// Acumulador residual — dispensa o delta de recuo em varios ticks pra
		// nao teleportar o cursor. Cada bullet fire pode causar uma mudanca
		// grande no punch angle (~1 grau = ~21px a 1920); 200Hz tick com
		// step max de 4px dispensa em ~5ms-25ms.
		static float g_owedX = 0.f;
		static float g_owedY = 0.f;

		if (!Config::GameSettings.rcs) {
			g_owedX = g_owedY = 0.f;
			g_prevPunch = {};
			return;
		}
		// Ativa o RCS se LMB pressionado (tiro manual) OU se o triggerbot
		// estiver ligado e a tecla dele pressionada — o trigger so injeta
		// LMB por ~10ms quando atira, curto pra RCS pegar; tratar a tecla
		// do trigger como "em modo de tiro" garante compensacao continua
		// enquanto o usuario segura.
		bool lmb = (LI_CACHED(GetAsyncKeyState)(VK_LBUTTON) & 0x8000) != 0;
		bool triggerActive = Config::GameSettings.triggerbot &&
			(LI_CACHED(GetAsyncKeyState)(Config::GameSettings.triggerKey) & 0x8000) != 0;
		if (!lmb && !triggerActive) {
			g_owedX = g_owedY = 0.f;
			g_prevPunch = {};
			return;
		}

		// (Cursor visivel ja gate'd no Tick() global — sem dupla checagem aqui.)

		Vec2 cur = { LocalPunchAngle.x, LocalPunchAngle.y };
		float scale = Config::GameSettings.rcsStrength * 0.01f;
		if (cur.x * cur.x + cur.y * cur.y < 0.0001f) {
			g_owedX = g_owedY = 0.f;
			g_prevPunch = {};
			return;
		}

		Vec2 delta = { cur.x - g_prevPunch.x, cur.y - g_prevPunch.y };
		g_prevPunch = cur;

		float pxPerDeg = ScreenWidth / 90.0f;
		// RCS so compensa o eixo VERTICAL — user pediu descida pura sem
		// ajuste lateral. Horizontal fica zerado.
		g_owedX = 0.f;
		g_owedY -= delta.x * RECOIL_SCALE * scale * pxPerDeg;

		// Step max por tick — 4-5px a 200Hz da rampa visivelmente smooth.
		const float kMaxStep = 4.5f;
		float stepX = g_owedX;
		float stepY = g_owedY;
		if (stepX >  kMaxStep) stepX =  kMaxStep;
		if (stepX < -kMaxStep) stepX = -kMaxStep;
		if (stepY >  kMaxStep) stepY =  kMaxStep;
		if (stepY < -kMaxStep) stepY = -kMaxStep;

		rx = static_cast<int>(stepX);
		ry = static_cast<int>(stepY);
		g_owedX -= rx;
		g_owedY -= ry;
	}

	void Tick() {
		if (ClientDLL.load() == 0) return;
		// Gate global: cursor do Windows visivel = jogo perdeu foco (menu,
		// scoreboard, console, dead spec). Nao mexer cursor do desktop.
		if (IsCursorVisible()) { g_lastTargetIdx = 0; return; }
		if (IsGrenade()) { g_prevPunch = {}; return; }

		Target t = FindClosest(Config::GameSettings.aimFov);

		if (Config::GameSettings.aimHighlight) {
			AimTarget.index.store(t.found ? t.index : 0);
			AimTarget.valid.store(t.found);
			if (t.found) {
				AimTarget.bonePos = t.worldPos;
				AimTarget.targetBone = t.bone;
			}
		} else {
			AimTarget.valid.store(false);
		}

		int rcsDx = 0, rcsDy = 0;
		GetRCSPixels(rcsDx, rcsDy);

		bool aimKeyDown = (LI_CACHED(GetAsyncKeyState)(Config::GameSettings.aimKey) & 0x8000) != 0;

		if (Config::GameSettings.aimAssistMode && t.found) {
			bool shooting = (LI_CACHED(GetAsyncKeyState)(VK_LBUTTON) & 0x8000) != 0;
			float assistRadius = Config::GameSettings.aimFov * 0.4f;
			if (!shooting || t.angleDist > assistRadius || !t.found) {
				t.found = false;
			}
			if (t.found && Config::GameSettings.aimRequireVisible) {
				bool boneVis = false;
				std::lock_guard<std::mutex> lock(EntitiesMutex);
				for (const auto& e : Entities) {
					if (e.index == t.index) { boneVis = e.isVisible; break; }
				}
				if (!boneVis) t.found = false;
			}
		}

		if (!Config::GameSettings.aimbot || !aimKeyDown || !t.found) {
			g_residualX = 0.f;
			g_residualY = 0.f;
			if (!t.found && g_lastTargetIdx != 0) g_lastTargetIdx = 0;
			if (rcsDx || rcsDy) MoveMouse(rcsDx, rcsDy);
			return;
		}

		if (t.index != g_lastTargetIdx) {
			g_lastTargetIdx = t.index;
			g_residualX = 0.f;
			g_residualY = 0.f;
		}

		float rawDx = t.screenDx;
		float rawDy = t.screenDy;
		float dist = std::sqrt(rawDx * rawDx + rawDy * rawDy);

		if (dist < 0.5f) {
			g_residualX = 0.f;
			g_residualY = 0.f;
			if (rcsDx || rcsDy) MoveMouse(rcsDx, rcsDy);
			return;
		}

		float smoothPct = Config::GameSettings.aimSmooth;
		if (smoothPct < 0.f) smoothPct = 0.f;
		if (smoothPct > 100.f) smoothPct = 100.f;

		int aimDx, aimDy;

		if (!Config::GameSettings.aimHumanize || smoothPct < 1.f) {
			float frac = 1.f - smoothPct * 0.0098f;
			if (frac < 0.02f) frac = 0.02f;
			aimDx = static_cast<int>(rawDx * frac);
			aimDy = static_cast<int>(rawDy * frac);
			if (dist > 1.f && aimDx == 0 && aimDy == 0) {
				aimDx = rawDx > 0.f ? 1 : -1;
				aimDy = rawDy > 0.f ? 1 : -1;
			}
		} else {
			float frac = 1.f - smoothPct * 0.0093f;
			if (frac < 0.07f) frac = 0.07f;

			float fovPx = Config::GameSettings.aimFov;
			if (fovPx < 1.f) fovPx = 1.f;
			float normDist = dist / fovPx;
			if (normDist > 1.f) normDist = 1.f;

			float speedCurve = 0.4f + 0.6f * normDist;
			float tickFrac = frac * speedCurve;

			float moveX = rawDx * tickFrac + g_residualX;
			float moveY = rawDy * tickFrac + g_residualY;

			float jitterScale = normDist * 0.6f;
			if (jitterScale > 0.5f) jitterScale = 0.5f;
			moveX += RandSigned() * jitterScale;
			moveY += RandSigned() * jitterScale;

			aimDx = static_cast<int>(moveX);
			aimDy = static_cast<int>(moveY);

			g_residualX = moveX - static_cast<float>(aimDx);
			g_residualY = moveY - static_cast<float>(aimDy);

			if (dist > 1.f && aimDx == 0 && aimDy == 0) {
				if (std::fabs(rawDx) >= std::fabs(rawDy))
					aimDx = rawDx > 0.f ? 1 : -1;
				else
					aimDy = rawDy > 0.f ? 1 : -1;
				g_residualX = 0.f;
				g_residualY = 0.f;
			}

			if (dist < 2.f) {
				aimDx = static_cast<int>(rawDx);
				aimDy = static_cast<int>(rawDy);
				g_residualX = 0.f;
				g_residualY = 0.f;
			}
		}

		MoveMouse(aimDx + rcsDx, aimDy + rcsDy);
	}

	void Work() {
		while (true) {
			Sys::Sleep(5);
			if (ClientDLL.load() == 0) return;
			Tick();
		}
	}
}
