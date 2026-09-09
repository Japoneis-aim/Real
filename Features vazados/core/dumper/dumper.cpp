#include <core/dumper/dumper.hpp>
#include <console.h>
#include <xorstr.h>
#include <vmp.h>

namespace Dumper {
	using namespace Context;

	PatternEntry entries[] = {
		{ &Offsets.dwEntityList,            ("dwEntityList"),            ("48 89 0D ?? ?? ?? ?? E9 ?? ?? ?? ?? CC"), 3, 7 },
		{ &Offsets.dwLocalPlayerController, ("dwLocalPlayerController"), ("48 8B 05 ?? ?? ?? ?? 41 89 BE"), 3, 7 },
		{ &Offsets.dwViewMatrix,            ("dwViewMatrix"),            ("48 8D 0D ?? ?? ?? ?? 48 C1 E0 06"), 3, 7 },
		{ &Offsets.dwGameRules,             ("dwGameRules"),             ("F6 C1 01 0F 85 ?? ?? ?? ?? 4C 8B 05 ?? ?? ?? ?? 4D 85"), 12, 16 },
		{ &Offsets.dwGlobalVars,            ("dwGlobalVars"),            ("48 89 15 ?? ?? ?? ?? 48 89 42"), 3, 7 },
		{ &Offsets.dwPlantedC4,             ("dwPlantedC4"),             ("48 8B 1D ?? ?? ?? ?? 45 32 F6"), 3, 7 },
		{ &Offsets.dwWeaponC4,              ("dwWeaponC4"),              ("48 8B 15 ?? ?? ?? ?? 48 8B 5C 24 ?? FF C0 89 05 ?? ?? ?? ?? 48 8B C6 48 89 34 EA 80 BE"), 3, 7 },
	};

	bool DumpOffsets() {
		VMP_BEGIN("dump_offsets");
		auto client = Memory->CacheModule(std::string(xorstr_("client.dll").c_str()));
		if (!client.Valid()) {
			Console::Error(("Failed to cache client.dll"));
			return false;
		}

		uintptr_t base = client.Base();
		Console::Info(("client.dll: 0x%llX (%zu MB)"), base, client.Size() / (1024 * 1024));

		for (auto& e : entries) {
			auto addr = client.Scan(e.pattern);
			if (!addr) {
				Console::Error(("  %s: NOT FOUND"), e.name);
				*e.target = 0;
				continue;
			}
			int disp = Memory->Read<int>(addr + e.dispOffset);
			uintptr_t resolved = (addr + e.instrSize + disp) - base;
			*e.target = resolved;
			Console::Success(("  %s = 0x%llX"), e.name, resolved);
		}

		auto pawnResults = client.Scan(("48 8D 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54"));
		if (pawnResults) {
			int disp = Memory->Read<int>(pawnResults + 3);
			uintptr_t prediction = (pawnResults + 7 + disp) - base;

			auto pawnOffset = client.Scan(("4C 39 B6 ?? ?? ?? ?? 74 ?? 44 88 BE ?? ?? ?? ??"));
			if (pawnOffset) {
				int disp2 = Memory->Read<int>(pawnOffset + 3);
				Offsets.dwLocalPlayerPawn = prediction + disp2;
				Console::Success(("  dwLocalPlayerPawn = 0x%llX"), Offsets.dwLocalPlayerPawn);
			}
		}

		auto csgoInput = client.Scan(("48 89 05 ?? ?? ?? ?? 0F 57 C0 0F 11 05"));
		if (csgoInput) {
			int disp = Memory->Read<int>(csgoInput + 3);
			Offsets.dwCSGOInput = (csgoInput + 7 + disp) - base;
			Console::Success(("  dwCSGOInput = 0x%llX"), Offsets.dwCSGOInput);

			auto viewAngles = client.Scan(("F2 42 0F 10 84 28 ?? ?? ?? ??"));
			if (viewAngles) {
				uint32_t vaOffset = Memory->Read<uint32_t>(viewAngles + 6);
				Offsets.dwViewAngles = Offsets.dwCSGOInput + vaOffset;
				Console::Success(("  dwViewAngles = 0x%llX"), Offsets.dwViewAngles);
			}
		}

		VMP_END;
		return true;
	}

	bool DumpSchemas() {
		VMP_BEGIN("dump_schemas");
		auto schema = Memory->CacheModule(std::string(xorstr_("schemasystem.dll").c_str()));
		if (!schema.Valid()) {
			Console::Error(("Failed to cache schemasystem.dll"));
			return false;
		}

		uintptr_t schemaBase = schema.Base();
		Console::Info(("schemasystem.dll: 0x%llX"), schemaBase);

		auto patAddr = schema.Scan(("4C 8D 35 ?? ?? ?? ?? 0F 28 45 ??"));
		if (!patAddr) {
			Console::Error(("  SchemaSystem pattern not found"));
			return false;
		}

		int relOffset = Memory->Read<int>(patAddr + 3);
		uintptr_t schemaSystemPtr = patAddr + 7 + relOffset;

		auto sys = Memory->Read<SchemaSystem>(schemaSystemPtr);

		for (int i = 0; i < sys.type_scopes.size; i++) {
			auto scopePtr = (uintptr_t)sys.type_scopes.GetElement(i);
			auto scope = Memory->Read<SchemaSystemTypeScope>(scopePtr);

			std::string scopeName(scope.name);
			if (scopeName.find(std::string(("client.dll"))) == std::string::npos)
				continue;

			uint64_t bindingsAddr = scopePtr + 0x560;
			auto classes = scope.class_bindings.Enumerate(bindingsAddr);

			Console::Info(("  Scope '%s': %zu classes"), scopeName.c_str(), classes.size());

			for (auto classPtr : classes) {
				auto cls = Memory->Read<SchemaClassBinding>((uint64_t)classPtr);
				if (!cls.name || cls.field_count <= 0)
					continue;

				char nameBuf[256]{};
				Memory->ReadRaw((uintptr_t)cls.name, nameBuf, 255);
				std::string className(nameBuf);

				SchemaMap[className] = {};

				for (int f = 0; f < cls.field_count; f++) {
					uint64_t fieldAddr = (uint64_t)cls.fields + (f * sizeof(SchemaClassFieldData));
					auto field = Memory->Read<SchemaClassFieldData>(fieldAddr);
					if (!field.name) continue;

					char fieldNameBuf[256]{};
					Memory->ReadRaw((uintptr_t)field.name, fieldNameBuf, 255);
					SchemaMap[className][std::string(fieldNameBuf)] = field.offset;
				}
			}
		}

		Console::Success(("  Dumped %zu classes"), SchemaMap.size());
		VMP_END;
		return true;
	}

	void ResolveSchemaOffsets() {
		VMP_BEGIN_MUTATE("resolve_schemas");
		Schema.m_iTeamNum = ResolveSchema(std::string(("C_BaseEntity")), std::string(("m_iTeamNum")));
		Schema.m_lifeState = ResolveSchema(std::string(("C_BaseEntity")), std::string(("m_lifeState")));
		Schema.m_iHealth = ResolveSchema(std::string(("C_BaseEntity")), std::string(("m_iHealth")));
		Schema.m_hPlayerPawn = ResolveSchema(std::string(("CCSPlayerController")), std::string(("m_hPlayerPawn")));
		Schema.m_hPawn = ResolveSchema(std::string(("CBasePlayerController")), std::string(("m_hPawn")));
		Schema.m_vOldOrigin = ResolveSchema(std::string(("C_BasePlayerPawn")), std::string(("m_vOldOrigin")));
		Schema.m_pGameSceneNode = ResolveSchema(std::string(("C_BaseEntity")), std::string(("m_pGameSceneNode")));
		Schema.m_entitySpottedState = ResolveSchema(std::string(("C_CSPlayerPawn")), std::string(("m_entitySpottedState")));
		Schema.m_iIDEntIndex = ResolveSchema(std::string(("C_CSPlayerPawn")), std::string(("m_iIDEntIndex")));
		Schema.m_pInGameMoneyServices = ResolveSchema(std::string(("CCSPlayerController")), std::string(("m_pInGameMoneyServices")));
		Schema.m_iAccount = ResolveSchema(std::string(("CCSPlayerController_InGameMoneyServices")), std::string(("m_iAccount")));
		Schema.m_sSanitizedPlayerName = ResolveSchema(std::string(("CCSPlayerController")), std::string(("m_sSanitizedPlayerName")));
		Schema.m_flFlashDuration = ResolveSchema(std::string(("C_CSPlayerPawnBase")), std::string(("m_flFlashDuration")));
		// Em updates recentes do CS2 a Valve removeu m_aimPunchAngle do pawn
		// e moveu pra CPlayer_CameraServices::m_vecCsViewPunchAngle. Caminho
		// novo: pawn -> m_pCameraServices -> m_vecCsViewPunchAngle.
		// Resolvido em ambos os locais — RCS prefere o servico (path nova),
		// fallback pro field antigo se presente.
		Schema.m_aimPunchAngle = ResolveSchema(std::string(("C_CSPlayerPawnBase")), std::string(("m_aimPunchAngle")));
		if (Schema.m_aimPunchAngle == 0)
			Schema.m_aimPunchAngle = ResolveSchema(std::string(("C_CSPlayerPawn")), std::string(("m_aimPunchAngle")));
		Schema.m_pCameraServices = ResolveSchema(std::string(("C_BasePlayerPawn")), std::string(("m_pCameraServices")));
		Schema.m_vecCsViewPunchAngle = ResolveSchema(std::string(("CPlayer_CameraServices")), std::string(("m_vecCsViewPunchAngle")));
		Schema.m_iShotsFired = ResolveSchema(std::string(("C_CSPlayerPawn")), std::string(("m_iShotsFired")));
		Schema.m_ArmorValue = ResolveSchema(std::string(("C_CSPlayerPawn")), std::string(("m_ArmorValue")));
		Schema.m_pObserverServices = ResolveSchema(std::string(("C_BasePlayerPawn")), std::string(("m_pObserverServices")));
		Schema.m_hObserverTarget = ResolveSchema(std::string(("CPlayer_ObserverServices")), std::string(("m_hObserverTarget")));
		Schema.m_modelState = ResolveSchema(std::string(("CSkeletonInstance")), std::string(("m_modelState")));
		Schema.m_pWeaponServices = ResolveSchema(std::string(("C_BasePlayerPawn")), std::string(("m_pWeaponServices")));
		Schema.m_pClippingWeapon = ResolveSchema(std::string(("CPlayer_WeaponServices")), std::string(("m_hActiveWeapon")));
		Schema.m_vecVelocity = ResolveSchema(std::string(("C_BaseEntity")), std::string(("m_vecVelocity")));
		Schema.m_vecAbsVelocity = ResolveSchema(std::string(("C_BaseEntity")), std::string(("m_vecAbsVelocity")));
		Schema.m_vecViewOffset = ResolveSchema(std::string(("C_BaseModelEntity")), std::string(("m_vecViewOffset")));
		Schema.m_flThrowStrength = ResolveSchema(std::string(("C_BaseCSGrenade")), std::string(("m_flThrowStrength")));
		Schema.m_nSubclassID = ResolveSchema(std::string(("C_BaseEntity")), std::string(("m_nSubclassID")));
		Schema.m_flThrowVelocity = ResolveSchema(std::string(("CCSWeaponBaseVData")), std::string(("m_flThrowVelocity")));
		Schema.m_AttributeManager = ResolveSchema(std::string(("C_EconEntity")), std::string(("m_AttributeManager")));
		Schema.m_Item = ResolveSchema(std::string(("C_AttributeContainer")), std::string(("m_Item")));
		Schema.m_iItemDefinitionIndex = ResolveSchema(std::string(("CEconItemView")), std::string(("m_iItemDefinitionIndex")));
		if (!Schema.m_iItemDefinitionIndex)
			Schema.m_iItemDefinitionIndex = ResolveSchema(std::string(("C_EconItemView")), std::string(("m_iItemDefinitionIndex")));

		Schema.m_angEyeAngles = ResolveSchema(std::string(("C_CSPlayerPawnBase")), std::string(("m_angEyeAngles")));
		if (!Schema.m_angEyeAngles)
			Schema.m_angEyeAngles = ResolveSchema(std::string(("C_CSPlayerPawn")), std::string(("m_angEyeAngles")));
		Schema.m_bIsScoped = ResolveSchema(std::string(("C_CSPlayerPawn")), std::string(("m_bIsScoped")));
		if (!Schema.m_bIsScoped)
			Schema.m_bIsScoped = ResolveSchema(std::string(("C_CSPlayerPawnBase")), std::string(("m_bIsScoped")));
		Schema.m_flFlashBangTime = ResolveSchema(std::string(("C_CSPlayerPawnBase")), std::string(("m_flFlashBangTime")));
		if (!Schema.m_flFlashBangTime)
			Schema.m_flFlashBangTime = ResolveSchema(std::string(("C_CSPlayerPawn")), std::string(("m_flFlashBangTime")));
		Schema.m_hActiveWeapon = ResolveSchema(std::string(("CPlayer_WeaponServices")), std::string(("m_hActiveWeapon")));

		Schema.m_bBombDefused = ResolveSchema(std::string(("C_PlantedC4")), std::string(("m_bBombDefused")));
		Schema.m_flDefuseCountDown = ResolveSchema(std::string(("C_PlantedC4")), std::string(("m_flDefuseCountDown")));
		Schema.m_flC4Blow = ResolveSchema(std::string(("C_PlantedC4")), std::string(("m_flC4Blow")));
		Schema.m_bBeingDefused = ResolveSchema(std::string(("C_PlantedC4")), std::string(("m_bBeingDefused")));
		Schema.m_nBombSite = ResolveSchema(std::string(("C_PlantedC4")), std::string(("m_nBombSite")));

#ifdef OXYGEN_LOGS
		Console::Info(("[schema] m_iTeamNum=0x%X m_lifeState=0x%X m_iHealth=0x%X"),
			Schema.m_iTeamNum, Schema.m_lifeState, Schema.m_iHealth);
		Console::Info(("[schema] m_hPlayerPawn=0x%X m_hPawn=0x%X m_vOldOrigin=0x%X"),
			Schema.m_hPlayerPawn, Schema.m_hPawn, Schema.m_vOldOrigin);
		Console::Info(("[schema] m_pGameSceneNode=0x%X m_modelState=0x%X"),
			Schema.m_pGameSceneNode, Schema.m_modelState);
		Console::Info(("[schema] m_angEyeAngles=0x%X m_aimPunchAngle=0x%X m_vecViewOffset=0x%X"),
			Schema.m_angEyeAngles, Schema.m_aimPunchAngle, Schema.m_vecViewOffset);
		Console::Info(("[schema] m_pWeaponServices=0x%X m_hActiveWeapon=0x%X"),
			Schema.m_pWeaponServices, Schema.m_hActiveWeapon);
		Console::Info(("[schema] m_AttributeManager=0x%X m_Item=0x%X m_iItemDefinitionIndex=0x%X"),
			Schema.m_AttributeManager, Schema.m_Item, Schema.m_iItemDefinitionIndex);
		Console::Info(("[schema] m_vecVelocity=0x%X m_vecAbsVelocity=0x%X"),
			Schema.m_vecVelocity, Schema.m_vecAbsVelocity);
		Console::Info(("[schema] m_iIDEntIndex=0x%X m_entitySpottedState=0x%X"),
			Schema.m_iIDEntIndex, Schema.m_entitySpottedState);

		int zeros = 0;
		if (!Schema.m_iTeamNum) { Console::Error(("[schema] m_iTeamNum=0 !!")); zeros++; }
		if (!Schema.m_lifeState) { Console::Error(("[schema] m_lifeState=0 !!")); zeros++; }
		if (!Schema.m_iHealth) { Console::Error(("[schema] m_iHealth=0 !!")); zeros++; }
		if (!Schema.m_hPlayerPawn) { Console::Error(("[schema] m_hPlayerPawn=0 !!")); zeros++; }
		if (!Schema.m_vOldOrigin) { Console::Error(("[schema] m_vOldOrigin=0 !!")); zeros++; }
		if (!Schema.m_pGameSceneNode) { Console::Error(("[schema] m_pGameSceneNode=0 !!")); zeros++; }
		if (!Schema.m_modelState) { Console::Error(("[schema] m_modelState=0 !!")); zeros++; }
		if (zeros)
			Console::Warn(("[schema] %d critical schemas are ZERO — ESP/aim will not work"), zeros);
		else
			Console::Success(("[schema] All critical schemas resolved"));
#endif
		VMP_END;
	}

	bool Dump() {
		if (!DumpOffsets()) return false;
		if (!DumpSchemas()) return false;
		ResolveSchemaOffsets();
		PrintLeagueOffsets();
		Console::Success(("Dump complete"));
		return true;
	}

	void PrintLeagueOffsets() {
		Console::Info("================================================");
		Console::Info("  zimo cs2-league — copiar para offsets.h");
		Console::Info("================================================");
		Console::Info("");
		Console::Info("namespace Offsets {");
		Console::Info("    inline uintptr_t dwEntityList              = 0x%llX;", Offsets.dwEntityList);
		Console::Info("    inline uintptr_t dwLocalPlayerController   = 0x%llX;", Offsets.dwLocalPlayerController);
		Console::Info("    inline uintptr_t dwLocalPlayerPawn         = 0x%llX;", Offsets.dwLocalPlayerPawn);
		Console::Info("    inline uintptr_t dwGlobalVars              = 0x%llX;", Offsets.dwGlobalVars);
		Console::Info("}");
		Console::Info("");
		Console::Info("namespace Schema {");
		Console::Info("    inline uintptr_t m_iTeamNum                = 0x%X;", Schema.m_iTeamNum);
		Console::Info("    inline uintptr_t m_iHealth                 = 0x%X;", Schema.m_iHealth);
		Console::Info("    inline uintptr_t m_lifeState               = 0x%X;", Schema.m_lifeState);
		Console::Info("    inline uintptr_t m_vOldOrigin              = 0x%X;", Schema.m_vOldOrigin);
		Console::Info("    inline uintptr_t m_pGameSceneNode          = 0x%X;", Schema.m_pGameSceneNode);
		Console::Info("    inline uintptr_t m_hPlayerPawn             = 0x%X;", Schema.m_hPlayerPawn);
		Console::Info("    inline uintptr_t m_sSanitizedPlayerName    = 0x%X;", Schema.m_sSanitizedPlayerName);
		Console::Info("    inline uintptr_t m_pInGameMoneyServices    = 0x%X;", Schema.m_pInGameMoneyServices);
		Console::Info("    inline uintptr_t m_iAccount                = 0x%X;", Schema.m_iAccount);
		Console::Info("    inline uintptr_t m_ArmorValue              = 0x%X;", Schema.m_ArmorValue);
		Console::Info("    inline uintptr_t m_angEyeAngles            = 0x%X;", Schema.m_angEyeAngles);
		Console::Info("    inline uintptr_t m_bIsScoped               = 0x%X;", Schema.m_bIsScoped);
		Console::Info("    inline uintptr_t m_flFlashBangTime         = 0x%X;", Schema.m_flFlashBangTime);
		Console::Info("    inline uintptr_t m_entitySpottedState      = 0x%X;", Schema.m_entitySpottedState);
		Console::Info("    inline uintptr_t m_bSpotted                = 0x8;");
		Console::Info("    inline uintptr_t m_vecVelocity             = 0x%X;", Schema.m_vecVelocity);
		Console::Info("    inline uintptr_t m_vecViewOffset           = 0x%X;", Schema.m_vecViewOffset);
		Console::Info("    inline uintptr_t m_modelState              = 0x%X;", Schema.m_modelState);
		Console::Info("    inline uintptr_t m_pWeaponServices         = 0x%X;", Schema.m_pWeaponServices);
		Console::Info("    inline uintptr_t m_hActiveWeapon           = 0x%X;", Schema.m_hActiveWeapon);
		Console::Info("    inline uintptr_t m_AttributeManager        = 0x%X;", Schema.m_AttributeManager);
		Console::Info("    inline uintptr_t m_Item                    = 0x%X;", Schema.m_Item);
		Console::Info("    inline uintptr_t m_iItemDefinitionIndex    = 0x%X;", Schema.m_iItemDefinitionIndex);
		Console::Info("}");
		Console::Info("================================================");
	}
}
