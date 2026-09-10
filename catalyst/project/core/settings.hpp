#pragma once

namespace settings {

	struct combat
	{
		struct aimbot
		{
			config::f<bool, "enabled"> enabled{ true };
			config::f<int, "key"> key{ VK_XBUTTON2 };
			// Tecla que alterna o aimbot/aim-assist on/off (toggle, não hold)
			config::f<int, "toggle_key"> toggle_key{ VK_F2 };

			config::f<int, "fov"> fov{ 5 };
			config::f<int, "smoothing"> smoothing{ 5 };

			config::f<bool, "autowall"> autowall{ true };
			config::f<float, "min damage"> min_damage{ 90.0f };

			config::f<bool, "head only"> head_only{ true };
			config::f<bool, "visible only"> visible_only{ true };

			config::f<bool, "draw fov"> draw_fov{ true };
			config::f<zdraw::rgba, "fov color"> fov_color{ { 255, 255, 255, 220 } };  // branco

			config::f<bool, "predictive"> predictive{ true };

			// ========== NOVAS FEATURES ==========
			config::f<bool, "assist_mode"> assist_mode{ false };
			config::f<float, "assist_strength"> assist_strength{ 0.3f };
			// Tecla hold para ativar aim assist (independente da tecla do aimbot)
			config::f<int, "assist_key"> assist_key{ VK_XBUTTON2 };
			config::f<bool, "humanize"> humanize{ true };
			config::f<float, "humanize_strength"> humanize_strength{ 50.0f };
			// RCS
			config::f<bool, "rcs_enabled"> rcs_enabled{ true };
			config::f<float, "rcs_strength"> rcs_strength{ 70.0f };
			// Tecla que alterna o RCS on/off independentemente do aimbot
			config::f<int, "rcs_key"> rcs_key{ VK_F3 };

			// Multipoint: testa múltiplos pontos da hitbox e escolhe o de menor FOV
			config::f<bool, "multipoint"> multipoint{ false };

			CONFIG_MEMBERS( enabled, key, toggle_key, fov, smoothing, autowall, min_damage, head_only, visible_only, draw_fov, fov_color, predictive, assist_mode, assist_strength, assist_key, humanize, humanize_strength, rcs_enabled, rcs_strength, rcs_key, multipoint )
		};

		struct triggerbot
		{
			config::f<bool, "enabled"> enabled{ true };
			config::f<int, "key"> key{ VK_XBUTTON2 };

			config::f<float, "hitchance"> hitchance{ 75.0f };
			config::f<int, "delay"> delay{ 10 };

			config::f<bool, "autowall"> autowall{ true };
			config::f<float, "min damage"> min_damage{ 90.0f };

			config::f<bool, "predictive"> predictive{ true };

			CONFIG_MEMBERS( enabled, key, hitchance, delay, autowall, min_damage, predictive )
		};

		struct other
		{
			config::f<bool, "penetration crosshair"> penetration_crosshair{ true };

			config::f<zdraw::rgba, "penetration color yes"> penetration_color_yes{ { 50, 255, 50, 125 } };
			config::f<zdraw::rgba, "penetration color no"> penetration_color_no{ { 255, 50, 50, 125 } };

			CONFIG_MEMBERS( penetration_crosshair, penetration_color_yes, penetration_color_no )
		};

		struct group_config
		{
			aimbot aimbot{};
			triggerbot triggerbot{};

			void register_config( std::string_view category )
			{
				std::string sub;
				sub.reserve( category.size( ) + 16 );

				sub = category; sub += " - aimbot"; aimbot.register_config( sub );
				sub = category; sub += " - triggerbot"; triggerbot.register_config( sub );
			}
		};

		static constexpr std::uint32_t k_group_count{ 6 };

		std::array<group_config, k_group_count> groups{};
		config::g<other, "other"> m_other{};

		group_config& get( std::uint32_t weapon_type )
		{
			const auto idx = weapon_type - cstypes::pistol;
			return this->groups[ idx < k_group_count ? idx : 2 ];
		}

		const group_config& get( std::uint32_t weapon_type ) const
		{
			const auto idx = weapon_type - cstypes::pistol;
			return this->groups[ idx < k_group_count ? idx : 2 ];
		}

		void register_config( std::string_view category )
		{
			static constexpr std::array<std::string_view, k_group_count> k_suffixes{ "pistol", "smg", "rifle", "shotgun", "sniper", "lmg" };
			::config::register_array( this->groups, category, k_suffixes );
			::config::register_one( this->m_other, category );
		}
	};

	struct esp
	{
		struct player
		{
			config::f<bool, "enabled"> enabled{ true };

			struct box
			{
				enum class style_type : std::uint8_t { full, cornered };

				config::f<bool, "enabled"> enabled{ true };
				config::f<style_type, "style"> style{ style_type::cornered };
				config::f<bool, "fill"> fill{ false };   // fill desativado por padrão — cobria o boneco
				config::f<bool, "outline"> outline{ true };
				config::f<float, "corner length"> corner_length{ 10.0f };

				config::f<zdraw::rgba, "visible color"> visible_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "occluded color"> occluded_color{ { 255, 255, 255, 160 } };

				CONFIG_MEMBERS( enabled, style, fill, outline, corner_length, visible_color, occluded_color )
			};

			config::g<box, "box"> m_box{};

			struct skeleton
			{
				config::f<bool, "enabled"> enabled{ true };
				config::f<float, "thickness"> thickness{ 1.0f };

				// Branco para visíveis, branco semi-transparente para ocluídos
				config::f<zdraw::rgba, "visible color"> visible_color{ { 255, 255, 255, 220 } };
				config::f<zdraw::rgba, "occluded color"> occluded_color{ { 255, 255, 255, 120 } };

				CONFIG_MEMBERS( enabled, thickness, visible_color, occluded_color )
			};

			config::g<skeleton, "skeleton"> m_skeleton{};

			struct hitboxes
			{
				config::f<bool, "enabled"> enabled{ false };  // desativado por padrão — skeleton lines aparecem errado em bots

				config::f<zdraw::rgba, "visible color"> visible_color{ { 255, 255, 255, 30 } };
				config::f<zdraw::rgba, "occluded color"> occluded_color{ { 255, 255, 255, 15 } };

				config::f<bool, "fill"> fill{ false };
				config::f<bool, "outline"> outline{ true };

				CONFIG_MEMBERS( enabled, visible_color, occluded_color, fill, outline )
			};

			config::g<hitboxes, "hitboxes"> m_hitboxes{};

			struct health_bar
			{
				enum class position_type : std::uint8_t { left, top, bottom };

				config::f<bool, "enabled"> enabled{ true };
				config::f<position_type, "position"> position{ position_type::left };
				config::f<bool, "outline"> outline{ true };
				config::f<bool, "gradient"> gradient{ true };
				config::f<bool, "show value"> show_value{ true };

				config::f<zdraw::rgba, "full color"> full_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "low color"> low_color{ { 200, 200, 200, 255 } };
				config::f<zdraw::rgba, "background color"> background_color{ { 15, 16, 22, 150 } };
				config::f<zdraw::rgba, "outline color"> outline_color{ { 15, 16, 22, 255 } };
				config::f<zdraw::rgba, "text color"> text_color{ { 255, 255, 255, 255 } };

				CONFIG_MEMBERS( enabled, position, outline, gradient, show_value, full_color, low_color, background_color, outline_color, text_color )
			};

			config::g<health_bar, "health bar"> m_health_bar{};

			struct ammo_bar
			{
				enum class position_type : std::uint8_t { left, top, bottom };

				config::f<bool, "enabled"> enabled{ true };
				config::f<position_type, "position"> position{ position_type::bottom };
				config::f<bool, "outline"> outline{ true };
				config::f<bool, "gradient"> gradient{ true };
				config::f<bool, "show value"> show_value{ false };

				config::f<zdraw::rgba, "full color"> full_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "low color"> low_color{ { 200, 200, 200, 255 } };
				config::f<zdraw::rgba, "background color"> background_color{ { 15, 16, 22, 150 } };
				config::f<zdraw::rgba, "outline color"> outline_color{ { 15, 16, 22, 255 } };
				config::f<zdraw::rgba, "text color"> text_color{ { 255, 255, 255, 255 } };

				CONFIG_MEMBERS( enabled, position, outline, gradient, show_value, full_color, low_color, background_color, outline_color, text_color )
			};

			config::g<ammo_bar, "ammo bar"> m_ammo_bar{};

			struct info_flags
			{
				enum flag : std::uint8_t
				{
					none = 0,
					money = 1 << 0,
					armor = 1 << 1,
					kit = 1 << 2,
					scoped = 1 << 3,
					defusing = 1 << 4,
					flashed = 1 << 5,
					ping = 1 << 6,
					distance = 1 << 7
				};

				config::f<bool, "enabled"> enabled{ true };
				config::f<std::uint8_t, "flags"> flags{ static_cast< std::uint8_t >( flag::money | flag::armor | flag::kit | flag::scoped | flag::defusing | flag::flashed | flag::ping ) };

				config::f<zdraw::rgba, "money color"> money_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "armor color"> armor_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "kit color"> kit_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "scoped color"> scoped_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "defusing color"> defusing_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "flashed color"> flashed_color{ { 255, 255, 255, 255 } };
				config::f<zdraw::rgba, "distance color"> distance_color{ { 255, 255, 255, 180 } };
				config::f<zdraw::rgba, "look dir color"> look_dir_color{ { 255, 255, 255, 180 } };

				[[nodiscard]] bool has( flag f ) const { return this->flags.value & f; }

				// look_dir controlado por bool separado — não cabe no uint8 flags
				config::f<bool, "look dir"> look_dir{ false };
				// mostra label "bot" sobre bots — bool separado dos flags
				config::f<bool, "show bot label"> show_bot_label{ true };
				config::f<zdraw::rgba, "bot color"> bot_color{ { 255, 80, 80, 255 } };

				CONFIG_MEMBERS( enabled, flags, money_color, armor_color, kit_color, scoped_color, defusing_color, flashed_color, distance_color, look_dir_color, look_dir, show_bot_label, bot_color )
			};

			config::g<info_flags, "info flags"> m_info_flags{};

			struct name
			{
				config::f<bool, "enabled"> enabled{ true };
				config::f<zdraw::rgba, "color"> color{ { 255, 255, 255, 230 } };

				CONFIG_MEMBERS( enabled, color )
			};

			config::g<name, "name"> m_name{};

			struct weapon
			{
				enum class display_type : std::uint8_t { text, icon, text_and_icon };

				config::f<bool, "enabled"> enabled{ true };
				config::f<display_type, "display"> display{ display_type::icon };

				config::f<zdraw::rgba, "text color"> text_color{ { 255, 255, 255, 210 } };
				config::f<zdraw::rgba, "icon color"> icon_color{ { 255, 255, 255, 230 } };

				CONFIG_MEMBERS( enabled, display, text_color, icon_color )
			};

			config::g<weapon, "weapon"> m_weapon{};

			CONFIG_MEMBERS( enabled, m_box, m_skeleton, m_hitboxes, m_health_bar, m_ammo_bar, m_info_flags, m_name, m_weapon )
		};
		config::g<player, "player"> m_player{};

		struct item
		{
			config::f<bool, "enabled"> enabled{ true };
			config::f<float, "max distance"> max_distance{ 40.0f };

			struct icon
			{
				config::f<bool, "enabled"> enabled{ true };
				config::f<zdraw::rgba, "color"> color{ { 255, 255, 255, 200 } };

				CONFIG_MEMBERS( enabled, color )
			};

			config::g<icon, "icon"> m_icon{};

			struct name
			{
				config::f<bool, "enabled"> enabled{ false };
				config::f<zdraw::rgba, "color"> color{ { 255, 255, 255, 180 } };

				CONFIG_MEMBERS( enabled, color )
			};

			config::g<name, "name"> m_name{};

			struct ammo
			{
				config::f<bool, "enabled"> enabled{ true };
				config::f<zdraw::rgba, "color"> color{ { 255, 255, 255, 200 } };
				config::f<zdraw::rgba, "empty color"> empty_color{ { 200, 200, 200, 200 } };

				CONFIG_MEMBERS( enabled, color, empty_color )
			};

			config::g<ammo, "ammo"> m_ammo{};

			struct filters
			{
				config::f<bool, "rifles"> rifles{ true };
				config::f<bool, "smgs"> smgs{ true };
				config::f<bool, "shotguns"> shotguns{ true };
				config::f<bool, "snipers"> snipers{ true };
				config::f<bool, "pistols"> pistols{ true };
				config::f<bool, "heavy"> heavy{ true };
				config::f<bool, "grenades"> grenades{ true };
				config::f<bool, "utility"> utility{ true };

				CONFIG_MEMBERS( rifles, smgs, shotguns, snipers, pistols, heavy, grenades, utility )
			};

			config::g<filters, "filters"> m_filters{};

			CONFIG_MEMBERS( enabled, max_distance, m_icon, m_name, m_ammo, m_filters )
		};

		config::g<item, "item"> m_item{};

		struct projectile
		{
			config::f<bool, "enabled"> enabled{ true };

			config::f<bool, "show icon"> show_icon{ true };
			config::f<bool, "show name"> show_name{ true };
			config::f<bool, "show timer bar"> show_timer_bar{ true };
			config::f<bool, "show inferno bounds"> show_inferno_bounds{ true };

			config::f<zdraw::rgba, "default color"> default_color{ { 255, 255, 255, 200 } };
			config::f<zdraw::rgba, "color he"> color_he{ { 255, 255, 255, 220 } };
			config::f<zdraw::rgba, "color flash"> color_flash{ { 255, 255, 255, 220 } };
			config::f<zdraw::rgba, "color smoke"> color_smoke{ { 255, 255, 255, 220 } };
			config::f<zdraw::rgba, "color molotov"> color_molotov{ { 255, 255, 255, 220 } };
			config::f<zdraw::rgba, "color decoy"> color_decoy{ { 255, 255, 255, 200 } };

			config::f<zdraw::rgba, "timer high color"> timer_high_color{ { 255, 255, 255, 255 } };
			config::f<zdraw::rgba, "timer low color"> timer_low_color{ { 220, 100, 100, 255 } };
			config::f<zdraw::rgba, "bar background"> bar_background{ { 15, 16, 22, 150 } };

			CONFIG_MEMBERS( enabled, show_icon, show_name, show_timer_bar, show_inferno_bounds, default_color, color_he, color_flash, color_smoke, color_molotov, color_decoy, timer_high_color, timer_low_color, bar_background )
		};

		config::g<projectile, "projectile"> m_projectile{};

		CONFIG_MEMBERS( m_player, m_item, m_projectile )
	};

	struct misc
	{
		struct grenades
		{
			config::f<bool, "enabled"> enabled{ true };
			config::f<bool, "local only"> local_only{ true };
			config::f<zdraw::rgba, "color"> color{ { 255, 255, 255, 200 } };

			CONFIG_MEMBERS( enabled, local_only, color )
		};

		config::g<grenades, "grenades"> m_grenades{};

		config::f<bool, "limit fps"> limit_fps{ true };
		config::f<int, "fps limit"> fps_limit{ 240 };

		// ========== NOVAS FEATURES ==========
		struct wallbang
		{
			config::f<bool, "enabled"> enabled{ true };
			// Verde quando pode penetrar/acertar, vermelho quando não
			config::f<zdraw::rgba, "color_yes"> color_yes{ { 50, 255, 50, 200 } };
			config::f<zdraw::rgba, "color_no">  color_no { { 255, 50, 50, 200 } };
			CONFIG_MEMBERS(enabled, color_yes, color_no)
		};
		config::g<wallbang, "wallbang"> m_wallbang{};

		struct bombtimer
		{
			config::f<bool, "enabled"> enabled{ true };
			// Posição do painel na tela em pixels
			config::f<float, "pos_x"> pos_x{ 12.f };
			config::f<float, "pos_y_frac"> pos_y_frac{ 0.35f }; // fração da altura da tela
			CONFIG_MEMBERS(enabled, pos_x, pos_y_frac)
		};
		config::g<bombtimer, "bombtimer"> m_bombtimer{};

		struct stream_mode
		{
			config::f<bool, "enabled"> enabled{ false };
			CONFIG_MEMBERS(enabled)
		};
		config::g<stream_mode, "stream_mode"> m_stream_mode{};

		struct impacts_cfg
		{
			config::f<bool, "enabled"> enabled{ true };
			config::f<float, "lifetime"> lifetime{ 4.0f };
			config::f<zdraw::rgba, "color"> color{ { 255, 220, 0, 220 } }; // amarelo padrão
			CONFIG_MEMBERS(enabled, lifetime, color)
		};
		config::g<impacts_cfg, "impacts"> m_impacts{};

		struct no_flash
		{
			config::f<bool, "enabled"> enabled{ false };
			// opacity: 0.0 = flash completamente removida, 1.0 = flash original intacta.
			// Valores intermediários simulam flash parcial (menos suspeito que remoção total).
			config::f<float, "opacity"> opacity{ 0.0f };
			CONFIG_MEMBERS(enabled, opacity)
		};
		config::g<no_flash, "no_flash"> m_no_flash{};

		struct speed_esp_cfg
		{
			config::f<bool, "enabled"> enabled{ true };
			CONFIG_MEMBERS(enabled)
		};
		config::g<speed_esp_cfg, "speed_esp"> m_speed_esp{};

		struct radar_cfg
		{
			config::f<bool, "enabled"> enabled{ false };
			// Escala do mapa no radar: 1.0 = padrão, >1 = mais zoom, <1 = mais afastado.
			config::f<float, "scale"> scale{ 1.0f };
			// Zoom do radar em metros de raio visível (menor = mais fechado/detalhado).
			config::f<float, "zoom"> zoom{ 500.0f };
			// Posição na tela em pixels (canto superior-esquerdo do painel).
			config::f<float, "pos_x"> pos_x{ 20.0f };
			config::f<float, "pos_y"> pos_y{ 20.0f };
			// Tamanho do painel em pixels.
			config::f<float, "size"> size{ 200.0f };
			// Cor dos inimigos no radar.
			config::f<zdraw::rgba, "enemy_color"> enemy_color{ { 220, 60, 60, 230 } };
			// Mostrar nome do jogador no radar.
			config::f<bool, "show_names"> show_names{ false };
			CONFIG_MEMBERS(enabled, scale, zoom, pos_x, pos_y, size, enemy_color, show_names)
		};
		config::g<radar_cfg, "radar"> m_radar{};

		CONFIG_MEMBERS( m_grenades, limit_fps, fps_limit, m_wallbang, m_bombtimer, m_stream_mode, m_impacts, m_no_flash, m_speed_esp, m_radar )
	};

	inline combat g_combat{};
	inline esp g_esp{};
	inline misc g_misc{};

} // namespace settings
