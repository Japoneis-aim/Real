#include <stdafx.hpp>

void menu::draw( )
{
	if ( GetAsyncKeyState( VK_INSERT ) & 1 )
	{
		this->m_open = !this->m_open;
	}

	if ( !this->m_open )
	{
		return;
	}

	zui::begin( );

	if ( zui::begin_window( "catalyst##main", this->m_x, this->m_y, this->m_w, this->m_h, true, 580.0f, 440.0f ) )
	{
		const auto [avail_w, avail_h] = zui::get_content_region_avail( );

		if ( zui::begin_nested_window( "##inner", avail_w, avail_h ) )
		{
			constexpr auto header_h{ 28.0f };
			constexpr auto padding{ 6.0f };

			zui::set_cursor_pos( padding, padding );
			this->draw_header( avail_w - padding * 2.0f, header_h );

			zui::set_cursor_pos( padding, padding + header_h + padding );
			this->draw_content( avail_w - padding * 2.0f, avail_h - header_h - padding * 3.0f );

			if ( const auto win = zui::detail::get_current_window( ) )
			{
				this->draw_accent_lines( win->bounds );
			}

			zui::end_nested_window( );
		}

		zui::end_window( );
	}

	zui::end( );
}

void menu::draw_header( float width, float height )
{
	if ( !zui::begin_nested_window( "##header", width, height ) )
	{
		return;
	}

	const auto current = zui::detail::get_current_window( );
	if ( !current )
	{
		zui::end_nested_window( );
		return;
	}

	const auto& style = zui::get_style( );
	const auto dt = zdraw::get_delta_time( );
	const auto bx = current->bounds.x;
	const auto by = current->bounds.y;
	const auto bw = current->bounds.w;
	const auto bh = current->bounds.h;

	zdraw::get_draw_list( ).add_rect_filled( bx, by, bw, bh, zdraw::rgba{ 14, 14, 14, 255 } );
	zdraw::get_draw_list( ).add_rect( bx, by, bw, bh, zdraw::rgba{ 38, 38, 38, 255 } );

	{
		constexpr auto title{ "catalyst" };
		auto [tw, th] = zdraw::measure_text( title );
		zdraw::get_draw_list( ).add_text( bx + 10.0f, by + ( bh - th ) * 0.5f, title, nullptr, style.accent );
	}

	static constexpr std::pair<const char*, tab> tabs[ ]
	{
		{ "combat", tab::combat },
		{ "esp",    tab::esp    },
		{ "misc",   tab::misc   },
	};

	constexpr auto tab_count = static_cast< int >( std::size( tabs ) );
	constexpr auto tab_spacing{ 16.0f };

	struct tab_anim { float v{ 0.0f }; };
	static std::array<tab_anim, tab_count> anims{};

	auto cursor_x = bx + bw - 10.0f;

	for ( int i = tab_count - 1; i >= 0; --i )
	{
		const auto& t = tabs[ i ];
		const auto is_sel = ( this->m_tab == t.second );
		auto [tw, th] = zdraw::measure_text( t.first );

		cursor_x -= tw;

		const auto tab_rect = zui::rect{ cursor_x, by, tw, bh };
		const auto hovered = zui::detail::mouse_hovered( tab_rect ) && !zui::detail::overlay_blocking_input( );

		if ( hovered && zui::detail::mouse_clicked( ) )
		{
			this->m_tab = t.second;
		}

		auto& anim = anims[ i ];
		anim.v += ( ( is_sel ? 1.0f : 0.0f ) - anim.v ) * std::min( 10.0f * dt, 1.0f );

		const auto text_y = by + ( bh - th ) * 0.5f;
		const auto col = is_sel ? zui::lighten( style.accent, 1.0f + 0.1f * anim.v ) : zui::lerp( zdraw::rgba{ 110, 110, 110, 255 }, style.text, hovered ? 1.0f : 0.0f );

		zdraw::get_draw_list( ).add_text( cursor_x, text_y, t.first, nullptr, col );

		cursor_x -= tab_spacing;
	}

	zui::end_nested_window( );
}

void menu::draw_content( float width, float height )
{
	zui::push_style_var( zui::style_var::window_padding_x, 10.0f );
	zui::push_style_var( zui::style_var::window_padding_y, 10.0f );

	if ( !zui::begin_nested_window( "##content", width, height ) )
	{
		zui::pop_style_var( 2 );
		return;
	}

	if ( const auto win = zui::detail::get_current_window( ) )
	{
		this->draw_accent_lines( win->bounds );
	}

	switch ( this->m_tab )
	{
	case tab::combat: this->draw_combat( ); break;
	case tab::esp:    this->draw_esp( );    break;
	case tab::misc:   this->draw_misc( );   break;
	default: break;
	}

	zui::pop_style_var( 2 );
	zui::end_nested_window( );
}

void menu::draw_accent_lines( const zui::rect& bounds, float fade_ratio )
{
	const auto ix = bounds.x + 1.0f;
	const auto iw = bounds.w - 2.0f;
	const auto top_y = bounds.y + 1.0f;
	const auto bot_y = bounds.y + bounds.h - 2.0f;
	const auto accent = zui::get_accent_color( );
	const auto trans = zdraw::rgba{ accent.r, accent.g, accent.b, 0 };
	const auto fade_w = iw * fade_ratio;
	const auto solid_w = iw - fade_w * 2.0f;

	for ( const auto ly : { top_y, bot_y } )
	{
		zdraw::get_draw_list( ).add_rect_filled_multi_color( ix, ly, fade_w, 1.0f, trans, accent, accent, trans );
		zdraw::get_draw_list( ).add_rect_filled( ix + fade_w, ly, solid_w, 1.0f, accent );
		zdraw::get_draw_list( ).add_rect_filled_multi_color( ix + fade_w + solid_w, ly, fade_w, 1.0f, accent, trans, trans, accent );
	}
}

void menu::draw_combat( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;
	const auto& style = zui::get_style( );

	{
		const auto win = zui::detail::get_current_window( );
		if ( win )
		{
			constexpr auto group_spacing{ 12.0f };
			constexpr auto bar_h{ 22.0f };
			auto gx = win->bounds.x + style.window_padding_x;
			const auto gy = win->bounds.y + win->cursor_y;

			for ( auto i = 0; i < 6; ++i )
			{
				auto [tw, th] = zdraw::measure_text( k_weapon_groups[ i ] );
				const auto gr = zui::rect{ gx, gy, tw, bar_h };
				const auto hov = zui::detail::mouse_hovered( gr ) && !zui::detail::overlay_blocking_input( );

				if ( hov && zui::detail::mouse_clicked( ) )
				{
					this->m_weapon_group = i;
				}

				const auto sel = ( this->m_weapon_group == i );
				const auto col = sel ? zui::get_accent_color( ) : zui::lerp( zdraw::rgba{ 100, 100, 100, 255 }, style.text, hov ? 1.0f : 0.0f );

				zdraw::get_draw_list( ).add_text( gx, gy + ( bar_h - th ) * 0.5f, k_weapon_groups[ i ], nullptr, col );

				if ( sel )
				{
					const auto accent = zui::get_accent_color( );
					const auto trans = zdraw::rgba{ accent.r, accent.g, accent.b, 0 };
					const auto fade = tw * 0.3f;
					zdraw::get_draw_list( ).add_rect_filled_multi_color( gx, gy + bar_h - 2.0f, fade, 1.0f, trans, accent, accent, trans );
					zdraw::get_draw_list( ).add_rect_filled( gx + fade, gy + bar_h - 2.0f, tw - fade * 2.0f, 1.0f, accent );
					zdraw::get_draw_list( ).add_rect_filled_multi_color( gx + tw - fade, gy + bar_h - 2.0f, fade, 1.0f, accent, trans, trans, accent );
				}

				gx += tw + group_spacing;
			}

			win->cursor_y += bar_h + style.item_spacing_y;
			win->line_height = 0.0f;
		}
	}

	auto& cfg = settings::g_combat.groups[ this->m_weapon_group ];

	if ( zui::begin_group_box( "aimbot", col_w ) )
	{
		zui::checkbox( "enabled##ab", cfg.aimbot.enabled );
		zui::keybind( "key##ab", cfg.aimbot.key );

		zui::slider_int( "fov##ab", cfg.aimbot.fov, 1, 45 );
		zui::slider_int( "smoothing##ab", cfg.aimbot.smoothing, 0, 50 );
		zui::checkbox( "head only##ab", cfg.aimbot.head_only );
		zui::checkbox( "visible only##ab", cfg.aimbot.visible_only );

		if ( cfg.aimbot.visible_only )
		{
			zui::checkbox( "autowall##ab", cfg.aimbot.autowall );

			if ( zui::begin_popup( "##ab_aw_popup", 200.0f ) )
			{
				zui::slider_float( "min damage##ab", cfg.aimbot.min_damage, 1.0f, 100.0f, "%.0f" );
				zui::end_popup( );
			}
		}

		zui::checkbox( "predictive##ab", cfg.aimbot.predictive );
		zui::separator( );
		zui::checkbox( "draw fov##ab", cfg.aimbot.draw_fov );

		if ( zui::begin_popup( "##ab_tb_popup", 200.0f ) )
		{
			zui::color_picker( "color##ab", cfg.aimbot.fov_color );
			zui::end_popup( );
		}

		zui::end_group_box( );
	}

	zui::same_line( );

	if ( zui::begin_group_box( "triggerbot", col_w ) )
	{
		zui::checkbox( "enabled##tb", cfg.triggerbot.enabled );
		zui::keybind( "key##tb", cfg.triggerbot.key );
		zui::slider_float( "hitchance##tb", cfg.triggerbot.hitchance, 0.0f, 100.0f, "%.0f%%" );
		zui::slider_int( "delay (ms)##tb", cfg.triggerbot.delay, 0, 500 );
		zui::checkbox( "autowall##tb", cfg.triggerbot.autowall );

		if ( zui::begin_popup( "##tb_aw_popup", 200.0f ) )
		{
			zui::slider_float( "min damage##tb", cfg.triggerbot.min_damage, 1.0f, 100.0f, "%.0f" );
			zui::end_popup( );
		}

		zui::checkbox( "predictive##tb", cfg.triggerbot.predictive );
		zui::end_group_box( );
	}

	if ( zui::begin_group_box( "other", col_w ) )
	{
		zui::checkbox( "penetration crosshair##ot", settings::g_combat.m_other.penetration_crosshair );

		if ( zui::begin_popup( "##ot_colors_popup", 200.0f ) )
		{
			zui::color_picker( "can penetrate##ot", settings::g_combat.m_other.penetration_color_yes );
			zui::color_picker( "cannot penetrate##ot", settings::g_combat.m_other.penetration_color_no );
			zui::end_popup( );
		}

		zui::checkbox( "zeusbot##ot", settings::g_combat.m_other.m_zeusbot.enabled );

		if ( zui::begin_popup( "##ot_zeusbot_popup", 200.0f ) )
		{
			zui::keybind( "key##ot_zb", settings::g_combat.m_other.m_zeusbot.key );
			zui::slider_float( "max fov##ot_zb", settings::g_combat.m_other.m_zeusbot.max_fov, 1.0f, 180.0f, "%.0f" );
			zui::end_popup( );
		}

		zui::end_group_box( );
	}
}

void menu::draw_esp( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;
	auto& p = settings::g_esp.m_player;

	if ( zui::begin_group_box( "players", col_w ) )
	{
		zui::checkbox( "enabled##pl", p.enabled );

		zui::checkbox( "box##bx", p.m_box.enabled );
		if ( zui::begin_popup( "##bx_popup", 200.0f ) )
		{
			constexpr const char* box_styles[ ]{ "full", "cornered" };
			zui::combo( "style##bx", p.m_box.style.value, box_styles, 2 );

			zui::checkbox( "fill##bx", p.m_box.fill );
			zui::checkbox( "outline##bx", p.m_box.outline );

			if ( p.m_box.style == settings::esp::player::box::style_type::cornered )
			{
				zui::slider_float( "corner len##bx", p.m_box.corner_length, 4.0f, 30.0f, "%.0f" );
			}

			zui::color_picker( "visible##bx", p.m_box.visible_color );
			zui::color_picker( "occluded##bx", p.m_box.occluded_color );
			zui::end_popup( );
		}

		zui::checkbox( "skeleton##sk", p.m_skeleton.enabled );
		if ( zui::begin_popup( "##sk_popup", 200.0f ) )
		{
			zui::slider_float( "thickness##sk", p.m_skeleton.thickness, 0.5f, 4.0f, "%.1f" );
			zui::color_picker( "visible##sk", p.m_skeleton.visible_color );
			zui::color_picker( "occluded##sk", p.m_skeleton.occluded_color );
			zui::end_popup( );
		}

		zui::checkbox( "health bar##hb", p.m_health_bar.enabled );
		if ( zui::begin_popup( "##hb_popup", 200.0f ) )
		{
			constexpr const char* hb_positions[ ]{ "left", "top", "bottom" };
			zui::combo( "position##hb", p.m_health_bar.position.value, hb_positions, 3 );

			zui::checkbox( "outline##hb", p.m_health_bar.outline );
			zui::checkbox( "gradient##hb", p.m_health_bar.gradient );
			zui::checkbox( "show value##hb", p.m_health_bar.show_value );
			zui::color_picker( "full##hb", p.m_health_bar.full_color );
			zui::color_picker( "low##hb", p.m_health_bar.low_color );
			zui::color_picker( "background##hb", p.m_health_bar.background_color );
			zui::color_picker( "outline color##hb", p.m_health_bar.outline_color );
			zui::color_picker( "text color##hb", p.m_health_bar.text_color );
			zui::end_popup( );
		}

		zui::checkbox( "ammo bar##amb", p.m_ammo_bar.enabled );
		if ( zui::begin_popup( "##amb_popup", 200.0f ) )
		{
			constexpr const char* amb_positions[ ]{ "left", "top", "bottom" };
			zui::combo( "position##amb", p.m_ammo_bar.position.value, amb_positions, 3 );

			zui::checkbox( "outline##amb", p.m_ammo_bar.outline );
			zui::checkbox( "gradient##amb", p.m_ammo_bar.gradient );
			zui::checkbox( "show value##amb", p.m_ammo_bar.show_value );
			zui::color_picker( "full##amb", p.m_ammo_bar.full_color );
			zui::color_picker( "low##amb", p.m_ammo_bar.low_color );
			zui::color_picker( "background##amb", p.m_ammo_bar.background_color );
			zui::color_picker( "outline color##amb", p.m_ammo_bar.outline_color );
			zui::color_picker( "text color##amb", p.m_ammo_bar.text_color );
			zui::end_popup( );
		}

		zui::checkbox( "name##nm", p.m_name.enabled );
		if ( zui::begin_popup( "##nm_popup", 200.0f ) )
		{
			zui::color_picker( "color##nm", p.m_name.color );
			zui::end_popup( );
		}

		zui::checkbox( "weapon##wp", p.m_weapon.enabled );
		if ( zui::begin_popup( "##wp_popup", 200.0f ) )
		{
			constexpr const char* disp_types[ ]{ "text", "icon", "text + icon" };
			zui::combo( "display##wp", p.m_weapon.display.value, disp_types, 3 );

			zui::color_picker( "text color##wp", p.m_weapon.text_color );
			zui::color_picker( "icon color##wp", p.m_weapon.icon_color );
			zui::end_popup( );
		}

		zui::checkbox( "hitboxes##hx", p.m_hitboxes.enabled );
		if ( zui::begin_popup( "##hx_popup", 200.0f ) )
		{
			zui::checkbox( "fill##hx", p.m_hitboxes.fill );
			zui::checkbox( "outline##hx", p.m_hitboxes.outline );
			zui::color_picker( "visible##hx", p.m_hitboxes.visible_color );
			zui::color_picker( "occluded##hx", p.m_hitboxes.occluded_color );
			zui::end_popup( );
		}

		zui::checkbox( "info flags##if", p.m_info_flags.enabled );
		if ( zui::begin_popup( "##if_popup", 200.0f ) )
		{
			constexpr const char* flag_names[ ]{ "money", "armor", "kit", "scoped", "defusing", "flashed", "distance" };
			constexpr settings::esp::player::info_flags::flag flag_values[ ]{ settings::esp::player::info_flags::money, settings::esp::player::info_flags::armor, settings::esp::player::info_flags::kit, settings::esp::player::info_flags::scoped, settings::esp::player::info_flags::defusing, settings::esp::player::info_flags::flashed, settings::esp::player::info_flags::distance, };

			for ( auto i = 0; i < 7; ++i )
			{
				auto active = p.m_info_flags.has( flag_values[ i ] );

				if ( zui::checkbox( flag_names[ i ], active ) )
				{
					if ( active )
					{
						p.m_info_flags.flags.value |= flag_values[ i ];
					}
					else
					{
						p.m_info_flags.flags.value &= ~flag_values[ i ];
					}
				}
			}

			zui::separator( );
			zui::color_picker( "money##if", p.m_info_flags.money_color );
			zui::color_picker( "armor##if", p.m_info_flags.armor_color );
			zui::color_picker( "kit##if", p.m_info_flags.kit_color );
			zui::color_picker( "scoped##if", p.m_info_flags.scoped_color );
			zui::color_picker( "defusing##if", p.m_info_flags.defusing_color );
			zui::color_picker( "flashed##if", p.m_info_flags.flashed_color );
			zui::color_picker( "distance##if", p.m_info_flags.distance_color );
			zui::end_popup( );
		}

		zui::end_group_box( );
	}

	zui::same_line( );

	if ( zui::begin_group_box( "items", col_w ) )
	{
		auto& it = settings::g_esp.m_item;
		zui::checkbox( "enabled##it", it.enabled );
		zui::slider_float( "max dist##it", it.max_distance, 5.0f, 150.0f, "%.0fm" );

		zui::checkbox( "icon##it", it.m_icon.enabled );
		if ( zui::begin_popup( "##it_icon_popup", 180.0f ) )
		{
			zui::color_picker( "color##it_icon", it.m_icon.color );
			zui::end_popup( );
		}

		zui::checkbox( "name##it", it.m_name.enabled );
		if ( zui::begin_popup( "##it_name_popup", 180.0f ) )
		{
			zui::color_picker( "color##it_name", it.m_name.color );
			zui::end_popup( );
		}

		zui::checkbox( "ammo##it", it.m_ammo.enabled );
		if ( zui::begin_popup( "##it_ammo_popup", 180.0f ) )
		{
			zui::color_picker( "color##it_ammo", it.m_ammo.color );
			zui::color_picker( "empty##it_ammo", it.m_ammo.empty_color );
			zui::end_popup( );
		}

		zui::separator( );

		auto& f = it.m_filters;
		constexpr const char* filter_items[ ]{ "rifles", "smgs", "shotguns", "snipers", "pistols", "heavy", "grenades", "utility" };
		static bool filter_selected[ 8 ]{ true, true, true, true, true, true, true, true };

		zui::multicombo( "filters##f", filter_selected, filter_items, 8 );

		f.rifles = filter_selected[ 0 ];
		f.smgs = filter_selected[ 1 ];
		f.shotguns = filter_selected[ 2 ];
		f.snipers = filter_selected[ 3 ];
		f.pistols = filter_selected[ 4 ];
		f.heavy = filter_selected[ 5 ];
		f.grenades = filter_selected[ 6 ];
		f.utility = filter_selected[ 7 ];
		zui::end_group_box( );
	}

	if ( zui::begin_group_box( "projectiles", col_w ) )
	{
		auto& pr = settings::g_esp.m_projectile;
		zui::checkbox( "enabled##pr", pr.enabled );
		zui::checkbox( "icon##pr", pr.show_icon );
		zui::checkbox( "name##pr", pr.show_name );
		zui::checkbox( "timer bar##pr", pr.show_timer_bar );
		zui::checkbox( "inferno bounds##pr", pr.show_inferno_bounds );

		if ( zui::begin_popup( "##pr_colors_popup", 200.0f ) )
		{
			zui::color_picker( "default##pr", pr.default_color );
			zui::color_picker( "he##pr", pr.color_he );
			zui::color_picker( "flash##pr", pr.color_flash );
			zui::color_picker( "smoke##pr", pr.color_smoke );
			zui::color_picker( "molotov##pr", pr.color_molotov );
			zui::color_picker( "decoy##pr", pr.color_decoy );
			zui::separator( );
			zui::color_picker( "timer high##pr", pr.timer_high_color );
			zui::color_picker( "timer low##pr", pr.timer_low_color );
			zui::color_picker( "bar background##pr", pr.bar_background );
			zui::end_popup( );
		}

		zui::end_group_box( );
	}
}

void menu::draw_misc( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;

	if ( zui::begin_group_box( "main", col_w ) )
	{
		zui::checkbox( "grenade prediction##gr", settings::g_misc.m_grenades.enabled );

		if ( zui::begin_popup( "##gr_popup", 200.0f ) )
		{
			zui::checkbox( "local only##gr", settings::g_misc.m_grenades.local_only );
			zui::color_picker( "color##gr", settings::g_misc.m_grenades.color );
			zui::end_popup( );
		}

		zui::checkbox( "limit fps##fps", settings::g_misc.limit_fps );

		if ( zui::begin_popup( "##fps_popup", 200.0f ) )
		{
			zui::slider_int( "fps limit##fps", settings::g_misc.fps_limit, 30, 1000 );
			zui::end_popup( );
		}

		zui::end_group_box( );
	}

	zui::same_line( );

	if ( zui::begin_group_box( "config", col_w ) )
	{
		static std::string search_buf;
		static std::vector<std::wstring> config_list;
		static int selected{ -1 };
		static bool needs_refresh{ true };
		static bool confirm_delete{};
		static float confirm_delete_timer{};
		static bool confirm_save{};
		static float confirm_save_timer{};

		if ( needs_refresh )
		{
			config_list = config::registry::list( );
			needs_refresh = false;

			if ( selected >= static_cast< int >( config_list.size( ) ) )
			{
				selected = -1;
			}
		}

		const auto dt = zdraw::get_delta_time( );

		if ( confirm_delete && ( confirm_delete_timer += dt ) > 3.0f )
		{
			confirm_delete = false;
		}

		if ( confirm_save && ( confirm_save_timer += dt ) > 3.0f )
		{
			confirm_save = false;
		}

		auto to_narrow = []( const std::wstring& w )
			{
				char buf[ 128 ]{};
				WideCharToMultiByte( CP_UTF8, 0, w.c_str( ), -1, buf, sizeof( buf ), nullptr, nullptr );
				return std::string( buf );
			};

		auto to_wide = []( const std::string& s )
			{
				wchar_t buf[ 128 ]{};
				MultiByteToWideChar( CP_UTF8, 0, s.c_str( ), -1, buf, 128 );
				return std::wstring( buf );
			};

		auto to_lower = []( std::string s )
			{
				for ( auto& c : s )
				{
					c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
				}
				return s;
			};

		zui::text_input( "##config_search", search_buf, 64, "search or new name..." );

		const auto& s = zui::get_style( );
		const auto [row_avail_w, row_avail_h] = zui::get_content_region_avail( );
		constexpr auto btn_h{ 22.0f };
		const auto btn_w = ( row_avail_w - s.item_spacing_x * 2.0f ) / 3.0f;
		const auto has_selection = selected >= 0 && selected < static_cast< int >( config_list.size( ) );

		auto exact_match{ false };

		if ( !search_buf.empty( ) )
		{
			const auto wbuf = to_wide( search_buf );

			for ( const auto& cfg : config_list )
			{
				if ( _wcsicmp( cfg.c_str( ), wbuf.c_str( ) ) == 0 )
				{
					exact_match = true;
					break;
				}
			}
		}

		const auto can_create = !search_buf.empty( ) && !exact_match;

		if ( can_create )
		{
			if ( zui::button( "create##cfg", btn_w, btn_h ) )
			{
				const auto wname = to_wide( search_buf );
				config::registry::save( wname );
				needs_refresh = true;

				const auto fresh = config::registry::list( );
				for ( int i = 0; i < static_cast< int >( fresh.size( ) ); ++i )
				{
					if ( _wcsicmp( fresh[ i ].c_str( ), wname.c_str( ) ) == 0 )
					{
						selected = i;
						break;
					}
				}

				search_buf.clear( );
			}
		}
		else if ( !confirm_save )
		{
			if ( zui::button( has_selection ? "save##cfg" : "save##cfg_dis", btn_w, btn_h ) && has_selection )
			{
				confirm_save = true;
				confirm_save_timer = 0.0f;
			}
		}
		else
		{
			zui::push_style_color( zui::style_color::button_bg, zdraw::rgba{ 180, 140, 60, 120 } );

			if ( zui::button( "confirm##save", btn_w, btn_h ) && has_selection )
			{
				config::registry::save( config_list[ selected ] );
				confirm_save = false;
			}

			zui::pop_style_color( 1 );
		}

		zui::same_line( );

		if ( zui::button( has_selection ? "load##cfg" : "load##cfg_dis", btn_w, btn_h ) && has_selection )
		{
			config::registry::load( config_list[ selected ] );
		}

		zui::same_line( );

		if ( !confirm_delete )
		{
			if ( zui::button( has_selection ? "delete##cfg" : "delete##cfg_dis", btn_w, btn_h ) && has_selection )
			{
				confirm_delete = true;
				confirm_delete_timer = 0.0f;
			}
		}
		else
		{
			zui::push_style_color( zui::style_color::button_bg, zdraw::rgba{ 180, 60, 60, 120 } );

			if ( zui::button( "confirm##del", btn_w, btn_h ) && has_selection )
			{
				config::registry::remove( config_list[ selected ] );
				selected = -1;
				search_buf.clear( );
				needs_refresh = true;
				confirm_delete = false;
			}

			zui::pop_style_color( 1 );
		}

		zui::new_line( );

		{
			static std::unordered_map<zui::widget_id, float> hover_anims;
			static std::unordered_map<zui::widget_id, float> sel_anims;
			static std::unordered_map<zui::widget_id, float> last_click_time;
			static float anim_clock{ 0.0f };
			anim_clock += dt;

			const auto win = zui::detail::get_current_window( );
			const auto blocking = zui::detail::overlay_blocking_input( );
			const auto search_lower = to_lower( search_buf );
			auto any_shown{ false };

			for ( int i = 0; i < static_cast< int >( config_list.size( ) ); ++i )
			{
				const auto name = to_narrow( config_list[ i ] );

				if ( !search_lower.empty( ) && to_lower( name ).find( search_lower ) == std::string::npos )
				{
					continue;
				}

				constexpr auto row_h{ 24.0f };
				const auto [list_avail_w, list_avail_h] = zui::get_content_region_avail( );

				const auto rx = win->bounds.x + s.window_padding_x;
				const auto ry = win->bounds.y + win->cursor_y;
				const auto row_rect = zui::rect{ rx, ry, list_avail_w, row_h };

				const auto id = zui::detail::generate_id( name );
				const auto is_selected = ( selected == i );
				const auto row_hovered = !blocking && zui::detail::mouse_hovered( row_rect );

				auto& hover_anim = hover_anims[ id ];
				auto& sel_anim = sel_anims[ id ];
				hover_anim += ( ( row_hovered ? 1.0f : 0.0f ) - hover_anim ) * std::min( 12.0f * dt, 1.0f );
				sel_anim += ( ( is_selected ? 1.0f : 0.0f ) - sel_anim ) * std::min( 10.0f * dt, 1.0f );

				auto& dl = zdraw::get_draw_list( );

				if ( sel_anim > 0.01f )
				{
					auto bg = s.accent;
					bg.a = static_cast< std::uint8_t >( 25.0f * sel_anim );
					dl.add_rect_filled( rx, ry, list_avail_w, row_h, bg );
				}

				if ( hover_anim > 0.01f && !is_selected )
				{
					dl.add_rect_filled( rx, ry, list_avail_w, row_h, zdraw::rgba{ 255, 255, 255, static_cast< std::uint8_t >( 6.0f * hover_anim ) } );
				}

				if ( sel_anim > 0.01f )
				{
					auto bar = s.accent;
					bar.a = static_cast< std::uint8_t >( 255.0f * sel_anim );
					dl.add_rect_filled( rx, ry + 5.0f, 2.0f, row_h - 10.0f, bar );
				}

				const auto [tw, th] = zdraw::measure_text( name );
				const auto text_dim = zdraw::rgba{ 130, 130, 130, 255 };
				const auto name_col = zui::lerp( text_dim, s.text, std::max( hover_anim, sel_anim ) );
				dl.add_text( rx + ( is_selected ? 10.0f : 6.0f ), ry + ( row_h - th ) * 0.5f, name, nullptr, name_col );

				if ( row_hovered && zui::detail::mouse_clicked( ) )
				{
					auto& last_t = last_click_time[ id ];

					if ( anim_clock - last_t < 0.4f )
					{
						config::registry::load( config_list[ i ] );
						last_t = 0.0f;
					}
					else
					{
						selected = i;
						confirm_delete = false;
						confirm_save = false;
						last_t = anim_clock;
					}
				}

				win->cursor_y += row_h;
				win->line_height = 0.0f;
				any_shown = true;
			}

			if ( !any_shown )
			{
				const auto msg = config_list.empty( ) ? "no configs" : "no results";
				const auto text_dim = zdraw::rgba{ 130, 130, 130, 255 };
				const auto [tw, th] = zdraw::measure_text( msg );

				const auto rx = win->bounds.x + s.window_padding_x;
				const auto ry = win->bounds.y + win->cursor_y;

				zdraw::get_draw_list( ).add_text( rx + 6.0f, ry + 6.0f, msg, nullptr, text_dim );

				win->cursor_y += th + 12.0f;
				win->line_height = 0.0f;
			}

			win->cursor_y += s.item_spacing_y;
		}

		{
			const auto [foot_avail_w, foot_avail_h] = zui::get_content_region_avail( );
			const auto half_w = ( foot_avail_w - s.item_spacing_x ) * 0.5f;

			if ( zui::button( "export##clipboard", half_w, btn_h ) )
			{
				std::string name;

				if ( has_selection )
				{
					name = to_narrow( config_list[ selected ] );
				}
				else if ( !search_buf.empty( ) )
				{
					name = search_buf;
				}

				const auto code = config::export_share( name );
				if ( !code.empty( ) && OpenClipboard( nullptr ) )
				{
					EmptyClipboard( );
					const auto size = ( code.size( ) + 1 ) * sizeof( char );
					auto mem = GlobalAlloc( GMEM_MOVEABLE, size );

					if ( mem )
					{
						auto dest = GlobalLock( mem );
						if ( dest )
						{
							std::memcpy( dest, code.c_str( ), size );
							GlobalUnlock( mem );
						}

						SetClipboardData( CF_TEXT, mem );
					}

					CloseClipboard( );
				}
			}

			zui::same_line( );

			if ( zui::button( "import##clipboard", half_w, btn_h ) )
			{
				std::string clip;

				if ( OpenClipboard( nullptr ) )
				{
					auto mem = GetClipboardData( CF_TEXT );
					if ( mem )
					{
						auto data = static_cast< const char* >( GlobalLock( mem ) );
						if ( data )
						{
							clip = data;
							GlobalUnlock( mem );
						}
					}

					CloseClipboard( );
				}

				if ( !clip.empty( ) )
				{
					const auto result = config::import_share( clip );
					if ( result.success )
					{
						std::wstring wname;

						if ( !result.name.empty( ) )
						{
							wname = to_wide( result.name );
						}
						else
						{
							SYSTEMTIME st{};
							GetLocalTime( &st );
							wchar_t buf[ 128 ]{};
							std::swprintf( buf, 128, L"import_%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
							wname = buf;
						}

						config::registry::save( wname );
						needs_refresh = true;
					}
				}
			}
		}

		zui::end_group_box( );
	}
}