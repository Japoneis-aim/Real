#pragma once
#include <core/context.h>
#include <core/config.h>
#include <nt/wrap.h>

namespace Data {
	using namespace Context;

	inline Bone VisCheckBones[] = {
		Bone::Head, Bone::Neck0, Bone::Spine2, Bone::Spine1, Bone::Pelvis
	};

	inline Bone SingleBoneFromConfig() {
		switch (Config::GameSettings.aimBone) {
		case 1: return Bone::Neck0;
		case 2: return Bone::Spine2;
		default: return Bone::Head;
		}
	}

	struct VisEntry {
		uint32_t index;
		bool isLocal;
		bool isTeam;
		float distance;
		std::array<Vec3, MAX_BONES> bones;
		uint32_t result;
	};

	constexpr float VIS_MAX_DISTANCE = 3000.0f;
	constexpr int VIS_MAX_ENTITIES = 6;

	void UpdateLocal(uintptr_t base, uintptr_t pawn);
	bool ReadEntity(uintptr_t entityList, uintptr_t localController,
		int idx, Entity& ent, int localTeam, const Vec3& localPos);
	void VisCheckWork();
	void Work();
}
