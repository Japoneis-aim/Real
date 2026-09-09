#include <stdafx.hpp>
#include "aim_controller.hpp"

namespace features::combat {

// ============================================================================
// INIT
// ============================================================================

void AimController::initialize_offsets( )
{
	if ( m_offsets_loaded )
		return;

	// FIX: seed RNG com tempo real para que o padrão de jitter seja único por sessão.
	m_xorshift_state = static_cast<uint32_t>(
		std::chrono::steady_clock::now( ).time_since_epoch( ).count( ) & 0xFFFFFFFF
	);
	if ( m_xorshift_state == 0 )
		m_xorshift_state = 0xABCD1234;

	m_offsets_loaded = true;
}

// ============================================================================
// ON FRAME — pipeline principal
// ============================================================================

void AimController::on_frame(
	const math::vector3& eye_pos,
	math::vector3 view_angles,
	const math::vector3& aim_point,
	float deg_per_pixel,
	float delta_time,
	const AimConfig& cfg,
	const void* target_ptr )
{
	if ( !m_offsets_loaded )
		initialize_offsets( );

	// Target-lock: se o alvo mudou, descarta o resíduo sub-pixel acumulado
	// para evitar que a mira "puxe" para um lado no primeiro frame do novo alvo.
	if ( target_ptr != m_last_target_ptr )
	{
		m_subpixel_err = {};
		m_last_target_ptr = target_ptr;
	}

	// Ângulo desejado e delta bruto (em graus)
	const auto target_angle = math::helpers::calculate_angle( eye_pos, aim_point );
	math::vector2 raw_delta
	{
		target_angle.x - view_angles.x,
		math::helpers::normalize_yaw( target_angle.y - view_angles.y )
	};

	if ( raw_delta.length_sqr( ) < 1e-6f )
		return;

	// =========================================================================
	// PIPELINE (ordem correta):
	//  1. Aim Assist  — pull suave em direção ao alvo
	//  2. Smoothing   — amortece o movimento final
	//  3. Humanize    — jitter para humanização
	//  4. Aimbot      — ajuste final (hard-lock, se ativo)
	//
	// RCS foi removido deste pipeline — é gerenciado exclusivamente por
	// features::combat::g_rcs::tick() que roda no mesmo thread de combat.
	// =========================================================================

	if ( cfg.assist_enabled )
		apply_assist( raw_delta, eye_pos, aim_point, cfg );

	if ( cfg.smooth_enabled )
		apply_smoothing( raw_delta, delta_time, cfg );

	if ( cfg.humanize_enabled )
		apply_humanize( raw_delta, cfg );

	if ( cfg.aimbot_enabled && !cfg.use_assist_only )
		apply_aimbot( raw_delta, cfg );

	// Normaliza apenas o wrap de yaw (não clampeia delta de pitch — sem sentido)
	normalize_delta( raw_delta );

	// Graus → counts de mouse
	const float want_x = -raw_delta.y; // yaw  → mouse X
	const float want_y =  raw_delta.x; // pitch → mouse Y

	// Acumular sub-pixel error para não perder precisão em movimentos lentos
	m_subpixel_err.x += want_x / deg_per_pixel;
	m_subpixel_err.y += want_y / deg_per_pixel;

	const int counts_x = static_cast<int>( m_subpixel_err.x );
	const int counts_y = static_cast<int>( m_subpixel_err.y );

	m_subpixel_err.x -= static_cast<float>( counts_x );
	m_subpixel_err.y -= static_cast<float>( counts_y );

	if ( counts_x != 0 || counts_y != 0 )
		g::input.inject_mouse( counts_x, counts_y, input::move );
}

// ============================================================================
// AIM ASSIST
// ============================================================================

void AimController::apply_assist( math::vector2& delta, const math::vector3& eye_pos, const math::vector3& aim_point, const AimConfig& cfg )
{
	// Assist: reduz a velocidade angular em direção ao alvo (magnetismo / stickiness).
	// Quando o alvo está exatamente na mira (delta ≈ 0), sem efeito.
	// Quando está na borda do FOV de assist, o pull é máximo (move ~strength% do delta a menos).
	//
	// FIX: a versão anterior MULTIPLICAVA delta por (1 + effective), amplificando o movimento.
	// O correto para aim assist é REDUZIR o delta — a mira "gruda" no alvo movendo-se
	// mais devagar (pull suave), não mais rápido.
	const float len = std::sqrtf( delta.length_sqr( ) );
	if ( len < 0.001f )
		return;

	// Dead zone: se já estamos muito perto do alvo, desliga o assist completamente.
	// Sem isso, a mira nunca chega exatamente no alvo (o assist sempre "trava" o movimento).
	// 0.15° é imperceptível para o jogador mas resolve o "stickiness" que impede headshots.
	constexpr float k_dead_zone_deg = 0.15f;
	if ( len < k_dead_zone_deg )
		return;

	const float s = std::clamp( cfg.assist_strength, 0.0f, 1.0f );

	// Quanto mais perto do alvo (delta menor), mais o assist "segura" (redução maior).
	// Curva: 1 - exp(-len * 0.15) vai de 0 (delta=0) até ~1 (delta grande).
	// Isso faz o assist ser mais forte perto do alvo e fraco longe.
	const float proximity_factor = 1.0f - std::exp( -len * 0.15f );
	const float reduction = s * ( 1.0f - proximity_factor * 0.4f );

	// Reduzir o delta — mover menos por tick = pull suave em direção ao alvo
	delta.x *= ( 1.0f - reduction );
	delta.y *= ( 1.0f - reduction );
}

// ============================================================================
// SMOOTHING
// ============================================================================

void AimController::apply_smoothing( math::vector2& delta, float delta_time, const AimConfig& cfg )
{
	// Exponential smoothing: frame-rate independente.
	// t = 0 → não move; t = 1 → move tudo de uma vez.
	// smooth_amount grande = movimento mais lento.
	const float amt = std::max( 1.0f, cfg.smooth_amount );
	const float t   = 1.0f - std::exp( -delta_time * ( 60.0f / amt ) );

	delta.x *= t;
	delta.y *= t;
}

// ============================================================================
// HUMANIZE
// ============================================================================

void AimController::apply_humanize( math::vector2& delta, const AimConfig& cfg )
{
	// FIX: jitter não deve escalar com distância — em combate próximo o humanize
	// era ~zero (dist/1000 ≈ 0.1) que é justamente onde é mais suspeito não ter.
	// Agora o amplitude é constante, escalada apenas pelo strength.
	// O jitter é aplicado como fração do delta atual para ser proporcional ao movimento.
	const float len = std::sqrtf( delta.length_sqr( ) );
	if ( len < 0.001f )
		return;

	// Amplitude: ~5% do delta * strength, com cap em 0.5° para não desfigurar o aim
	const float strength  = std::clamp( cfg.humanize_strength, 0.0f, 100.0f ) / 100.0f;
	const float amplitude = std::min( 0.5f, len * 0.05f * strength );

	delta.x += rng_float( -amplitude, amplitude );
	delta.y += rng_float( -amplitude, amplitude );
}

// ============================================================================
// AIMBOT FINAL
// ============================================================================

void AimController::apply_aimbot( math::vector2& delta, const AimConfig& cfg )
{
	// Hard-lock: nenhuma modificação adicional — o delta já foi calculado, suavizado,
	// e humanizado acima. Se quiser clampar o movimento máximo por tick, faz aqui.
	// Exemplo: limitar a 5° por tick para não parecer teleportação de mira.
	constexpr float k_max_step_deg = 5.0f;
	delta.x = std::clamp( delta.x, -k_max_step_deg, k_max_step_deg );
	delta.y = std::clamp( delta.y, -k_max_step_deg, k_max_step_deg );

	( void )cfg;
}

// ============================================================================
// HELPERS
// ============================================================================

void AimController::normalize_delta( math::vector2& a )
{
	// FIX: não clampeia pitch do DELTA em ±89° — isso não faz sentido para deltas.
	// O clamp de pitch se aplica ao ÂNGULO ABSOLUTO resultante, não ao delta.
	// Aqui só normalizamos o yaw para manter no range [-180, 180].
	while ( a.y >  180.0f ) a.y -= 360.0f;
	while ( a.y < -180.0f ) a.y += 360.0f;
}

float AimController::rng_float( float minv, float maxv )
{
	// Xorshift32 — rápido, sem alocação, suficiente para jitter
	uint32_t x = m_xorshift_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	m_xorshift_state = x;

	const float r = static_cast<float>( x & 0xFFFFFF ) / 16777216.0f; // [0, 1)
	return minv + r * ( maxv - minv );
}

} // namespace features::combat
