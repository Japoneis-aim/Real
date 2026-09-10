#include <stdafx.hpp>

namespace systems {

	void local::update( )
	{
		const auto local_controller = g::memory.read<std::uintptr_t>( g::offsets.local_player_controller );
		if ( !local_controller )
		{
			this->reset( );
			return;
		}

		const auto player_pawn_handle = g::memory.read<std::uint32_t>( local_controller + SCHEMA( "CCSPlayerController", "m_hPlayerPawn"_hash ) );
		if ( !player_pawn_handle )
		{
			this->reset( );
			return;
		}

		const auto player_pawn = systems::g_entities.lookup( player_pawn_handle );
		if ( !player_pawn )
		{
			this->reset( );
			return;
		}

		this->m_controller.store( local_controller );
		this->m_pawn.store( player_pawn );

		// ── Batch read do pawn local ──────────────────────────────────────────
		// Agrupa team, health, velocity, flash_time, weapon_services em 1 syscall.
		// read_batch_fixed<0x1400>: buffer inline, zero heap, roda a 200 Hz.
		const auto pawn_batch = g::memory.read_batch_fixed<0x1400>( player_pawn );
		if ( !pawn_batch.valid )
		{
			this->reset( );
			return;
		}

		const auto team_num = pawn_batch.get<int>( SCHEMA( "C_BaseEntity", "m_iTeamNum"_hash ) );
		this->m_team.store( team_num );

		const auto health = pawn_batch.get<int>( SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) );
		this->m_alive.store( health > 0 );

		const auto velocity = pawn_batch.get<math::vector3>( SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
		this->m_velocity_z.store( velocity.z );
		this->m_velocity_xy.store( std::sqrtf( velocity.x * velocity.x + velocity.y * velocity.y ) );

		// No-flash via batch — escreve de volta se necessário
		if ( settings::g_misc.m_no_flash.enabled )
		{
			const auto flash_time_off = SCHEMA( "C_CSPlayerPawnBase", "m_flFlashBangTime"_hash );
			if ( flash_time_off )
			{
				const auto cur_flash = pawn_batch.get<float>( flash_time_off );
				if ( cur_flash > 0.0f )
				{
					const float target = cur_flash * std::clamp( settings::g_misc.m_no_flash.opacity.value, 0.f, 1.f );
					g::memory.write<float>( player_pawn + flash_time_off, target );
				}
			}
		}

		if ( this->m_alive.load( ) )
		{
			this->m_view_team.store( team_num );
			this->m_observer_pawn.store( 0 );

			{
				const auto weapon_services = pawn_batch.get<std::uintptr_t>( SCHEMA( "C_BasePlayerPawn", "m_pWeaponServices"_hash ) );
				if ( !weapon_services )
				{
					this->m_weapon.store( 0 );
					this->m_weapon_vdata.store( 0 );
					this->m_weapon_type.store( 0 );
					goto update_ffa;
				}

				const auto active_weapon_handle = g::memory.read<std::uint32_t>( weapon_services + SCHEMA( "CPlayer_WeaponServices", "m_hActiveWeapon"_hash ) );
				if ( !active_weapon_handle || active_weapon_handle == 0xFFFFFFFF )
				{
					this->m_weapon.store( 0 );
					this->m_weapon_vdata.store( 0 );
					this->m_weapon_type.store( 0 );
					goto update_ffa;
				}

				const auto active_weapon = systems::g_entities.lookup( active_weapon_handle );
				if ( !active_weapon )
				{
					this->m_weapon.store( 0 );
					this->m_weapon_vdata.store( 0 );
					this->m_weapon_type.store( 0 );
					goto update_ffa;
				}

				this->m_weapon.store( active_weapon );

				const auto vdata = g::memory.read<std::uintptr_t>( active_weapon + SCHEMA( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
				if ( !vdata )
				{
					this->m_weapon_vdata.store( 0 );
					this->m_weapon_type.store( 0 );
					goto update_ffa;
				}

				this->m_weapon_vdata.store( vdata );
				this->m_weapon_type.store( g::memory.read<std::uint32_t>( vdata + SCHEMA( "CCSWeaponBaseVData", "m_WeaponType"_hash ) ) );
			}
		}
		else
		{
			this->m_weapon.store( 0 );
			this->m_weapon_vdata.store( 0 );
			this->m_weapon_type.store( 0 );

			const auto observer_pawn_handle = g::memory.read<std::uint32_t>( local_controller + SCHEMA( "CCSPlayerController", "m_hObserverPawn"_hash ) );
			if ( !observer_pawn_handle ) goto update_ffa;

			const auto observer_pawn = systems::g_entities.lookup( observer_pawn_handle );
			if ( !observer_pawn ) goto update_ffa;

			const auto observer_services = g::memory.read<std::uintptr_t>( observer_pawn + SCHEMA( "C_BasePlayerPawn", "m_pObserverServices"_hash ) );
			if ( !observer_services ) goto update_ffa;

			const auto observer_target_handle = g::memory.read<std::uint32_t>( observer_services + SCHEMA( "CPlayer_ObserverServices", "m_hObserverTarget"_hash ) );
			if ( !observer_target_handle || observer_target_handle == 0xFFFFFFFF ) goto update_ffa;

			const auto observer_target = systems::g_entities.lookup( observer_target_handle );
			if ( !observer_target ) goto update_ffa;

			this->m_observer_pawn.store( observer_target );
			this->m_view_team.store( g::memory.read<int>( observer_target + SCHEMA( "C_BaseEntity", "m_iTeamNum"_hash ) ) );
		}

		update_ffa:
		{
			// game_type/game_mode cacheados — convars raramente mudam mid-game.
			// Sem cache: 2 leituras de memória a 200 Hz = 400 reads/s desnecessários.
			// Re-lê apenas a cada k_ffa_cache_ticks ticks (~1 s a 200 Hz).
			static int s_game_type{ -1 };
			static int s_game_mode{ -1 };
			static int s_ffa_cache_counter{ 0 };
			constexpr int k_ffa_cache_ticks = 200; // revalida a cada ~1 s

			if ( s_ffa_cache_counter <= 0 || s_game_type < 0 )
			{
				s_game_type = systems::g_convars.get<int>( CONVAR( "game_type"_hash ) );
				s_game_mode = systems::g_convars.get<int>( CONVAR( "game_mode"_hash ) );
				s_ffa_cache_counter = k_ffa_cache_ticks;
			}
			else
			{
				--s_ffa_cache_counter;
			}

			const auto is_ffa = ( s_game_type == 1 && s_game_mode == 2 )
			                 || ( s_game_type == 2 && s_game_mode == 0 );
			this->m_team_mode.store( !is_ffa );
		}
	}

	void local::reset( )
	{
		this->m_controller.store( 0 );
		this->m_pawn.store( 0 );
		this->m_observer_pawn.store( 0 );
		this->m_team.store( 0 );
		this->m_view_team.store( 0 );
		this->m_alive.store( false );
		this->m_weapon.store( 0 );
		this->m_weapon_vdata.store( 0 );
		this->m_weapon_type.store( 0 );
		this->m_velocity_z.store( 0.f );
		this->m_velocity_xy.store( 0.f );
	}

} // namespace systems