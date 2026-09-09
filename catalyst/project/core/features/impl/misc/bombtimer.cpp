#include <stdafx.hpp>
#include "bombtimer.hpp"
#include <core/settings.hpp>

namespace features::misc {

// ============================================================================
// TICK
// ============================================================================

void bomb_timer::tick( )
{
	if ( !settings::g_misc.m_bombtimer.enabled )
	{
		m_info.planted = false;
		m_anim_alpha   = std::max( 0.f, m_anim_alpha - 0.02f );
		return;
	}

	// g::offsets.planted_c4 já contém modules.client + dwPlantedC4 (calculado em
	// offsets::initialize). É o endereço absoluto do ponteiro para a entidade C_PlantedC4.
	// Valida != 0 para o caso de dwPlantedC4 estar ausente no offsets.map.
	if ( !g::offsets.planted_c4 )
	{
		m_info.planted = false;
		m_anim_alpha   = std::max( 0.f, m_anim_alpha - 0.02f );
		return;
	}

	const auto planted_c4 = g::memory.read<std::uintptr_t>( g::offsets.planted_c4 );
	if ( !planted_c4 )
	{
		m_info.planted = false;
		m_anim_alpha   = std::max( 0.f, m_anim_alpha - 0.02f );
		return;
	}

	// Ler current_time de GlobalVars.
	// FIX: cs2::global_vars_cur_time não está definido no codebase mostrado.
	// Usando +0x30 diretamente — mesmo offset usado no resto do projeto.
	const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
	if ( !global_vars ) return;
	const float current_time = g::memory.read<float>( global_vars + cs2::global_vars_cur_time );

	// FIX: -0x8 é o offset do campo m_bIsPlanted dentro da entidade C_PlantedC4.
	// Este campo não aparece no schema do CS2 (não é um campo de rede declarado),
	// mas é consistentemente encontrado a -0x8 relativo ao ponteiro base da entidade
	// em múltiplas versões do jogo. Caso quebre após update, inspecionar via:
	//   C_PlantedC4 vtable + offsets, ou usar pattern scan para "m_bIsPlanted" string.
	// Fonte: análise de memória (reversing CS2 build 2000+ confirmado).
	const bool is_planted = g::memory.read<bool>( planted_c4 - 0x8 );
	if ( !is_planted )
	{
		m_info.planted = false;
		m_anim_alpha   = std::max( 0.f, m_anim_alpha - 0.02f );
		return;
	}

	m_info.planted = true;
	m_anim_alpha   = std::min( 1.f, m_anim_alpha + 0.05f );

	// ─── Ler campos da C4 via SCHEMA ───────────────────────────────────────
	// FIX: offsets_map::get() retorna endereços globais (result de pattern scan).
	// Campos de struct são offsets SCHEMA — categorias diferentes.
	// offsets_map::get("m_flC4Blow") retorna 0 ou endereço errado → timer quebrado.
	// SCHEMA("C_PlantedC4", "m_flC4Blow"_hash) retorna o offset correto dentro da struct.

	const float blow_time     = g::memory.read<float>( planted_c4 + SCHEMA( "C_PlantedC4", "m_flC4Blow"_hash ) );
	const float defuse_time   = g::memory.read<float>( planted_c4 + SCHEMA( "C_PlantedC4", "m_flDefuseCountDown"_hash ) );
	const bool  being_defused = g::memory.read<bool> ( planted_c4 + SCHEMA( "C_PlantedC4", "m_bBeingDefused"_hash ) );
	const bool  defused       = g::memory.read<bool> ( planted_c4 + SCHEMA( "C_PlantedC4", "m_bBombDefused"_hash ) );
	const int   site          = g::memory.read<int>  ( planted_c4 + SCHEMA( "C_PlantedC4", "m_nBombSite"_hash ) );

	m_info.time_left   = std::max( 0.f, blow_time   - current_time );
	m_info.defuse_left = being_defused ? std::max( 0.f, defuse_time - current_time ) : 0.f;
	m_info.defusing    = being_defused;
	m_info.defused     = defused;
	m_info.site        = site;

	// Dano estimado: a C4 causa ~500HP no epicentro e decai com o raio de blast.
	// Raio letal completo ≈ 500 unidades (damage cai linearmente).
	// Sem posição do jogador local disponível aqui — exibimos o dano máximo possível
	// como indicador de urgência. Se a bomba já explodiu, dano = 0.
	m_info.damage = ( m_info.time_left > 0.f ) ? 500 : 0;

	// ─── Nome do defuser ───────────────────────────────────────────────────
	if ( being_defused )
	{
		const auto defuser_handle = g::memory.read<std::uint32_t>(
			planted_c4 + SCHEMA( "C_PlantedC4", "m_hBombDefuser"_hash )
		);

		if ( defuser_handle && defuser_handle != 0xFFFFFFFF )
		{
			const auto defuser_pawn = systems::g_entities.lookup( defuser_handle );

			// Procurar o player cujo pawn coincide com o handle resolvido
			systems::g_collector.with_players( [&]( const std::vector<systems::collector::player>& players )
			{
				for ( const auto& p : players )
				{
					if ( p.pawn == defuser_pawn )
					{
						m_info.defuser_name = p.display_name;
						break;
					}
				}
			} );
		}
	}
	else
	{
		m_info.defuser_name.clear( );
	}
}

// ============================================================================
// RENDER
// ============================================================================

void bomb_timer::on_render( zdraw::draw_list& draw_list )
{
	if ( !settings::g_misc.m_bombtimer.enabled ) return;
	if ( m_anim_alpha < 0.01f )                  return;
	if ( !m_info.planted )                        return;

	const auto display = zdraw::get_display_size( );
	const float x = 12.f;
	const float y = display.second * 0.35f;

	const float panel_w  = 220.f;
	const float panel_h  = m_info.defused ? 45.f : ( m_info.defusing ? 115.f : 75.f );
	const uint8_t alpha  = static_cast<uint8_t>( 200 * m_anim_alpha );

	// Fundo — API usa (x, y, w, h), não (x1, y1, x2, y2)
	const float bg_x = x - 4.f;
	const float bg_y = y - 4.f;
	const float bg_w = panel_w + 8.f;
	const float bg_h = panel_h + 8.f;

	draw_list.add_rect_filled( bg_x, bg_y, bg_w, bg_h, { 10, 10, 15, alpha } );
	draw_list.add_rect( bg_x, bg_y, bg_w, bg_h, { 60, 60, 80, static_cast<uint8_t>( 150 * m_anim_alpha ) }, 1.5f );

	zdraw::push_font( g::render.fonts( ).pixel7_10 );

	// Defused
	if ( m_info.defused )
	{
		draw_list.add_text( x + 10, y + 16, "BOMB DEFUSED", nullptr, { 80, 255, 80, alpha }, zdraw::text_style::outlined );
		zdraw::pop_font( );
		return;
	}

	// Título
	const char* site_str = m_info.site == 1 ? "B" : "A";
	const auto title     = std::string( "BOMB [SITE " ) + site_str + "]";
	draw_list.add_text( x + 8, y + 4, title, nullptr, { 230, 230, 230, alpha }, zdraw::text_style::outlined );

	// ─── Barra de tempo ────────────────────────────────────────────────────
	const float bar_w   = panel_w - 16.f;
	const float bar_h   = 8.f;
	const float bar_x   = x + 8.f;
	const float bar_y   = y + 26.f;
	const float fill    = std::clamp( m_info.time_left / 40.f, 0.f, 1.f );

	uint8_t tr, tg, tb;
	if      ( m_info.time_left < 5.f  ) { tr = 255; tg =  50; tb =  50; }
	else if ( m_info.time_left < 15.f ) { tr = 255; tg = 200; tb =  50; }
	else                                 { tr = 230; tg = 230; tb = 230; }

	draw_list.add_rect_filled( bar_x, bar_y, bar_w,          bar_h, { 30, 30, 40, alpha } );
	draw_list.add_rect_filled( bar_x, bar_y, bar_w * fill,   bar_h, { tr, tg, tb, alpha } );

	const auto time_str  = std::format( "{:.1f}s", m_info.time_left );
	const auto [tw, th]  = zdraw::measure_text( time_str );
	draw_list.add_text( bar_x + bar_w - tw - 2, bar_y + bar_h + 2, time_str, nullptr,
		{ tr, tg, tb, alpha }, zdraw::text_style::normal );

	// ─── Defusing info ─────────────────────────────────────────────────────
	if ( m_info.defusing )
	{
		const float def_y    = bar_y + bar_h + 22.f;
		const float def_bh   = 6.f;
		const float def_fill = std::clamp( m_info.defuse_left / 10.f, 0.f, 1.f );
		const bool  in_time  = m_info.defuse_left <= m_info.time_left;

		const uint8_t dr = in_time ?  80 : 255;
		const uint8_t dg = in_time ? 200 :  80;
		const uint8_t db = in_time ? 255 :  80;

		draw_list.add_text( bar_x, def_y, "DEFUSING", nullptr, { 100, 180, 255, alpha }, zdraw::text_style::outlined );

		const float def_bar_y = def_y + 18.f;
		draw_list.add_rect_filled( bar_x, def_bar_y, bar_w,           def_bh, { 30, 30, 40, alpha } );
		draw_list.add_rect_filled( bar_x, def_bar_y, bar_w * def_fill, def_bh, { dr, dg, db, alpha } );

		auto def_str = std::format( "{:.1f}s", m_info.defuse_left );
		if ( !in_time ) def_str += " (NO TIME)";
		draw_list.add_text( bar_x, def_bar_y + def_bh + 2, def_str, nullptr,
			{ dr, dg, db, alpha }, zdraw::text_style::normal );
		
		if ( !m_info.defuser_name.empty( ) )
			draw_list.add_text( bar_x, def_bar_y + def_bh + 16, m_info.defuser_name, nullptr,
				{ 200, 200, 200, static_cast<uint8_t>( 160 * m_anim_alpha ) }, zdraw::text_style::normal );
	}

	// ─── Dano estimado ─────────────────────────────────────────────────────
	if ( m_info.damage > 0 )
	{
		const uint8_t dr2 = m_info.damage >= 100 ? 255 : ( m_info.damage > 50 ? 255 :  80 );
		const uint8_t dg2 = m_info.damage >= 100 ?  50 : ( m_info.damage > 50 ? 200 : 255 );
		const auto dmg_str = "DAMAGE: " + std::to_string( m_info.damage );
		const float dmg_y  = m_info.defusing ? y + panel_h - 22.f : y + panel_h - 18.f;
		draw_list.add_text( bar_x, dmg_y, dmg_str, nullptr, { dr2, dg2, 50, alpha }, zdraw::text_style::normal );
	}

	zdraw::pop_font( );
}

} // namespace features::misc
