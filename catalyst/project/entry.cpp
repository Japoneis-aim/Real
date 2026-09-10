#include <stdafx.hpp>

#include <timeapi.h>
#pragma comment( lib, "winmm.lib" )

// ============================================================================
// Emergency cleanup — invoked from set_terminate and SEH filter
// ============================================================================
namespace {

	// Sinaliza shutdown e destrói a overlay D3D para não deixar o processo
	// zumbi com a janela presa na tela quando uma thread filha crasha.
	static void emergency_cleanup( ) noexcept
	{
		__try
		{
			threads::shutdown( );
		}
		__except ( EXCEPTION_EXECUTE_HANDLER ) { }

		__try
		{
			if ( g::render )
			{
				const auto hwnd = g::render.hwnd( );
				if ( hwnd )
					::DestroyWindow( hwnd );
			}
		}
		__except ( EXCEPTION_EXECUTE_HANDLER ) { }
	}

	// terminate handler: chamado quando std::terminate() é invocado
	// (exceção não capturada, noexcept violado, etc.)
	static void on_terminate( ) noexcept
	{
		g::console.print( "terminate: unhandled exception — performing emergency cleanup." );
		emergency_cleanup( );
		::TerminateProcess( ::GetCurrentProcess( ), 1 );
	}

} // anonymous namespace

int main( )
{
	// Instala o handler o mais cedo possível, antes de qualquer thread ser
	// criada, para que exceções não capturadas em threads filhas também
	// passem pelo nosso cleanup.
	std::set_terminate( on_terminate );

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