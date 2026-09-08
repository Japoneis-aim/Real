#include <stdafx.hpp>

namespace features::combat {

	void legit::on_render( zdraw::draw_list& draw_list )
	{
		const auto eye_pos = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );

		const auto& ctx = g_shared.ctx( );
		const auto& cfg = settings::g_combat.get( ctx.weapon_type );

		if ( !ctx.valid )
		{
			return;
		}

		const auto valid_weapon = cstypes::is_weapon_valid( ctx.weapon_type );
		if ( valid_weapon && settings::g_combat.m_other.penetration_crosshair )
		{
			this->draw_penetration_crosshair( draw_list, eye_pos, view_angles );
		}

		this->m_fov_alpha.set_target( valid_weapon && cfg.aimbot.draw_fov && cfg.aimbot.enabled ? 1.0f : 0.0f );
		this->m_fov_alpha.update( );

		if ( this->m_fov_alpha.value( ) <= 0.01f )
		{
			return;
		}

		this->draw_fov( draw_list, eye_pos, view_angles, cfg.aimbot );
	}

	void legit::tick( )
	{
		if ( !this->m_rng_seeded )
		{
			this->m_rng.seed( static_cast< int >( std::chrono::steady_clock::now( ).time_since_epoch( ).count( ) & 0x7fffffff ) );
			this->m_rng_seeded = true;
		}

		if ( this->m_trigger_held )
		{
			const auto& ctx = g_shared.ctx( );
			if ( !ctx.valid || ctx.current_time >= this->m_trigger_release_time )
			{
				g::input.inject_mouse( 0, 0, input::left_up );
				this->m_trigger_held = false;
			}
		}

		const auto& ctx = g_shared.ctx( );
		if ( !ctx.valid )
		{
			return;
		}

		const auto eye_pos = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );
		const auto players = systems::g_collector.players( );

		if ( ctx.weapon_type == cstypes::weapon_type::taser && !ctx.is_reloading && ctx.weapon_ready )
		{
			if ( settings::g_combat.m_other.m_zeusbot.enabled )
			{
				this->zeusbot( eye_pos, view_angles, players );
			}
		}

		const auto valid_weapon = cstypes::is_weapon_valid( ctx.weapon_type );
		const auto& cfg = settings::g_combat.get( ctx.weapon_type );

		if ( !valid_weapon )
		{
			return;
		}

		if ( !ctx.is_reloading && ctx.weapon_ready )
		{
			if ( cfg.aimbot.enabled )
			{
				const auto target = this->select_target( eye_pos, view_angles, players, cfg );
				if ( target.player )
				{
					this->aimbot( eye_pos, view_angles, target, cfg.aimbot );
				}
			}

			if ( cfg.triggerbot.enabled )
			{
				this->triggerbot( eye_pos, view_angles, players, cfg.triggerbot );
			}
		}
	}

	legit::target legit::select_target( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::group_config& cfg ) const
	{
		target best{};
		best.fov = static_cast< float >( cfg.aimbot.fov );

		for ( const auto& player : players )
		{
			if ( !systems::g_local.is_enemy( player.team ) )
			{
				continue;
			}

			if ( player.invulnerable || player.hitboxes.count <= 0 )
			{
				continue;
			}

			const auto bones = systems::g_bones.get( player.bone_cache );
			if ( !bones.is_valid( ) )
			{
				continue;
			}

			auto damage{ 0.0f };
			auto hitbox{ -1 };
			auto penetrated{ false };

			const auto aim_point = this->get_aim_point( eye_pos, player, bones, cfg, damage, hitbox, penetrated );
			if ( hitbox < 0 )
			{
				continue;
			}

			const auto fov = this->get_fov( view_angles, eye_pos, aim_point );
			if ( fov > best.fov )
			{
				continue;
			}

			best.player = &player;
			best.bones = bones;
			best.aim_point = aim_point;
			best.hitbox = hitbox;
			best.damage = damage;
			best.fov = fov;
			best.penetrated = penetrated;
		}

		return best;
	}

	math::vector3 legit::get_aim_point( const math::vector3& eye_pos, const systems::collector::player& player, const systems::bones::data& bones, const settings::combat::group_config& cfg, float& out_damage, int& out_hitbox, bool& out_penetrated ) const
	{
		out_hitbox = -1;

		for ( const auto& hb : player.hitboxes )
		{
			if ( hb.index < 0 || hb.bone < 0 )
			{
				continue;
			}

			if ( cfg.aimbot.head_only && hb.index > 1 )
			{
				continue;
			}

			const auto pos = bones.get_position( hb.bone );
			const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );

			if ( !cfg.aimbot.visible_only )
			{
				out_damage = combat::g_shared.pen( ).get_max_damage( hitgroup, player.armor, player.has_helmet, player.team );
				out_hitbox = hb.index;
				out_penetrated = false;
				return pos;
			}

			const auto trace = systems::g_bvh.trace_ray( eye_pos, pos );
			const auto visible = !trace.hit || trace.fraction > 0.97f;

			if ( visible )
			{
				out_damage = combat::g_shared.pen( ).get_max_damage( hitgroup, player.armor, player.has_helmet, player.team );
				out_hitbox = hb.index;
				out_penetrated = false;
				return pos;
			}

			if ( cfg.aimbot.autowall )
			{
				shared::penetration::result pen_result{};
				if ( combat::g_shared.pen( ).run( eye_pos, pos, player, bones, pen_result ) )
				{
					if ( pen_result.damage >= cfg.aimbot.min_damage )
					{
						out_damage = pen_result.damage;
						out_hitbox = pen_result.hitbox;
						out_penetrated = pen_result.penetrated;
						return pos;
					}
				}
			}
		}

		return {};
	}

	float legit::get_fov( const math::vector3& view_angles, const math::vector3& eye_pos, const math::vector3& target_pos ) const
	{
		return math::helpers::calculate_fov( view_angles, eye_pos, target_pos );
	}

	float legit::get_fov_radius( const math::vector3& eye_pos, const math::vector3& view_angles, float fov_degrees ) const
	{
		if ( fov_degrees <= 0.0f )
		{
			return 0.0f;
		}

		math::vector3 forward{};
		view_angles.to_directions( &forward, nullptr, nullptr );

		auto offset_angles = view_angles;
		offset_angles.x -= fov_degrees;

		math::vector3 offset_forward{};
		offset_angles.to_directions( &offset_forward, nullptr, nullptr );

		const auto center = systems::g_view.project( eye_pos + forward * 1000.0f );
		const auto edge = systems::g_view.project( eye_pos + offset_forward * 1000.0f );

		if ( !systems::g_view.projection_valid( center ) || !systems::g_view.projection_valid( edge ) )
		{
			return 0.0f;
		}

		const auto dx = edge.x - center.x;
		const auto dy = edge.y - center.y;

		return std::sqrtf( dx * dx + dy * dy );
	}

	void legit::draw_penetration_crosshair( zdraw::draw_list& draw_list, const math::vector3& eye_pos, const math::vector3& view_angles )
	{
		const auto& cfg = settings::g_combat.m_other;

		math::vector3 forward{};
		view_angles.to_directions( &forward, nullptr, nullptr );

		const auto first_hit = systems::g_bvh.trace_ray( eye_pos, eye_pos + forward * g_shared.pen( ).get_weapon_data( ).range );
		if ( !first_hit.hit )
		{
			return;
		}

		auto pen_damage{ 0.0f };
		const auto can_pen = g_shared.pen( ).can( eye_pos, forward, pen_damage );

		const auto& n = first_hit.normal;
		const auto ref = ( std::abs( n.z ) < 0.9f ) ? math::vector3{ 0.0f, 0.0f, 1.0f } : math::vector3{ 1.0f, 0.0f, 0.0f };

		const auto d = ref.dot( n );
		const auto tangent = ( ref - n * d ).normalized( );
		const auto bitangent = n.cross( tangent );

		const auto center = first_hit.end_pos + n * 0.05f;
		constexpr auto half_size{ 3.5f };

		const math::vector3 corners[ 4 ]
		{
			center - tangent * half_size - bitangent * half_size,
			center + tangent * half_size - bitangent * half_size,
			center + tangent * half_size + bitangent * half_size,
			center - tangent * half_size + bitangent * half_size,
		};

		float sx[ 5 ]{}, sy[ 5 ]{};

		for ( auto i = 0; i < 4; ++i )
		{
			const auto proj = systems::g_view.project( corners[ i ] );
			if ( !systems::g_view.projection_valid( proj ) )
			{
				return;
			}

			sx[ i ] = proj.x;
			sy[ i ] = proj.y;
		}

		const auto center_proj = systems::g_view.project( center );
		if ( !systems::g_view.projection_valid( center_proj ) )
		{
			return;
		}

		sx[ 4 ] = center_proj.x;
		sy[ 4 ] = center_proj.y;

		const auto& color = can_pen ? cfg.penetration_color_yes.value : cfg.penetration_color_no.value;
		const auto edge = zdraw::rgba{ color.r, color.g, color.b, static_cast< std::uint8_t >( color.a / 4 ) };

		for ( auto i = 0; i < 4; ++i )
		{
			const auto j = ( i + 1 ) % 4;
			draw_list.add_triangle_filled_multi_color( sx[ 4 ], sy[ 4 ], sx[ i ], sy[ i ], sx[ j ], sy[ j ], color, edge, edge );
		}

		float screen[ 8 ]{ sx[ 0 ], sy[ 0 ], sx[ 1 ], sy[ 1 ], sx[ 2 ], sy[ 2 ], sx[ 3 ], sy[ 3 ] };
		draw_list.add_polyline( { screen, 8 }, { color.r, color.g, color.b, 255 }, true, 1.0f );
	}

	void legit::draw_fov( zdraw::draw_list& draw_list, const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::aimbot& cfg )
	{
		const auto target_radius = this->get_fov_radius( eye_pos, view_angles, static_cast< float >( cfg.fov ) );
		const auto alpha = this->m_fov_alpha.value( );
		const auto radius = target_radius * alpha;

		if ( radius <= 0.5f )
		{
			return;
		}

		const auto [w, h] = zdraw::get_display_size( );
		const auto color = zdraw::rgba{ cfg.fov_color.value.r, cfg.fov_color.value.g, cfg.fov_color.value.b, static_cast< std::uint8_t >( alpha * 125.0f ) };

		draw_list.add_circle( w * 0.5f, h * 0.5f, radius, color, 16 );
	}

	void legit::aimbot( const math::vector3& eye_pos, const math::vector3& view_angles, const target& tgt, const settings::combat::aimbot& cfg )
	{
		if ( !( GetAsyncKeyState( cfg.key ) & 0x8000 ) )
		{
			this->m_aim_error = {};
			return;
		}

		constexpr auto m_yaw{ 0.022f };
		const auto sensitivity = systems::g_convars.get<float>( CONVAR( "sensitivity"_hash ) );
		const auto fov_adjust = g::memory.read<float>( systems::g_local.pawn( ) + SCHEMA( "C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash ) );
		const auto deg_per_pixel = sensitivity * m_yaw * fov_adjust;

		if ( deg_per_pixel <= 0.0f )
		{
			return;
		}

		const auto freshest = systems::g_bones.get( tgt.player->bone_cache );
		if ( !freshest.is_valid( ) )
		{
			return;
		}

		auto aim_point = freshest.get_position( tgt.player->hitboxes.entries[ tgt.hitbox ].bone );

		if ( cfg.predictive )
		{
			const auto velocity = g::memory.read<math::vector3>( tgt.player->pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
			const auto prediction_time = g_shared.get_prediction_time( );

			aim_point = aim_point + velocity * prediction_time;
		}

		auto desired = math::helpers::calculate_angle( eye_pos, aim_point );
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
			const auto base_smooth = static_cast< float >( cfg.smoothing );
			const auto distance_factor = std::clamp( delta_length / 10.0f, 0.0f, 1.0f );
			const auto ease = 1.0f - std::powf( distance_factor, 2.0f );
			auto smooth_factor = ( 0.3f + ease * 0.7f ) / base_smooth;

			smooth_factor *= random::normal_clamped( 1.0f, 0.06f, 0.85f, 1.15f );

			const auto x_bias = random::normal_clamped( 1.0f, 0.02f, 0.95f, 1.05f );
			const auto y_bias = random::normal_clamped( 0.97f, 0.03f, 0.90f, 1.04f );

			delta_x *= smooth_factor * x_bias;
			delta_y *= smooth_factor * y_bias;

			if ( delta_length < 2.0f && delta_length > 0.3f && random::floating( 0.0f, 1.0f ) < 0.15f )
			{
				const auto overshoot = random::normal_clamped( 1.2f, 0.08f, 1.05f, 1.4f );
				delta_x *= overshoot;
				delta_y *= overshoot;
			}
		}

		const auto want_x = -delta_y + this->m_aim_error.x;
		const auto want_y = delta_x + this->m_aim_error.y;

		const auto counts_x = std::roundf( want_x / deg_per_pixel );
		const auto counts_y = std::roundf( want_y / deg_per_pixel );

		this->m_aim_error.x = want_x - counts_x * deg_per_pixel;
		this->m_aim_error.y = want_y - counts_y * deg_per_pixel;

		const auto dx = static_cast< int >( counts_x );
		const auto dy = static_cast< int >( counts_y );

		if ( dx != 0 || dy != 0 )
		{
			g::input.inject_mouse( dx, dy, input::move );
		}
	}

	void legit::zeusbot( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players )
	{
		const auto& cfg = settings::g_combat.m_other.m_zeusbot;

		if ( this->m_trigger_held )
		{
			return;
		}

		if ( !( GetAsyncKeyState( cfg.key ) & 0x8000 ) )
		{
			this->m_zeus_fire_time = 0.0f;
			return;
		}

		const auto& ctx = g_shared.ctx( );
		if ( !ctx.weapon_ready )
		{
			return;
		}

		if ( this->m_zeus_fire_time > 0.0f )
		{
			if ( ctx.current_time >= this->m_zeus_fire_time )
			{
				const auto hold_ms = this->m_rng.random_float( 50.0f, 120.0f );

				g::input.inject_mouse( 0, 0, input::left_down );
				this->m_trigger_held = true;
				this->m_trigger_release_time = ctx.current_time + hold_ms * 0.001f;
				this->m_zeus_fire_time = 0.0f;
			}

			return;
		}

		const auto range = g_shared.pen( ).get_weapon_data( ).range * 0.85f;
		if ( range <= 0.0f )
		{
			return;
		}

		const systems::collector::player* best_player{};
		math::vector3 best_aim{};
		auto best_fov = cfg.max_fov.value;

		for ( const auto& player : players )
		{
			if ( !systems::g_local.is_enemy( player.team ) )
			{
				continue;
			}

			if ( player.invulnerable || player.hitboxes.count <= 0 )
			{
				continue;
			}

			const auto bones = systems::g_bones.get( player.bone_cache );
			if ( !bones.is_valid( ) )
			{
				continue;
			}

			for ( const auto& hb : player.hitboxes )
			{
				if ( hb.index < 0 || hb.bone < 0 )
				{
					continue;
				}

				if ( hb.bone >= cstypes::bone_ids::left_hip )
				{
					continue;
				}

				const auto bone_pos = bones.get_position( hb.bone );
				const auto bone_rot = bones.get_rotation( hb.bone );
				const auto center = bone_pos + bone_rot.rotate_vector( ( hb.mins + hb.maxs ) * 0.5f );

				if ( ( center - eye_pos ).length( ) > range )
				{
					continue;
				}

				const auto fov = math::helpers::calculate_fov( view_angles, eye_pos, center );
				if ( fov > best_fov )
				{
					continue;
				}

				const auto trace = systems::g_bvh.trace_ray( eye_pos, center );
				if ( trace.hit && trace.fraction < 0.97f )
				{
					continue;
				}

				best_player = &player;
				best_aim = center;
				best_fov = fov;
			}
		}

		if ( !best_player )
		{
			return;
		}

		const auto desired = math::helpers::calculate_angle( eye_pos, best_aim );
		const auto delta_pitch = desired.x - view_angles.x;
		const auto delta_yaw = math::helpers::normalize_yaw( desired.y - view_angles.y );

		constexpr auto m_yaw{ 0.022f };
		const auto sensitivity = systems::g_convars.get<float>( CONVAR( "sensitivity"_hash ) );
		const auto fov_adjust = g::memory.read<float>( systems::g_local.pawn( ) + SCHEMA( "C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash ) );
		const auto deg_per_pixel = sensitivity * m_yaw * fov_adjust;

		if ( deg_per_pixel <= 0.0f )
		{
			return;
		}

		const auto dx = static_cast< int >( -delta_yaw / deg_per_pixel );
		const auto dy = static_cast< int >( delta_pitch / deg_per_pixel );

		if ( dx != 0 || dy != 0 )
		{
			g::input.inject_mouse( dx, dy, input::move );
		}

		this->m_zeus_fire_time = ctx.current_time + 0.050f;
	}

	legit::trigger_result legit::trace_crosshair( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot& cfg ) const
	{
		trigger_result result{};

		math::vector3 forward{};
		view_angles.to_directions( &forward, nullptr, nullptr );

		constexpr auto max_range{ 8192.0f };
		auto best_dist_sq = max_range * max_range;
		const auto end_pos = eye_pos + forward * max_range;
		const auto prediction_time = cfg.predictive ? g_shared.get_prediction_time( ) : 0.0f;

		for ( const auto& player : players )
		{
			if ( !systems::g_local.is_enemy( player.team ) )
			{
				continue;
			}

			if ( player.invulnerable || player.hitboxes.count <= 0 )
			{
				continue;
			}

			const auto bones = systems::g_bones.get( player.bone_cache );
			if ( !bones.is_valid( ) )
			{
				continue;
			}

			math::vector3 velocity{};
			if ( cfg.predictive )
			{
				velocity = g::memory.read<math::vector3>( player.pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
			}

			for ( const auto& hb : player.hitboxes )
			{
				if ( hb.index < 0 || hb.bone < 0 )
				{
					continue;
				}

				const auto bone_pos = bones.get_position( hb.bone );
				const auto bone_rot = bones.get_rotation( hb.bone );

				const auto capsule_start = bone_pos + bone_rot.rotate_vector( hb.mins ) + velocity * prediction_time;
				const auto capsule_end = bone_pos + bone_rot.rotate_vector( hb.maxs ) + velocity * prediction_time;
				const auto radius = ( hb.radius > 0.0f ? hb.radius : 3.5f ) * 0.85f;

				if ( !g_shared.ray_hits_capsule( eye_pos, forward, capsule_start, capsule_end, radius ) )
				{
					continue;
				}

				const auto capsule_center = ( capsule_start + capsule_end ) * 0.5f;
				const auto dist_sq = ( capsule_center - eye_pos ).length_sqr( );

				if ( dist_sq >= best_dist_sq )
				{
					continue;
				}

				const auto vis_trace = systems::g_bvh.trace_ray( eye_pos, capsule_center );
				const auto visible = !vis_trace.hit || vis_trace.fraction > 0.97f;

				if ( visible )
				{
					const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
					const auto damage = combat::g_shared.pen( ).get_max_damage( hitgroup, player.armor, player.has_helmet, player.team );

					best_dist_sq = dist_sq;
					result.player = &player;
					result.bones = bones;
					result.hitbox = hb.index;
					result.hitgroup = hitgroup;
					result.damage = damage;
					result.penetrated = false;
				}
				else if ( cfg.autowall )
				{
					shared::penetration::result pen_result{};
					if ( combat::g_shared.pen( ).run( eye_pos, capsule_center, player, bones, pen_result ) )
					{
						if ( pen_result.damage >= cfg.min_damage )
						{
							best_dist_sq = dist_sq;
							result.player = &player;
							result.bones = bones;
							result.hitbox = pen_result.hitbox;
							result.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( pen_result.hitbox );
							result.damage = pen_result.damage;
							result.penetrated = pen_result.penetrated;
						}
					}
				}
			}
		}

		return result;
	}

	void legit::triggerbot( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot& cfg )
	{
		if ( this->m_trigger_held )
		{
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

		if ( cfg.hitchance > 0.0f && !g_shared.is_weapon_max_accuracy( ) )
		{
			const auto required = cfg.hitchance / 100.0f;
			const auto hc = g_shared.calculate_hitchance( eye_pos, view_angles, *result.player, result.bones );

			if ( hc < required )
			{
				this->m_trigger_waiting = false;
				return;
			}
		}

		const auto now = ctx.current_time;

		if ( !this->m_trigger_waiting )
		{
			this->m_trigger_waiting = true;
			this->m_trigger_delay_end = now + static_cast< float >( cfg.delay ) * 0.001f;
			return;
		}

		if ( now < this->m_trigger_delay_end )
		{
			return;
		}

		this->m_trigger_waiting = false;

		const auto hold_ms = this->m_rng.random_float( 50.0f, 120.0f );

		g::input.inject_mouse( 0, 0, input::left_down );
		this->m_trigger_held = true;
		this->m_trigger_release_time = now + hold_ms * 0.001f;
	}

} // namespace features::combat