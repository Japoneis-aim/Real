#pragma once

namespace threads {

	// Handles das threads gerenciadas (jthread — parada cooperativa via stop_token)
	inline std::jthread game_thread{};
	inline std::jthread combat_thread{};

	void game( std::stop_token stop );
	void combat( std::stop_token stop );

	// Sinaliza encerramento de ambas as threads e aguarda término
	inline void shutdown( )
	{
		game_thread.request_stop( );
		combat_thread.request_stop( );
	}

} // namespace threads
