#include <stdafx.hpp>
#include "rcs.hpp"

namespace features::combat {

// Pixels máximos por tick — evita saltos bruscos
constexpr float k_max_step    = 4.5f;
// Limita injeção total de mouse por chamada
constexpr int   k_max_move_px = 120;
// CS2: punch já em graus, fator ×2 para compensação completa
constexpr float k_recoil_scale = 2.0f;

// ----------------------------------------------------------------------------
// Inicialização
// ----------------------------------------------------------------------------

void rcs::initialize_offsets( )
{
	if ( m_offsets_loaded )
		return;

	// CS2 atual: punch angle foi migrado para pawn → m_pCameraServices → m_vecCsViewPunchAngle.
	// Guardamos os dois offsets e decidimos qual usar no tick() com base em qual está disponível.
	m_punch_offset          = SCHEMA( "C_CSPlayerPawn",           "m_aimPunchAngle"_hash );
	m_camera_services_offset = SCHEMA( "C_BasePlayerPawn",         "m_pCameraServices"_hash );
	m_view_punch_offset      = SCHEMA( "CPlayer_CameraServices",   "m_vecCsViewPunchAngle"_hash );

	m_offsets_loaded = true;
}

// ----------------------------------------------------------------------------
// Injeção de mouse
// ----------------------------------------------------------------------------

void rcs::move_mouse( int dx, int dy )
{
	dx = std::clamp( dx, -k_max_move_px, k_max_move_px );
	dy = std::clamp( dy, -k_max_move_px, k_max_move_px );

	if ( dx == 0 && dy == 0 )
		return;

	g::input.inject_mouse( dx, dy, input::move );
}

// ----------------------------------------------------------------------------
// Reset de estado
// ----------------------------------------------------------------------------

void rcs::reset( )
{
	m_owed_x      = 0.f;
	m_owed_y      = 0.f;
	m_prev_punch  = {};
	m_active      = false;
	m_punch_synced = false;
}

// ----------------------------------------------------------------------------
// Tick principal — chamado a 128 Hz pelo thread de combat
// ----------------------------------------------------------------------------

void rcs::tick( )
{
	initialize_offsets( );

	// Processar toggle por tecla (detecta borda de subida — bit 0)
	const auto& ctx_for_cfg = features::combat::g_shared.ctx( );
	if ( ctx_for_cfg.valid )
	{
		const auto& cfg_rcs = settings::g_combat.get( ctx_for_cfg.weapon_type );
		const int rcs_key = static_cast<int>( cfg_rcs.aimbot.rcs_key );
		if ( rcs_key && ( ::GetAsyncKeyState( rcs_key ) & 1 ) )
		{
			m_toggle_on = !m_toggle_on;
		}
	}

	// Guard: local player precisa existir
	if ( !systems::g_local.valid( ) )
	{
		reset( );
		return;
	}

	// Gate: cursor visível = jogo sem foco (menu, scoreboard, console, dead).
	if ( systems::g_local.is_cursor_visible( ) )
	{
		reset( );
		return;
	}

	// Guard: contexto de arma válido
	const auto& ctx = features::combat::g_shared.ctx( );
	if ( !ctx.valid )
	{
		reset( );
		return;
	}

	// Granadas — RCS não faz sentido
	const bool is_grenade = ( ctx.weapon_type == cstypes::grenade );
	if ( is_grenade )
	{
		reset( );
		return;
	}

	// Verificar toggle e rcs_strength
	const auto& cfg = settings::g_combat.get( ctx.weapon_type );
	const float rcs_scale = static_cast<float>( cfg.aimbot.rcs_strength ) / 100.0f;

	// RCS desativado se: toggle off, ou strength == 0
	if ( !m_toggle_on || rcs_scale <= 0.f )
	{
		reset( );
		return;
	}

	// RCS ativo somente com LMB pressionado
	const bool lmb = ( ::GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0;
	if ( !lmb )
	{
		reset( );
		return;
	}

	// Gate first-shot: recoil_index < 1.0 significa que ainda não houve recuo
	// acumulado suficiente (primeiro tiro). O CS2 não gera drift lateral no
	// primeiro disparo — compensar aqui causaria um salto brusco desnecessário.
	if ( ctx.recoil_index < 1.0f )
	{
		reset( );
		return;
	}

	// Lê punch atual
	const auto local_pawn = systems::g_local.pawn( );
	if ( !local_pawn || !m_punch_offset )
	{
		reset( );
		return;
	}

	// CS2 atual: punch angle está em pawn → m_pCameraServices → m_vecCsViewPunchAngle.
	// Fallback para m_aimPunchAngle direto no pawn caso CameraServices não esteja disponível.
	math::vector2 cur_punch{};
	bool got_punch = false;

	if ( m_camera_services_offset && m_view_punch_offset )
	{
		const auto cam_svc = g::memory.read<std::uintptr_t>( local_pawn + m_camera_services_offset );
		if ( cam_svc )
		{
			// m_vecCsViewPunchAngle é um QAngle (3 floats). Lemos os 2 primeiros (pitch/yaw).
			const auto v = g::memory.read<math::vector3>( cam_svc + m_view_punch_offset );
			cur_punch   = { v.x, v.y };
			got_punch   = true;
		}
	}

	if ( !got_punch && m_punch_offset )
	{
		const auto v  = g::memory.read<math::vector3>( local_pawn + m_punch_offset );
		cur_punch     = { v.x, v.y };
		got_punch     = true;
	}

	if ( !got_punch )
	{
		reset( );
		return;
	}

	// Se punch zerado (sem recuo), reseta estado acumulado
	if ( cur_punch.x * cur_punch.x + cur_punch.y * cur_punch.y < 0.0001f )
	{
		reset( );
		return;
	}

	m_active = true;

	// Primeiro frame com recuo: sincroniza sem injetar
	if ( !m_punch_synced )
	{
		m_prev_punch   = cur_punch;
		m_punch_synced = true;
		return;
	}

	// Delta de recuo desde o último tick
	const math::vector2 delta{
		cur_punch.x - m_prev_punch.x,
		cur_punch.y - m_prev_punch.y
	};
	m_prev_punch = cur_punch;

	// Pixels por grau (resolução horizontal / FOV horizontal)
	const auto [sw, sh] = zdraw::get_display_size( );
	const float camera_fov = systems::g_view.has_camera( ) ? systems::g_view.fov( ) : 90.0f;
	const float px_per_deg = static_cast<float>( sw ) / camera_fov;

	// Compensação vertical (pitch) e horizontal (yaw).
	// delta.x = pitch → mouse Y (recuo para cima = mover mouse para baixo)
	// delta.y = yaw   → mouse X (drift lateral do padrão, ex: AK47)
	m_owed_y -= delta.x * k_recoil_scale * rcs_scale * px_per_deg;
	m_owed_x -= delta.y * k_recoil_scale * rcs_scale * px_per_deg;

	// Aplica step máximo para movimento suave
	float step_x = std::clamp( m_owed_x, -k_max_step, k_max_step );
	float step_y = std::clamp( m_owed_y, -k_max_step, k_max_step );

	const int rx = static_cast<int>( step_x );
	const int ry = static_cast<int>( step_y );

	// Acumula residual sub-pixel
	m_owed_x -= static_cast<float>( rx );
	m_owed_y -= static_cast<float>( ry );

	if ( rx != 0 || ry != 0 )
		move_mouse( rx, ry );
}

} // namespace features::combat
