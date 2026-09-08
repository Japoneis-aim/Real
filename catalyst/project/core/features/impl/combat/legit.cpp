#include <stdafx.hpp>

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

		const auto eye_pos = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );

		if ( valid_weapon && settings::g_combat.m_other.penetration_crosshair )
			this->draw_penetration_crosshair( draw_list, eye_pos, view_angles );

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

		const auto eye_pos = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );
		const auto players = systems::g_collector.players( );

		if ( ctx.weapon_type == cstypes::weapon_type::taser && !ctx.is_reloading && ctx.weapon_ready )
		{
			if ( settings::g_combat.m_other.m_zeusbot.enabled )
				this->zeusbot( eye_pos, view_angles, players );
		}

		const auto valid_weapon = cstypes::is_weapon_valid( ctx.weapon_type );
		if ( !valid_weapon )
			return;

		const auto& cfg = settings::g_combat.get( ctx.weapon_type );

		if ( ctx.is_reloading || !ctx.weapon_ready )
			return;

		if ( cfg.aimbot.enabled )
		{
			const auto target = this->select_target( eye_pos, view_angles, players, cfg );
			if ( target.player )
				this->aimbot( eye_pos, view_angles, target, cfg.aimbot );
		}

		if ( cfg.triggerbot.enabled )
			this->triggerbot( eye_pos, view_angles, players, cfg.triggerbot );
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

			const auto bones = systems::g_bones.get( player.bone_cache );
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
	// AIMBOT
	// ============================================================================

	void legit::aimbot(
		const math::vector3& eye_pos,
		const math::vector3& view_angles,
		const target& tgt,
		const settings::combat::aimbot& cfg )
	{
		if ( !( GetAsyncKeyState( cfg.key ) & 0x8000 ) )
		{
			this->m_aim_error = {};
			return;
		}

		const auto deg_per_pixel = this->calculate_deg_per_pixel( );
		if ( deg_per_pixel <= 0.0f )
		{
			this->m_aim_error = {};
			return;
		}

		const auto freshest = systems::g_bones.get( tgt.player->bone_cache );
		if ( !freshest.is_valid( ) )
		{
			this->m_aim_error = {};
			return;
		}

		auto aim_point = freshest.get_position( tgt.player->hitboxes.entries[ tgt.hitbox ].bone );

		if ( cfg.predictive )
		{
			const auto velocity = g::memory.read<math::vector3>(
				tgt.player->pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash )
			);
			aim_point = aim_point + velocity * g_shared.get_prediction_time( );
		}

		this->apply_aim( eye_pos, view_angles, aim_point, deg_per_pixel, cfg );
	}

	float legit::calculate_deg_per_pixel( ) const
	{
		constexpr auto m_yaw{ 0.022f };
		const auto sensitivity = systems::g_convars.get<float>( CONVAR( "sensitivity"_hash ) );
		const auto fov_adjust = g::memory.read<float>(
			systems::g_local.pawn( ) + SCHEMA( "C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash )
		);
		return sensitivity * m_yaw * fov_adjust;
	}

	void legit::apply_aim(
		const math::vector3& eye_pos,
		const math::vector3& view_angles,
		const math::vector3& aim_point,
		float deg_per_pixel,
		const settings::combat::aimbot& cfg )
	{
		const auto desired = math::helpers::calculate_angle( eye_pos, aim_point );
		auto delta_x = desired.x - view_angles.x;
		auto delta_y = math::helpers::normalize_yaw( desired.y - view_angles.y );
		const auto delta_length = std::sqrtf( delta_x * delta_x + delta_y * delta_y );

		if ( delta_length < 0.001f )
		{
			this->m_aim_error = {};
			return;
		}

		if ( cfg.smoothing > 1 )
		{
			delta_x = this->apply_smoothing( delta_x, delta_length, cfg.smoothing );
			delta_y = this->apply_smoothing( delta_y, delta_length, cfg.smoothing );
		}

		const auto want_x = -delta_y + this->m_aim_error.x;
		const auto want_y = delta_x + this->m_aim_error.y;

		const auto counts_x = std::roundf( want_x / deg_per_pixel );
		const auto counts_y = std::roundf( want_y / deg_per_pixel );

		this->m_aim_error.x = want_x - counts_x * deg_per_pixel;
		this->m_aim_error.y = want_y - counts_y * deg_per_pixel;

		if ( static_cast<int>( counts_x ) != 0 || static_cast<int>( counts_y ) != 0 )
			g::input.inject_mouse( static_cast<int>( counts_x ), static_cast<int>( counts_y ), input::move );
	}

	float legit::apply_smoothing( float delta, float delta_length, int smoothing ) const
	{
		const auto base_smooth = static_cast<float>( smoothing );
		const float proximity = std::clamp( 1.0f / ( delta_length + 0.1f ), 0.0f, 1.0f );
		float smooth_factor = ( 1.0f / base_smooth ) * ( 1.0f - ( proximity * 0.4f ) );
		smooth_factor *= random::normal_clamped( 1.0f, 0.03f, 0.97f, 1.03f );
		return delta * smooth_factor;
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
	// STUBS
	// ============================================================================

	void legit::draw_penetration_crosshair( zdraw::draw_list&, const math::vector3&, const math::vector3& ) { }
	void legit::draw_fov( zdraw::draw_list& draw_list, const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::aimbot& cfg )
	{
		// Verifica flag e alpha (m_fov_alpha já controla visibilidade)
		if ( !cfg.draw_fov )
			return;

		const auto col = cfg.fov_color.value;
		if ( col.a < 8 )
			return;

		// Converter configuração de FOV (presumida em graus) para pixels.
		// Base simplificada: 90 graus -> screen_width pixels
		const auto display = zdraw::get_display_size();
		const float screen_w = static_cast<float>( display.first );
		const float fov_deg = static_cast<float>( cfg.fov );
		const float fov_px = fov_deg * ( screen_w / 90.0f );

		// Centro da tela
		const float cx = screen_w * 0.5f;
		const float cy = static_cast<float>( display.second ) * 0.5f;

		// Desenha círculo FOV
		draw_list.add_circle( cx, cy, fov_px, col, 64, 1.5f );
	}
	void legit::zeusbot( const math::vector3&, const math::vector3&, const std::vector<systems::collector::player>& ) { }

	legit::trigger_result legit::trace_crosshair( const math::vector3&, const math::vector3&, const std::vector<systems::collector::player>&, const settings::combat::triggerbot& ) const
	{
		return {};
	}

} // namespace features::combat
