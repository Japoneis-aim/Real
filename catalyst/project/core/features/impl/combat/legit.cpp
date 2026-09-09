#include <stdafx.hpp>
#include "aim_controller.hpp"

namespace features::combat {

	// ============================================================================
	// RENDER
	// ============================================================================

	void legit::on_render( zdraw::draw_list& draw_list )
	{
		const auto& ctx = g_shared.ctx( );
		if ( !ctx.valid )
			return;

		const auto valid_weapon = cstypes::is_weapon_valid( ctx.weapon_type );
		const auto& cfg = settings::g_combat.get( ctx.weapon_type );

		// Cache de offsets do punch angle — feito uma única vez.
		// CS2 atual: punch está em pawn → m_pCameraServices → m_vecCsViewPunchAngle.
		// Guardamos os dois paths; o tick usará CameraServices se disponível, com
		// fallback para m_aimPunchAngle direto no pawn (path antigo, pré-CS2 atual).
		if ( !this->m_offsets_cached ) {
			this->m_aim_punch_offset       = SCHEMA( "C_CSPlayerPawn",         "m_aimPunchAngle"_hash );
			this->m_camera_services_offset = SCHEMA( "C_BasePlayerPawn",       "m_pCameraServices"_hash );
			this->m_view_punch_offset      = SCHEMA( "CPlayer_CameraServices", "m_vecCsViewPunchAngle"_hash );
			this->m_offsets_cached = true;
		}

		const auto eye_pos = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );

		if ( valid_weapon && settings::g_combat.m_other.penetration_crosshair )
		{
			// draw_penetration_crosshair stub removido — funcionalidade provida pela wallbang_indicator
		}

		this->m_fov_alpha.set_target(
			valid_weapon && cfg.aimbot.draw_fov && cfg.aimbot.enabled ? 1.0f : 0.0f
		);
		this->m_fov_alpha.update( );

		if ( this->m_fov_alpha.value( ) <= 0.01f )
			return;

		this->draw_fov( draw_list, eye_pos, view_angles, cfg.aimbot );
	}

	// ============================================================================
	// TICK
	// ============================================================================

	void legit::tick( )
	{
		this->ensure_rng_seeded( );
		this->update_trigger_state( );

		const auto& ctx = g_shared.ctx( );
		if ( !ctx.valid )
			return;

		const auto valid_weapon = cstypes::is_weapon_valid( ctx.weapon_type );
		if ( !valid_weapon )
			return;

		const auto& cfg = settings::g_combat.get( ctx.weapon_type );

		if ( ctx.is_reloading || !ctx.weapon_ready )
			return;

		// Verifica se a tecla de ativação está pressionada
		const bool aim_key_held    = ( ::GetAsyncKeyState( static_cast<int>( cfg.aimbot.key ) ) & 0x8000 ) != 0;
		const bool trigger_key_held = ( ::GetAsyncKeyState( static_cast<int>( cfg.triggerbot.key ) ) & 0x8000 ) != 0;

		// Toggle do aim-assist/aimbot por tecla dedicada
		if ( ::GetAsyncKeyState( static_cast<int>( cfg.aimbot.toggle_key ) ) & 1 )
			features::combat::g_aimbot_enabled = !features::combat::g_aimbot_enabled;

		const bool aimbot_enabled_flag = static_cast<bool>( cfg.aimbot.enabled ) && features::combat::g_aimbot_enabled;
		const bool aimbot_active  = aimbot_enabled_flag && aim_key_held;
		const bool assist_active  = static_cast<bool>( cfg.aimbot.assist_mode ) && aim_key_held && features::combat::g_aimbot_enabled;
		const bool trigger_active = static_cast<bool>( cfg.triggerbot.enabled ) && trigger_key_held;

		if ( !aimbot_active && !assist_active && !trigger_active )
			return;

		const auto eye_pos    = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );

		// Usa with_players para evitar cópia do vetor completo (~7.5 KB)
		systems::g_collector.with_players( [&]( const std::vector<systems::collector::player>& players )
		{

		if ( ctx.weapon_type == cstypes::weapon_type::taser && !ctx.is_reloading && ctx.weapon_ready )
		{
			// zeusbot stub removido — não implementado
		}

		// ======================================================================
		// AIM PIPELINE — Só roda se aimbot ou assist estiverem ativos E
		// houver um alvo dentro do FOV configurado.
		// ======================================================================
		if ( aimbot_active || assist_active )
		{
			const auto target = this->select_target( eye_pos, view_angles, players, cfg );

			if ( target.player )
			{
				AimConfig aim_cfg{};
				aim_cfg.aimbot_enabled   = aimbot_active;
				aim_cfg.assist_enabled   = assist_active;
				aim_cfg.use_assist_only  = assist_active && !aimbot_active;

				// rcs_enabled removido — RCS gerenciado por g_rcs.tick()
				aim_cfg.rcs_scale        = static_cast<float>( cfg.aimbot.rcs_strength ) / 100.0f;

				aim_cfg.smooth_enabled   = static_cast<int>( cfg.aimbot.smoothing ) > 1;
				aim_cfg.smooth_amount    = static_cast<float>( cfg.aimbot.smoothing );

				aim_cfg.humanize_enabled = static_cast<bool>( cfg.aimbot.humanize );
				aim_cfg.humanize_strength = static_cast<float>( cfg.aimbot.humanize_strength );

				aim_cfg.aim_fov          = static_cast<float>( cfg.aimbot.fov );
				aim_cfg.assist_strength  = static_cast<float>( cfg.aimbot.assist_strength );

				const auto deg_per_pixel = this->calculate_deg_per_pixel( );
				if ( deg_per_pixel > 0.0f )
				{
					const auto now       = ctx.current_time;
					const float delta_time = ( this->m_last_frame_time > 0.0f )
						? ( now - this->m_last_frame_time )
						: ( 1.0f / 128.0f ); // thread de combat roda a 128Hz
					this->m_last_frame_time = now;

					if ( !this->m_aim_controller.is_offsets_loaded( ) )
						this->m_aim_controller.initialize_offsets( );

					this->m_aim_controller.on_frame(
						eye_pos, view_angles, target.aim_point,
						deg_per_pixel, delta_time, aim_cfg,
						static_cast<const void*>( target.player )
					);
				}
			}
			else
			{
				// Sem alvo: resetar m_last_frame_time para evitar delta enorme
				// quando um alvo aparecer depois de um intervalo longo
				this->m_last_frame_time = 0.0f;
			}
		}

		// Triggerbot é independente do aim pipeline
		if ( trigger_active )
			this->triggerbot( eye_pos, view_angles, players, cfg.triggerbot );

		} ); // with_players
	}

	// ============================================================================
	// TARGET SELECTION
	// ============================================================================

	legit::target legit::select_target(
		const math::vector3& eye_pos,
		const math::vector3& view_angles,
		const std::vector<systems::collector::player>& players,
		const settings::combat::group_config& cfg ) const
	{
		target best{};
		float best_score = std::numeric_limits<float>::max( );

		for ( const auto& player : players )
		{
			if ( !this->is_valid_target( player ) )
				continue;

			// Usa bones já cacheados pelo collector — evita segunda leitura de memória
			const auto& bones = player.cached_bones;
			if ( !bones.is_valid( ) )
				continue;

			auto damage{ 0.0f };
			auto hitbox{ -1 };
			auto penetrated{ false };

			const auto aim_point = this->get_aim_point(
				eye_pos, player, bones, cfg, damage, hitbox, penetrated
			);

			if ( hitbox < 0 )
				continue;

			const auto fov = this->get_fov( view_angles, eye_pos, aim_point );
			if ( fov > static_cast<float>( cfg.aimbot.fov ) )
				continue;

			const auto score = this->calculate_target_score( fov, eye_pos, aim_point );
			if ( score < best_score )
			{
				best_score = score;
				best = this->build_target( player, bones, aim_point, hitbox, damage, fov, penetrated );
			}
		}

		return best;
	}

	bool legit::is_valid_target( const systems::collector::player& player ) const
	{
		return systems::g_local.is_enemy( player.team )
			&& !player.invulnerable
			&& player.hitboxes.count > 0;
	}

	float legit::calculate_target_score( float fov, const math::vector3& eye_pos, const math::vector3& aim_point ) const
	{
		const auto dist = ( aim_point - eye_pos ).length( );
		return fov + ( dist * 0.001f );
	}

	legit::target legit::build_target(
		const systems::collector::player& player,
		const systems::bones::data& bones,
		const math::vector3& aim_point,
		int hitbox,
		float damage,
		float fov,
		bool penetrated ) const
	{
		target result{};
		result.player = &player;
		result.bones = bones;
		result.aim_point = aim_point;
		result.hitbox = hitbox;
		result.damage = damage;
		result.fov = fov;
		result.penetrated = penetrated;
		return result;
	}

	// ============================================================================
	// AIM POINT
	// ============================================================================

	math::vector3 legit::get_aim_point(
		const math::vector3& eye_pos,
		const systems::collector::player& player,
		const systems::bones::data& bones,
		const settings::combat::group_config& cfg,
		float& out_damage,
		int& out_hitbox,
		bool& out_penetrated ) const
	{
		out_hitbox = -1;
		float best_dmg = -1.0f;
		math::vector3 best_pos{};

		for ( const auto& hb : player.hitboxes )
		{
			if ( !this->is_valid_hitbox( hb, cfg ) )
				continue;

			const auto pos = bones.get_position( hb.bone );
			const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
			const auto current_dmg = combat::g_shared.pen( ).get_max_damage(
				hitgroup, player.armor, player.has_helmet, player.team
			);

			if ( cfg.aimbot.visible_only )
			{
				if ( !this->try_visible_hitbox( eye_pos, pos, player, bones, hb, cfg, current_dmg, best_dmg, best_pos, out_hitbox, out_penetrated ) )
					continue;
			}
			else
			{
				if ( current_dmg > best_dmg )
				{
					best_dmg = current_dmg;
					best_pos = pos;
					out_hitbox = hb.index;
					out_penetrated = false;
				}
			}
		}

		out_damage = best_dmg;
		return best_pos;
	}

	bool legit::is_valid_hitbox( const systems::collector::hitbox& hb, const settings::combat::group_config& cfg ) const
	{
		if ( hb.index < 0 || hb.bone < 0 )
			return false;

		if ( cfg.aimbot.head_only && hb.index > 1 )
			return false;

		return true;
	}

	bool legit::try_visible_hitbox(
		const math::vector3& eye_pos,
		const math::vector3& pos,
		const systems::collector::player& player,
		const systems::bones::data& bones,
		const systems::collector::hitbox& hb,
		const settings::combat::group_config& cfg,
		float current_dmg,
		float& best_dmg,
		math::vector3& best_pos,
		int& out_hitbox,
		bool& out_penetrated ) const
	{
		const auto trace = systems::g_bvh.trace_ray( eye_pos, pos );
		const auto is_visible = !trace.hit || trace.fraction > 0.97f;

		if ( cfg.aimbot.autowall )
		{
			shared::penetration::result pen_result{};
			if ( !combat::g_shared.pen( ).run( eye_pos, pos, player, bones, pen_result ) )
				return false;

			if ( pen_result.damage <= best_dmg || pen_result.damage < cfg.aimbot.min_damage )
				return false;

			best_dmg = pen_result.damage;
			best_pos = pos;
			out_hitbox = pen_result.hitbox;
			out_penetrated = pen_result.penetrated;
			return true;
		}

		if ( !is_visible )
			return false;

		if ( current_dmg > best_dmg )
		{
			best_dmg = current_dmg;
			best_pos = pos;
			out_hitbox = hb.index;
			out_penetrated = false;
		}

		return true;
	}

	// ============================================================================
	// AIMBOT (REMOVIDO: A lógica agora é 100% do AimController no tick())
	// ============================================================================

	float legit::calculate_deg_per_pixel( ) const
	{
		const auto pawn = systems::g_local.pawn( );
		if ( !pawn )
			return 0.0f; // proteção: jogador morto ou inválido

		constexpr auto m_yaw{ 0.022f };
		const auto sensitivity = systems::g_convars.get<float>( CONVAR( "sensitivity"_hash ) );
		const auto fov_adjust = g::memory.read<float>(
			pawn + SCHEMA( "C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash )
		);
		return sensitivity * m_yaw * fov_adjust;
	}

	// ============================================================================
	// TRIGGERBOT
	// ============================================================================

	void legit::triggerbot(
		const math::vector3& eye_pos,
		const math::vector3& view_angles,
		const std::vector<systems::collector::player>& players,
		const settings::combat::triggerbot& cfg )
	{
		if ( this->m_trigger_held )
			return;

		// Gate: cursor visível = jogo sem foco (menu, scoreboard, console, dead).
		// Não injetar input nesses estados.
		if ( systems::g_local.is_cursor_visible( ) )
		{
			this->m_trigger_waiting = false;
			return;
		}

		// Gate: velocidade Z alta = player no ar (pulo/queda).
		// Não disparar no ar — spread enorme e óbvio nos demos.
		constexpr float k_max_z_velocity = 100.f;
		if ( std::fabsf( systems::g_local.velocity_z( ) ) > k_max_z_velocity )
		{
			this->m_trigger_waiting = false;
			return;
		}

		if ( !( GetAsyncKeyState( cfg.key ) & 0x8000 ) )
		{
			this->m_trigger_waiting = false;
			return;
		}

		const auto& ctx = g_shared.ctx( );
		if ( !ctx.weapon_ready )
		{
			this->m_trigger_waiting = false;
			return;
		}

		const auto result = this->trace_crosshair( eye_pos, view_angles, players, cfg );
		if ( !result.player )
		{
			this->m_trigger_waiting = false;
			return;
		}

		if ( result.penetrated && result.damage < cfg.min_damage )
		{
			this->m_trigger_waiting = false;
			return;
		}

		const auto dynamic_delay = this->calculate_trigger_delay( eye_pos, view_angles, result, cfg );
		const auto now = ctx.current_time;

		// -1.0f sinaliza hitchance insuficiente — não deve atirar ainda
		if ( dynamic_delay < 0.0f )
		{
			this->m_trigger_waiting = false;
			return;
		}

		if ( !this->m_trigger_waiting )
		{
			this->m_trigger_waiting = true;
			this->m_trigger_delay_end = now + dynamic_delay * 0.001f;
			return;
		}

		if ( now < this->m_trigger_delay_end )
			return;

		this->execute_trigger( now );
	}

	float legit::calculate_trigger_delay(
		const math::vector3& eye_pos,
		const math::vector3& view_angles,
		const trigger_result& result,
		const settings::combat::triggerbot& cfg ) const
	{
		float delay = static_cast<float>( cfg.delay );

		if ( cfg.hitchance <= 0.0f || g_shared.is_weapon_max_accuracy( ) )
			return delay;

		const auto required = cfg.hitchance / 100.0f;
		const auto hc = g_shared.calculate_hitchance( eye_pos, view_angles, *result.player, result.bones );

		if ( hc < required )
			return -1.0f; // sinaliza que não deve atirar

		delay += ( 1.0f - hc ) * 50.0f;
		return delay;
	}

	void legit::execute_trigger( float now )
	{
		this->m_trigger_waiting = false;
		const auto hold_ms = this->m_rng.random_float( 50.0f, 120.0f );

		g::input.inject_mouse( 0, 0, input::left_down );
		this->m_trigger_held = true;
		this->m_trigger_release_time = now + hold_ms * 0.001f;
	}

	// ============================================================================
	// HELPERS
	// ============================================================================

	void legit::ensure_rng_seeded( )
	{
		if ( this->m_rng_seeded )
			return;

		const auto seed = static_cast<int>(
			std::chrono::steady_clock::now( ).time_since_epoch( ).count( ) & 0x7fffffff
		);
		this->m_rng.seed( seed );
		this->m_rng_seeded = true;
	}

	void legit::update_trigger_state( )
	{
		if ( !this->m_trigger_held )
			return;

		const auto& ctx = g_shared.ctx( );
		if ( !ctx.valid || ctx.current_time >= this->m_trigger_release_time )
		{
			g::input.inject_mouse( 0, 0, input::left_up );
			this->m_trigger_held = false;
		}
	}

	float legit::get_fov( const math::vector3& view_angles, const math::vector3& eye_pos, const math::vector3& target_pos ) const
	{
		const auto desired = math::helpers::calculate_angle( eye_pos, target_pos );
		const auto dx = desired.x - view_angles.x;
		const auto dy = math::helpers::normalize_yaw( desired.y - view_angles.y );
		return std::sqrtf( dx * dx + dy * dy );
	}

	// ============================================================================
	// STUBS REMOVIDOS
	// ============================================================================
	// zeusbot() — não implementado, removido do tick()
	// draw_penetration_crosshair() — não implementado; funcionalidade provida por wallbang_indicator

	// ============================================================================
	// DRAW FOV
	// ============================================================================

	void legit::draw_fov( zdraw::draw_list& draw_list, const math::vector3& /*eye_pos*/, const math::vector3& /*view_angles*/, const settings::combat::aimbot& cfg )
	{
		// Verifica flag e alpha (m_fov_alpha já controla visibilidade no caller)
		if ( !cfg.draw_fov )
			return;

		const auto col = cfg.fov_color.value;
		if ( col.a < 8 )
			return;

		// Converter FOV configurado (graus) para pixels usando a projeção perspectiva.
		// Fórmula correta: raio_px = tan(fov_deg/2) / tan(camera_fov/2) * (screen_w/2)
		// Isso respeita a natureza angular do FOV (ao contrário da divisão linear).
		const auto display    = zdraw::get_display_size( );
		const float screen_w  = static_cast<float>( display.first );
		const float camera_fov = systems::g_view.has_camera( ) ? systems::g_view.fov( ) : 90.0f;
		const float fov_deg   = static_cast<float>( cfg.fov );

		const float half_cam_rad = math::helpers::deg_to_rad( camera_fov * 0.5f );
		const float half_fov_rad = math::helpers::deg_to_rad( fov_deg    * 0.5f );
		const float fov_px       = std::tanf( half_fov_rad ) / std::tanf( half_cam_rad ) * ( screen_w * 0.5f );

		// Centro da tela
		const float cx = screen_w * 0.5f;
		const float cy = static_cast<float>( display.second ) * 0.5f;

		// Desenha círculo FOV
		draw_list.add_circle( cx, cy, fov_px, col, 64, 1.5f );
	}

	legit::trigger_result legit::trace_crosshair(
		const math::vector3& eye_pos,
		const math::vector3& view_angles,
		const std::vector<systems::collector::player>& players,
		const settings::combat::triggerbot& cfg ) const
	{
		// Calcula o forward vector a partir dos ângulos de visão
		math::vector3 forward, right, up;
		math::helpers::angle_vectors( view_angles, forward, right, up );

		// Seleciona o alvo mais próximo na mira — não o primeiro do vetor.
		// O vetor já vem ordenado por distância (mais próximo primeiro),
		// mas iteramos todos para garantir que pegamos o menor em caso de sobreposição.
		const systems::collector::player* best_player    = nullptr;
		systems::bones::data              best_bones{};
		int                               best_hitbox    = -1;
		int                               best_hitgroup  = -1;
		float                             best_damage    = 0.f;
		bool                              best_penetrated = false;
		float                             best_dist      = std::numeric_limits<float>::max( );

		for ( const auto& player : players )
		{
			if ( !systems::g_local.is_enemy( player.team ) )
				continue;

			if ( player.invulnerable || player.hitboxes.count <= 0 )
				continue;

			// Usa bones já cacheados pelo collector — evita segunda leitura de memória
			const auto& bones = player.cached_bones;
			if ( !bones.is_valid( ) )
				continue;

			const float player_dist = ( player.origin - eye_pos ).length( );

			// Otimização: se este jogador já é mais longe que o melhor atual, skip
			if ( player_dist >= best_dist )
				continue;

			for ( const auto& hb : player.hitboxes )
			{
				if ( hb.index < 0 || hb.bone < 0 || hb.radius <= 0.0f )
					continue;

				// Centro da hitbox no espaço mundo
				const auto& bone        = bones.bones[ hb.bone ];
				const auto center_local = ( hb.mins + hb.maxs ) * 0.5f;
				const auto center_world = bone.position + math::helpers::rotate_by_quat( bone.rotation, center_local );

				// Teste de colisão raio x cápsula: verifica se a mira aponta para esta hitbox
				if ( !g_shared.ray_hits_capsule( eye_pos, forward, bone.position, center_world, hb.radius ) )
					continue;

				// Verificar obstrução por BVH (geometria do mapa)
				const auto trace       = systems::g_bvh.trace_ray( eye_pos, center_world );
				const bool has_wall    = trace.hit && ( trace.distance < ( center_world - eye_pos ).length( ) - 1.0f );
				const auto hitgroup    = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );

				shared::penetration::result pen_result{};

				if ( has_wall )
				{
					// Há parede — só atira se autowall estiver ativo e o dano for suficiente
					if ( !cfg.autowall )
						continue;

					if ( !g_shared.pen( ).run( eye_pos, center_world, player, bones, pen_result ) )
						continue;

					if ( pen_result.damage < cfg.min_damage )
						continue;
				}
				else
				{
					// Linha de visão limpa — calcula dano máximo teórico
					pen_result.damage    = g_shared.pen( ).get_max_damage( hitgroup, player.armor, player.has_helmet, player.team );
					pen_result.hitbox    = hb.index;
					pen_result.penetrated = false;

					if ( pen_result.damage < cfg.min_damage )
						continue;
				}

				// Candidato válido — atualiza se for o mais próximo encontrado
				best_dist       = player_dist;
				best_player     = &player;
				best_bones      = bones;
				best_hitbox     = hb.index;
				best_hitgroup   = hitgroup;
				best_damage     = pen_result.damage;
				best_penetrated = has_wall;
				break; // basta um hitbox válido por jogador
			}
		}

		if ( best_player )
		{
			trigger_result result{};
			result.player     = best_player;
			result.bones      = best_bones;
			result.hitbox     = best_hitbox;
			result.hitgroup   = best_hitgroup;
			result.damage     = best_damage;
			result.penetrated = best_penetrated;
			return result;
		}

		// Nenhum alvo encontrado
		return {};
	}

	// legit has no manual destructor; m_aim_controller is a direct member

} // namespace features::combat
