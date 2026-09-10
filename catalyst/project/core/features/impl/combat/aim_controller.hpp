#pragma once
#include <utilities/math/math.hpp>

namespace features::combat {

struct AimConfig
{
	bool aimbot_enabled    = false;
	bool assist_enabled    = true;
	bool use_assist_only   = false;

	// rcs_enabled removido — RCS gerenciado exclusivamente por features::combat::g_rcs::tick()
	bool smooth_enabled    = true;
	bool humanize_enabled  = false;

	float aim_fov          = 10.0f;
	float assist_strength  = 0.3f;   // 0..1 — fração do pull em direção ao alvo
	float smooth_amount    = 8.0f;   // maior = mais lento
	float rcs_scale        = 0.8f;   // reservado para compatibilidade futura
	float humanize_strength = 0.5f;  // 0..100 — amplitude do jitter
};

class AimController
{
public:
	void initialize_offsets( );
	bool is_offsets_loaded( ) const { return m_offsets_loaded; }

	void on_frame(
		const math::vector3& eye_pos,
		math::vector3 view_angles,
		const math::vector3& aim_point,
		float deg_per_pixel,
		float delta_time,
		const AimConfig& cfg,
		const void* target_ptr = nullptr  // ponteiro do player — para reset de resíduo ao trocar de alvo
	);

private:
	// Pipeline stages
	// apply_rcs removido — RCS é gerenciado exclusivamente por features::combat::g_rcs::tick()
	void apply_assist  ( math::vector2& delta, const math::vector3& eye_pos, const math::vector3& aim_point, const AimConfig& cfg );
	void apply_smoothing( math::vector2& delta, float delta_time, const AimConfig& cfg );
	void apply_humanize ( math::vector2& delta, const AimConfig& cfg );
	void apply_aimbot   ( math::vector2& delta, const AimConfig& cfg );

	// Helpers
	void  normalize_delta( math::vector2& a );
	float rng_float( float minv, float maxv );

	// State
	bool           m_offsets_loaded{ false };

	math::vector2  m_subpixel_err{ 0.f, 0.f };  // acumula erro sub-pixel entre frames
	uint32_t       m_xorshift_state{ 0xDEADBEEF }; // RNG — re-seeded com tempo real no init

	// Target-lock: reseta sub-pixel error quando o alvo muda, evitando
	// que o resíduo do alvo anterior "puxe" a mira no primeiro frame do novo alvo.
	const void*    m_last_target_ptr{ nullptr };

	// Hysteresis da dead zone do aim assist.
	// true = assist desligado (dentro da zona morta); false = assist ativo.
	// Só sai do estado "dead" quando len > k_dead_zone_exit (ver apply_assist).
	bool           m_assist_in_dead_zone{ false };
};

} // namespace features::combat
