#include <stdafx.hpp>

#include <timeapi.h>
#pragma comment( lib, "winmm.lib" )

int main( )
{
	timeBeginPeriod( 1 );

	{
		settings::g_combat.register_config( "combat" );
		settings::g_esp.register_config( "esp" );
		settings::g_misc.register_config( "misc" );
		config::initialize( );
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
		std::thread( threads::game ).detach( );
		std::thread( threads::combat ).detach( );

		if ( !g::render.initialize( ) )
		{
			return 1;
		}
	}

	return 0;
}