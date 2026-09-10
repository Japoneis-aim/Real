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
// Helper: deg/count — mesma fórmula que legit::calculate_deg_per_pixel()
// Retorna quantos graus de visão correspondem a 1 count de mouse injection.
// Formula: sensitivity * m_yaw * fov_sensitivity_adjust
// Inverso (counts/grau) é usado para converter graus de punch em counts.
// Resultado é cacheado por k_deg_cache_ticks ticks para evitar leituras de
// memória e convar desnecessárias (fov_adjust raramente muda mid-game).
// ----------------------------------------------------------------------------

float rcs::calculate_deg_per_count( )
{
	// Retorna do cache se ainda válido
	if ( m_deg_cache_tick > 0 )
	{
		--m_deg_cache_tick;
		return m_cached_deg_per_count;
	}

	const auto pawn = systems::g_local.pawn( );
	if ( !pawn )
		return 0.0f;

	constexpr float m_yaw = 0.022f; // CS2: yaw fixo (não é convar)
	const auto sensitivity = systems::g_convars.get<float>( CONVAR( "sensitivity"_hash ) );
	if ( sensitivity <= 0.0f )
		return 0.0f;

	const auto fov_adjust = g::memory.read<float>(
		pawn + SCHEMA( "C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash )
	);
	const auto adj = ( fov_adjust > 0.0f ) ? fov_adjust : 1.0f;

	m_cached_deg_per_count = sensitivity * m_yaw * adj;
	m_deg_cache_tick = k_deg_cache_ticks;
	return m_cached_deg_per_count;
}

// ----------------------------------------------------------------------------
// Tick principal — chamado a 128 Hz pelo thread de combat
// ----------------------------------------------------------------------------

void rcs::tick( )
{
	// ── Toggle por tecla: edge detection explícito ────────────────────────
	// GetAsyncKeyState bit 0 ("tecla pressionada desde o último GetAsyncKeyState")
	// pode ser consumido por outra thread antes de chegarmos aqui.
	// Usamos bit 15 (estado atual) + estado anterior para detectar a borda de subida.
	{
		const auto& ctx_for_cfg = features::combat::g_shared.ctx( );
		if ( ctx_for_cfg.valid )
		{
			const auto& cfg_rcs = settings::g_combat.get( ctx_for_cfg.weapon_type );

			// Se o RCS está desabilitado na config, não faz nada
			if ( !cfg_rcs.aimbot.rcs_enabled )
			{
				reset( );
				m_prev_key_state = false;
				return;
			}

			const int rcs_key = static_cast<int>( cfg_rcs.aimbot.rcs_key );
			if ( rcs_key )
			{
				const bool key_down = ( ::GetAsyncKeyState( rcs_key ) & 0x8000 ) != 0;
				if ( key_down && !m_prev_key_state )
					m_toggle_on = !m_toggle_on;
				m_prev_key_state = key_down;
			}
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

	// Lê ponteiro do pawn local — necessário tanto para o gate de shots_fired
	// quanto para a leitura do punch angle logo abaixo.
	const auto local_pawn = systems::g_local.pawn( );
	if ( !local_pawn )
	{
		reset( );
		return;
	}

	// Gate shots_fired: verifica via collector se o jogador local disparou pelo menos
	// 2 tiros. Mais confiável que recoil_index sozinho porque shots_fired é atualizado
	// sincronamente com o disparo, enquanto recoil_index pode ter latência de um tick.
	// Buscamos o pawn local no vetor de players do collector (vem via with_players).
	// Se shots_fired == 0, é primeiro tiro desta rajada — não compensar.
	{
		bool shots_gate_pass = true; // default: pass (se não achar o player, confia no recoil_index)
		systems::g_collector.with_players( [&]( const std::vector<systems::collector::player>& players )
		{
			for ( const auto& p : players )
			{
				if ( p.pawn == local_pawn )
				{
					// shots_fired == 0 ou 1 → primeiro disparo da rajada, não compensar ainda
					shots_gate_pass = ( p.shots_fired >= 2 );
					break;
				}
			}
		} );

		if ( !shots_gate_pass )
		{
			reset( );
			return;
		}
	}

	// CS2 atual: punch angle está em pawn → m_pCameraServices → m_vecCsViewPunchAngle.
	// Offsets centralizados em g_shared — inicializados via lazy-init thread-safe
	// na primeira chamada (evita duplicar flag/campo em cada feature).
	// Fallback para m_aimPunchAngle direto no pawn caso CameraServices não esteja disponível.
	const auto cam_svc_off  = features::combat::g_shared.camera_services_offset( );
	const auto view_pch_off = features::combat::g_shared.view_punch_offset( );
	const auto aim_pch_off  = features::combat::g_shared.aim_punch_offset( );

	math::vector2 cur_punch{};
	bool got_punch = false;

	if ( cam_svc_off && view_pch_off )
	{
		const auto cam_svc = g::memory.read<std::uintptr_t>( local_pawn + cam_svc_off );
		if ( cam_svc )
		{
			// m_vecCsViewPunchAngle é um QAngle (3 floats). Lemos os 2 primeiros (pitch/yaw).
			const auto v = g::memory.read<math::vector3>( cam_svc + view_pch_off );
			cur_punch   = { v.x, v.y };
			got_punch   = true;
		}
	}

	if ( !got_punch && aim_pch_off )
	{
		const auto v  = g::memory.read<math::vector3>( local_pawn + aim_pch_off );
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

	// ── FIX: fórmula de conversão graus → counts ─────────────────────────
	// ERRADO: sw / camera_fov  (pixels de tela / FOV  ≠  counts de mouse / grau)
	// CORRETO: 1 / deg_per_count  onde deg_per_count = sensitivity * m_yaw * fov_adj
	//
	// Raciocínio: o CS2 aplica o yaw da câmera como:
	//   view_angle += mouse_counts * sensitivity * m_yaw * fov_adj
	// Portanto para compensar N graus de punch precisamos de:
	//   counts = N / (sensitivity * m_yaw * fov_adj)
	const float deg_per_count = calculate_deg_per_count( );
	if ( deg_per_count <= 0.0f )
		return;
	const float counts_per_deg = 1.0f / deg_per_count;

	// Compensação vertical (pitch) e horizontal (yaw).
	// delta.x = pitch → mouse Y (recuo para cima = mover mouse para baixo)
	// delta.y = yaw   → mouse X (drift lateral do padrão, ex: AK47)
	m_owed_y -= delta.x * k_recoil_scale * rcs_scale * counts_per_deg;
	m_owed_x -= delta.y * k_recoil_scale * rcs_scale * counts_per_deg;

	// Aplica step máximo para movimento suave
	const int rx = static_cast<int>( std::clamp( m_owed_x, -k_max_step, k_max_step ) );
	const int ry = static_cast<int>( std::clamp( m_owed_y, -k_max_step, k_max_step ) );

	// Acumula residual sub-pixel
	m_owed_x -= static_cast<float>( rx );
	m_owed_y -= static_cast<float>( ry );

	// FIX: cap APÓS subtrair o step — evita cortar compensação legítima antes de aplicar.
	// Cap em 2× k_max_step: limita dívida acumulada sem perder precisão por frame.
	constexpr float k_owed_cap = k_max_step * 2.0f;
	m_owed_x = std::clamp( m_owed_x, -k_owed_cap, k_owed_cap );
	m_owed_y = std::clamp( m_owed_y, -k_owed_cap, k_owed_cap );

	if ( rx != 0 || ry != 0 )
		move_mouse( rx, ry );
}

} // namespace features::combat
