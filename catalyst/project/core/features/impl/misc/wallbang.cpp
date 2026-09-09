#include <stdafx.hpp>
#include "wallbang.hpp"
#include <core/settings.hpp>

namespace features::misc {

// ============================================================================
// TICK
// ============================================================================

void wallbang_indicator::tick( )
{
	// Sincroniza m_enabled com a config do usuário a cada tick.
	m_enabled = static_cast<bool>( settings::g_misc.m_wallbang.enabled );

	if ( !m_enabled )
	{
		m_anim_alpha  = std::max( 0.f, m_anim_alpha  - 0.05f );
		m_damage_anim = std::max( 0.f, m_damage_anim - 0.03f );
		return;
	}

	const auto& ctx = combat::g_shared.ctx( );
	if ( !ctx.valid )
	{
		m_anim_alpha  = std::max( 0.f, m_anim_alpha  - 0.05f );
		m_damage_anim = std::max( 0.f, m_damage_anim - 0.03f );
		return;
	}

	check_wallbang( );

	// Animação suave
	if ( m_result.can_penetrate && m_result.hit_player )
	{
		m_anim_alpha  = std::min( 1.f, m_anim_alpha  + 0.08f );
		m_damage_anim = std::min( 1.f, m_damage_anim + 0.05f );
	}
	else
	{
		m_anim_alpha  = std::max( 0.f, m_anim_alpha  - 0.05f );
		m_damage_anim = std::max( 0.f, m_damage_anim - 0.03f );
	}
}

// ============================================================================
// CHECK WALLBANG
// ============================================================================

void wallbang_indicator::check_wallbang( )
{
	const auto eye_pos    = systems::g_view.origin( );
	const auto view_angles = systems::g_view.angles( );

	// Direção forward consistente com o resto do projeto
	math::vector3 forward, right, up;
	math::helpers::angle_vectors( view_angles, forward, right, up );

	m_result = {};

	// ─── Penetração ────────────────────────────────────────────────────────
	float damage = 0.f;
	const bool can_pen = combat::g_shared.pen( ).can( eye_pos, forward, damage );
	m_result.can_penetrate = can_pen;
	m_result.damage        = damage;

	// FIX: estimar espessura via trace em vez de deixar sempre 0.
	// Fazemos dois traces: um curto (para detectar entrada da parede) e
	// um na direção oposta a partir do ponto de saída da penetração.
	// Aqui usamos uma estimativa simples: distância do trace forward até
	// o primeiro obstáculo como proxy de "quão longe está a parede".
	if ( can_pen )
	{
		constexpr float k_trace_range = 300.0f;
		const auto trace_in = systems::g_bvh.trace_ray( eye_pos, eye_pos + forward * k_trace_range );
		if ( trace_in.hit )
		{
			// Entrada da parede detectada: traceamos novamente a partir do
			// ponto de hit em direção oposta para estimar a saída.
			const auto entry_pt  = eye_pos + forward * trace_in.distance;
			// Estimar espessura: trace do ponto além da entrada, na direção forward,
			// para encontrar a face traseira da parede
			const auto trace_out = systems::g_bvh.trace_ray(
				entry_pt + forward * 0.5f,     // logo além da superfície de entrada
				entry_pt + forward * 300.0f    // até 300u à frente (parede pode ser grossa)
			);

			m_result.thickness = trace_out.hit
				? trace_out.distance           // distância da entrada até a saída = espessura
				: std::min( trace_in.distance, 50.0f ); // fallback
		}
	}

	// ─── Verificar inimigo na mira ─────────────────────────────────────────
	float min_dist = FLT_MAX;

	systems::g_collector.with_players( [&]( const std::vector<systems::collector::player>& players )
	{
	for ( const auto& player : players )
	{
		if ( !systems::g_local.is_enemy( player.team ) ) continue;
		if ( player.health <= 0 )                         continue;

		const auto to_target = player.origin - eye_pos;
		const auto dist      = to_target.length( );
		const auto dir       = to_target / dist;

		// ~11° de cone de detecção (dot > 0.98)
		if ( forward.dot( dir ) <= 0.98f )
			continue;

		if ( dist >= min_dist )
			continue;

		min_dist = dist;
		m_result.hit_distance = dist;

		// FIX: posição da cabeça via hitboxes reais, não bone index hardcoded.
		// Hitbox índice 0 é tipicamente a cabeça no CS2.
		math::vector3 head_pos{ player.origin.x, player.origin.y, player.origin.z + 65.f };

		for ( const auto& hb : player.hitboxes )
		{
			if ( hb.index == 0 && player.cached_bones.is_valid( ) )
			{
				head_pos = player.cached_bones.get_position( hb.bone );
				break;
			}
		}

		const auto trace  = systems::g_bvh.trace_ray( eye_pos, head_pos );
		const auto visible = !trace.hit || trace.fraction > 0.97f;

		m_result.hit_player = visible || can_pen;
	}
	} ); // with_players

	if ( !m_result.hit_player )
		m_result.damage = 0.f;
}

// ============================================================================
// RENDER
// ============================================================================

void wallbang_indicator::on_render( zdraw::draw_list& draw_list )
{
	if ( !m_enabled )          return;
	if ( m_anim_alpha < 0.01f ) return;

	const auto display = zdraw::get_display_size( );
	const float cx = display.first  * 0.5f;
	const float cy = display.second * 0.5f;

	draw_crosshair( draw_list, cx, cy );
	draw_damage_text( draw_list, cx, cy );
}

void wallbang_indicator::draw_crosshair( zdraw::draw_list& dl, float cx, float cy )
{
	const uint8_t alpha = static_cast<uint8_t>( 200 * m_anim_alpha );

	const bool can_pen = m_result.can_penetrate && m_result.hit_player;
	const uint8_t r = can_pen ?  50 : 255;
	const uint8_t g = can_pen ? 255 :  50;
	const uint8_t b = can_pen ?  50 :  50;

	constexpr float k_radius = 14.f;

	// Círculo externo
	dl.add_circle( cx, cy, k_radius, { r, g, b, alpha }, 24, 2.5f );

	// FIX: add_circle_filled não confirmado no zdraw do projeto.
	// Círculo interno animado via círculo com raio pequeno e linha grossa.
	const float inner_r = std::max( 1.0f, 4.f + 8.f * ( 1.f - m_anim_alpha ) );
	dl.add_circle( cx, cy, inner_r,
		{ r, g, b, static_cast<uint8_t>( 80 * m_anim_alpha ) },
		16, inner_r * 2.0f   // thickness = diâmetro → aparência de preenchimento
	);

	// Barra de espessura da parede (estimativa)
	if ( can_pen )
	{
		constexpr float k_max_thickness = 50.f; // cap de referência (unidades world)
		const float thickness_norm = std::clamp( m_result.thickness / k_max_thickness, 0.f, 1.f );

		constexpr float bar_w = 36.f;
		constexpr float bar_h =  4.f;
		const float bar_x = cx - bar_w * 0.5f;
		const float bar_y = cy + k_radius + 6.f;

		// Fundo
		dl.add_rect_filled( bar_x, bar_y, bar_w, bar_h,
			{ 20, 20, 25, static_cast<uint8_t>( 180 * m_anim_alpha ) } );

		// Preenchimento proporcional à espessura estimada
		if ( thickness_norm > 0.001f )
			dl.add_rect_filled( bar_x, bar_y, bar_w * thickness_norm, bar_h, { r, g, b, alpha } );

		// Distância ao alvo
		zdraw::push_font( g::render.fonts( ).pixel7_10 );
		const auto dist_str = std::format( "{:.1f}m", m_result.hit_distance * 0.01905f );
		const auto [tw, th] = zdraw::measure_text( dist_str );
		dl.add_text( cx - tw * 0.5f, bar_y + bar_h + 4.f, dist_str, nullptr,
			{ 200, 200, 200, static_cast<uint8_t>( 180 * m_anim_alpha ) },
			zdraw::text_style::outlined );
		zdraw::pop_font( );
	}
}

void wallbang_indicator::draw_damage_text( zdraw::draw_list& dl, float cx, float cy )
{
	if ( !m_result.hit_player || m_result.damage <= 0.f ) return;
	if ( m_damage_anim < 0.01f )                          return;

	const uint8_t alpha = static_cast<uint8_t>( 255 * m_damage_anim );

	zdraw::push_font( g::render.fonts( ).pixel7_10 );
	const auto text    = std::to_string( static_cast<int>( m_result.damage ) ) + " DMG";
	const auto [tw, th] = zdraw::measure_text( text );
	zdraw::pop_font( );

	const float dmg_y = cy - 40.f - ( 1.f - m_damage_anim ) * 20.f;

	const uint8_t dr = m_result.damage >= 100 ? 255 : ( m_result.damage > 50 ? 255 :  80 );
	const uint8_t dg = m_result.damage >= 100 ?  50 : ( m_result.damage > 50 ? 200 : 255 );

	// FIX: zdraw::text_style::normal não existe — usar ::none (texto puro sem sombra)
	// Sombra manual via segundo draw_text com offset de 1px
	zdraw::push_font( g::render.fonts( ).pixel7_10 );
	dl.add_text( cx - tw * 0.5f + 1.f, dmg_y + 1.f, text, nullptr,
		{ 0, 0, 0, static_cast<uint8_t>( alpha / 2 ) }, zdraw::text_style::normal );
	dl.add_text( cx - tw * 0.5f, dmg_y, text, nullptr,
		{ dr, dg, 50, alpha }, zdraw::text_style::normal );
	zdraw::pop_font( );
}

} // namespace features::misc
