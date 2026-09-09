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
	void initialize_offsets( );
	void move_mouse( int dx, int dy );

	bool           m_offsets_loaded{ false };
	std::uintptr_t m_punch_offset{ 0 };           // m_aimPunchAngle (fallback, path antigo)
	std::uintptr_t m_camera_services_offset{ 0 }; // m_pCameraServices (path atual CS2)
	std::uintptr_t m_view_punch_offset{ 0 };      // m_vecCsViewPunchAngle dentro de CameraServices

	math::vector2  m_prev_punch{};
	float          m_owed_x{ 0.f };
	float          m_owed_y{ 0.f };
	bool           m_active{ false };
	bool           m_punch_synced{ false };

	// Toggle de sessão — alterado via rcs_key
	bool           m_toggle_on{ true };
};

inline rcs g_rcs{};

} // namespace features::combat
