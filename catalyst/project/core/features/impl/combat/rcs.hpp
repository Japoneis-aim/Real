#pragma once

namespace features::combat {

// RCS standalone — compensa recuo vertical independente do aimbot.
// Ativo quando rcs_strength > 0 E LMB pressionado.
// Toggle on/off via rcs_key (F3 por padrão) — independente do aimbot.
class rcs
{
public:
	void tick( );
	void reset( );
	[[nodiscard]] bool is_active( ) const { return m_active; }
	[[nodiscard]] bool is_enabled( ) const { return m_toggle_on; }

private:
	void move_mouse( int dx, int dy );
	[[nodiscard]] float calculate_deg_per_count( ); // não-const: atualiza cache interno

	// Nota: offsets de punch angle centralizados em g_shared — rcs acessa via
	// g_shared.aim_punch_offset(), g_shared.camera_services_offset(), g_shared.view_punch_offset().

	math::vector2  m_prev_punch{};
	float          m_owed_x{ 0.f };
	float          m_owed_y{ 0.f };
	bool           m_active{ false };
	bool           m_punch_synced{ false };

	// Toggle de sessão — alterado via rcs_key
	bool           m_toggle_on{ true };
	// Edge detection do toggle key: estado da tecla no tick anterior
	bool           m_prev_key_state{ false };

	// Cache de deg_per_count — evita leitura de memória e convar a cada tick.
	// Invalidado a cada k_deg_cache_ticks ticks (fov_adjust raramente muda mid-game).
	float          m_cached_deg_per_count{ 0.f };
	int            m_deg_cache_tick{ 0 };
	static constexpr int k_deg_cache_ticks = 16; // revalida a cada ~125ms a 128Hz
};

inline rcs g_rcs{};

} // namespace features::combat
