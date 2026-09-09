#pragma once
#include <memory/ntdll.h>
#include <physics/bvh.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <unordered_map>
#include <Windows.h>
#include <LazyDlls/Lazyimporter.hpp>
#include <math/vec3.h>
#include <math/matrix.h>
#include <core/entity.h>

namespace Context {
	inline Mem::NtdllMemory* Memory = new Mem::NtdllMemory();
	inline std::atomic<uintptr_t> ClientDLL = 0;

	struct OffsetsData {
		uintptr_t dwEntityList = 0;
		uintptr_t dwLocalPlayerController = 0;
		uintptr_t dwLocalPlayerPawn = 0;
		uintptr_t dwViewMatrix = 0;
		uintptr_t dwGameRules = 0;
		uintptr_t dwGlobalVars = 0;
		uintptr_t dwPlantedC4 = 0;
		uintptr_t dwWeaponC4 = 0;
		uintptr_t dwCSGOInput = 0;
		uintptr_t dwViewAngles = 0;
	};
	inline OffsetsData Offsets;

	struct SchemaData {
		uintptr_t m_iTeamNum = 0;
		uintptr_t m_lifeState = 0;
		uintptr_t m_iHealth = 0;
		uintptr_t m_hPlayerPawn = 0;
		uintptr_t m_hPawn = 0;
		uintptr_t m_vOldOrigin = 0;
		uintptr_t m_pGameSceneNode = 0;
		uintptr_t m_entitySpottedState = 0;
		uintptr_t m_bSpotted = 0x8;
		uintptr_t m_iIDEntIndex = 0;
		uintptr_t m_pInGameMoneyServices = 0;
		uintptr_t m_iAccount = 0;
		uintptr_t m_sSanitizedPlayerName = 0;
		uintptr_t m_flFlashDuration = 0;
		uintptr_t m_aimPunchAngle = 0;
		uintptr_t m_pCameraServices = 0;       // pawn -> CPlayer_CameraServices*
		uintptr_t m_vecCsViewPunchAngle = 0;   // CameraServices -> QAngle (current view punch)
		uintptr_t m_iShotsFired = 0;
		uintptr_t m_ArmorValue = 0;
		uintptr_t m_pObserverServices = 0;
		uintptr_t m_hObserverTarget = 0;
		uintptr_t m_modelState = 0;
		uintptr_t m_pWeaponServices = 0;
		uintptr_t m_pClippingWeapon = 0;
		uintptr_t m_vecVelocity = 0;
		uintptr_t m_vecAbsVelocity = 0;
		uintptr_t m_vecViewOffset = 0;
		uintptr_t m_flThrowStrength = 0;
		uintptr_t m_iItemDefinitionIndex = 0;
		uintptr_t m_nSubclassID = 0;
		uintptr_t m_flThrowVelocity = 0;
		uintptr_t m_AttributeManager = 0;
		uintptr_t m_Item = 0;
		uintptr_t m_angEyeAngles = 0;
		uintptr_t m_bIsScoped = 0;
		uintptr_t m_flFlashBangTime = 0;
		uintptr_t m_hActiveWeapon = 0;
		uintptr_t m_bBombDefused = 0;
		uintptr_t m_flDefuseCountDown = 0;
		uintptr_t m_flC4Blow = 0;
		uintptr_t m_bBeingDefused = 0;
		uintptr_t m_nBombSite = 0;
	};
	inline SchemaData Schema;

	inline std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> SchemaMap;

	// Busca em QUALQUER classe que contenha o field. Em Source 2 a heranca
	// e' sequencial, entao o offset do field e' invariante entre todas as
	// classes onde ele aparece — se nao bate o className exato, achar em
	// qualquer outra ainda da o offset correto.
	inline uint32_t ResolveSchemaAnyClass(const std::string& fieldName) {
		for (const auto& [cls, fields] : SchemaMap) {
			auto fit = fields.find(fieldName);
			if (fit != fields.end()) return fit->second;
		}
		return 0;
	}

	inline uint32_t ResolveSchema(const std::string& className, const std::string& fieldName) {
		auto it = SchemaMap.find(className);
		if (it != SchemaMap.end()) {
			auto fit = it->second.find(fieldName);
			if (fit != it->second.end())
				return fit->second;
		}
		// Fallback: tenta achar o field em outra classe qualquer.
		return ResolveSchemaAnyClass(fieldName);
	}

	inline std::mutex EntitiesMutex;
	inline std::vector<Entity> Entities;
	inline std::atomic<uintptr_t> LocalPlayerPawn = 0;
	inline std::atomic<uint32_t> LocalIndex = 0;
	inline Matrix4x4 ViewMatrix{};
	inline std::string MapName;

	inline Vec3 LocalEyePos;
	inline Vec3 LocalAimAngles;
	inline Vec3 LocalVelocity;
	inline Vec3 LocalPunchAngle;
	inline std::atomic<int> LocalHealth = 0;
	inline std::atomic<int> LocalTeam = 0;
	inline std::atomic<uint16_t> LocalWeaponId = 0;
	inline std::atomic<int> LocalShotsFired = 0;

	// Gate global: cursor do Windows visivel = jogo nao capturou o mouse
	// (scoreboard/console/menu/dead). Aimbot/Trigger/RCS pulam o tick nesse
	// estado pra nao mexer o cursor do desktop em vez da camera.
	inline bool IsCursorVisible() {
		CURSORINFO ci{};
		ci.cbSize = sizeof(ci);
		return LI_CACHED(GetCursorInfo)(&ci) && (ci.flags & CURSOR_SHOWING);
	}
	// Indice da entidade que o player ta telando quando morto. Zero quando
	// vivo / nao spectating. ESP usa pra nao desenhar caixa em volta de quem
	// voce ta vendo na tela.
	inline std::atomic<uint32_t> LocalSpectatedIdx = 0;

	inline Physics::BVH MapBVH;
	inline std::mutex MapBVHMutex;
	inline std::atomic<bool> MapBVHReady = false;

	inline int ScreenWidth = 1920;
	inline int ScreenHeight = 1080;

	struct AimTargetData {
		std::atomic<uint32_t> index = 0;
		std::atomic<bool> valid = false;
		Vec3 bonePos;
		Bone targetBone = Bone::Head;
	};
	inline AimTargetData AimTarget;
}

