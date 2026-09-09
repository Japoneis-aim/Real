#include <stdafx.hpp>

namespace features::combat {

	// ============================================================================
	// DETAIL — math helpers
	// ============================================================================

	namespace detail {

		inline static float remap_value( float val, float a, float b, float c, float d )
		{
			if ( b == a )
				return ( val - b >= 0.0f ) ? d : c;

			const auto t = std::clamp( ( val - a ) / ( b - a ), 0.0f, 1.0f );
			return c + ( d - c ) * t;
		}

		inline static float normalize_angle( float a )
		{
			return a - std::floorf( a * 0.0027777778f + 0.5f ) * 360.0f;
		}

		inline static float quantize_angle( float a )
		{
			return std::floorf( normalize_angle( a ) * 2.0f ) * 0.5f;
		}

		inline static float ease_dat( float value, float curve )
		{
			auto v = std::clamp( value, 0.0f, 1.0f );
			auto c = std::max( curve, 1.1754944e-38f );
			c = std::fminf( 1.0f, c );
			return v / ( ( ( 1.0f / c - 2.0f ) * ( 1.0f - v ) ) + 1.0f );
		}

		static void scale_damage(
			int hitgroup, int armor, bool has_helmet, int team,
			float armor_ratio, float headshot_multiplier,
			float& damage )
		{
			// Cacheamos os ponteiros das convars uma única vez (thread-safe via static init).
			// Os valores flutuantes são relidos a cada chamada pois podem mudar em modo custom,
			// mas a resolução do ponteiro (find convar) é cara e só precisa acontecer uma vez.
			static const auto cv_ct_head = CONVAR( "mp_damage_scale_ct_head"_hash );
			static const auto cv_t_head  = CONVAR( "mp_damage_scale_t_head"_hash );
			static const auto cv_ct_body = CONVAR( "mp_damage_scale_ct_body"_hash );
			static const auto cv_t_body  = CONVAR( "mp_damage_scale_t_body"_hash );

			const auto ct_head = systems::g_convars.get<float>( cv_ct_head );
			const auto t_head  = systems::g_convars.get<float>( cv_t_head );
			const auto ct_body = systems::g_convars.get<float>( cv_ct_body );
			const auto t_body  = systems::g_convars.get<float>( cv_t_body );

			const auto is_ct = ( team == 3 );
			const auto head_scale = is_ct ? ct_head : t_head;
			const auto body_scale = is_ct ? ct_body : t_body;

			switch ( hitgroup )
			{
				case 1:  damage *= headshot_multiplier * head_scale; break;
				case 2:
				case 4:
				case 5:
				case 8:  damage *= body_scale; break;
				case 3:  damage *= 1.25f * body_scale; break;
				case 6:
				case 7:  damage *= 0.75f * body_scale; break;
				default: break;
			}

			const auto is_head = ( hitgroup == 1 );
			const auto is_armored = ( hitgroup >= 1 && hitgroup <= 5 ) || ( hitgroup == 8 );

			if ( armor <= 0 || !is_armored || ( is_head && !has_helmet ) )
			{
				damage = std::floor( damage );
				return;
			}

			constexpr auto armor_bonus{ 0.5f };
			const auto armor_ratio_scaled = armor_ratio * 0.5f;

			auto damage_to_health = damage * armor_ratio_scaled;
			auto damage_to_armor  = ( damage - damage_to_health ) * armor_bonus;

			if ( damage_to_armor > static_cast<float>( armor ) )
				damage_to_health = damage - ( static_cast<float>( armor ) / armor_bonus );

			damage = std::floor( damage_to_health );
		}

	} // namespace detail

	// ============================================================================
	// PENETRATION
	// ============================================================================

	void shared::penetration::prepare( std::uintptr_t weapon_vdata, std::uintptr_t weapon )
	{
		if ( !weapon_vdata || !weapon )
			return;

		this->m_weapon_data = weapon_data
		{
			.damage            = static_cast<float>( g::memory.read<int>(    weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_nDamage"_hash ) ) ),
			.penetration       = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flPenetration"_hash ) ),
			.range_modifier    = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flRangeModifier"_hash ) ),
			.range             = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flRange"_hash ) ),
			.armor_ratio       = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flArmorRatio"_hash ) ),
			.headshot_multiplier = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flHeadshotMultiplier"_hash ) )
		};
	}

	// ---- Penetration surface helpers ----

	static float get_surface_pen_mod( int surface_type )
	{
		if ( ( ( surface_type - 85 ) & 0xfffffffd ) == 0 )
			return 3.0f;
		if ( surface_type == 76 )
			return 2.0f;
		return 0.0f; // default: no override
	}

	static bool is_thin_special_surface( int surface_type, float thickness )
	{
		return thickness < 6.0f && ( surface_type == 71 || surface_type == 89 );
	}

	static void apply_surface_override( float& pen_mod, float& damage_modifier, int surface_type, float thickness )
	{
		const auto override = get_surface_pen_mod( surface_type );
		if ( override > 0.0f )
			pen_mod = override;

		if ( is_thin_special_surface( surface_type, thickness ) )
		{
			damage_modifier = 0.05f;
			pen_mod = 3.0f;
		}
	}

static bool should_stop_penetrating( const systems::bvh::penetration_segment& seg, float pen_mod )
{
	return seg.exit_distance > 3000.0f || pen_mod < 0.1f;
}

static float compute_damage_loss(
	float current_damage,
	const shared::penetration::weapon_data& data,
	float pen_mod,
	float damage_modifier,
	float thickness )
	{
		const auto inv_pen  = 1.0f / pen_mod;
		const auto base_loss = damage_modifier * current_damage;
	const auto pen_loss  = std::max( 0.0f, ( 3.0f / data.penetration ) * 1.25f ) * ( inv_pen * 3.0f );
		const auto dist_loss = ( thickness * thickness * inv_pen ) / 24.0f;
		return current_damage - ( base_loss + pen_loss + dist_loss );
	}

	// ---- Hitbox / capsule helpers ----

static math::vector3 get_capsule_axis( const math::vector3& half_extent )
	{
		const auto ax = std::abs( half_extent.x );
		const auto ay = std::abs( half_extent.y );
		const auto az = std::abs( half_extent.z );
	const auto longest = std::max( std::max( ax, ay ), az );

	if ( ax >= ay && ax >= az )
		return { longest, 0.0f, 0.0f };
	if ( ay >= az )
		return { 0.0f, longest, 0.0f };
	return { 0.0f, 0.0f, longest };
	}

static bool build_hitbox_capsule_v2(
	const systems::collector::hitbox& hb,
	const systems::bones::data::bone& bone,
	math::vector3& out_capsule_start,
	math::vector3& out_capsule_end,
	float& out_radius )
	{
		if ( hb.index < 0 || hb.bone < 0 )
			return false;

		const auto center_local = ( hb.mins + hb.maxs ) * 0.5f;
		const auto half_extent  = ( hb.maxs - hb.mins ) * 0.5f;
		const auto axis_local   = get_capsule_axis( half_extent );

		const auto center_world = bone.position + bone.rotation.rotate_vector( center_local );
		const auto axis_world   = bone.rotation.rotate_vector( axis_local );

		out_capsule_start = center_world - axis_world;
		out_capsule_end   = center_world + axis_world;
		out_radius        = hb.radius;
		return true;
	}

	// ---- Core penetration logic ----

	bool shared::penetration::run(
		const math::vector3& start,
		const math::vector3& end,
		const systems::collector::player& target,
		const systems::bones::data& bones,
		result& out ) const
	{
		if ( this->m_weapon_data.damage <= 0.0f )
			return false;

		const auto direction = ( end - start ).normalized( );
		const auto max_range = this->m_weapon_data.range;
		const auto ray_end   = start + direction * max_range;

		const auto all_hits = systems::g_bvh.trace_ray_all( start, ray_end );
		const auto segments = systems::g_bvh.build_segments( all_hits, max_range );

		auto current_damage     = this->m_weapon_data.damage;
		auto penetration_count  = 4;

		// Lambda: test all hitboxes on a ray segment
		auto check_target = [ & ]( const math::vector3& seg_start, float seg_start_dist, float seg_end_dist ) -> bool
		{
			for ( const auto& hb : target.hitboxes )
			{
				const auto& bone = bones.bones[ hb.bone ];

				math::vector3 capsule_start, capsule_end;
				float radius;
				if ( !build_hitbox_capsule_v2( hb, bone, capsule_start, capsule_end, radius ) )
					continue;

				if ( !g_shared.ray_hits_capsule( seg_start, direction, capsule_start, capsule_end, radius ) )
					continue;

				const auto to_center = capsule_start + capsule_end;
				const auto center_world = to_center * 0.5f; // midpoint
				const auto hit_dist = ( center_world - seg_start ).dot( direction );

				if ( hit_dist < 0.0f || ( seg_start_dist + hit_dist ) > seg_end_dist )
					continue;

				if ( current_damage < 1.0f )
					continue;

				const auto total_dist = seg_start_dist + hit_dist;
				auto damage = current_damage * std::pow( this->m_weapon_data.range_modifier, total_dist / max_range );

				if ( damage < 1.0f )
					continue;

				const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
				detail::scale_damage( hitgroup, target.armor, target.has_helmet, target.team,
					this->m_weapon_data.armor_ratio, this->m_weapon_data.headshot_multiplier, damage );

				if ( damage < 1.0f )
					continue;

				out.damage     = damage;
				out.hitbox     = hb.index;
				out.penetrated = ( penetration_count < 4 );
				return true;
			}
			return false;
		};

		// First segment (before any wall)
		const auto first_wall = segments.empty( ) ? max_range : segments[ 0 ].enter_distance;
		if ( check_target( start, 0.0f, first_wall ) )
			return true;

		// Penetrate walls
		for ( auto si = 0ull; si < segments.size( ); ++si )
		{
			const auto& seg = segments[ si ];

			auto pen_mod = seg.min_pen_mod;
			if ( seg.enter_surface.surface_type != seg.exit_surface.surface_type )
				pen_mod = std::min( pen_mod, seg.exit_surface.penetration );

			if ( should_stop_penetrating( seg, pen_mod ) )
				penetration_count = 0;

			if ( penetration_count <= 0 )
				return false;

			auto damage_modifier = 0.16f;
			if ( pen_mod >= 0.1f && seg.enter_surface.surface_type == seg.exit_surface.surface_type )
				apply_surface_override( pen_mod, damage_modifier, seg.enter_surface.surface_type, seg.thickness );

			current_damage = compute_damage_loss( current_damage, this->m_weapon_data, pen_mod, damage_modifier, seg.thickness );

			if ( current_damage < 1.0f )
				return false;

			--penetration_count;

			const auto next_wall = ( si + 1 < segments.size( ) ) ? segments[ si + 1 ].enter_distance : max_range;
			if ( check_target( seg.exit_pos, seg.exit_distance, next_wall ) )
				return true;
		}

		out = {};
		return false;
	}

	bool shared::penetration::can( const math::vector3& start, const math::vector3& direction, float& out_damage ) const
	{
		out_damage = 0.0f;

		if ( this->m_weapon_data.damage <= 0.0f )
			return false;

		const auto max_range = this->m_weapon_data.range;
		const auto ray_end   = start + direction * max_range;

		const auto first_hit = systems::g_bvh.trace_ray( start, ray_end );
		if ( !first_hit.hit )
			return false;

		const auto all_hits = systems::g_bvh.trace_ray_all( start, ray_end );
		const auto segments = systems::g_bvh.build_segments( all_hits, max_range );

		if ( segments.empty( ) )
		{
			if ( first_hit.surface.penetration >= 0.1f && this->m_weapon_data.penetration > 0.0f )
			{
				out_damage = this->m_weapon_data.damage;
				return true;
			}
			return false;
		}

		const auto& seg = segments[ 0 ];

		auto pen_mod = seg.min_pen_mod;
		if ( seg.enter_surface.surface_type != seg.exit_surface.surface_type )
			pen_mod = std::min( pen_mod, seg.exit_surface.penetration );

		if ( should_stop_penetrating( seg, pen_mod ) )
			return false;

		auto damage_modifier = 0.16f;
		if ( pen_mod >= 0.1f && seg.enter_surface.surface_type == seg.exit_surface.surface_type )
			apply_surface_override( pen_mod, damage_modifier, seg.enter_surface.surface_type, seg.thickness );

		const auto remaining = compute_damage_loss( this->m_weapon_data.damage, this->m_weapon_data, pen_mod, damage_modifier, seg.thickness );

		if ( remaining < 1.0f )
			return false;

		out_damage = remaining;
		return true;
	}

	float shared::penetration::get_max_damage( int hitgroup, int target_armor, bool has_helmet, int target_team ) const
	{
		if ( this->m_weapon_data.damage <= 0.0f )
			return 0.0f;

		auto damage = this->m_weapon_data.damage;
		detail::scale_damage( hitgroup, target_armor, has_helmet, target_team,
			this->m_weapon_data.armor_ratio, this->m_weapon_data.headshot_multiplier, damage );
		return damage;
	}

	// ============================================================================
	// CONTEXT (shared tick)
	// ============================================================================

	void shared::tick( )
	{
		context ctx{};

		const auto local_pawn = systems::g_local.pawn( );
		if ( !local_pawn )
		{
			this->store_context( {} );
			return;
		}

		const auto weapon_services = g::memory.read<std::uintptr_t>( local_pawn + SCHEMA( "C_BasePlayerPawn", "m_pWeaponServices"_hash ) );
		if ( !weapon_services )
		{
			this->store_context( {} );
			return;
		}

		const auto weapon_handle = g::memory.read<std::uint32_t>( weapon_services + SCHEMA( "CPlayer_WeaponServices", "m_hActiveWeapon"_hash ) );
		if ( !weapon_handle )
		{
			this->store_context( {} );
			return;
		}

		ctx.weapon = systems::g_entities.lookup( weapon_handle );
		if ( !ctx.weapon )
		{
			this->store_context( {} );
			return;
		}

		ctx.weapon_vdata = g::memory.read<std::uintptr_t>( ctx.weapon + SCHEMA( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
		if ( !ctx.weapon_vdata )
		{
			this->store_context( {} );
			return;
		}

		ctx.weapon_type = g::memory.read<std::uint32_t>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_WeaponType"_hash ) );
		ctx.item_def_idx  = g::memory.read<std::uint16_t>(
			ctx.weapon + SCHEMA( "C_EconEntity", "m_AttributeManager"_hash )
			+ SCHEMA( "C_AttributeContainer", "m_Item"_hash )
			+ SCHEMA( "C_EconItemView", "m_iItemDefinitionIndex"_hash )
		);
		ctx.num_bullets   = g::memory.read<int>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_nNumBullets"_hash ) );
		ctx.inaccuracy    = this->get_inaccuracy( local_pawn, ctx.weapon, ctx.weapon_vdata, systems::g_view.angles( ) );
		ctx.spread        = this->get_spread( ctx.weapon_vdata );
		ctx.recoil_index  = g::memory.read<float>( ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_flRecoilIndex"_hash ) );
		ctx.is_reloading  = g::memory.read<bool>( ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_bInReload"_hash ) );

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( global_vars )
			ctx.current_time = g::memory.read<float>( global_vars + cs2::global_vars_cur_time );

		ctx.cycle_time      = g::memory.read<float>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flCycleTime"_hash ) );
		ctx.last_shot_time  = g::memory.read<float>( ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_fLastShotTime"_hash ) );
		ctx.is_full_auto    = g::memory.read<bool>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_bIsFullAuto"_hash ) );

		// Inaccuracy base
		{
			const auto flags = g::memory.read<std::uint32_t>( local_pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
			const auto inaccuracy_stand  = g::memory.read<std::pair<float, float>>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyStand"_hash ) );
			const auto inaccuracy_crouch = g::memory.read<std::pair<float, float>>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyCrouch"_hash ) );

			const auto is_crouched = ( flags & ( 1 << 1 ) ) != 0;
			const auto& etc = is_crouched ? inaccuracy_crouch : inaccuracy_stand;

			const auto weapon_mode = g::memory.read<int>( ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_weaponMode"_hash ) );
			ctx.base_inaccuracy = weapon_mode ? etc.second : etc.first;
		}

		// Weapon ready / scoped state
		if ( ctx.weapon_type == cstypes::sniper )
		{
			ctx.weapon_ready = ( ctx.current_time - ctx.last_shot_time >= ctx.cycle_time );
			ctx.is_scoped    = g::memory.read<bool>( local_pawn + SCHEMA( "C_CSPlayerPawn", "m_bIsScoped"_hash ) );
		}
		else
		{
			ctx.weapon_ready = true;
		}

		ctx.valid = true;

		this->m_pen.prepare( ctx.weapon_vdata, ctx.weapon );
		this->store_context( ctx );
	}

	void shared::store_context( const context& ctx )
	{
		std::unique_lock lock( this->m_ctx_mutex );
		this->m_ctx = ctx;
	}

	// ============================================================================
	// HITCHANCE
	// ============================================================================

	float shared::calculate_hitchance(
		const math::vector3& eye_pos,
		const math::vector3& aim_angle,
		const systems::collector::player& target,
		const systems::bones::data& bones ) const
	{
		const auto& ctx = this->m_ctx;
		const auto total_spread = ctx.spread + ctx.inaccuracy;

		if ( total_spread < 0.0001f )
			return 1.0f;

		const auto range = this->m_pen.get_weapon_data( ).range;
		if ( range <= 0.0f )
			return 0.0f;

		struct capsule_t
		{
			math::vector3 start;
			math::vector3 end;
			float radius;
		};

		std::array<capsule_t, 20> capsules;
		auto capsule_count = 0;

		for ( const auto& hb : target.hitboxes )
		{
			const auto& bone = bones.bones[ hb.bone ];

			math::vector3 cap_start, cap_end;
			float radius;
			if ( !build_hitbox_capsule_v2( hb, bone, cap_start, cap_end, radius ) )
				continue;

			capsules[ capsule_count++ ] = { cap_start, cap_end, radius };
		}

		if ( capsule_count == 0 )
			return 0.0f;

		math::vector3 forward, right, up;
		aim_angle.to_directions( &forward, &right, &up );

		// 64 samples dão precisão estatística suficiente para o gate do triggerbot
		// (erro ~±6% vs ±3% com 256) com 4× menos custo no thread de combat.
		constexpr auto samples = 64;
		auto hits = 0;

		for ( int seed = 0; seed < samples; ++seed )
		{
			const auto spread = this->calculate_spread( seed, ctx.inaccuracy, ctx.spread, ctx.recoil_index, ctx.item_def_idx, ctx.num_bullets );
			const auto direction = ( forward + right * spread.x + up * spread.y ).normalized( );

			for ( auto i = 0; i < capsule_count; ++i )
			{
				if ( this->ray_hits_capsule( eye_pos, direction, capsules[ i ].start, capsules[ i ].end, capsules[ i ].radius ) )
				{
					++hits;
					break;
				}
			}

			const auto remaining = samples - ( seed + 1 );
			if ( hits + remaining < samples / 4 )
				break;
		}

		return static_cast<float>( hits ) / static_cast<float>( samples );
	}

	// ============================================================================
	// SPREAD
	// ============================================================================

	std::uint32_t shared::get_spread_seed( const math::vector3& angles, int tick ) const
	{
		struct
		{
			float pitch;
			float yaw;
			int player_render_tick;
		} buffer{};

		buffer.pitch             = detail::quantize_angle( angles.x );
		buffer.yaw               = detail::quantize_angle( angles.y );
		buffer.player_render_tick = tick;

		random::sha1 hash;
		hash.reset( );
		hash.update( &buffer, 12 );
		hash.final( );

		return hash.get_first_uint32( );
	}

	namespace detail_spread {

		struct spread_params
		{
			float inac_r, inac_a;
			float spr_r,  spr_a;
		};

		inline static void apply_revolver_curve( float& r )
		{
			r = 1.0f - ( r * r );
		}

		inline static void apply_negev_curve( float& r, float recoil_index )
		{
			auto v = r;
			auto c = 3;
			do { --c; v *= v; } while ( static_cast<float>( c ) > recoil_index );
			r = 1.0f - v;
		}

		inline static spread_params compute_raw_spread(
			random::valve_rng& rng,
			float inaccuracy, float spread,
			float recoil_index, int item_def_idx, int num_bullets )
		{
			constexpr auto two_pi = 2.0f * std::numbers::pi_v<float>;
			constexpr std::uint16_t revolver_id{ 64 };
			constexpr std::uint16_t negev_id{ 28 };

			spread_params p{};
			p.inac_r = rng.random_float( 0.0f, 1.0f );
			p.inac_a = rng.random_float( 0.0f, two_pi );

			if ( item_def_idx == revolver_id && num_bullets == 1 )
				apply_revolver_curve( p.inac_r );
			else if ( item_def_idx == negev_id && recoil_index < 3.0f )
				apply_negev_curve( p.inac_r, recoil_index );

			p.inac_r *= inaccuracy;

			p.spr_r = rng.random_float( 0.0f, 1.0f );
			p.spr_a = rng.random_float( 0.0f, two_pi );

			if ( item_def_idx == revolver_id && num_bullets == 1 )
				apply_revolver_curve( p.spr_r );
			else if ( item_def_idx == negev_id && recoil_index < 3.0f )
				apply_negev_curve( p.spr_r, recoil_index );

			p.spr_r *= spread;

			return p;
		}

	} // namespace detail_spread

	math::vector2 shared::calculate_spread(
		int seed, float inaccuracy, float spread,
		float recoil_index, int item_def_idx, int num_bullets ) const
	{
		random::valve_rng rng;
		rng.seed( seed );

		const auto p = detail_spread::compute_raw_spread( rng, inaccuracy, spread, recoil_index, item_def_idx, num_bullets );

		return
		{
			std::cosf( p.spr_a ) * p.spr_r + std::cosf( p.inac_a ) * p.inac_r,
			std::sinf( p.spr_a ) * p.spr_r + std::sinf( p.inac_a ) * p.inac_r
		};
	}

	// ============================================================================
	// PREDICTION / UTILS
	// ============================================================================

	float shared::get_prediction_time( ) const
	{
		const auto pawn       = systems::g_local.pawn( );
		const auto controller = systems::g_local.controller( );

		if ( !pawn || !controller )
			return 0.0f;

		const auto ping        = g::memory.read<int>( controller + SCHEMA( "CCSPlayerController", "m_iPing"_hash ) );
		const auto latency     = static_cast<float>( ping ) * 0.001f;
		// FIX: substituído offset hardcoded +0x290 por SCHEMA().
		// m_flInterpolationAmount é o campo correto para interp time no CS2.
		static const auto interp_offset = SCHEMA( "C_BasePlayerPawn", "m_flInterpolationAmount"_hash );
		const auto interp_time = interp_offset
			? g::memory.read<float>( pawn + interp_offset )
			: 0.0f;

		return latency * 0.5f + interp_time;
	}

	float shared::get_spread( std::uintptr_t weapon_vdata ) const
	{
		return g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flSpread"_hash ) );
	}

	// ============================================================================
	// INACCURACY
	// ============================================================================

	namespace detail_inaccuracy {

		inline static float select_mode( const std::pair<float, float>& p, int fire_mode )
		{
			return fire_mode ? p.second : p.first;
		}

		inline static float compute_move_factor( float speed, float edge0, float edge1 )
		{
			if ( edge0 == edge1 )
				return ( speed - edge1 >= 0.0f ) ? 1.0f : 0.0f;
			return std::clamp( ( speed - edge0 ) / ( edge1 - edge0 ), 0.0f, 1.0f );
		}

		inline static float compute_air_inaccuracy(
			float velocity_z,
			float jump_impulse,
			float inaccuracy_jump_initial,
			float inaccuracy_jump_apex )
		{
			const auto sqrt_threshold = std::sqrtf( std::fabsf( jump_impulse ) );
			const auto sqrt_vertical  = std::sqrtf( std::fabsf( velocity_z ) );
			const auto lo = sqrt_threshold * 0.25f;

			float air_inaccuracy;
			if ( lo == sqrt_threshold )
			{
				air_inaccuracy = ( sqrt_vertical - sqrt_threshold >= 0.0f )
					? inaccuracy_jump_initial
					: inaccuracy_jump_apex;
			}
			else
			{
				const auto frac = ( sqrt_vertical - lo ) / ( sqrt_threshold - lo );
				air_inaccuracy = inaccuracy_jump_apex + frac * ( inaccuracy_jump_initial - inaccuracy_jump_apex );
			}

			return std::clamp( air_inaccuracy, 0.0f, inaccuracy_jump_initial * 2.0f );
		}

	} // namespace detail_inaccuracy

	float shared::get_inaccuracy(
		std::uintptr_t pawn,
		std::uintptr_t weapon,
		std::uintptr_t weapon_vdata,
		const math::vector3& eye_angles ) const
	{
		const auto forcespread = systems::g_convars.get<float>( CONVAR( "weapon_accuracy_forcespread"_hash ) );
		if ( forcespread > 0.0f )
			return std::fminf( forcespread, 1.0f );

		const auto nospread = systems::g_convars.get<bool>( CONVAR( "weapon_accuracy_nospread"_hash ) );
		if ( nospread )
			return 0.0f;

		const auto fire_mode = g::memory.read<int>( weapon + SCHEMA( "C_CSWeaponBase", "m_weaponMode"_hash ) );
		auto inaccuracy = g::memory.read<float>( weapon + SCHEMA( "C_CSWeaponBase", "m_fAccuracyPenalty"_hash ) );
		const auto turning_inaccuracy = g::memory.read<float>( weapon + SCHEMA( "C_CSWeaponBase", "m_flTurningInaccuracy"_hash ) );

		const auto max_speed_pair         = g::memory.read<std::pair<float, float>>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flMaxSpeed"_hash ) );
		const auto inaccuracy_move_pair   = g::memory.read<std::pair<float, float>>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyMove"_hash ) );
		const auto inaccuracy_jump_initial = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyJumpInitial"_hash ) );
		const auto inaccuracy_jump_apex    = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyJumpApex"_hash ) );

		const auto max_speed      = detail_inaccuracy::select_mode( max_speed_pair, fire_mode );
		const auto inaccuracy_move = detail_inaccuracy::select_mode( inaccuracy_move_pair, fire_mode );

		const auto player_velocity = g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecVelocity"_hash ) );
		const auto speed = player_velocity.length_2d( );
		const auto flags = g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
		const auto is_walking = g::memory.read<bool>( pawn + SCHEMA( "C_CSPlayerPawn", "m_bIsWalking"_hash ) );
		const auto on_ground  = ( flags & 1 ) != 0;

		const auto edge0 = max_speed * 0.34f;
		const auto edge1 = max_speed * 0.95f;

		auto move_inaccuracy = 0.0f;
		auto move_factor = detail_inaccuracy::compute_move_factor( speed, edge0, edge1 );

		if ( move_factor > 0.0f )
		{
			if ( !is_walking )
				move_factor = std::powf( move_factor, 0.25f );
			move_inaccuracy = move_factor * inaccuracy_move;
		}

		auto air_inaccuracy = 0.0f;
		if ( !on_ground )
		{
			const auto jump_impulse = systems::g_convars.get<float>( CONVAR( "sv_jump_impulse"_hash ) );
			air_inaccuracy = detail_inaccuracy::compute_air_inaccuracy(
				player_velocity.z, jump_impulse,
				inaccuracy_jump_initial, inaccuracy_jump_apex
			);
		}

		return std::fminf( 1.0f, turning_inaccuracy + move_inaccuracy + air_inaccuracy + inaccuracy );
	}

	// ============================================================================
	// RAY / CAPSULE
	// ============================================================================

	bool shared::ray_hits_capsule(
		const math::vector3& ray_origin,
		const math::vector3& ray_dir,
		const math::vector3& capsule_start,
		const math::vector3& capsule_end,
		float radius ) const
	{
		const auto capsule_vec = capsule_end - capsule_start;
		const auto capsule_length = capsule_vec.length( );

		// Degenerate capsule (sphere)
		if ( capsule_length < 0.001f )
		{
			const auto to_center = capsule_start - ray_origin;
			const auto projection = to_center.dot( ray_dir );
			if ( projection < 0.0f )
				return false;

			const auto closest = ray_origin + ray_dir * projection;
			return ( closest - capsule_start ).length_sqr( ) <= radius * radius;
		}

		const auto capsule_dir = capsule_vec / capsule_length;
		const auto w = ray_origin - capsule_start;

		const auto a = ray_dir.dot( ray_dir );
		const auto b = ray_dir.dot( capsule_dir );
		const auto c = capsule_dir.dot( capsule_dir );
		const auto d = ray_dir.dot( w );
		const auto e = capsule_dir.dot( w );

		const auto denom = a * c - b * b;

		float s, t;
		if ( std::abs( denom ) < 0.0001f )
		{
			s = 0.0f;
			t = ( b > c ? d / b : e / c );
		}
		else
		{
			s = ( b * e - c * d ) / denom;
			t = ( a * e - b * d ) / denom;
		}

		t = std::clamp( t, 0.0f, capsule_length );
		if ( s < 0.0f )
			return false;

		const auto point_on_capsule = capsule_start + capsule_dir * t;
		const auto point_on_ray     = ray_origin + ray_dir * s;

		return ( point_on_ray - point_on_capsule ).length_sqr( ) <= radius * radius;
	}

	// ============================================================================
	// STATE
	// ============================================================================

	bool shared::is_weapon_max_accuracy( ) const
	{
		return this->m_ctx.valid && this->m_ctx.inaccuracy <= this->m_ctx.base_inaccuracy + 0.0001f;
	}

} // namespace features::combat

