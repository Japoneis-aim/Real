#include <stdafx.hpp>

namespace systems {

	// ============================================================================
	// Player validation helper
	// ============================================================================

	// Returns true if player_pawn is a valid, living, non-local player pawn
	// that should be processed by the collector.
	//
	// Centralises the 3 early-out checks that were previously copy-pasted in
	// collect_players (and duplicated in any feature that iterates players):
	//   1. Pointer valid
	//   2. Not the local player's view pawn
	//   3. Alive (health > 0)
	[[nodiscard]] static bool is_relevant_player( std::uintptr_t player_pawn )
	{
		if ( !player_pawn )
			return false;

		if ( player_pawn == systems::g_local.view_pawn( ) )
			return false;

		const auto health = g::memory.read<int>( player_pawn + SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) );
		return health > 0;
	}

	// ============================================================================
	// collector::run
	// ============================================================================

	void collector::run( )
	{
		const auto raw = systems::g_entities.all( );

		this->collect_players( raw );
		this->collect_items( raw );
		this->collect_projectiles( raw );
	}

	std::vector<collector::player> collector::players( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return this->m_players;
	}

	std::vector<collector::item> collector::items( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return this->m_items;
	}

	std::vector<collector::projectile> collector::projectiles( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return this->m_projectiles;
	}

	void collector::collect_players( const std::vector<entities::cached>& raw )
	{
		std::vector<player> fresh{};
		fresh.reserve( 64 );

		for ( const auto& entry : raw )
		{
			if ( entry.type != entities::type::player )
			{
				continue;
			}

			const auto player_pawn_handle = g::memory.read<std::uint32_t>( entry.ptr + SCHEMA( "CCSPlayerController", "m_hPlayerPawn"_hash ) );
			if ( !player_pawn_handle )
			{
				continue;
			}

			const auto player_pawn = systems::g_entities.lookup( player_pawn_handle );

			if ( !player_pawn || player_pawn == systems::g_local.view_pawn( ) )
				continue;

			player p{};
			p.controller = entry.ptr;
			p.pawn = player_pawn;

			// ── Batch read do pawn: 1 ReadProcessMemory, buffer inline no stack ──
			// read_batch_fixed<0x1400>: std::array — zero alocação heap por jogador por frame.
			// Feito ANTES do filtro de saúde para evitar TOCTOU: is_relevant_player()
			// lia health individualmente, depois o batch lia de novo — entre as duas
			// leituras o jogador podia morrer, passando o filtro com health > 0 mas
			// chegando com dados de ragdoll (bones colapsadas no chão).
			const auto pawn_batch = g::memory.read_batch_fixed<0x1400>( player_pawn );

			if ( !pawn_batch.valid )
				continue;

			// ── Dormancy check ────────────────────────────────────────────────
			// m_bDormant: jogadores fora do PVS ficam com posições congeladas.
			// Sem esse check o ESP mostra posições erradas quando o jogador
			// está fora do campo de visão do servidor (out-of-PVS).
			{
				const auto dormant_off = SCHEMA( "CGameSceneNode", "m_bDormant"_hash );
				if ( dormant_off )
				{
					if ( pawn_batch.get<bool>( dormant_off ) )
						continue; // jogador dormente — pular completamente
				}
			}

			p.health = pawn_batch.get<int>( SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) );

			// Filtro de vida: health <= 0 = morto/ragdoll — não renderizar.
			// m_lifeState != 0 = morto/despawning mesmo com health > 0 momentaneamente.
			if ( p.health <= 0 )
				continue;

			{
				const auto life_state_off = SCHEMA( "C_BaseEntity", "m_lifeState"_hash );
				if ( life_state_off )
				{
					const auto life_state = pawn_batch.get<std::uint8_t>( life_state_off );
					if ( life_state != 0 ) // 0 = LIFE_ALIVE
						continue;
				}
			}
			p.team         = pawn_batch.get<int>(  SCHEMA( "C_BaseEntity",     "m_iTeamNum"_hash ) );
			p.invulnerable = pawn_batch.get<bool>( SCHEMA( "C_CSPlayerPawn",   "m_bGunGameImmunity"_hash ) );
			p.armor        = pawn_batch.get<int>(  SCHEMA( "C_CSPlayerPawn",   "m_ArmorValue"_hash ) );
			p.is_scoped    = pawn_batch.get<bool>( SCHEMA( "C_CSPlayerPawn",   "m_bIsScoped"_hash ) );
			p.is_defusing  = pawn_batch.get<bool>( SCHEMA( "C_CSPlayerPawn",   "m_bIsDefusing"_hash ) );
			p.eye_angles   = pawn_batch.get<math::vector3>( SCHEMA( "C_CSPlayerPawn", "m_angEyeAngles"_hash ) );

			// m_flFlashBangTime: determina se o jogador está flashado (> 0 = flashado)
			{
				const auto flash_off = SCHEMA( "C_CSPlayerPawnBase", "m_flFlashBangTime"_hash );
				p.is_flashed = flash_off ? ( pawn_batch.get<float>( flash_off ) > 0.0f ) : false;
			}

			if ( const auto shots_off = SCHEMA( "C_CSPlayerPawn", "m_iShotsFired"_hash ) )
				p.shots_fired = pawn_batch.get<int>( shots_off );

			// game_scene_node — ponteiro, precisa de leitura separada
			const auto game_scene_node = pawn_batch.get<std::uintptr_t>( SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );

			// ── Batch read do controller: ping, bot flag, name ptr, money ptr ──
			// read_batch_fixed<0x800>: buffer inline — zero alocação heap por jogador.
			const auto ctrl_batch = g::memory.read_batch_fixed<0x800>( entry.ptr );

			p.ping = ctrl_batch.get<int>( SCHEMA( "CCSPlayerController", "m_iPing"_hash ) );

			{
				const auto controlled_by_human_off = SCHEMA( "CCSPlayerController", "m_bIsControlledByHuman"_hash );
				p.is_bot = controlled_by_human_off ? !ctrl_batch.get<bool>( controlled_by_human_off ) : false;
			}
			if ( game_scene_node )
			{
				p.game_scene_node = game_scene_node;
				p.bone_cache = g::memory.read<std::uintptr_t>( game_scene_node + SCHEMA( "CSkeletonInstance", "m_modelState"_hash ) + 0x80 );
				p.origin = g::memory.read<math::vector3>( game_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );

				{
					// Bones: sempre relê todo frame (1 syscall, ~2 KB).
					// Cache de bones foi removido porque o CS2 reutiliza o mesmo
					// bone_cache ptr após respawn de bot — nenhuma das condições de
					// dirty-flag (pawn changed, bone_ptr changed, teleport) disparava,
					// deixando o esqueleto na posição de morte/spawn anterior.
					p.cached_bones = systems::g_bones.get( p.bone_cache );

					// Hitboxes: cache por controller ptr, invalidado por pawn ou
					// bone_cache ptr. Hitboxes mudam só ao trocar de modelo (raro),
					// então o cache aqui é seguro e vale o custo evitado.
					auto& bh = this->m_bone_hitbox_cache[ entry.ptr ];

					const bool pawn_changed     = ( bh.last_pawn != player_pawn );
					const bool bone_ptr_changed = ( bh.last_bone_cache_ptr != p.bone_cache );

					if ( pawn_changed || bone_ptr_changed )
					{
						bh.cached_hitboxes     = systems::g_hitboxes.query( game_scene_node );
						bh.last_bone_cache_ptr = p.bone_cache;
						bh.last_pawn           = player_pawn;
					}

					p.hitboxes = bh.cached_hitboxes;

					// is_visible multi-ponto: testa cabeça (bone 6), peito (bone 4) e pelvis (bone 1).
					// Considerar visível se QUALQUER ponto passar — evita que o player seja marcado
					// como occluded quando só a cabeça está atrás de uma parede mas o torso não.
					const auto view_org = systems::g_view.origin( );

					// Early-out por distância: skip raycasts BVH se jogador está além de 3000u.
					// Raycasts são caros (~0.5-1µs cada); para targets distantes a visibilidade
					// não afeta o comportamento do ESP (cor já é occluded = semi-transparente).
					constexpr float k_visibility_skip_dist_sq = 3000.0f * 3000.0f;
					const auto dist_sq = ( p.origin - view_org ).length_sqr( );
					if ( dist_sq > k_visibility_skip_dist_sq )
					{
						p.is_visible = false;
						bh.vis_valid = false; // força re-check se jogador se aproximar
					}
					else
					{
						// Cache de visibilidade com dirty flag por movimento.
						// Só refaz os 3 raycasts BVH se o jogador OU a câmera se moveu > threshold.
						// Em frames estáticos (jogador parado ou ADing) elimina ~90% dos raycasts.
						// threshold = 5u (quadrado = 25u²) — imperceptível mas diferencia posições.
						const float player_moved_sq = ( p.origin  - bh.last_vis_player_origin ).length_sqr( );
						const float cam_moved_sq    = ( view_org  - bh.last_vis_view_origin    ).length_sqr( );

						if ( bh.vis_valid
							&& player_moved_sq <= k_vis_origin_threshold_sq
							&& cam_moved_sq    <= k_vis_origin_threshold_sq )
						{
							// Cache válido — reutiliza o resultado do frame anterior sem raycast
							p.is_visible = bh.cached_is_visible;
						}
						else
						{
							// Cache inválido ou primeiro frame — executa os 3 raycasts
							const auto check_visible = [ & ]( std::uint32_t bone_idx ) -> bool {
								const auto pos = p.cached_bones.get_position( bone_idx );
								if ( pos.length_sqr( ) < 1.0f ) return false;
								return !systems::g_bvh.trace_ray( view_org, pos ).hit;
							};
							const bool is_vis = check_visible( 6 ) || check_visible( 4 ) || check_visible( 1 );

							// Persiste no cache para os próximos frames
							bh.cached_is_visible      = is_vis;
							bh.last_vis_player_origin = p.origin;
							bh.last_vis_view_origin   = view_org;
							bh.vis_valid              = true;

							p.is_visible = is_vis;
						}
					}
				}
			}

			const auto item_services = pawn_batch.get<std::uintptr_t>( SCHEMA( "C_BasePlayerPawn", "m_pItemServices"_hash ) );
			if ( item_services )
			{
				// read_batch_fixed<64>: buffer inline — zero alocação heap.
				// has_helmet e has_defuser estão a ~8 bytes de distância — 64 bytes cobre ambos.
				const auto item_svc_batch = g::memory.read_batch_fixed<64>( item_services );
				if ( item_svc_batch.valid )
				{
					p.has_helmet  = item_svc_batch.get<bool>( SCHEMA( "CCSPlayer_ItemServices", "m_bHasHelmet"_hash ) );
					p.has_defuser = item_svc_batch.get<bool>( SCHEMA( "CCSPlayer_ItemServices", "m_bHasDefuser"_hash ) );
				}
			}

			const auto weapon_services = pawn_batch.get<std::uintptr_t>( SCHEMA( "C_BasePlayerPawn", "m_pWeaponServices"_hash ) );
			if ( weapon_services )
			{
				const auto active_weapon_handle = g::memory.read<std::uint32_t>( weapon_services + SCHEMA( "CPlayer_WeaponServices", "m_hActiveWeapon"_hash ) );
				if ( active_weapon_handle && active_weapon_handle != 0xFFFFFFFF )
				{
					p.weapon.ptr = systems::g_entities.lookup( active_weapon_handle );
					if ( p.weapon.ptr )
					{
						p.weapon.vdata = g::memory.read<std::uintptr_t>( p.weapon.ptr + SCHEMA( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
						if ( p.weapon.vdata )
						{
							p.weapon.ammo = g::memory.read<int>( p.weapon.ptr + SCHEMA( "C_BasePlayerWeapon", "m_iClip1"_hash ) );
							p.weapon.max_ammo = g::memory.read<int>( p.weapon.vdata + SCHEMA( "CBasePlayerWeaponVData", "m_iMaxClip1"_hash ) );

							// Cache do nome da arma por weapon.ptr — o nome não muda enquanto
							// o jogador não trocar de arma (ptr diferente = cache miss = releitura).
							// Evita read_string + alocação de string a cada frame por jogador.
							// Cache invalidado quando o BVH muda de mapa (mapa novo = armas novas).
							// O cache é membro da classe (m_weapon_name_cache / m_weapon_name_bvh_count)
							// para ter lifetime controlado e não depender de inicialização de static local.
							const auto bvh_count = systems::g_bvh.count( );
							if ( bvh_count != this->m_weapon_name_bvh_count )
							{
								this->m_weapon_name_cache.clear( );
								this->m_weapon_name_bvh_count = bvh_count;
							}
							auto it = this->m_weapon_name_cache.find( p.weapon.ptr );
							if ( it != this->m_weapon_name_cache.end( ) )
							{
								p.weapon.name = it->second;
							}
							else
							{
								const auto weapon_name_ptr = g::memory.read<std::uintptr_t>( p.weapon.vdata + SCHEMA( "CCSWeaponBaseVData", "m_szName"_hash ) );
								if ( weapon_name_ptr )
								{
									p.weapon.name = g::memory.read_string( weapon_name_ptr, 64 );
									if ( p.weapon.name.starts_with( "weapon_" ) )
										p.weapon.name.erase( 0, 7 );

									// Limita o cache a 128 entradas para não vazar memória indefinidamente
									if ( this->m_weapon_name_cache.size( ) >= 128 )
										this->m_weapon_name_cache.clear( );

									this->m_weapon_name_cache.emplace( p.weapon.ptr, p.weapon.name );
								}
							}
						}
					}
				}
			}

			const auto name_ptr = ctrl_batch.get<std::uintptr_t>( SCHEMA( "CCSPlayerController", "m_sSanitizedPlayerName"_hash ) );
			if ( name_ptr )
			{
				p.display_name = g::memory.read_string( name_ptr, 128 );
				std::ranges::transform( p.display_name, p.display_name.begin( ), [ ]( unsigned char c ) { return std::tolower( c ); } );
			}

			const auto money_services = ctrl_batch.get<std::uintptr_t>( SCHEMA( "CCSPlayerController", "m_pInGameMoneyServices"_hash ) );
			if ( money_services )
			{
				p.money = g::memory.read<int>( money_services + SCHEMA( "CCSPlayerController_InGameMoneyServices", "m_iAccount"_hash ) );
			}

			fresh.push_back( std::move( p ) );
		}

		{
			// Ordena do mais próximo para o mais distante.
			// O triggerbot (trace_crosshair) retorna o primeiro hit válido,
			// então os jogadores mais próximos devem vir primeiro no vetor.
			//
			// Insertion sort em vez de std::ranges::sort (O(n log n)):
			//   - Com 10 jogadores e ordem quase estável entre frames, insertion sort
			//     é O(n) no melhor caso (já ordenado) e tem constante menor que sort.
			//   - Pré-computa dist² para cada player uma única vez (evita recalcular
			//     n*log(n) vezes como o lambda faria no sort padrão).
			//   - stable: ordem relativa de jogadores equidistantes é preservada.
			const auto view_origin = systems::g_view.origin( );

			// Pré-calcula dist² para cada player (evita recomputar no comparador)
			const auto n = static_cast<int>( fresh.size( ) );
			std::array<float, 64> dist_sq_cache{};
			for ( int i = 0; i < n && i < 64; ++i )
				dist_sq_cache[ i ] = ( fresh[ i ].origin - view_origin ).length_sqr( );

			// Insertion sort estável — O(n) se já ordenado, O(n²) no pior caso
			// (raro: jogadores raramente trocam de posição relativa entre frames)
			for ( int i = 1; i < n; ++i )
			{
				player key = std::move( fresh[ i ] );
				float key_dist = dist_sq_cache[ i ];
				int j = i - 1;
				while ( j >= 0 && dist_sq_cache[ j ] > key_dist )
				{
					fresh[ j + 1 ] = std::move( fresh[ j ] );
					dist_sq_cache[ j + 1 ] = dist_sq_cache[ j ];
					--j;
				}
				fresh[ j + 1 ] = std::move( key );
				dist_sq_cache[ j + 1 ] = key_dist;
			}
		}

		std::unique_lock lock( this->m_mutex );
		this->m_players = std::move( fresh );
	}

	void collector::collect_items( const std::vector<entities::cached>& raw )
	{
		std::vector<item> fresh{};
		fresh.reserve( 64 );

		for ( const auto& entry : raw )
		{
			if ( entry.type != entities::type::item )
			{
				continue;
			}

			const auto subtype = classify_item( entry.schema_hash );
			if ( subtype == item_subtype::unknown )
			{
				continue;
			}

			// Batch único de entry.ptr: agrupa owner_handle + game_scene_node + ammo + vdata_ptr
			// em 1 ReadProcessMemory em vez de 4-5 reads individuais.
			// read_batch_fixed<0x800>: buffer inline, zero heap por item por frame.
			const auto entity_batch = g::memory.read_batch_fixed<0x800>( entry.ptr );
			if ( !entity_batch.valid )
				continue;

			const auto owner_handle = entity_batch.get<std::uint32_t>( SCHEMA( "C_BaseEntity", "m_hOwnerEntity"_hash ) );
			if ( owner_handle && owner_handle != 0xffffffff )
			{
				continue;
			}

			const auto game_scene_node = entity_batch.get<std::uintptr_t>( SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
			if ( !game_scene_node )
			{
				continue;
			}

			item i{};
			i.entity = entry.ptr;
			i.game_scene_node = game_scene_node;
			i.subtype = subtype;
			i.origin = g::memory.read<math::vector3>( game_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );

			// Reutiliza o mesmo batch para ammo e vdata ptr — sem syscall extra.
			i.ammo = entity_batch.get<int>( SCHEMA( "C_BasePlayerWeapon", "m_iClip1"_hash ) );

			const auto weapon_vdata = entity_batch.get<std::uintptr_t>(
				SCHEMA( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
			if ( weapon_vdata )
			{
				i.max_ammo = g::memory.read<int>( weapon_vdata + SCHEMA( "CBasePlayerWeaponVData", "m_iMaxClip1"_hash ) );
			}

			fresh.push_back( std::move( i ) );
		}

		{
			const auto view_origin = systems::g_view.origin( );
			std::ranges::sort( fresh, [ &view_origin ]( const item& a, const item& b ) { return ( a.origin - view_origin ).length_sqr( ) > ( b.origin - view_origin ).length_sqr( ); } );
		}

		std::unique_lock lock( this->m_mutex );
		this->m_items = std::move( fresh );
	}

	void collector::collect_projectiles( const std::vector<entities::cached>& raw )
	{
		std::vector<projectile> fresh{};
		fresh.reserve( 32 );

		const auto gv_ptr = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( !gv_ptr ) return; // global_vars não disponível — evita read em endereço inválido
		const auto current_time = g::memory.read<float>( gv_ptr + cs2::global_vars_cur_time );

		for ( const auto& entry : raw )
		{
			if ( entry.type != entities::type::projectile )
			{
				continue;
			}

			const auto subtype = classify_projectile( entry.schema_hash );
			if ( subtype == projectile_subtype::unknown )
			{
				continue;
			}

			if ( subtype == projectile_subtype::molotov_fire )
			{
				const auto fire_count = g::memory.read<int>( entry.ptr + SCHEMA( "C_Inferno", "m_fireCount"_hash ) );
				if ( fire_count <= 0 )
				{
					continue;
				}

				const auto fire_positions_base = entry.ptr + SCHEMA( "C_Inferno", "m_firePositions"_hash );
				const auto fire_active_base = entry.ptr + SCHEMA( "C_Inferno", "m_bFireIsBurning"_hash );

				std::vector<math::vector3> fire_points{};
				fire_points.reserve( std::min( fire_count, 64 ) );

				for ( auto i = 0; i < std::min( fire_count, 64 ); ++i )
				{
					if ( !g::memory.read<bool>( fire_active_base + i ) )
					{
						continue;
					}

					fire_points.push_back( g::memory.read<math::vector3>( fire_positions_base + i * sizeof( math::vector3 ) ) );
				}

				if ( fire_points.empty( ) )
				{
					continue;
				}

				math::vector3 center{};

				for ( const auto& pt : fire_points )
				{
					center = center + pt;
				}

				center = center * ( 1.0f / static_cast< float >( fire_points.size( ) ) );

				const auto effect_tick = g::memory.read<int>( entry.ptr + SCHEMA( "C_Inferno", "m_nFireEffectTickBegin"_hash ) );
				const auto start_time = static_cast< float >( effect_tick ) * ( 1.0f / 64.0f );
				constexpr auto inferno_duration = 7.0f;

				projectile p{};
				p.entity = entry.ptr;
				p.subtype = subtype;
				p.origin = center;
				p.fire_points = std::move( fire_points );
				p.expire_time = start_time + inferno_duration;

				fresh.push_back( std::move( p ) );
				continue;
			}

			const auto game_scene_node = g::memory.read<std::uintptr_t>( entry.ptr + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
			if ( !game_scene_node )
			{
				continue;
			}

			projectile p{};
			p.entity = entry.ptr;
			p.game_scene_node = game_scene_node;
			p.subtype = subtype;
			p.origin = g::memory.read<math::vector3>( game_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
			p.velocity = g::memory.read<math::vector3>( entry.ptr + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
			p.thrower_handle = g::memory.read<std::uint32_t>( entry.ptr + SCHEMA( "C_BaseGrenade", "m_hThrower"_hash ) );
			p.bounces = g::memory.read<int>( entry.ptr + SCHEMA( "C_BaseCSGrenadeProjectile", "m_nBounces"_hash ) );

			if ( subtype == projectile_subtype::he_grenade || subtype == projectile_subtype::flashbang )
			{
				const auto detonate_tick = g::memory.read<int>( entry.ptr + SCHEMA( "C_BaseCSGrenadeProjectile", "m_nExplodeEffectTickBegin"_hash ) );
				p.detonated = detonate_tick > 0;
			}
			else if ( subtype == projectile_subtype::smoke_grenade )
			{
				p.effect_tick_begin = g::memory.read<int>( entry.ptr + SCHEMA( "C_SmokeGrenadeProjectile", "m_nSmokeEffectTickBegin"_hash ) );
				p.smoke_active = g::memory.read<bool>( entry.ptr + SCHEMA( "C_SmokeGrenadeProjectile", "m_bDidSmokeEffect"_hash ) );
			}
			else if ( subtype == projectile_subtype::decoy )
			{
				p.effect_tick_begin = g::memory.read<int>( entry.ptr + SCHEMA( "C_DecoyProjectile", "m_nDecoyShotTick"_hash ) );
			}

			fresh.push_back( std::move( p ) );
		}

		{
			const auto view_origin = systems::g_view.origin( );
			std::ranges::sort( fresh, [ &view_origin ]( const projectile& a, const projectile& b ) { return ( a.origin - view_origin ).length_sqr( ) > ( b.origin - view_origin ).length_sqr( ); } );
		}

		std::unique_lock lock( this->m_mutex );
		this->m_projectiles = std::move( fresh );
	}

	collector::item_subtype collector::classify_item( std::uint32_t schema_hash )
	{
		switch ( schema_hash )
		{
		case "C_AK47"_hash:               return item_subtype::ak47;
		case "C_WeaponM4A1"_hash:         return item_subtype::m4a4;
		case "C_WeaponM4A1Silencer"_hash: return item_subtype::m4a1s;
		case "C_WeaponAWP"_hash:          return item_subtype::awp;
		case "C_WeaponAug"_hash:          return item_subtype::aug;
		case "C_WeaponFamas"_hash:        return item_subtype::famas;
		case "C_WeaponGalilAR"_hash:      return item_subtype::galil_ar;
		case "C_WeaponSG556"_hash:        return item_subtype::sg553;
		case "C_WeaponG3SG1"_hash:        return item_subtype::g3sg1;
		case "C_WeaponSCAR20"_hash:       return item_subtype::scar20;
		case "C_WeaponSSG08"_hash:        return item_subtype::ssg08;
		case "C_WeaponMAC10"_hash:        return item_subtype::mac10;
		case "C_WeaponMP5SD"_hash:        return item_subtype::mp5sd;
		case "C_WeaponMP7"_hash:          return item_subtype::mp7;
		case "C_WeaponMP9"_hash:          return item_subtype::mp9;
		case "C_WeaponBizon"_hash:        return item_subtype::pp_bizon;
		case "C_WeaponP90"_hash:          return item_subtype::p90;
		case "C_WeaponUMP45"_hash:        return item_subtype::ump45;
		case "C_WeaponNOVA"_hash:         return item_subtype::nova;
		case "C_WeaponSawedoff"_hash:     return item_subtype::sawed_off;
		case "C_WeaponXM1014"_hash:       return item_subtype::xm1014;
		case "C_WeaponMag7"_hash:         return item_subtype::mag7;
		case "C_WeaponM249"_hash:         return item_subtype::m249;
		case "C_WeaponNegev"_hash:        return item_subtype::negev;
		case "C_DEagle"_hash:             return item_subtype::deagle;
		case "C_WeaponElite"_hash:        return item_subtype::dual_berettas;
		case "C_WeaponFiveSeven"_hash:    return item_subtype::five_seven;
		case "C_WeaponGlock"_hash:        return item_subtype::glock;
		case "C_WeaponHKP2000"_hash:      return item_subtype::p2000;
		case "C_WeaponUSPSilencer"_hash:  return item_subtype::usps;
		case "C_WeaponP250"_hash:         return item_subtype::p250;
		case "C_WeaponCZ75a"_hash:        return item_subtype::cz75;
		case "C_WeaponTec9"_hash:         return item_subtype::tec9;
		case "C_WeaponRevolver"_hash:     return item_subtype::r8_revolver;
		case "C_WeaponTaser"_hash:        return item_subtype::taser;
		case "C_Knife"_hash:              return item_subtype::knife;
		case "C_C4"_hash:                 return item_subtype::c4;
		case "C_Item_Healthshot"_hash:    return item_subtype::healthshot;
		case "C_HEGrenade"_hash:          return item_subtype::he_grenade;
		case "C_Flashbang"_hash:          return item_subtype::flashbang;
		case "C_SmokeGrenade"_hash:       return item_subtype::smoke_grenade;
		case "C_MolotovGrenade"_hash:     return item_subtype::molotov;
		case "C_IncendiaryGrenade"_hash:  return item_subtype::incendiary;
		case "C_DecoyGrenade"_hash:       return item_subtype::decoy;
		default:                          return item_subtype::unknown;
		}
	}

	collector::projectile_subtype collector::classify_projectile( std::uint32_t schema_hash )
	{
		switch ( schema_hash )
		{
		case "C_HEGrenadeProjectile"_hash:    return projectile_subtype::he_grenade;
		case "C_FlashbangProjectile"_hash:    return projectile_subtype::flashbang;
		case "C_SmokeGrenadeProjectile"_hash: return projectile_subtype::smoke_grenade;
		case "C_MolotovProjectile"_hash:      return projectile_subtype::molotov;
		case "C_Inferno"_hash:                return projectile_subtype::molotov_fire;
		case "C_DecoyProjectile"_hash:        return projectile_subtype::decoy;
		default:                              return projectile_subtype::unknown;
		}
	}

} // namespace systems