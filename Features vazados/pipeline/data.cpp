#include <pipeline/data.hpp>
#include <xorstr.h>
#include <console.h>

namespace Data {
	using namespace Context;

	void UpdateLocal(uintptr_t base, uintptr_t pawn) {
		Vec3 origin = Memory->Read<Vec3>(pawn + Schema.m_vOldOrigin);
		Vec3 viewOffset = Memory->Read<Vec3>(pawn + Schema.m_vecViewOffset);

		LocalEyePos = origin + viewOffset;
		LocalVelocity = Memory->Read<Vec3>(pawn + Schema.m_vecVelocity);
		LocalHealth.store(Memory->Read<int>(pawn + Schema.m_iHealth));
		LocalTeam.store(Memory->Read<int>(pawn + Schema.m_iTeamNum));

		// View angles: primeiro tenta o m_angEyeAngles da pawn (mais
		// confiavel — eh oque a engine usa de fato). Se vier zerado,
		// fallback pro csgoInput->viewAngles (input bruto do mouse).
		Vec3 angsPawn = Schema.m_angEyeAngles
			? Memory->Read<Vec3>(pawn + Schema.m_angEyeAngles)
			: Vec3{0, 0, 0};

		if (angsPawn.x != 0.f || angsPawn.y != 0.f) {
			LocalAimAngles = angsPawn;
		} else if (Offsets.dwViewAngles && Offsets.dwCSGOInput) {
			uintptr_t csgoInput = Memory->Read<uintptr_t>(base + Offsets.dwCSGOInput);
			if (csgoInput) {
				uintptr_t vaOffset = Offsets.dwViewAngles - Offsets.dwCSGOInput;
				LocalAimAngles = Memory->Read<Vec3>(csgoInput + vaOffset);
			}
		}

		static int s_angleTick = 0;
		if (++s_angleTick % 300 == 0)
			Console::Info("[data] yaw=%.2f pitch=%.2f (pawn yaw=%.2f m_ang=0x%X)",
				LocalAimAngles.y, LocalAimAngles.x,
				angsPawn.y, (unsigned)Schema.m_angEyeAngles);

		// CS2 atual: m_aimPunchAngle nao existe mais no pawn — agora e' lido
		// via pawn -> m_pCameraServices -> m_vecCsViewPunchAngle. Fallback
		// pro path antigo se a Valve reverter.
		if (Schema.m_pCameraServices && Schema.m_vecCsViewPunchAngle) {
			uintptr_t camSvc = Memory->Read<uintptr_t>(pawn + Schema.m_pCameraServices);
			if (camSvc)
				LocalPunchAngle = Memory->Read<Vec3>(camSvc + Schema.m_vecCsViewPunchAngle);
		} else if (Schema.m_aimPunchAngle) {
			LocalPunchAngle = Memory->Read<Vec3>(pawn + Schema.m_aimPunchAngle);
		}
		if (Schema.m_iShotsFired)
			LocalShotsFired.store(Memory->Read<int>(pawn + Schema.m_iShotsFired));

		// No Flash: zera m_flFlashDuration da pawn local a cada tick. Engine
		// soma a duracao no rendering — 0 = sem efeito de cegueira. Port da
		// source ZIMOCS2 (Pipeline/Data.cs:33).
		if (Config::GameSettings.noFlash && Schema.m_flFlashDuration) {
			Memory->Write<float>(pawn + Schema.m_flFlashDuration, 0.f);
		}

		// Quando morto, le quem ta sendo telado via observer services pra
		// o ESP nao desenhar caixa em volta de quem voce esta vendo na tela.
		// Vivo = 0 (sem spectator).
		uint32_t spectatedIdx = 0;
		if (LocalHealth.load() <= 0 && Schema.m_pObserverServices && Schema.m_hObserverTarget) {
			uintptr_t os = Memory->Read<uintptr_t>(pawn + Schema.m_pObserverServices);
			if (os) {
				uint32_t obsHandle = Memory->Read<uint32_t>(os + Schema.m_hObserverTarget);
				if (obsHandle != 0xFFFFFFFF) spectatedIdx = obsHandle & 0x7FFF;
			}
		}
		LocalSpectatedIdx.store(spectatedIdx);

		if (Schema.m_pWeaponServices && Schema.m_hActiveWeapon) {
			uintptr_t ws = Memory->Read<uintptr_t>(pawn + Schema.m_pWeaponServices);
			if (ws) {
				uint32_t wh = Memory->Read<uint32_t>(ws + Schema.m_hActiveWeapon);
				if (wh) {
					uintptr_t el = Memory->Read<uintptr_t>(base + Offsets.dwEntityList);
					if (el) {
						uintptr_t le = Memory->Read<uintptr_t>(el + ((wh & 0x7FFF) >> 9) * 8 + 16);
						if (le) {
							uintptr_t w = Memory->Read<uintptr_t>(le + (wh & 0x1FF) * 112);
							if (w && Schema.m_AttributeManager && Schema.m_Item && Schema.m_iItemDefinitionIndex)
								LocalWeaponId.store(Memory->Read<uint16_t>(
									w + Schema.m_AttributeManager + Schema.m_Item + Schema.m_iItemDefinitionIndex));
						}
					}
				}
			}
		}
	}

	bool ReadEntity(uintptr_t entityList, uintptr_t localController,
		int idx, Entity& ent, int localTeam, const Vec3& localPos) {

		uintptr_t listEntry = Memory->Read<uintptr_t>(entityList + ((idx & 0x7FFF) >> 9) * 8 + 16);
		if (!listEntry) return false;

		uintptr_t controller = Memory->Read<uintptr_t>(listEntry + (uint64_t)(idx & 0x1FF) * 112);
		if (!controller) return false;

		bool isLocal = (controller == localController);
		if (isLocal) LocalIndex.store(static_cast<uint32_t>(idx));

		uint32_t pawnHandle = Memory->Read<uint32_t>(controller + Schema.m_hPlayerPawn);
		if (!pawnHandle) return false;

		uintptr_t le2 = Memory->Read<uintptr_t>(entityList + ((pawnHandle & 0x7FFF) >> 9) * 8 + 16);
		if (!le2) return false;

		uintptr_t pawn = Memory->Read<uintptr_t>(le2 + (pawnHandle & 0x1FF) * 112);
		if (!pawn) return false;

		int lifeState = Memory->Read<int>(pawn + Schema.m_lifeState);
		if (lifeState != 256) return false;

		int health = Memory->Read<int>(pawn + Schema.m_iHealth);
		if (health < 1) return false;

		ent.index = pawnHandle & 0x7FFF;
		ent.address = pawn;
		ent.controller = controller;
		ent.position = Memory->Read<Vec3>(pawn + Schema.m_vOldOrigin);
		ent.health = health;
		ent.armor = Memory->Read<int>(pawn + Schema.m_ArmorValue);
		if (Schema.m_vecVelocity)
			ent.velocity = Memory->Read<Vec3>(pawn + Schema.m_vecVelocity);
		if (Schema.m_angEyeAngles)
			ent.eyeAngles = Memory->Read<Vec3>(pawn + Schema.m_angEyeAngles);
		if (Schema.m_bIsScoped)
			ent.isScoped = Memory->Read<bool>(pawn + Schema.m_bIsScoped);
		if (Schema.m_flFlashBangTime) {
			float flashTime = Memory->Read<float>(pawn + Schema.m_flFlashBangTime);
			ent.isFlashed = flashTime > 0.0f;
		}
		if (Schema.m_pWeaponServices && Schema.m_hActiveWeapon && Schema.m_AttributeManager && Schema.m_Item && Schema.m_iItemDefinitionIndex) {
			uintptr_t weapSvc = Memory->Read<uintptr_t>(pawn + Schema.m_pWeaponServices);
			if (weapSvc) {
				uint32_t weapHandle = Memory->Read<uint32_t>(weapSvc + Schema.m_hActiveWeapon);
				if (weapHandle) {
					uintptr_t el = Memory->Read<uintptr_t>(entityList + ((weapHandle & 0x7FFF) >> 9) * 8 + 16);
					if (el) {
						uintptr_t weapon = Memory->Read<uintptr_t>(el + (weapHandle & 0x1FF) * 112);
						if (weapon) {
							uintptr_t attrMgr = weapon + Schema.m_AttributeManager;
							uintptr_t item = attrMgr + Schema.m_Item;
							ent.weaponId = Memory->Read<uint16_t>(item + Schema.m_iItemDefinitionIndex);
#ifdef OXYGEN_LOGS
							static int wpnLogged = 0;
							if (wpnLogged < 4) {
								Console::Info("[wpn] idx=%d pawn=0x%llX weapSvc=0x%llX weapHandle=0x%X weapon=0x%llX attrMgr=0x%llX item=0x%llX +idx=0x%llX → id=%u",
									ent.index, pawn, weapSvc, weapHandle, weapon,
									attrMgr, item, item + Schema.m_iItemDefinitionIndex,
									ent.weaponId);
								wpnLogged++;
							}
#endif
						}
					}
				}
			}
		}
		ent.distance = Vec3::Distance(localPos, ent.position);

		int team = Memory->Read<int>(pawn + Schema.m_iTeamNum);
		ent.isTeam = (team == localTeam);
		ent.isLocal = isLocal;

		uintptr_t moneySvc = Memory->Read<uintptr_t>(controller + Schema.m_pInGameMoneyServices);
		ent.money = moneySvc ? Memory->Read<int>(moneySvc + Schema.m_iAccount) : 0;

		uintptr_t namePtr = Memory->Read<uintptr_t>(controller + Schema.m_sSanitizedPlayerName);
		char nameBuf[128]{};
		if (namePtr) Memory->ReadRaw(namePtr, nameBuf, 127);
		ent.name = std::string(nameBuf);

		uintptr_t node = Memory->Read<uintptr_t>(pawn + Schema.m_pGameSceneNode);
		if (!node) return true;

		uintptr_t boneMatrix = Memory->Read<uintptr_t>(node + Schema.m_modelState + 0x80);
		if (boneMatrix) {
			// Batch read da bone matrix em UMA syscall em vez de 17.
			// BoneList vai de Pelvis(1) ate AnkleR(22) — lemos do indice 1
			// inclusive ate 22 inclusive = 22 entries × 32 bytes = 704 bytes.
			// Save: ~9600 syscalls/sec (10 ents × 60Hz × 16 reads economizadas).
			constexpr size_t kStride = 32;
			constexpr size_t kFirstBone = 1;   // Pelvis
			constexpr size_t kLastBone  = 22;  // AnkleR
			constexpr size_t kBufBytes  = (kLastBone - kFirstBone + 1) * kStride;
			uint8_t bonesBuf[kBufBytes];
			if (Memory->ReadRaw(boneMatrix + kFirstBone * kStride, bonesBuf, kBufBytes)) {
				for (Bone bone : BoneList) {
					const size_t idx = static_cast<size_t>(bone);
					Vec3 bp = *reinterpret_cast<const Vec3*>(bonesBuf + (idx - kFirstBone) * kStride);
					if (bone == Bone::Head) bp.z -= 1.0f;
					ent.bones[idx] = bp;
				}
			}
		}

		return true;
	}

	void VisCheckWork() {
		VisEntry workBuf[VIS_MAX_ENTITIES];

		while (true) {
			Sys::Sleep(5);
			if (ClientDLL.load() == 0) return;
			if (!MapBVHReady.load()) continue;

			Vec3 eye = LocalEyePos;
			bool teamCheck = Config::GameSettings.teamCheck;
			bool dynamic = Config::GameSettings.aimDynamic;
			int count = 0;

			{
				std::lock_guard<std::mutex> lock(EntitiesMutex);
				for (const auto& e : Entities) {
					if (e.isLocal) continue;
					if (teamCheck && e.isTeam) continue;
					if (e.distance > VIS_MAX_DISTANCE) continue;
					if (count < VIS_MAX_ENTITIES) {
						workBuf[count] = { e.index, e.isLocal, e.isTeam, e.distance, e.bones, 0 };
						count++;
					} else {
						int farthest = 0;
						for (int j = 1; j < VIS_MAX_ENTITIES; j++)
							if (workBuf[j].distance > workBuf[farthest].distance) farthest = j;
						if (e.distance < workBuf[farthest].distance)
							workBuf[farthest] = { e.index, e.isLocal, e.isTeam, e.distance, e.bones, 0 };
					}
				}
			}

			for (int i = 0; i < count; i++) {
				auto& w = workBuf[i];
				if (dynamic) {
					for (Bone b : VisCheckBones) {
						Vec3 bp = w.bones[static_cast<size_t>(b)];
						if (bp.x == 0.0f && bp.y == 0.0f && bp.z == 0.0f) continue;
						if (MapBVH.IsVisible(eye, bp))
							w.result |= (1u << static_cast<uint32_t>(b));
					}
				} else {
					Bone target = SingleBoneFromConfig();
					Vec3 bp = w.bones[static_cast<size_t>(target)];
					if (bp.x != 0.0f || bp.y != 0.0f || bp.z != 0.0f)
						if (MapBVH.IsVisible(eye, bp))
							w.result |= (1u << static_cast<uint32_t>(target));
				}
			}

			{
				std::lock_guard<std::mutex> lock(EntitiesMutex);
				for (int i = 0; i < count; i++) {
					for (auto& e : Entities) {
						if (e.index == workBuf[i].index) {
							e.visibleBones = workBuf[i].result;
							e.isVisible = workBuf[i].result != 0;
							break;
						}
					}
				}
			}
		}
	}

	void Work() {
#ifdef OXYGEN_LOGS
		Console::Info("[data] Work() loop started (offsets: entityList=0x%llX localPawn=0x%llX localController=0x%llX viewMatrix=0x%llX)",
			Offsets.dwEntityList, Offsets.dwLocalPlayerPawn, Offsets.dwLocalPlayerController, Offsets.dwViewMatrix);
		bool loggedFirstPawn = false;
		bool loggedFirstController = false;
		bool loggedFirstEntityList = false;
		bool loggedFirstEntities = false;
		int loopTick = 0;
#endif
		while (true) {
			Sys::Sleep(10);

			auto base = ClientDLL.load();
			if (base == 0) { Console::Warn("[data] base=0, exit Work()"); return; }
			if (!Memory->IsAttached()) { Console::Warn("[data] not attached, exit Work()"); return; }

			uintptr_t globalVars = Memory->Read<uintptr_t>(base + Offsets.dwGlobalVars);
			if (globalVars) {
				uintptr_t mapNamePtr = Memory->Read<uintptr_t>(globalVars + 0x180);
				if (mapNamePtr) {
					char buf[256]{};
					Memory->ReadRaw(mapNamePtr, buf, 255);
					std::string temp(buf);
					if (temp.rfind(xorstr_("mg_"), 0) == 0) temp = temp.substr(3);
					MapName = temp;
				} else {
					MapName.clear();
				}
			}

			// Limpa estado de match quando saimos pra lobby. CS2 process segue
			// rodando (Window::Watcher::OnLost so dispara quando o exe fecha),
			// mas dwLocalPlayerPawn/Controller/EntityList viram 0 entre
			// partidas. Sem limpar, ESP/aim desenham fantasmas do tick
			// anterior.
			auto clearMatchState = [] {
				{
					std::lock_guard<std::mutex> lock(EntitiesMutex);
					Entities.clear();
				}
				LocalPlayerPawn.store(0);
				LocalHealth.store(0);
				LocalSpectatedIdx.store(0);
				AimTarget.valid.store(false);
				AimTarget.index.store(0);
			};

			uintptr_t localPawn = Memory->Read<uintptr_t>(base + Offsets.dwLocalPlayerPawn);
#ifdef OXYGEN_LOGS
			if (++loopTick % 500 == 0)
				Console::Info("[data] tick=%d localPawn=0x%llX entities=%zu map='%s'",
					loopTick, localPawn, Entities.size(), MapName.c_str());
#endif
			if (!localPawn) { clearMatchState(); continue; }
			LocalPlayerPawn.store(localPawn);
#ifdef OXYGEN_LOGS
			if (!loggedFirstPawn) { Console::Success("[data] First localPawn=0x%llX", localPawn); loggedFirstPawn = true; }
#endif

			uintptr_t localController = Memory->Read<uintptr_t>(base + Offsets.dwLocalPlayerController);
			if (!localController) { clearMatchState(); continue; }
#ifdef OXYGEN_LOGS
			if (!loggedFirstController) { Console::Success("[data] First localController=0x%llX", localController); loggedFirstController = true; }
#endif

			UpdateLocal(base, localPawn);
			ViewMatrix = Memory->Read<Matrix4x4>(base + Offsets.dwViewMatrix);

			uintptr_t entityList = Memory->Read<uintptr_t>(base + Offsets.dwEntityList);
			if (!entityList) { clearMatchState(); continue; }
#ifdef OXYGEN_LOGS
			if (!loggedFirstEntityList) { Console::Success("[data] First entityList=0x%llX", entityList); loggedFirstEntityList = true; }
#endif

			int localTeam = LocalTeam.load();
			Vec3 localPos = Memory->Read<Vec3>(localPawn + Schema.m_vOldOrigin);

			std::vector<Entity> tmpList;
			tmpList.reserve(64);

			for (int i = 0; i < 64; i++) {
				Entity ent;
				if (ReadEntity(entityList, localController, i, ent, localTeam, localPos))
					tmpList.push_back(std::move(ent));
			}

#ifdef OXYGEN_LOGS
			if (!loggedFirstEntities && !tmpList.empty()) {
				Console::Success("[data] First Entities batch: %zu (localTeam=%d, localHP=%d)",
					tmpList.size(), localTeam, LocalHealth.load());
				loggedFirstEntities = true;
			}
#endif

			{
				std::lock_guard<std::mutex> lock(EntitiesMutex);
				for (auto& newEnt : tmpList) {
					for (const auto& old : Entities) {
						if (old.index == newEnt.index) {
							newEnt.visibleBones = old.visibleBones;
							newEnt.isVisible = old.isVisible;
							break;
						}
					}
				}
				Entities = std::move(tmpList);
			}
		}
	}
}
