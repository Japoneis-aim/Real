#pragma once

namespace threads {

	// Handles das threads gerenciadas (jthread — parada cooperativa via stop_token)
	inline std::jthread game_thread{};
	inline std::jthread combat_thread{};

	void game( std::stop_token stop );
	void combat( std::stop_token stop );

	// ── Watchdog ─────────────────────────────────────────────────────────────
	// Cada thread atualiza seu timestamp a cada tick.
	// O thread principal (ou qualquer observer) pode chamar is_hung() para
	// detectar travamento (deadlock, loop infinito, etc.).
	//
	// Uso:
	//   threads::game_last_tick.store( now, std::memory_order_relaxed );  // no tick
	//   if ( threads::is_hung( threads::game_last_tick, 5000 ) ) { ... } // no watcher
	//
	// time_point default (epoch) = thread ainda não iniciou; hung_ms deve ser
	// maior que o tempo de inicialização (~1–2 s) para evitar falso positivo.

	using clock      = std::chrono::steady_clock;
	using time_point = clock::time_point;

	// Timestamps atualizados pelos threads a cada tick
	inline std::atomic<time_point> game_last_tick{ time_point{} };
	inline std::atomic<time_point> combat_last_tick{ time_point{} };

	// Retorna true se o timestamp não foi atualizado por mais de hung_ms milissegundos
	// E o thread já iniciou (timestamp != epoch).
	[[nodiscard]] inline bool is_hung( const std::atomic<time_point>& ts, int hung_ms = 3000 )
	{
		const auto last = ts.load( std::memory_order_relaxed );
		if ( last == time_point{} )
			return false; // thread ainda não iniciou — não é hang

		const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
			clock::now( ) - last ).count( );
		return age > hung_ms;
	}

	// Sinaliza encerramento de ambas as threads e aguarda término
	inline void shutdown( )
	{
		game_thread.request_stop( );
		combat_thread.request_stop( );
	}

} // namespace threads
