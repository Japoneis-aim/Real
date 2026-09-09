#include <stdafx.hpp>

namespace threads {

	// Verifica se o processo CS2 ainda está rodando.
	static bool is_target_alive( )
	{
		const auto handle = g::memory.handle( );
		if ( !handle )
			return false;

		DWORD exit_code = STILL_ACTIVE;
		if ( !::GetExitCodeProcess( static_cast<HANDLE>( handle ), &exit_code ) )
			return false;

		return exit_code == STILL_ACTIVE;
	}

	void game( std::stop_token stop )
	{
		std::string last_map{};

		std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );

		while ( !stop.stop_requested( ) )
		{
			// Graceful shutdown: se o CS2 fechou, encerra o overlay inteiro.
			if ( !is_target_alive( ) )
			{
				g::console.print( "cs2.exe terminated — shutting down." );
				::PostQuitMessage( 0 );
				return;
			}

			systems::g_local.update( );

			if ( systems::g_local.valid( ) )
			{
				systems::g_entities.refresh( );
				systems::g_collector.run( );

				const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
				if ( global_vars )
				{
					const auto map_ptr = g::memory.read<std::uintptr_t>( global_vars + cs2::global_vars_map_name );
					const auto current_map = map_ptr ? g::memory.read_string( map_ptr ) : std::string{};

					if ( !current_map.empty( ) && current_map != "<empty>" && current_map != last_map )
					{
						g::console.print( "map change: {} -> {}", last_map.empty( ) ? "none" : last_map, current_map );
						last_map = current_map;
						systems::g_bvh.clear( );
						g::console.print( "parsing bvh for {}...", current_map );
						systems::g_bvh.parse( );
						g::console.success( "bvh parsed." );
					}
				}
			}
			else
			{
				if ( !last_map.empty( ) )
				{
					last_map = {};
					systems::g_bvh.clear( );
				}
			}

			std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
		}
	}

	void combat( std::stop_token stop )
	{
		constexpr auto target_tps{ 128 };
		constexpr auto tick_interval = std::chrono::nanoseconds( 1'000'000'000 / target_tps );
		auto next_tick = std::chrono::steady_clock::now( );

		while ( !stop.stop_requested( ) )
		{
			if ( systems::g_local.valid( ) && systems::g_bvh.valid( ) )
			{
				features::combat::g_shared.tick( );
				features::combat::g_legit.tick( );
				features::combat::g_rcs.tick( );
				features::misc::g_wallbang.tick( );
				features::misc::g_bomb_timer.tick( );
			}

			next_tick += tick_interval;

			const auto now = std::chrono::steady_clock::now( );
			if ( next_tick < now )
			{
				next_tick = now;
				continue;
			}

			std::this_thread::sleep_until( next_tick - std::chrono::milliseconds( 1 ) );

			while ( std::chrono::steady_clock::now( ) < next_tick )
			{
				_mm_pause( );
			}
		}
	}

} // namespace threads
