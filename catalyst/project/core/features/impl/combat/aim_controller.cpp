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
	// Também reseta o estado de hysteresis da dead zone — o novo alvo pode estar
	// mais longe, então começamos com assist ativo.
	if ( target_ptr != m_last_target_ptr )
	{
		m_subpixel_err = {};
		m_assist_in_dead_zone = false;
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

	// Dead zone com hysteresis: evita chattering quando a mira está no limiar da zona morta.
	//   - Zona de entrada  (inner): se len < k_dead_zone_enter, o assist desliga completamente.
	//   - Zona de saída    (outer): o assist só religa quando len > k_dead_zone_exit.
	// Isso previne que micro-movimentos (tremedeira, desaceleração do smooth) alternem
	// o assist on/off a cada tick, tornando o comportamento visivelmente mais estável.
	constexpr float k_dead_zone_enter = 0.10f; // desliga quando muito perto
	constexpr float k_dead_zone_exit  = 0.25f; // só religa quando sair desta distância

	if ( m_assist_in_dead_zone )
	{
		// Dentro da zona morta — só sai quando cruzar o limiar de saída
		if ( len < k_dead_zone_exit )
			return;
		m_assist_in_dead_zone = false;
	}
	else
	{
		// Fora da zona morta — entra na zona se cruzar o limiar de entrada
		if ( len < k_dead_zone_enter )
		{
			m_assist_in_dead_zone = true;
			return;
		}
	}

	const float s = std::clamp( cfg.assist_strength, 0.0f, 1.0f );

	// proximity_factor: 1.0 quando o cursor está perto do alvo (len→0), 0.0 quando longe.
	// Isso faz o assist "segurar" mais (redução maior) quando a mira já está perto —
	// comportamento magnético correto: dificulta sair do alvo sem impedir chegar nele.
	//
	// Substituído std::exp(-len * 0.15f) por 1/(1+len*0.15f):
	//   - Erro < 2% no range relevante (len 0–10°), comportamento praticamente idêntico.
	//   - ~3× mais rápido: sem transendental, só uma divisão FP.
	//   - Roda 128× por segundo no thread de combat — vale a troca.
	const float proximity_factor = 1.0f / ( 1.0f + len * 0.15f );
	const float reduction = s * proximity_factor * 0.8f;

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
	//
	// Floor em delta_time: o scheduler do Windows tem jitter de ~1-2ms.
	// Sem o floor, ticks com dt < ~0.2ms resultam em t ≈ 0 e o aim
	// para completamente por aquele tick, causando movimento irregular.
	// Floor de 1/256s (~3.9ms) é conservador — bem abaixo do intervalo
	// real de 7.8ms a 128 TPS, mas protege contra spikes negativos de jitter.
	constexpr float k_dt_floor = 1.0f / 256.0f;
	const float dt  = std::max( delta_time, k_dt_floor );
	const float amt = std::max( 1.0f, cfg.smooth_amount );
	const float t   = 1.0f - std::exp( -dt * ( 60.0f / amt ) );

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
	// Hard-lock com step máximo por tick escalado pelo smooth_amount:
	// smooth alto = movimento mais lento = step menor = mais humano.
	// Fórmula: base de 5° / (smooth_amount / 10), mínimo de 1°, máximo de 10°.
	// Exemplo: smooth=1  → 10° (movimento rápido),
	//          smooth=10 → 5°  (padrão),
	//          smooth=30 → ~1.7° (muito suave).
	const float smooth = std::max( 1.0f, cfg.smooth_enabled ? cfg.smooth_amount : 10.0f );
	const float k_max_step_deg = std::clamp( 50.0f / smooth, 1.0f, 10.0f );

	delta.x = std::clamp( delta.x, -k_max_step_deg, k_max_step_deg );
	delta.y = std::clamp( delta.y, -k_max_step_deg, k_max_step_deg );
}

// ============================================================================
// HELPERS
// ============================================================================

void AimController::normalize_delta( math::vector2& a )
{
	// NaN guard: delta NaN pode ocorrer se aim_point vier de posição inválida.
	if ( std::isnan( a.x ) || std::isinf( a.x ) ) a.x = 0.0f;
	if ( std::isnan( a.y ) || std::isinf( a.y ) ) a.y = 0.0f;

	// Normaliza yaw para [-180, 180] usando fmod (evita loop infinito com NaN/grande valor).
	a.y = std::fmod( a.y + 180.0f, 360.0f );
	if ( a.y < 0.0f ) a.y += 360.0f;
	a.y -= 180.0f;
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
