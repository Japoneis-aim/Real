#include <stdafx.hpp>

namespace features::misc {

void speed_esp::on_render( zdraw::draw_list& draw_list )
{
    if ( !settings::g_misc.m_speed_esp.enabled )
        return;

    if ( !systems::g_local.alive( ) )
        return;

    const auto speed = systems::g_local.velocity_xy( );
    const auto display = zdraw::get_display_size( );

    // Exibe no canto inferior central — clássico para bhop/surf.
    // Formato: "XXX u/s" com cor que muda por faixa de velocidade.
    zdraw::push_font( g::render.fonts( ).pixel7_10 );

    const auto text = std::format( "{:.0f} u/s", speed );
    const auto [tw, th] = zdraw::measure_text( text );

    const float cx = static_cast<float>( display.first ) * 0.5f - tw * 0.5f;
    const float cy = static_cast<float>( display.second ) * 0.72f;

    // Cor: branco normal, amarelo acima de walk speed (250), verde acima de bhop speed (285)
    zdraw::rgba col;
    if ( speed >= 285.f )      col = { 80, 255, 80, 230 };
    else if ( speed >= 250.f ) col = { 255, 220, 50, 230 };
    else                       col = { 230, 230, 230, 200 };

    draw_list.add_text( cx, cy, text, nullptr, col, zdraw::text_style::outlined );
    zdraw::pop_font( );
}

} // namespace features::misc
