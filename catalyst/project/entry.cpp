#include <stdafx.hpp>

#include <timeapi.h>
#pragma comment( lib, "winmm.lib" )

int main( )
{
	timeBeginPeriod( 1 );

	// Carrega o mapa de offsets gerado (output/offsets.map).
	// Deve ser feito antes de qualquer inicialização que dependa de offsets_map::get(),
	// como o bomb timer (dwPlantedC4) e o aim punch offset em legit.
	offsets_map::load_from_file( "output/offsets.map" );

	{
		settings::g_combat.register_config( "combat" );
		settings::g_esp.register_config( "esp" );
		settings::g_misc.register_config( "misc" );
		config::initialize( );
		config_persist::load( ); // carrega config salva anteriormente (se existir)
	}

	{
		if ( !g::console.initialize( " :> (id recommend you cap your ingame fps for better performance)" ) )
		{
			return 1;
		}

		if ( !g::input.initialize( ) )
		{
			return 1;
		}

		if ( !g::memory.initialize( L"cs2.exe" ) )
		{
			return 1;
		}
	}

	{
		if ( !g::modules.initialize( ) )
		{
			return 1;
		}

		if ( !g::offsets.initialize( ) )
		{
			return 1;
		}

		if ( !systems::g_icons.initialize( ) )
		{
			return 1;
		}
	}

	{
		threads::game_thread   = std::jthread( threads::game );
		threads::combat_thread = std::jthread( threads::combat );

		if ( !g::render.initialize( ) )
		{
			return 1;
		}
	}

	return 0;
}