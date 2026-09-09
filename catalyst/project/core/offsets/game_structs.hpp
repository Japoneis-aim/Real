#pragma once

#include <cstddef>
#include "globals.hpp"  // global_vars_cur_time, global_vars_map_name

// ============================================================================
// AVISO: game_structs.hpp contém offsets HARDCODED para CS2.
//
// !! NÃO usar estas constantes em código novo !!
// Use SCHEMA("ClassName", "field"_hash) para leitura de campos de struct.
// Os valores abaixo são mantidos apenas como referência e podem estar
// DESATUALIZADOS após atualizações do jogo.
//
// Exceção: global_vars_cur_time e global_vars_map_name foram movidos para
// globals.hpp e são importados acima — continue usando cs2::global_vars_*.
// ============================================================================

namespace cs2 {

	// -------------------------------------------------------------------------
	// DEPRECATED — use SCHEMA() no lugar
	// -------------------------------------------------------------------------

	// C_BaseEntity (client.dll)
	constexpr std::uintptr_t m_iHealth         = 0x334;
	constexpr std::uintptr_t m_iTeamNum        = 0x3C3;
	constexpr std::uintptr_t m_lifeState       = 0x338;
	constexpr std::uintptr_t m_vOldOrigin      = 0x12B4;
	// m_vecAbsVelocity: o offset correto para projectiles é 0x3F8 (confirmado em offsets.map).
	// O valor anterior 0x12C0 era incorreto. Fonte: output/offsets.map (scanner dinâmico).
	constexpr std::uintptr_t m_vecAbsVelocity  = 0x3F8;
	constexpr std::uintptr_t m_vecViewOffset   = 0x12CC;
	constexpr std::uintptr_t m_fFlags          = 0x3C8;
	constexpr std::uintptr_t m_pGameSceneNode  = 0x328;
	constexpr std::uintptr_t m_nSubclassID     = 0x1A8;

	// C_BasePlayerPawn
	constexpr std::uintptr_t m_ArmorValue         = 0x136C;
	constexpr std::uintptr_t m_bGunGameImmunity   = 0x13C0;
	constexpr std::uintptr_t m_bIsScoped          = 0x13B4;
	constexpr std::uintptr_t m_bIsDefusing        = 0x13A0;
	constexpr std::uintptr_t m_flFlashBangTime    = 0x1340;
	constexpr std::uintptr_t m_pWeaponServices    = 0x1270;
	constexpr std::uintptr_t m_pItemServices      = 0x1268;

	// CCSPlayerController
	constexpr std::uintptr_t m_hPlayerPawn           = 0x5B8;
	constexpr std::uintptr_t m_szPlayerName          = 0x650;
	constexpr std::uintptr_t m_iPing                 = 0x674;
	constexpr std::uintptr_t m_sSanitizedPlayerName  = 0x6C0;
	constexpr std::uintptr_t m_pInGameMoneyServices  = 0x7B8;

	// C_BasePlayerWeapon
	constexpr std::uintptr_t m_iClip1              = 0x1480;
	constexpr std::uintptr_t m_nSubclassID_weapon  = 0x1A8;

	// C_CSWeaponBase
	constexpr std::uintptr_t m_flRecoilIndex         = 0x18B0;
	constexpr std::uintptr_t m_bInReload             = 0x18A0;
	constexpr std::uintptr_t m_fLastShotTime         = 0x1890;
	constexpr std::uintptr_t m_weaponMode            = 0x1880;
	constexpr std::uintptr_t m_fAccuracyPenalty      = 0x1870;
	constexpr std::uintptr_t m_flTurningInaccuracy   = 0x1860;

	// C_BaseCSGrenade
	constexpr std::uintptr_t m_bPinPulled      = 0x1680;
	constexpr std::uintptr_t m_fThrowTime      = 0x1690;
	constexpr std::uintptr_t m_flThrowStrength = 0x16A0;

	// C_BaseCSGrenadeProjectile
	constexpr std::uintptr_t m_vInitialPosition         = 0x1680;
	constexpr std::uintptr_t m_vInitialVelocity         = 0x1690;
	constexpr std::uintptr_t m_nBounces                 = 0x16C0;
	constexpr std::uintptr_t m_nExplodeEffectTickBegin  = 0x16D0;

	// C_SmokeGrenadeProjectile
	constexpr std::uintptr_t m_nSmokeEffectTickBegin = 0x16D0;
	constexpr std::uintptr_t m_bDidSmokeEffect       = 0x16E0;

	// C_DecoyProjectile
	constexpr std::uintptr_t m_nDecoyShotTick = 0x16D0;

	// C_Inferno
	constexpr std::uintptr_t m_fireCount               = 0x1680;
	constexpr std::uintptr_t m_firePositions           = 0x1690;
	constexpr std::uintptr_t m_bFireIsBurning          = 0x16D0;
	constexpr std::uintptr_t m_nFireEffectTickBegin    = 0x16E0;

} // namespace cs2
