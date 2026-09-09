#pragma once
#include <string>
#include <array>
#include <math/vec3.h>

enum class Bone : uint32_t {
	Pelvis = 1,
	Spine2 = 3,
	Spine1 = 4,
	Neck0 = 6,
	Head = 7,
	ArmUpperL = 9,
	ArmLowerL = 10,
	HandL = 11,
	ArmUpperR = 13,
	ArmLowerR = 14,
	HandR = 15,
	LegUpperL = 17,
	LegLowerL = 18,
	AnkleL = 19,
	LegUpperR = 20,
	LegLowerR = 21,
	AnkleR = 22
};

constexpr size_t MAX_BONES = 28;

inline Bone BoneList[] = {
	Bone::Pelvis, Bone::Spine2, Bone::Spine1, Bone::Neck0, Bone::Head,
	Bone::ArmUpperL, Bone::ArmLowerL, Bone::HandL,
	Bone::ArmUpperR, Bone::ArmLowerR, Bone::HandR,
	Bone::LegUpperL, Bone::LegLowerL, Bone::AnkleL,
	Bone::LegUpperR, Bone::LegLowerR, Bone::AnkleR
};

struct Entity {
	uint32_t index = 0;
	std::string name;
	uintptr_t address = 0;
	uintptr_t controller = 0;

	Vec3 position;
	Vec3 velocity;
	int health = 0;
	int armor = 0;
	int money = 0;
	float distance = 0.0f;

	bool isTeam = false;
	bool isLocal = false;
	bool isDormant = false;

	bool isVisible = false;
	bool isScoped = false;
	bool isFlashed = false;

	uint32_t visibleBones = 0;

	bool IsBoneVisible(Bone b) const { return (visibleBones >> static_cast<uint32_t>(b)) & 1; }
	void SetBoneVisible(Bone b) { visibleBones |= (1u << static_cast<uint32_t>(b)); }

	uint16_t weaponId = 0;
	Vec3 eyeAngles;

	std::array<Vec3, MAX_BONES> bones{};

	const Vec3& Head() const { return bones[static_cast<size_t>(Bone::Head)]; }
	const Vec3& Chest() const { return bones[static_cast<size_t>(Bone::Spine2)]; }
	const Vec3& Pelvis() const { return bones[static_cast<size_t>(Bone::Pelvis)]; }
};
