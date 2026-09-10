#pragma once

#include <vector>
#include <cstdint>
#include <shared_mutex>
#include <chrono>
#include <unordered_map>
// core systems types
#include "../systems/systems.hpp"

// Novas features
#include "impl/misc/wallbang.hpp"
#include "impl/misc/bombtimer.hpp"
#include "impl/misc/radar.hpp"
#include "impl/combat/aim_controller.hpp"
#include "impl/combat/rcs.hpp"
#include "../render/stream_mode.hpp"

namespace features {

	namespace combat {

			class AimController; // forward declare persistent controller

		class legit
		{
		public:
			void on_render(zdraw::draw_list& draw_list);
			void tick();

			// destructor default
			~legit() = default;

			struct target
			{
				const systems::collector::player* player{};
				systems::bones::data bones{};
				math::vector3 aim_point{};
				int hitbox{ -1 };
				int hitgroup{ -1 };
				float damage{};
				float fov{};
				bool penetrated{};
			};

			struct trigger_result
			{
				const systems::collector::player* player{};
				systems::bones::data bones{};
				int hitbox{ -1 };
				int hitgroup{ -1 };
				float damage{};
				bool penetrated{};
			};

		private:
			// --- Target Selection ---
			[[nodiscard]] target select_target(
				const math::vector3& eye_pos,
				const math::vector3& view_angles,
				const std::vector<systems::collector::player>& players,
				const settings::combat::group_config& cfg
			) const;

			[[nodiscard]] bool is_valid_target(const systems::collector::player& player) const;
			[[nodiscard]] float calculate_target_score(float fov, const math::vector3& eye_pos, const math::vector3& aim_point, bool is_visible) const;
			[[nodiscard]] target build_target(
				const systems::collector::player& player,
				const systems::bones::data& bones,
				const math::vector3& aim_point,
				int hitbox,
				float damage,
				float fov,
				bool penetrated
			) const;

			// --- Aim Point ---
			[[nodiscard]] math::vector3 get_aim_point(
				const math::vector3& eye_pos,
				const systems::collector::player& player,
				const systems::bones::data& bones,
				const settings::combat::group_config& cfg,
				float& out_damage,
				int& out_hitbox,
				bool& out_penetrated
			) const;

			[[nodiscard]] bool is_valid_hitbox(const systems::hitboxes::entry& hb, const settings::combat::group_config& cfg) const;

			[[nodiscard]] bool try_visible_hitbox(
				const math::vector3& eye_pos,
				const math::vector3& pos,
				const systems::collector::player& player,
				const systems::bones::data& bones,
				const systems::hitboxes::entry& hb,
				const settings::combat::group_config& cfg,
				float current_dmg,
				float& best_dmg,
				math::vector3& best_pos,
				int& out_hitbox,
				bool& out_penetrated
			) const;

			// --- FOV ---
			[[nodiscard]] float get_fov(const math::vector3& view_angles, const math::vector3& eye_pos, const math::vector3& target_pos) const;

			// --- Aim ---
			[[nodiscard]] float calculate_deg_per_pixel() const;
			// NOTE: apply_aim removed in favor of AimController member usage

			// --- Render ---
			void draw_fov(zdraw::draw_list& draw_list, const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::aimbot& cfg);

			// --- Triggerbot ---
			[[nodiscard]] trigger_result trace_crosshair(
				const math::vector3& eye_pos,
				const math::vector3& view_angles,
				const std::vector<systems::collector::player>& players,
				const settings::combat::triggerbot& cfg
			) const;

			void triggerbot(
				const math::vector3& eye_pos,
				const math::vector3& view_angles,
				const std::vector<systems::collector::player>& players,
				const settings::combat::triggerbot& cfg
			);

			[[nodiscard]] float calculate_trigger_delay(
				const math::vector3& eye_pos,
				const math::vector3& view_angles,
				const trigger_result& result,
				const settings::combat::triggerbot& cfg
			) const;

			void execute_trigger(float now);
			void ensure_rng_seeded();
			void update_trigger_state();

			// --- State ---
			animation::spring m_fov_alpha{};
			random::valve_rng m_rng{};
			bool m_rng_seeded{};
			math::vector2 m_aim_error{};
			// Aim pipeline state
			math::vector2 m_accum_recoil{};
			math::vector2 m_prev_punch{};
			std::chrono::steady_clock::time_point m_last_frame_steady{};
			// Nota: offsets de punch angle centralizados em g_shared — legit acessa via
			// g_shared.aim_punch_offset(), g_shared.camera_services_offset(), etc.
			// Persistent AimController to keep RCS/humanize state between frames
			AimController m_aim_controller{}; // objeto direto, sem ponteiro
			float m_trigger_delay_end{};
			bool m_trigger_waiting{};
			bool m_trigger_held{};
			float m_trigger_release_time{};
			float m_zeus_fire_time{};
			bool m_toggle_key_prev{};
		};

		class shared
		{
		public:
			struct context
			{
				std::uintptr_t weapon{};
				std::uintptr_t weapon_vdata{};
				std::uint32_t weapon_type{};
				std::uint16_t item_def_idx{};
				int num_bullets{};
				float inaccuracy{};
				float base_inaccuracy{};
				float spread{};
				float recoil_index{};
				bool is_reloading{};
				bool is_full_auto{};
				bool is_scoped{};
				bool weapon_ready{};
				float current_time{};
				float cycle_time{};
				float last_shot_time{};
				// Convars de escala de dano — lidos no tick() e cacheados aqui para
				// evitar 4 reads de memória por call de scale_damage (chamada em loops
				// de hitchance: 32 amostras × N hitboxes por tick).
				float damage_scale_ct_head{ 1.0f };
				float damage_scale_t_head{ 1.0f };
				float damage_scale_ct_body{ 1.0f };
				float damage_scale_t_body{ 1.0f };
				bool valid{};
			};

			class penetration
			{
			public:
				struct weapon_data
				{
					float damage{};
					float penetration{};
					float range_modifier{};
					float range{};
					float armor_ratio{};
					float headshot_multiplier{};
					// Escalas de dano por time/hitgroup — cacheadas do context no prepare()
					// para evitar leituras de convar dentro dos loops de penetração.
					float damage_scale_ct_head{ 1.0f };
					float damage_scale_t_head{ 1.0f };
					float damage_scale_ct_body{ 1.0f };
					float damage_scale_t_body{ 1.0f };
				};

				struct result
				{
					float damage{};
					int hitbox{ -1 };
					bool penetrated{};
				};

				void prepare(std::uintptr_t weapon_vdata, std::uintptr_t weapon);
				// Versão que recebe as escalas de dano já lidas do context —
				// evita 4 reads de convar extras por tick.
				void prepare_with_scales(
					std::uintptr_t weapon_vdata, std::uintptr_t weapon,
					float ct_head, float t_head, float ct_body, float t_body
				);
				[[nodiscard]] bool run(
					const math::vector3& start,
					const math::vector3& end,
					const systems::collector::player& target,
					const systems::bones::data& bones,
					result& out
				) const;
				[[nodiscard]] bool can(const math::vector3& start, const math::vector3& direction, float& out_damage) const;
				[[nodiscard]] float get_max_damage(int hitgroup, int target_armor, bool has_helmet, int target_team) const;
				[[nodiscard]] const weapon_data& get_weapon_data() const { return this->m_weapon_data; }

			private:
				weapon_data m_weapon_data{};
			};

			void tick();
			// ctx() retorna cópia por valor sob shared_lock — thread-safe.
			// Usar `const auto& ctx = g_shared.ctx()` seria data race se store_context()
			// sobrescrever m_ctx entre a aquisição do lock e o uso dos dados.
			[[nodiscard]] context ctx() const
			{
				std::shared_lock lock( this->m_ctx_mutex );
				return this->m_ctx;
			}
			[[nodiscard]] const penetration& pen() const { return this->m_pen; }

			[[nodiscard]] float calculate_hitchance(
				const math::vector3& eye_pos,
				const math::vector3& aim_angle,
				const systems::collector::player& target,
				const systems::bones::data& bones
			) const;

			[[nodiscard]] std::uint32_t get_spread_seed(const math::vector3& angles, int tick) const;
			[[nodiscard]] math::vector2 calculate_spread(
				int seed,
				float accuracy,
				float spread,
				float recoil_index,
				int item_def_idx,
				int num_bullets
			) const;

			[[nodiscard]] float get_prediction_time() const;
			[[nodiscard]] float get_spread(std::uintptr_t weapon_vdata) const;
			[[nodiscard]] float get_inaccuracy(
				std::uintptr_t pawn,
				std::uintptr_t weapon,
				std::uintptr_t weapon_vdata,
				const math::vector3& eye_angles
			) const;

			// Versão batched — usa batches pré-lidos para eliminar reads duplicados.
			// Aceita tanto batch_reader (dinâmico) quanto static_batch_reader<N>
			// (este é implicitamente convertido via operator batch_reader).
			[[nodiscard]] float get_inaccuracy_batched(
				std::uintptr_t pawn,
				const memory::batch_reader& weapon_batch,
				const memory::batch_reader& vdata_batch,
				const math::vector3& eye_angles
			) const;

			[[nodiscard]] bool ray_hits_capsule(
				const math::vector3& ray_origin,
				const math::vector3& ray_dir,
				const math::vector3& capsule_start,
				const math::vector3& capsule_end,
				float radius
			) const;

			[[nodiscard]] bool is_weapon_max_accuracy() const;

			// ── Offsets de punch angle centralizados ─────────────────────────────
			// CS2: punch está em pawn → m_pCameraServices → m_vecCsViewPunchAngle.
			// Cacheados aqui para evitar que legit, rcs e outros repetam a mesma
			// lógica de "lazy-init + flag de carregado".
			// Preenchidos automaticamente no primeiro uso (thread-safe via atomic).
			[[nodiscard]] std::uintptr_t aim_punch_offset( ) const { ensure_punch_offsets( ); return m_aim_punch_offset; }
			[[nodiscard]] std::uintptr_t camera_services_offset( ) const { ensure_punch_offsets( ); return m_camera_services_offset; }
			[[nodiscard]] std::uintptr_t view_punch_offset( ) const { ensure_punch_offsets( ); return m_view_punch_offset; }

		private:
			void store_context(const context& ctx);

			context m_ctx{};
			penetration m_pen{};
			mutable std::shared_mutex m_ctx_mutex{};

			// Cache de hitchance — evita recalcular a cada tick quando posição/ângulos
			// não mudaram significativamente. Invalidado por distância angular > threshold.
			struct hitchance_cache_entry
			{
				float result{ -1.f };
				math::vector3 last_eye_pos{};
				math::vector3 last_aim_angle{};
				std::chrono::steady_clock::time_point timestamp{};
				const systems::collector::player* last_target{};
			};
			mutable hitchance_cache_entry m_hitchance_cache{};
			// Cache válido por até k_hitchance_cache_ms milissegundos
			static constexpr float k_hitchance_cache_ms = 16.f; // ~1 frame a 60Hz
			// Threshold de mudança de ângulo para invalidar o cache (em graus²)
			static constexpr float k_hitchance_angle_thresh_sq = 0.01f; // 0.1°

			// ── Offsets de punch angle — preenchidos uma única vez ────────────
			// std::atomic<bool> garante visibilidade entre threads sem mutex.
			mutable std::atomic<bool>      m_punch_offsets_loaded{ false };
			mutable std::uintptr_t         m_aim_punch_offset{};
			mutable std::uintptr_t         m_camera_services_offset{};
			mutable std::uintptr_t         m_view_punch_offset{};

			void ensure_punch_offsets( ) const
			{
				if ( m_punch_offsets_loaded.load( std::memory_order_acquire ) )
					return;
				// Leitura única: safe mesmo com concorrência — o pior caso é
				// dois threads inicializarem simultaneamente com os mesmos valores.
				m_aim_punch_offset       = static_cast<std::uintptr_t>( SCHEMA( "C_CSPlayerPawn",         "m_aimPunchAngle"_hash ) );
				m_camera_services_offset = static_cast<std::uintptr_t>( SCHEMA( "C_BasePlayerPawn",       "m_pCameraServices"_hash ) );
				m_view_punch_offset      = static_cast<std::uintptr_t>( SCHEMA( "CPlayer_CameraServices", "m_vecCsViewPunchAngle"_hash ) );
				m_punch_offsets_loaded.store( true, std::memory_order_release );
			}
		};

		inline legit g_legit{};
		inline shared g_shared{};

		// Toggle global do aimbot/aim-assist — alterado via toggle_key na config
		inline std::atomic<bool> g_aimbot_enabled{ true };

	} // namespace combat

	namespace esp {

		class player
		{
		public:
			void on_render(zdraw::draw_list& draw_list);

		private:
			struct draw_offsets
			{
				float left{};
				float top{};
				float bottom{};
				float right{};
			};

			void add_box(zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const settings::esp::player::box& cfg, bool is_visible);
			void add_skeleton(zdraw::draw_list& draw_list, const systems::bones::data& bones, const settings::esp::player::skeleton& cfg, bool is_visible);
			void add_hitboxes(zdraw::draw_list& draw_list, const systems::bones::data& bones, const systems::collector::player& player, const settings::esp::player::hitboxes& cfg, float current_time);
			void add_health_bar(zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::health_bar& cfg, draw_offsets& offsets);
			void add_ammo_bar(zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::ammo_bar& cfg, draw_offsets& offsets);
			void add_name(zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::name& cfg, draw_offsets& offsets);
			void add_weapon(zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::weapon& cfg, draw_offsets& offsets);
			void add_flags(zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::info_flags& cfg, draw_offsets& offsets);

			struct animation_data
			{
				animation::spring health{};
				animation::spring ammo{};
				bool initialized{};
				float last_damage_time{};
				int last_health{ 100 };
			};

			std::unordered_map<std::uintptr_t, animation_data> m_animations{};
		};

		class item
		{
		public:
			void on_render(zdraw::draw_list& draw_list);

		private:
			enum class category : std::uint8_t { rifle, smg, shotgun, sniper, pistol, heavy, grenade, utility };

			void add_icon(zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::icon& cfg, float& y_offset);
			void add_name(zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::name& cfg, float& y_offset);
			void add_ammo(zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::ammo& cfg, float& y_offset);

			[[nodiscard]] bool passes_filter(systems::collector::item_subtype subtype, const settings::esp::item::filters& filters) const;
			[[nodiscard]] category get_category(systems::collector::item_subtype subtype) const;
			[[nodiscard]] std::string get_icon(systems::collector::item_subtype subtype) const;
			[[nodiscard]] std::string get_display_name(systems::collector::item_subtype subtype) const;
		};

		class projectile
		{
		public:
			void on_render(zdraw::draw_list& draw_list);

		private:
			void draw_timer(zdraw::draw_list& draw_list, const math::vector2& screen, float& y_offset, float remaining, float frac, const settings::esp::projectile& cfg) const;
			void draw_inferno_bounds(zdraw::draw_list& draw_list, const systems::collector::projectile& proj, const settings::esp::projectile& cfg) const;

			[[nodiscard]] zdraw::rgba get_color(systems::collector::projectile_subtype type, const settings::esp::projectile& cfg) const;
			[[nodiscard]] std::string get_icon(systems::collector::projectile_subtype type) const;
			[[nodiscard]] std::string get_name(systems::collector::projectile_subtype type) const;
			[[nodiscard]] zdraw::rgba lerp_color(const zdraw::rgba& a, const zdraw::rgba& b, float t) const;
		};

		inline player g_player{};
		inline item g_item{};
		inline projectile g_projectile{};

	} // namespace esp

	namespace misc {

		class grenades
		{
		public:
			void on_render(zdraw::draw_list& draw_list);

		private:
			struct trajectory
			{
				std::vector<math::vector3> points{};
				std::vector<math::vector3> bounces{};
				math::vector3 end_pos{};
				float duration{};
				int end_tick{ -1 };
				bool valid{};
			};

			struct in_flight_grenade
			{
				std::uintptr_t entity{};
				std::uintptr_t weapon_hash{};
				trajectory traj{};
				std::chrono::steady_clock::time_point throw_time{};
				std::chrono::steady_clock::time_point detonate_time{};
				std::chrono::steady_clock::time_point last_seen{};
				bool detonated{};
				bool effect_started{};
				bool corrected{};
			};

			[[nodiscard]] bool can_predict() const;
			void update_weapon_properties();
			void setup_throw(math::vector3& origin, math::vector3& velocity);
			void update_in_flight();
			[[nodiscard]] std::uintptr_t hash_from_projectile(systems::collector::projectile_subtype type) const;
			void simulate(const math::vector3& start, const math::vector3& velocity, trajectory& out);
			void step_simulation(math::vector3& pos, math::vector3& vel, systems::bvh::trace_result& trace, bool& detonated);
			void resolve_collision(const systems::bvh::trace_result& trace, math::vector3& pos, math::vector3& vel, bool& detonated);
			[[nodiscard]] bool should_detonate(const math::vector3& vel, int tick) const;
			[[nodiscard]] math::vector3 clip_velocity(const math::vector3& velocity, const math::vector3& normal, float overbounce);
			void render_trajectory(zdraw::draw_list& draw_list, const trajectory& traj, float alpha) const;

			std::uintptr_t m_weapon_vdata{};
			std::uintptr_t m_weapon_hash{};
			float m_throw_velocity{};
			float m_detonate_time{ 1.5f };
			float m_velocity_threshold{ 0.1f };

			std::vector<in_flight_grenade> m_in_flight{};
			std::chrono::steady_clock::time_point m_last_throw_time{};
			bool m_was_holding{ false };

			float m_sv_gravity{};
			float m_molotov_max_slope_z{};

			static constexpr auto gravity_scale{ 0.4f };
			static constexpr auto elasticity{ 0.45f };
			static constexpr auto max_ticks{ 1024 };
			static constexpr auto ticks_per_point{ 4 };
			static constexpr auto throw_cooldown{ 1.0f };
			static constexpr auto missing_grace{ 0.1f };
			static constexpr auto k_hull_size{ 2.0f };
			static constexpr auto k_pull_back{ 6.0f };
			static constexpr auto k_forward_offset{ 22.0f };
			static constexpr auto k_velocity_inherit{ 1.25f };
			static constexpr auto k_stop_speed_sq{ 400.0f };
			static constexpr auto k_steep_bounce_normal_z{ 0.7f };
			static constexpr auto k_steep_bounce_speed_sq{ 96000.0f };
		};

		class impacts
		{
		public:
			void on_render(zdraw::draw_list& draw_list);
		};

		inline grenades g_grenades{};
		inline impacts g_impacts{};

		class speed_esp
		{
		public:
			void on_render( zdraw::draw_list& draw_list );
		};
		inline speed_esp g_speed_esp{};

	} // namespace misc

} // namespace features