#include <stdafx.hpp>

namespace features::misc {

	void grenades::on_render( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_misc.m_grenades;
		if ( !cfg.enabled )
		{
			return;
		}

		const auto now = std::chrono::steady_clock::now( );
		const auto has_bvh = systems::g_bvh.valid( );

		if ( has_bvh )
		{
			this->update_in_flight( );
		}

		std::erase_if( this->m_in_flight, [ & ]( const in_flight_grenade& g )
			{
				if ( !g.detonated )
				{
					return false;
				}

				const auto fade_elapsed = std::chrono::duration<float>( now - g.detonate_time ).count( );
				if ( fade_elapsed <= 0.5f )
				{
					return false;
				}

				for ( const auto& proj : systems::g_collector.projectiles( ) )
				{
					if ( proj.entity == g.entity )
					{
						return false;
					}
				}

				return true;
			} );

		for ( auto& gren : this->m_in_flight )
		{
			if ( !gren.traj.valid )
			{
				continue;
			}

			if ( !gren.detonated )
			{
				const auto elapsed = std::chrono::duration<float>( now - gren.throw_time ).count( );
				const auto fuse_expired = elapsed >= gren.traj.duration;
				const auto effect_started = gren.effect_started;

				if ( fuse_expired || effect_started )
				{
					gren.detonated = true;
					gren.detonate_time = now;
				}
			}

			auto alpha{ 1.0f };

			if ( gren.detonated )
			{
				const auto elapsed = std::chrono::duration<float>( now - gren.detonate_time ).count( );
				alpha = std::clamp( 1.0f - elapsed / 0.5f, 0.0f, 1.0f );
			}

			if ( alpha > 0.0f )
			{
				this->render_trajectory( draw_list, gren.traj, alpha );
			}
		}

		const auto& ctx = combat::g_shared.ctx( );
		auto holding_now{ false };

		if ( ctx.valid && ctx.weapon && ctx.weapon_type == cstypes::weapon_type::grenade )
		{
			const auto pin_pulled = g::memory.read<bool>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_bPinPulled"_hash ) );
			holding_now = pin_pulled;
		}

		if ( this->m_was_holding && !holding_now )
		{
			this->m_last_throw_time = now;
		}

		this->m_was_holding = holding_now;

		if ( !has_bvh || !this->can_predict( ) )
		{
			return;
		}

		this->update_weapon_properties( );

		math::vector3 origin{}, velocity{};
		this->setup_throw( origin, velocity );

		trajectory traj{};
		this->simulate( origin, velocity, traj );

		if ( traj.valid )
		{
			this->render_trajectory( draw_list, traj, 1.0f );
		}
	}

	bool grenades::can_predict( ) const
	{
		const auto& ctx = combat::g_shared.ctx( );
		if ( !ctx.valid || !ctx.weapon || !ctx.weapon_vdata )
		{
			return false;
		}

		if ( ctx.weapon_type != cstypes::weapon_type::grenade )
		{
			return false;
		}

		const auto pin_pulled = g::memory.read<bool>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_bPinPulled"_hash ) );
		if ( !pin_pulled )
		{
			const auto since_throw = std::chrono::duration<float>( std::chrono::steady_clock::now( ) - this->m_last_throw_time ).count( );
			if ( since_throw < throw_cooldown )
			{
				return false;
			}
		}

		const auto throw_time = g::memory.read<float>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_fThrowTime"_hash ) );
		if ( throw_time > 0.0f )
		{
			return false;
		}

		return true;
	}

	void grenades::update_weapon_properties( )
	{
		const auto& ctx = combat::g_shared.ctx( );
		if ( !ctx.weapon_vdata || ctx.weapon_vdata == this->m_weapon_vdata )
		{
			return;
		}

		this->m_weapon_vdata = ctx.weapon_vdata;
		this->m_throw_velocity = std::clamp( g::memory.read<float>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flThrowVelocity"_hash ) ), 1.0f, 10000.0f );

		const auto name_ptr = g::memory.read<std::uintptr_t>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_szName"_hash ) );
		if ( !name_ptr )
		{
			this->m_weapon_hash = 0;
			this->m_detonate_time = 1.5f;
			this->m_velocity_threshold = 0.1f;
			return;
		}

		char name[ 64 ]{};
		g::memory.read( name_ptr, name, sizeof( name ) - 1 );
		this->m_weapon_hash = fnv1a::runtime_hash( name );

		switch ( this->m_weapon_hash )
		{
		case "weapon_molotov"_hash:
		case "weapon_incgrenade"_hash:
			this->m_detonate_time = 2.0f;
			this->m_velocity_threshold = 0.0f;
			break;

		case "weapon_decoy"_hash:
			this->m_detonate_time = 2.0f;
			this->m_velocity_threshold = 0.2f;
			break;

		default:
			this->m_detonate_time = 1.5f;
			this->m_velocity_threshold = 0.1f;
			break;
		}
	}

	void grenades::setup_throw( math::vector3& origin, math::vector3& velocity )
	{
		const auto& ctx = combat::g_shared.ctx( );
		const auto pawn = systems::g_local.pawn( );

		auto strength{ 1.0f };

		const auto pin_pulled = g::memory.read<bool>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_bPinPulled"_hash ) );
		if ( pin_pulled )
		{
			strength = std::clamp( g::memory.read<float>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_flThrowStrength"_hash ) ), 0.0f, 1.0f );
			if ( std::fabsf( strength - 0.5f ) <= 0.1f )
			{
				strength = 0.5f;
			}
		}

		auto angles = systems::g_view.angles( );

		if ( angles.x > 90.0f )
		{
			angles.x -= 360.0f;
		}
		else if ( angles.x < -90.0f )
		{
			angles.x += 360.0f;
		}

		angles.x -= ( 90.0f - std::fabsf( angles.x ) ) * 10.0f / 90.0f;

		const auto player_velocity = g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );

		auto eye_pos = systems::g_view.origin( );
		eye_pos.z += strength * 12.0f - 12.0f;

		math::vector3 forward{};
		{
			const auto pitch = angles.x * ( std::numbers::pi_v<float> / 180.0f );
			const auto yaw = angles.y * ( std::numbers::pi_v<float> / 180.0f );

			forward =
			{
				std::cosf( pitch ) * std::cosf( yaw ),
				std::cosf( pitch ) * std::sinf( yaw ),
				-std::sinf( pitch )
			};
		}

		const auto hull_mins = math::vector3{ -k_hull_size, -k_hull_size, -k_hull_size };
		const auto hull_maxs = math::vector3{ k_hull_size, k_hull_size, k_hull_size };
		const auto trace = systems::g_bvh.trace_hull( eye_pos, eye_pos + forward * k_forward_offset, hull_mins, hull_maxs );
		origin = trace.end_pos - forward * k_pull_back;

		const auto throw_vel = std::clamp( this->m_throw_velocity * 0.9f, 15.0f, 750.0f );
		const auto throw_speed = ( strength * 0.7f + 0.3f ) * throw_vel;

		velocity = forward * throw_speed + player_velocity * k_velocity_inherit;
	}

	void grenades::update_in_flight( )
	{
		const auto& cfg = settings::g_misc.m_grenades;
		const auto projectiles = systems::g_collector.projectiles( );
		const auto controller = systems::g_local.controller( );

		if ( !controller )
		{
			return;
		}

		const auto local_pawn_handle = g::memory.read<std::uint32_t>( controller + SCHEMA( "CCSPlayerController", "m_hPlayerPawn"_hash ) );
		const auto now = std::chrono::steady_clock::now( );

		std::unordered_set<std::uintptr_t> alive{};

		for ( const auto& proj : projectiles )
		{
			if ( cfg.local_only && proj.thrower_handle != local_pawn_handle )
			{
				continue;
			}

			alive.insert( proj.entity );

			in_flight_grenade* existing{};

			for ( auto& gren : this->m_in_flight )
			{
				if ( gren.entity == proj.entity )
				{
					existing = &gren;
					break;
				}
			}

			if ( existing )
			{
				existing->last_seen = now;

				if ( !existing->corrected )
				{
					const auto initial_pos = g::memory.read<math::vector3>( proj.entity + SCHEMA( "C_BaseCSGrenadeProjectile", "m_vInitialPosition"_hash ) );
					const auto initial_vel = g::memory.read<math::vector3>( proj.entity + SCHEMA( "C_BaseCSGrenadeProjectile", "m_vInitialVelocity"_hash ) );

					if ( initial_vel.length_sqr( ) >= 1.0f )
					{
						const auto saved = this->m_weapon_hash;
						this->m_weapon_hash = existing->weapon_hash;
						this->simulate( initial_pos, initial_vel, existing->traj );
						this->m_weapon_hash = saved;
						existing->corrected = true;
					}
				}

				if ( !existing->detonated && proj.effect_tick_begin > 0 )
				{
					existing->effect_started = true;
					existing->detonated = true;
					existing->detonate_time = now;
				}

				continue;
			}

			const auto initial_pos = g::memory.read<math::vector3>( proj.entity + SCHEMA( "C_BaseCSGrenadeProjectile", "m_vInitialPosition"_hash ) );
			const auto initial_vel = g::memory.read<math::vector3>( proj.entity + SCHEMA( "C_BaseCSGrenadeProjectile", "m_vInitialVelocity"_hash ) );

			if ( initial_vel.length_sqr( ) < 1.0f )
			{
				continue;
			}

			if ( proj.effect_tick_begin > 0 )
			{
				continue;
			}

			const auto wh = this->hash_from_projectile( proj.subtype );
			const auto saved = this->m_weapon_hash;
			this->m_weapon_hash = wh;

			in_flight_grenade gren{};
			gren.entity = proj.entity;
			gren.weapon_hash = wh;
			gren.throw_time = now;
			gren.last_seen = now;
			gren.corrected = true;
			this->simulate( initial_pos, initial_vel, gren.traj );

			this->m_weapon_hash = saved;
			this->m_in_flight.push_back( std::move( gren ) );
		}

		for ( auto& gren : this->m_in_flight )
		{
			if ( gren.detonated )
			{
				continue;
			}

			if ( alive.contains( gren.entity ) )
			{
				continue;
			}

			const auto missing_for = std::chrono::duration<float>( now - gren.last_seen ).count( );
			if ( missing_for >= missing_grace )
			{
				gren.detonated = true;
				gren.detonate_time = now;
			}
		}
	}

	std::uintptr_t grenades::hash_from_projectile( systems::collector::projectile_subtype type ) const
	{
		switch ( type )
		{
		case systems::collector::projectile_subtype::he_grenade:    return "weapon_hegrenade"_hash;
		case systems::collector::projectile_subtype::flashbang:     return "weapon_flashbang"_hash;
		case systems::collector::projectile_subtype::smoke_grenade: return "weapon_smokegrenade"_hash;
		case systems::collector::projectile_subtype::molotov:       return "weapon_molotov"_hash;
		case systems::collector::projectile_subtype::decoy:         return "weapon_decoy"_hash;
		default:                                                    return 0;
		}
	}

	void grenades::simulate( const math::vector3& start, const math::vector3& velocity, trajectory& out )
	{
		this->m_sv_gravity = systems::g_convars.get<float>( CONVAR( "sv_gravity"_hash ) );

		const auto molotov_slope = systems::g_convars.get<float>( CONVAR( "weapon_molotov_maxdetonateslope"_hash ) );
		this->m_molotov_max_slope_z = std::cosf( molotov_slope * std::numbers::pi_v<float> / 180.0f );

		out.points.clear( );
		out.points.reserve( max_ticks / ticks_per_point );
		out.bounces.clear( );
		out.valid = false;
		out.end_tick = -1;

		auto pos = start;
		auto vel = velocity;
		auto bounce_count{ 0 };
		auto tick_timer{ 0 };

		for ( int tick = 0; tick < max_ticks; ++tick )
		{
			if ( tick_timer == 0 )
			{
				out.points.push_back( pos );
			}

			systems::bvh::trace_result trace{};
			auto impact_detonate{ false };

			this->step_simulation( pos, vel, trace, impact_detonate );

			if ( trace.hit )
			{
				++bounce_count;
				out.bounces.push_back( pos );
			}

			const auto velocity_stopped = std::fabsf( vel.x ) < 20.0f && std::fabsf( vel.y ) < 20.0f && vel.length_sqr( ) < k_stop_speed_sq;
			if ( impact_detonate || this->should_detonate( vel, tick ) || bounce_count > 20 || velocity_stopped )
			{
				out.end_tick = tick;
				out.end_pos = pos;
				out.duration = static_cast< float >( tick ) * cstypes::tick_interval;
				break;
			}

			if ( trace.hit || ++tick_timer >= ticks_per_point )
			{
				tick_timer = 0;
			}
		}

		if ( !out.points.empty( ) && out.end_tick >= 0 )
		{
			if ( out.points.back( ).distance_sqr( out.end_pos ) > 1.0f )
			{
				out.points.push_back( out.end_pos );
			}
		}

		out.valid = out.end_tick >= 0;
	}

	void grenades::step_simulation( math::vector3& pos, math::vector3& vel, systems::bvh::trace_result& trace, bool& detonated )
	{
		detonated = false;

		const auto gravity = this->m_sv_gravity * gravity_scale;
		const auto new_vel_z = vel.z - gravity * cstypes::tick_interval;

		const math::vector3 move
		{
			vel.x * cstypes::tick_interval,
			vel.y * cstypes::tick_interval,
			( vel.z + new_vel_z ) * 0.5f * cstypes::tick_interval
		};

		vel.z = new_vel_z;

		const auto hull_mins = math::vector3{ -k_hull_size, -k_hull_size, -k_hull_size };
		const auto hull_maxs = math::vector3{ k_hull_size, k_hull_size, k_hull_size };
		trace = systems::g_bvh.trace_hull( pos, pos + move, hull_mins, hull_maxs );
		pos = trace.end_pos;

		if ( trace.hit )
		{
			this->resolve_collision( trace, pos, vel, detonated );
		}
	}

	void grenades::resolve_collision( const systems::bvh::trace_result& trace, math::vector3& pos, math::vector3& vel, bool& detonated )
	{
		detonated = false;

		const auto is_molotov = this->m_weapon_hash == "weapon_molotov"_hash || this->m_weapon_hash == "weapon_incgrenade"_hash;
		if ( is_molotov )
		{
			if ( trace.normal.z >= this->m_molotov_max_slope_z || vel.length_sqr( ) < k_stop_speed_sq )
			{
				detonated = true;
				vel = {};
				return;
			}
		}

		auto new_vel = clip_velocity( vel, trace.normal, 2.0f );

		new_vel.x *= elasticity;
		new_vel.y *= elasticity;
		new_vel.z *= elasticity;

		if ( trace.normal.z > k_steep_bounce_normal_z )
		{
			const auto speed_sqr = new_vel.length_sqr( );

			if ( speed_sqr > k_steep_bounce_speed_sq )
			{
				const auto l = new_vel.normalized( ).dot( trace.normal );
				if ( l > 0.5f )
				{
					const auto dampen = 1.5f - l;
					new_vel.x *= dampen;
					new_vel.y *= dampen;
					new_vel.z *= dampen;
				}
			}

			if ( new_vel.length_sqr( ) < k_stop_speed_sq )
			{
				vel = {};
				return;
			}
		}

		vel = new_vel;

		const auto remaining = 1.0f - trace.fraction;
		if ( remaining > 0.0f )
		{
			const auto hull_mins = math::vector3{ -k_hull_size, -k_hull_size, -k_hull_size };
			const auto hull_maxs = math::vector3{ k_hull_size, k_hull_size, k_hull_size };
			const auto post_trace = systems::g_bvh.trace_hull( pos, pos + new_vel * ( remaining * cstypes::tick_interval ), hull_mins, hull_maxs );
			pos = post_trace.end_pos;
		}
	}

	bool grenades::should_detonate( const math::vector3& vel, int tick ) const
	{
		switch ( this->m_weapon_hash )
		{
		case "weapon_smokegrenade"_hash:
		case "weapon_decoy"_hash:
		{
			const auto speed_2d = std::sqrtf( vel.x * vel.x + vel.y * vel.y );
			const auto check_ticks = static_cast< int >( 0.2f / cstypes::tick_interval );
			return speed_2d < this->m_velocity_threshold && ( tick % check_ticks ) == 0;
		}

		case "weapon_molotov"_hash:
		case "weapon_incgrenade"_hash:
			return static_cast< float >( tick ) * cstypes::tick_interval > this->m_detonate_time;

		case "weapon_flashbang"_hash:
		case "weapon_hegrenade"_hash:
			return static_cast< float >( tick - 8 ) * cstypes::tick_interval > this->m_detonate_time;

		default:
			return false;
		}
	}

	math::vector3 grenades::clip_velocity( const math::vector3& velocity, const math::vector3& normal, float overbounce )
	{
		const auto backoff = velocity.dot( normal ) * overbounce;

		math::vector3 out
		{
			velocity.x - normal.x * backoff,
			velocity.y - normal.y * backoff,
			velocity.z - normal.z * backoff
		};

		if ( std::fabsf( out.x ) < 0.1f )
		{
			out.x = 0.0f;
		}

		if ( std::fabsf( out.y ) < 0.1f )
		{
			out.y = 0.0f;
		}

		if ( std::fabsf( out.z ) < 0.1f )
		{
			out.z = 0.0f;
		}

		return out;
	}

	void grenades::render_trajectory( zdraw::draw_list& draw_list, const trajectory& traj, float alpha ) const
	{
		if ( !traj.valid || traj.points.size( ) < 2 )
		{
			return;
		}

		const auto& cfg = settings::g_misc.m_grenades;
		const auto total = traj.points.size( );

		for ( std::size_t i = 0; i + 1 < total; ++i )
		{
			const auto s0 = systems::g_view.project( traj.points[ i ] );
			const auto s1 = systems::g_view.project( traj.points[ i + 1 ] );

			if ( !systems::g_view.projection_valid( s0 ) || !systems::g_view.projection_valid( s1 ) )
			{
				continue;
			}

			const auto t = static_cast< float >( i ) / static_cast< float >( total - 1 );
			const auto seg_alpha = alpha * ( 1.0f - t * 0.6f );
			const auto a = static_cast< std::uint8_t >( std::clamp( seg_alpha * static_cast< float >( cfg.color.value.a ), 0.0f, 255.0f ) );

			draw_list.add_line( s0.x, s0.y, s1.x, s1.y, { cfg.color.value.r, cfg.color.value.g, cfg.color.value.b, a }, 2.0f );
		}

		for ( const auto& bounce : traj.bounces )
		{
			const auto s = systems::g_view.project( bounce );
			if ( !systems::g_view.projection_valid( s ) )
			{
				continue;
			}

			const auto a = static_cast< std::uint8_t >( alpha * static_cast< float >( cfg.color.value.a ) );

			draw_list.add_rect_filled( s.x - 3.0f, s.y - 3.0f, 6.0f, 6.0f, { 0, 0, 0, a } );
			draw_list.add_rect_filled( s.x - 2.0f, s.y - 2.0f, 4.0f, 4.0f, { cfg.color.value.r, cfg.color.value.g, cfg.color.value.b, a } );
		}

		if ( traj.end_tick >= 0 )
		{
			const auto s = systems::g_view.project( traj.end_pos );
			if ( systems::g_view.projection_valid( s ) )
			{
				const auto a = static_cast< std::uint8_t >( alpha * static_cast< float >( cfg.color.value.a ) );

				draw_list.add_rect_filled( s.x - 5.0f, s.y - 5.0f, 10.0f, 10.0f, { 0, 0, 0, a } );
				draw_list.add_rect_filled( s.x - 4.0f, s.y - 4.0f, 8.0f, 8.0f, { cfg.color.value.r, cfg.color.value.g, cfg.color.value.b, a } );
			}
		}
	}

} // namespace features::misc