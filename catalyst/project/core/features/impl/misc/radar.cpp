#include <stdafx.hpp>
#include "radar.hpp"
#include <core/settings.hpp>

namespace features::misc {

void radar::tick( )
{
    const auto spotted_off = SCHEMA( "C_BaseEntity", "m_bSpotted"_hash );

    if ( !settings::g_misc.m_radar.enabled )
    {
        // Ao desabilitar: limpa m_bSpotted nos inimigos que foram spottados artificialmente.
        // Sem isso, os pontos ficam no minimap até o CS2 limpar o campo naturalmente.
        if ( spotted_off )
        {
            systems::g_collector.with_players( [&]( const std::vector<systems::collector::player>& players )
            {
                for ( const auto& player : players )
                {
                    if ( !systems::g_local.is_enemy( player.team ) ) continue;
                    if ( !player.pawn ) continue;

                    const bool spotted = g::memory.read<bool>( player.pawn + spotted_off );
                    if ( spotted )
                        g::memory.write<bool>( player.pawn + spotted_off, false );
                }
            } );
        }
        return;
    }

    // Seta m_bSpotted = true em todos os pawns inimigos.
    // O CS2 usa esse bool para exibir o ponto vermelho no minimap (radar).
    // Somente inimigos vivos que o collector já trackeia — sem iterar entity list extra.
    systems::g_collector.with_players( [&]( const std::vector<systems::collector::player>& players )
    {
        if ( !spotted_off )
            return;

        for ( const auto& player : players )
        {
            if ( !systems::g_local.is_enemy( player.team ) )
                continue;

            if ( !player.pawn )
                continue;

            // Lê primeiro para evitar write desnecessário (evita dirty page)
            const bool already_spotted = g::memory.read<bool>( player.pawn + spotted_off );
            if ( !already_spotted )
                g::memory.write<bool>( player.pawn + spotted_off, true );
        }
    } );
}


// ============================================================================
// ON_RENDER — painel custom (minimap overlay)
// ============================================================================
// Exibe um painel 2D no canto da tela com os inimigos posicionados relativamente
// ao jogador local. Usa pos_x/pos_y/size/zoom/enemy_color/show_names do settings.
// Não depende de nenhum dado do minimap nativo do CS2.
// ============================================================================

void radar::on_render( zdraw::draw_list& draw_list )
{
    const auto& cfg = settings::g_misc.m_radar;

    // O painel é desenhado independentemente do radar hack (m_bSpotted).
    // Pode ser usado sozinho como radar visual sem escrever no jogo.
    if ( !cfg.enabled )
        return;

    const auto local_pawn = systems::g_local.pawn( );
    if ( !local_pawn )
        return;

    // ── Parâmetros do painel ────────────────────────────────────────────────
    const float px      = cfg.pos_x.value;
    const float py      = cfg.pos_y.value;
    const float size    = std::max( 80.f, cfg.size.value );   // mínimo 80px
    const float zoom    = std::max( 50.f, cfg.zoom.value );   // unidades world/metade do painel
    const float half    = size * 0.5f;

    // ── Fundo ──────────────────────────────────────────────────────────────
    draw_list.add_rect_filled( px, py, size, size, { 10, 10, 15, 200 } );
    draw_list.add_rect( px, py, size, size, { 60, 60, 80, 180 }, 1.5f );

    // Cruz central
    const float cx = px + half;
    const float cy = py + half;
    constexpr float k_cross = 5.f;
    draw_list.add_line( cx - k_cross, cy, cx + k_cross, cy, { 100, 100, 120, 160 }, 1.f );
    draw_list.add_line( cx, cy - k_cross, cx, cy + k_cross, { 100, 100, 120, 160 }, 1.f );

    // ── Posição e ângulo do local ──────────────────────────────────────────
    const auto local_origin = g::memory.read<math::vector3>(
        local_pawn + SCHEMA( "C_BaseEntity", "m_vecAbsOrigin"_hash )
    );
    const float local_yaw_rad = systems::g_view.angles( ).y * ( 3.14159265f / 180.f );
    const float sin_yaw = std::sinf( local_yaw_rad );
    const float cos_yaw = std::cosf( local_yaw_rad );

    // ── Inimigos ───────────────────────────────────────────────────────────
    const auto& col = cfg.enemy_color.value;

    systems::g_collector.with_players( [&]( const std::vector<systems::collector::player>& players )
    {
        for ( const auto& player : players )
        {
            if ( !systems::g_local.is_enemy( player.team ) )
                continue;

            if ( player.health <= 0 )
                continue;

            // Delta world → delta no painel (rotacionado com o yaw local)
            const float dx_world = player.origin.x - local_origin.x;
            const float dy_world = player.origin.y - local_origin.y;

            // Rotacionar para ficar alinhado com a câmera (norte = frente)
            const float dx_rot =  dx_world * cos_yaw + dy_world * sin_yaw;
            const float dy_rot = -dx_world * sin_yaw + dy_world * cos_yaw;

            // Escalar para pixels (zoom = unidades world que cabem em half pixels)
            const float screen_x = cx + ( dx_rot / zoom ) * half;
            const float screen_y = cy - ( dy_rot / zoom ) * half; // Y invertido

            // Clipar ao painel
            if ( screen_x < px + 2.f || screen_x > px + size - 2.f ) continue;
            if ( screen_y < py + 2.f || screen_y > py + size - 2.f ) continue;

            // Ponto do inimigo
            constexpr float k_dot = 4.f;
            draw_list.add_rect_filled(
                screen_x - k_dot * 0.5f, screen_y - k_dot * 0.5f,
                k_dot, k_dot,
                { col.r, col.g, col.b, col.a }
            );
            // Outline escuro
            draw_list.add_rect(
                screen_x - k_dot * 0.5f - 1.f, screen_y - k_dot * 0.5f - 1.f,
                k_dot + 2.f, k_dot + 2.f,
                { 0, 0, 0, 180 }, 1.f
            );

            // Nome opcional
            if ( cfg.show_names && !player.display_name.empty( ) )
            {
                zdraw::push_font( g::render.fonts( ).pixel7_10 );
                const auto [tw, th] = zdraw::measure_text( player.display_name );
                draw_list.add_text(
                    screen_x - tw * 0.5f, screen_y + k_dot + 2.f,
                    player.display_name, nullptr,
                    { 220, 220, 220, 200 }, zdraw::text_style::outlined
                );
                zdraw::pop_font( );
            }
        }
    } );

    // ── Label "RADAR" no topo do painel ───────────────────────────────────
    zdraw::push_font( g::render.fonts( ).pixel7_10 );
    draw_list.add_text( px + 4.f, py + 2.f, "RADAR", nullptr,
        { 130, 130, 160, 200 }, zdraw::text_style::normal );
    zdraw::pop_font( );
}
