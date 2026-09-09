#pragma once
#include <Windows.h>
#include <cmath>
#include <math/w2s.h>
#include <core/context.h>
#include <core/config.h>
#include <nt/wrap.h>

namespace Features::AimBot {
	using namespace Context;

	constexpr int MAX_MOVE_PX = 120;
	constexpr float RECOIL_SCALE = 2.0f;

	inline uint32_t g_rngState = 0xDEADBEEF;

	inline uint32_t Xorshift32() {
		uint32_t x = g_rngState;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		g_rngState = x;
		return x;
	}

	inline float Rand01() {
		return (Xorshift32() & 0xFFFFFF) / 16777216.0f;
	}

	inline float RandSigned() {
		return Rand01() * 2.0f - 1.0f;
	}

	struct Target {
		Vec3 worldPos;
		float screenDx = 0;
		float screenDy = 0;
		float angleDist = 1e9f;
		uint32_t index = 0;
		Bone bone = Bone::Head;
		bool found = false;
	};

	inline Vec2 g_prevPunch{};
	inline float g_residualX = 0.f;
	inline float g_residualY = 0.f;
	inline uint32_t g_lastTargetIdx = 0;
	inline DWORD g_targetSwitchTime = 0;
	inline DWORD g_switchCooldown = 0;

	inline Bone DynamicBones[] = {
		Bone::Head, Bone::Neck0, Bone::Spine2, Bone::Spine1, Bone::Pelvis
	};

	inline Bone BoneFromConfig() {
		switch (Config::GameSettings.aimBone) {
		case 1: return Bone::Neck0;
		case 2: return Bone::Spine2;
		default: return Bone::Head;
		}
	}

	inline bool IsGrenade() {
		uint16_t id = LocalWeaponId.load();
		return (id >= 43 && id <= 48) || id == 68;
	}

	using NtUserSendInput_fn = UINT(__stdcall*)(UINT, LPINPUT, int);
	inline NtUserSendInput_fn g_pSendInput = nullptr;

	void TryBone(const Entity& e, Bone b, float fovPx, float cx, float cy, Target& best);
	Target FindClosest(float fovDeg);
	void InitSendInput();
	void MoveMouse(int dx, int dy);
	void GetRCSPixels(int& rx, int& ry);
	void Tick();
	void Work();
}
