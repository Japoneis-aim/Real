#include <stdafx.hpp>
#include "../../../memory/safe_read.hpp"
#include "../../../offsets/offsets.hpp" // synced
// offsets: namespace renamed to offsets_map in header; keep include to refresh build
#include "../../../offsets/game_structs.hpp"

namespace features::misc {

	void impacts::on_render( zdraw::draw_list& draw_list )
	{
		// Tentativa conservadora de ler um vetor global de impactos.
		// O endereço real deve ser fornecido via output/offsets.map com a chave "m_vecBulletImpacts".

	const auto impacts_addr = offsets_map::get( "m_vecBulletImpacts" );
		if ( !impacts_addr )
			return;

		// Layout incerto: vamos tentar ler até N entradas como { vec3 pos; float time; }
		constexpr auto max_impacts = 256;
		constexpr auto entry_size = sizeof( float ) * 4; // vec3 + time

		std::vector<std::byte> buf( max_impacts * entry_size );
		if ( !g::memory.read( impacts_addr, buf.data(), buf.size() ) )
			return;

		for ( auto i = 0u; i < max_impacts; ++i )
		{
			const auto off = i * entry_size;
			const float px = *reinterpret_cast<const float*>( buf.data() + off + 0 );
			const float py = *reinterpret_cast<const float*>( buf.data() + off + 4 );
			const float pz = *reinterpret_cast<const float*>( buf.data() + off + 8 );
			const float t  = *reinterpret_cast<const float*>( buf.data() + off + 12 );

			// filtro simples: posições inválidas ignoradas
			if ( std::isnan( px ) || std::isnan( py ) || std::isnan( pz ) )
				continue;

			math::vector3 world{ px, py, pz };
			const auto screen = systems::g_view.project( world );
			if ( !systems::g_view.projection_valid( screen ) )
				continue;

			// fade baseado em t (se t for timestamp epoch ou tempo do jogo, ajuste conforme necessário)
			float alpha = 1.0f;
			if ( t > 0.0f )
			{
				// assumir t é tempo de jogo em segundos, ler current_time do global_vars
				const auto gv = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
				const auto current_time = gv ? g::memory.read<float>( gv + 0x30 ) : 0.0f;
				const auto age = std::max( 0.0f, current_time - t );
				constexpr auto life = 5.0f;
				alpha = std::clamp( 1.0f - age / life, 0.0f, 1.0f );
			}

			if ( alpha <= 0.01f )
				continue;

			const auto col = zdraw::rgba{ 255, 200, 0, static_cast<unsigned char>( alpha * 255.0f ) };
			constexpr auto sz = 6.0f;

			draw_list.add_rect_filled( screen.x - sz * 0.5f, screen.y - sz * 0.5f, sz, sz, col );
			draw_list.add_line( screen.x - sz, screen.y - sz, screen.x + sz, screen.y + sz, zdraw::rgba{ 0,0,0, static_cast<unsigned char>( alpha*128 ) } );
		}
	}

} // namespace features::misc

