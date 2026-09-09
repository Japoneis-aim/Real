#include <stdafx.hpp>

// ============================================================================
// impacts.cpp — reescrita completa
//
// O placeholder anterior lia um "endereço global m_vecBulletImpacts" que não
// existe como struct global no CS2. Impactos de bala ficam em:
//   C_CSPlayerPawn  → m_pBulletServices (ponteiro)
//     └─ CCSPlayer_BulletServices → m_vecBulletImpacts (CNetworkUtlVectorBase<Vector>)
//
// A estratégia:
//   - A cada frame varremos todos os players (não só inimigos — todos atiram)
//   - Para cada player rastreamos a contagem do vetor de impactos
//   - Quando a contagem cresce, lemos as novas entradas e as armazenamos
//     localmente com timestamp do tempo atual do jogo
//   - Renderizamos os impactos armazenados com fade ao longo de k_lifetime segundos
// ============================================================================

namespace features::misc {

	// ─── Helpers locais ─────────────────────────────────────────────────────

	namespace {

		struct impact_entry
		{
			math::vector3 pos;
			float         game_time;
		};

		struct player_tracker
		{
			int last_count{ 0 };
		};

		std::vector<impact_entry>                           s_impacts;
		std::unordered_map<std::uintptr_t, player_tracker> s_trackers;

		constexpr float  k_sz            = 5.0f;
		constexpr int    k_max_per_frame = 32;
		constexpr int    k_max_stored    = 512;
		constexpr int    k_max_vec_count = 64;

		// Captura novos impactos de bala para todos os players e preenche s_impacts.
		// Retorna false se os offsets de schema ainda não estão prontos.
		static bool collect_impacts( float current_time )
		{
			static std::uintptr_t s_bullet_services_off = 0;
			static std::uintptr_t s_impacts_vec_off     = 0;
			static bool           s_schema_ready        = false;

			if ( !s_schema_ready )
			{
				s_bullet_services_off = SCHEMA( "C_CSPlayerPawn", "m_pBulletServices"_hash );
				s_impacts_vec_off     = SCHEMA( "CCSPlayer_BulletServices", "m_vecBulletImpacts"_hash );
				s_schema_ready        = true;
			}

			if ( !s_bullet_services_off || !s_impacts_vec_off )
				return false;

			int new_this_frame = 0;

			for ( const auto& player : systems::g_collector.players( ) )
			{
				if ( !player.pawn )
					continue;

				auto& tracker = s_trackers[ player.pawn ];

				const auto services = g::memory.read<std::uintptr_t>(
					player.pawn + s_bullet_services_off
				);
				if ( !services )
				{
					tracker.last_count = 0;
					continue;
				}

				const auto data_ptr = g::memory.read<std::uintptr_t>( services + s_impacts_vec_off );
				const int  count    = g::memory.read<std::int32_t> ( services + s_impacts_vec_off + 0x10 );

				if ( count <= 0 || !data_ptr || count > k_max_vec_count )
				{
					tracker.last_count = 0;
					continue;
				}

				if ( count <= tracker.last_count )
				{
					if ( count < tracker.last_count )
						tracker.last_count = 0;
					continue;
				}

				const int new_count = std::min( count - tracker.last_count, k_max_per_frame - new_this_frame );
				tracker.last_count  = count;

				if ( new_count <= 0 )
					continue;

				for ( int i = count - new_count; i < count && new_this_frame < k_max_per_frame; ++i )
				{
					const auto pos = g::memory.read<math::vector3>( data_ptr + i * sizeof( math::vector3 ) );

					if ( std::isnan( pos.x ) || std::isnan( pos.y ) || std::isnan( pos.z ) )
						continue;
					if ( pos.length_sqr( ) < 1.0f )
						continue;

					if ( static_cast<int>( s_impacts.size( ) ) < k_max_stored )
						s_impacts.push_back( { pos, current_time } );

					++new_this_frame;
				}
			}

			return true;
		}

		// Renderiza todos os impactos armazenados (com fade por tempo).
		static void render_impacts( zdraw::draw_list& draw_list, float current_time, float lifetime )
		{
			for ( const auto& impact : s_impacts )
			{
				const auto screen = systems::g_view.project( impact.pos );
				if ( !systems::g_view.projection_valid( screen ) )
					continue;

				const float age  = current_time > 0.0f
					? std::max( 0.0f, current_time - impact.game_time )
					: 0.0f;
				const float frac = std::clamp( 1.0f - age / lifetime, 0.0f, 1.0f );
				const auto  a    = static_cast<std::uint8_t>( frac * 220.0f );

				if ( a < 4 )
					continue;

				const float half = k_sz * 0.5f;
				draw_list.add_rect_filled(
					screen.x - half, screen.y - half, k_sz, k_sz,
					{ 255, 220, 0, a }
				);
				draw_list.add_rect(
					screen.x - half - 1.f, screen.y - half - 1.f, k_sz + 2.f, k_sz + 2.f,
					{ 0, 0, 0, static_cast<std::uint8_t>( a / 2 ) },
					1.0f
				);
			}
		}

	} // anonymous namespace

	// ────────────────────────────────────────────────────────────────────────

	void impacts::on_render( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_misc.m_impacts;
		if ( !cfg.enabled )
		{
			s_impacts.clear( );
			s_trackers.clear( );
			return;
		}

		// Ler current_time uma única vez
		const auto global_vars   = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		const float current_time = global_vars
			? g::memory.read<float>( global_vars + cs2::global_vars_cur_time )
			: 0.0f;

		// Lifetime configurável: mínimo 0.5s para evitar fade instantâneo
		const float k_lifetime = std::max( 0.5f, static_cast<float>( settings::g_misc.m_impacts.lifetime ) );

		// Capturar novos impactos (somente se current_time válido)
		if ( current_time > 0.0f )
		{
			collect_impacts( current_time );

			// Expirar impactos antigos
			std::erase_if( s_impacts, [ & ]( const impact_entry& e )
			{
				return ( current_time - e.game_time ) >= k_lifetime;
			} );
		}

		// Renderizar impactos armazenados
		render_impacts( draw_list, current_time, k_lifetime );
	}

} // namespace features::misc
