#include <stdafx.hpp>

// Fontes declaradas em render.cpp
extern ImFont* InterRegular;
extern ImFont* InterMedium;
extern ImFont* InterBold;
extern ImFont* InterBold12;
extern ImFont* InterLight;
extern ImFont* InterSemiBold;
extern ImFont* FontAwesomeSolid;

// ============================================================================
// Sub-tabs (igual ao exemplo)
// ============================================================================
enum SubTab
{
	NONE = -1,
	MISC_EXPLOITS,
	AIM_AIMBOT,
	AIM_TRIGGERBOT,
	AIM_RCS,
	VISUALS_ESP,
	OTHERS_SETTINGS,
};

static SubTab  s_sub_tab        = AIM_AIMBOT;
static int     s_main_tab       = 0;
static bool    s_initialized    = false;

static float   s_indicator_y    = 0.f;
static float   s_target_ind     = 0.f;
static float   s_circle_x       = -1.f;
static float   s_circle_y       = -1.f;
static float   s_circle_tx      = -1.f;
static float   s_circle_ty      = -1.f;

static ImVec2  s_win_pos        = { -1.f, -1.f };
static bool    s_dragging       = false;
static ImVec2  s_drag_offset    = {};

// Grupo de arma ativo e tecla do menu — declarados aqui para estarem
// disponíveis em draw() e em draw_settings()
static int s_weapon_group   = 0;
static int s_menu_key       = VK_INSERT;
static int s_menu_key_state = 0;
static constexpr const char* k_groups[]{ "Pistol","SMG","Rifle","Shotgun","Sniper","LMG" };

// ============================================================================
// on_init — chamado uma única vez após o contexto ImGui ser criado.
// Aplica o estilo global aqui, fora do loop de render, para garantir que
// as configs sejam aplicadas mesmo se o contexto ImGui for recriado.
// ============================================================================
void menu::on_init( )
{
	ImGuiStyle* Style = &ImGui::GetStyle( );

	Style->WindowRounding   = 7;
	Style->WindowBorderSize = 1;
	Style->WindowPadding    = ImVec2( 0, 0 );
	Style->WindowShadowSize = 0;
	Style->ScrollbarSize    = 8;

	Style->Colors[ ImGuiCol_Separator ]         = ImColor( 0, 0, 0, 0 );
	Style->Colors[ ImGuiCol_SeparatorActive ]   = ImColor( 0, 0, 0, 0 );
	Style->Colors[ ImGuiCol_SeparatorHovered ]  = ImColor( 0, 0, 0, 0 );
	Style->Colors[ ImGuiCol_ResizeGrip ]        = ImColor( 0, 0, 0, 0 );
	Style->Colors[ ImGuiCol_ResizeGripActive ]  = ImColor( 0, 0, 0, 0 );
	Style->Colors[ ImGuiCol_ResizeGripHovered ] = ImColor( 0, 0, 0, 0 );
	Style->Colors[ ImGuiCol_WindowBg ]          = ImColor( 12, 12, 12, 255 );
	Style->Colors[ ImGuiCol_ChildBg ]           = ImColor( 0, 0, 0, 0 );
	Style->Colors[ ImGuiCol_Border ]            = ImColor( 23, 24, 25 );
	Style->Colors[ ImGuiCol_Text ]              = ImColor( 1.f, 1.f, 1.f, 0.8f );
	Style->Colors[ ImGuiCol_TextSelectedBg ]    = ImColor( 75, 70, 175, 100 );
}

// ============================================================================
// draw — janela principal (100% fiel ao main.cpp do exemplo)
// ============================================================================
void menu::draw( )
{
	if ( ::GetAsyncKeyState( s_menu_key ) & 1 )
		this->m_open = !this->m_open;

	// Auto-sync do grupo de arma com a arma atual do jogador local.
	// Feito sempre — menu aberto ou fechado — para que ao abrir as settings
	// o grupo já esteja correto e as sliders reflitam a arma na mão.
	// O usuário ainda pode trocar manualmente no popup; na próxima troca de
	// arma o grupo é atualizado novamente.
	{
		const auto wtype = systems::g_local.weapon_type( );
		if ( cstypes::is_weapon_valid( wtype ) )
		{
			const auto auto_group = static_cast< int >( wtype - cstypes::pistol );
			if ( auto_group >= 0 && auto_group < static_cast< int >( settings::combat::k_group_count ) )
				s_weapon_group = auto_group;
		}
	}

	if ( !this->m_open )
		return;

	ImGuiIO& io       = ImGui::GetIO( );

	const ImVec2 windowSize = { 700.f, 400.f };
	const ImVec2 windowPos  = {
		( io.DisplaySize.x - windowSize.x ) * 0.5f,
		( io.DisplaySize.y - windowSize.y ) * 0.5f
	};

	if ( s_win_pos.x < 0 || s_win_pos.y < 0 )
		s_win_pos = windowPos;

	if ( !s_initialized )
	{
		s_main_tab   = 0;
		s_sub_tab    = AIM_AIMBOT;
		s_target_ind = s_indicator_y = ImGui::GetCursorScreenPos( ).y + 40.f;
		s_initialized = true;
	}

	ImGui::SetNextWindowSize( windowSize );
	ImGui::SetNextWindowPos( s_win_pos, ImGuiCond_Always );

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoCollapse;

	ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 12/255.f, 12/255.f, 12/255.f, 1.f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 8.0f );

	if ( !ImGui::Begin( "##MainWindow", nullptr, flags ) )
	{
		ImGui::End( );
		ImGui::PopStyleVar( );
		ImGui::PopStyleColor( );
		return;
	}

	ImDrawList* draw_list = ImGui::GetWindowDrawList( );
	ImVec2 winPos  = ImGui::GetWindowPos( );
	ImVec2 winSize = ImGui::GetWindowSize( );
	ImVec2 mousePos = io.MousePos;
	ImVec2 Pos  = ImGui::GetWindowPos( );
	ImVec2 Size = ImGui::GetWindowSize( );

	const float headerHeight  = 30.0f;
	const float sidebarWidth  = 190.0f;

	// drag area
	ImGui::SetCursorScreenPos( winPos );
	ImGui::InvisibleButton( "window_drag_area", ImVec2( winSize.x, headerHeight ) );
	if ( ImGui::IsItemActive( ) )
	{
		if ( !s_dragging )
		{
			s_dragging    = true;
			s_drag_offset = ImVec2( mousePos.x - winPos.x, mousePos.y - winPos.y );
		}
	}
	else s_dragging = false;
	if ( s_dragging )
		s_win_pos = ImVec2( mousePos.x - s_drag_offset.x, mousePos.y - s_drag_offset.y );

	// header background
	draw_list->AddRectFilled( winPos,
		ImVec2( winPos.x + winSize.x, winPos.y + headerHeight ),
		ImGui::GetColorU32( ImVec4( 0,0,0,1 ) ), 12.0f, ImDrawFlags_RoundCornersTop );

	// sidebar background
	draw_list->AddRectFilled(
		ImVec2( winPos.x, winPos.y + headerHeight ),
		ImVec2( winPos.x + sidebarWidth, winPos.y + winSize.y ),
		ImGui::GetColorU32( ImVec4( 10/255.f, 10/255.f, 10/255.f, 1.0f ) )
	);

	// título "aimware" no header
	ImGui::PushFont( InterBold );
	ImVec2 title_sz = ImGui::CalcTextSize( "aimware" );
	draw_list->AddText(
		ImVec2( winPos.x + ( sidebarWidth - title_sz.x ) * 0.5f, winPos.y + ( headerHeight - title_sz.y ) * 0.5f ),
		IM_COL32( 53, 147, 251, 255 ), "aimware"
	);
	ImGui::PopFont( );

	// ── sidebar ─────────────────────────────────────────────────────────────
	ImGui::SetCursorScreenPos( ImVec2( winPos.x + 10, winPos.y + headerHeight + 10 ) );
	ImGui::BeginChild( "SidebarChild", ImVec2( sidebarWidth - 20, winSize.y - headerHeight - 20 ), false, ImGuiWindowFlags_NoScrollbar );

	ImU32 colIndicator = IM_COL32( 53, 147, 251, 255 );

	// helper: aba principal
	auto SidebarTab = [ & ]( const char* name, int tabIndex, SubTab firstSubTab )
	{
		bool selected = ( s_main_tab == tabIndex );

		ImGui::PushStyleColor( ImGuiCol_Header,        ImVec4( 0,0,0,0 ) );
		ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0,0,0,0 ) );
		ImGui::PushStyleColor( ImGuiCol_HeaderActive,  ImVec4( 0,0,0,0 ) );
		ImGui::PushStyleColor( ImGuiCol_Text, selected ? ImVec4( 5.f,1.f,1.f,1.f ) : ImVec4( 0.5f,0.5f,0.5f,1.f ) );
		ImGui::PushFont( InterRegular );

		if ( ImGui::Selectable( name, selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2( 0, 25 ) ) )
		{
			s_main_tab = tabIndex;
			s_sub_tab  = firstSubTab;
			s_target_ind = ImGui::GetItemRectMin( ).y - winPos.y;
		}
		if ( selected )
			s_target_ind = ImGui::GetItemRectMin( ).y - winPos.y;

		ImGui::PopFont( );
		ImGui::PopStyleColor( 4 );
	};

	// helper: sub-aba com bolinha animada
	auto SubtabSelectable = [ & ]( const char* name, SubTab subTabId )
	{
		bool selected = ( s_sub_tab == subTabId );

		ImGui::PushStyleColor( ImGuiCol_Header,        ImVec4( 0,0,0,0 ) );
		ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0,0,0,0 ) );
		ImGui::PushStyleColor( ImGuiCol_HeaderActive,  ImVec4( 0,0,0,0 ) );

		ImVec2 cursorPos = ImGui::GetCursorScreenPos( );

		ImGui::PushStyleColor( ImGuiCol_Text, selected ? ImVec4( 1.f,1.f,1.f,1.f ) : ImVec4( 0.5f,0.5f,0.5f,1.f ) );

		constexpr float offsetX     = 10.0f;
		constexpr float circleSpace = 14.0f;

		ImGui::PushFont( InterRegular );
		ImGui::SetCursorScreenPos( ImVec2( cursorPos.x + circleSpace + offsetX, cursorPos.y ) );
		ImGui::TextUnformatted( name );
		ImGui::PopFont( );
		ImGui::PopStyleColor( );
		ImGui::PopStyleColor( 3 );

		std::string buttonId = "subtab_" + std::to_string( static_cast<int>( subTabId ) );
		ImVec2 btnPos = ImGui::GetCursorScreenPos( );
		btnPos.y -= ImGui::GetTextLineHeightWithSpacing( );
		ImGui::SetCursorScreenPos( btnPos );
		ImGui::InvisibleButton( buttonId.c_str( ), ImVec2( 100, ImGui::GetTextLineHeightWithSpacing( ) ) );
		if ( ImGui::IsItemClicked( ) ) s_sub_tab = subTabId;

		if ( selected )
		{
			ImVec2 wp = ImGui::GetWindowPos( );
			ImVec2 ts = ImGui::CalcTextSize( name );
			s_circle_tx = ( cursorPos.x + 6.0f ) - wp.x;
			s_circle_ty = ( cursorPos.y + ts.y * 0.5f ) - wp.y;
			if ( s_circle_x < 0.f ) s_circle_x = s_circle_tx;
			if ( s_circle_y < 0.f ) s_circle_y = s_circle_ty;
		}
	};

	SidebarTab( "Aim Assistance", 0, AIM_AIMBOT );
	if ( s_main_tab == 0 )
	{
		SubtabSelectable( "Aim Bot",     AIM_AIMBOT );
		SubtabSelectable( "Trigger Bot", AIM_TRIGGERBOT );
		SubtabSelectable( "RCS",         AIM_RCS );
	}

	SidebarTab( "Visuals", 1, VISUALS_ESP );
	if ( s_main_tab == 1 )
		SubtabSelectable( "Esp Main", VISUALS_ESP );

	SidebarTab( "Misc", 2, MISC_EXPLOITS );
	if ( s_main_tab == 2 )
		SubtabSelectable( "Exploits", MISC_EXPLOITS );

	SidebarTab( "Settings", 3, OTHERS_SETTINGS );
	if ( s_main_tab == 3 )
		SubtabSelectable( "General", OTHERS_SETTINGS );

	// animação bolinha
	const float animSpeed = 6.0f;
	s_circle_x += ( s_circle_tx - s_circle_x ) * io.DeltaTime * animSpeed;
	s_circle_y += ( s_circle_ty - s_circle_y ) * io.DeltaTime * animSpeed;

	if ( s_circle_x >= 0.f && s_circle_y >= 0.f )
	{
		ImDrawList* sdl = ImGui::GetWindowDrawList( );
		ImVec2 wp = ImGui::GetWindowPos( );
		sdl->AddCircleFilled( ImVec2( s_circle_x, s_circle_y ) + wp, 3.0f, IM_COL32( 53, 147, 251, 255 ) );
	}

	// animação indicador
	s_indicator_y += ( s_target_ind - s_indicator_y ) * 0.2f;
	draw_list->AddRectFilled(
		ImVec2( winPos.x,     winPos.y + s_indicator_y ),
		ImVec2( winPos.x + 3, winPos.y + s_indicator_y + 25 ),
		colIndicator, 1.0f
	);

	ImGui::EndChild( );

	// ── conteúdo principal ───────────────────────────────────────────────────
	ImGui::SetCursorPos( ImVec2( sidebarWidth + 10, headerHeight + 10 ) );
	ImGui::BeginChild( "MainContent", ImVec2( winSize.x - sidebarWidth - 20, winSize.y - headerHeight - 20 ), false );

	switch ( s_sub_tab )
	{
	case AIM_AIMBOT:      this->draw_aimbot( );     break;
	case AIM_TRIGGERBOT:  this->draw_triggerbot( ); break;
	case AIM_RCS:         this->draw_rcs( );        break;
	case VISUALS_ESP:     this->draw_esp( );        break;
	case MISC_EXPLOITS:   this->draw_misc( );       break;
	case OTHERS_SETTINGS: this->draw_settings( );   break;
	default: break;
	}

	ImGui::EndChild( );
	ImGui::End( );
	ImGui::PopStyleVar( );
	ImGui::PopStyleColor( );
}

// helper: converte zdraw::rgba ↔ ImVec4 sem static (lê settings todo frame)
static ImVec4 to_imvec4( const zdraw::rgba& c )
{
	return { c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f };
}
static void from_imvec4( zdraw::rgba& c, const ImVec4& v )
{
	c.r = static_cast<std::uint8_t>( std::clamp( v.x, 0.f, 1.f ) * 255.f );
	c.g = static_cast<std::uint8_t>( std::clamp( v.y, 0.f, 1.f ) * 255.f );
	c.b = static_cast<std::uint8_t>( std::clamp( v.z, 0.f, 1.f ) * 255.f );
	c.a = static_cast<std::uint8_t>( std::clamp( v.w, 0.f, 1.f ) * 255.f );
}

// helper color picker inline — lê e escreve de volta para settings todo frame
static void color_picker_inline( const char* label, const char* popup_id, zdraw::rgba& setting_color )
{
	constexpr float fixedX = 187.f;
	ImGui::PushFont( InterRegular );
	ImGui::AlignTextToFramePadding( );
	ImGui::Text( "%s", label );
	ImGui::SameLine( );
	ImGui::SetCursorPosX( fixedX );
	ImGui::SetCursorPosY( ImGui::GetCursorPosY( ) - 1 );

	// Lê o valor atual do setting a cada frame — garante sync após Load Config
	ImVec4 col = to_imvec4( setting_color );

	if ( ImGui::ColorButton( label, col ) )
		ImGui::OpenPopup( popup_id );
	if ( ImGui::BeginPopup( popup_id ) )
	{
		if ( ImGui::ColorPicker4( "##picker", reinterpret_cast<float*>( &col ),
			ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar |
			ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_PickerHueWheel ) )
		{
			// Escreve de volta para settings ao editar
			from_imvec4( setting_color, col );
		}
		ImGui::EndPopup( );
	}
	ImGui::PopFont( );
}

// ============================================================================
// AIM BOT
// ============================================================================
void menu::draw_aimbot( )
{
	auto& cfg = settings::g_combat.groups[ s_weapon_group ];
	const float half = ImGui::GetWindowSize( ).x / 2 - 25;

	ImGui::SetCursorPos( ImVec2( 20, 0 ) );
	ImGui::BeginGroup( );
	{
		if ( ImGui::CustomChild( "AIMBOT", ImVec2( half, 220 ) ) )
		{
			ImGui::Checkbox( "Enabled",       &cfg.aimbot.enabled.value );
			ImGui::KeyBind(  "Key",           &cfg.aimbot.key.value, &cfg.aimbot.key.value );
			ImGui::KeyBind(  "Toggle Key",    &cfg.aimbot.toggle_key.value, &cfg.aimbot.toggle_key.value );
			ImGui::Checkbox( "Visible Only",  &cfg.aimbot.visible_only.value );
			ImGui::Checkbox( "Head Only",     &cfg.aimbot.head_only.value );
			ImGui::Checkbox( "Multipoint",    &cfg.aimbot.multipoint.value );
			ImGui::Checkbox( "Predictive",    &cfg.aimbot.predictive.value );
			ImGui::Checkbox( "Draw FOV",      &cfg.aimbot.draw_fov.value );

			if ( cfg.aimbot.draw_fov )
				color_picker_inline( "Fov Color", "fovColorPicker", cfg.aimbot.fov_color.value );
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );

	ImGui::SetCursorPos( ImVec2( half + 35, 0 ) );
	ImGui::BeginGroup( );
	{
		if ( ImGui::CustomChild( "AIMBOT SETTINGS", ImVec2( half, 220 ) ) )
		{
			ImGui::SliderInt(   "Fov",       &cfg.aimbot.fov.value,       1,  45  );
			ImGui::SliderInt(   "Smoothing", &cfg.aimbot.smoothing.value,  0,  50  );
			ImGui::Checkbox(    "Humanize",  &cfg.aimbot.humanize.value );
			if ( cfg.aimbot.humanize )
				ImGui::SliderFloat( "Humanize Str", &cfg.aimbot.humanize_strength.value, 0, 100, "%.0f%%" );
			ImGui::Checkbox( "Aim Assist",   &cfg.aimbot.assist_mode.value );
			if ( cfg.aimbot.assist_mode )
			{
				ImGui::KeyBind( "Assist Key", &cfg.aimbot.assist_key.value, &cfg.aimbot.assist_key.value );
				ImGui::SliderFloat( "Strength##aa", &cfg.aimbot.assist_strength.value, 0, 1, "%.2f" );
			}
			ImGui::Checkbox( "Autowall", &cfg.aimbot.autowall.value );
			if ( cfg.aimbot.autowall )
				ImGui::SliderFloat( "Min Damage", &cfg.aimbot.min_damage.value, 1, 100, "%.0f" );
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );
}

// ============================================================================
// TRIGGER BOT
// ============================================================================
void menu::draw_triggerbot( )
{
	auto& cfg = settings::g_combat.groups[ s_weapon_group ];
	const float half = ImGui::GetWindowSize( ).x / 2 - 25;

	ImGui::SetCursorPos( ImVec2( 20, 0 ) );
	ImGui::BeginGroup( );
	{
		if ( ImGui::CustomChild( "TRIGGERBOT", ImVec2( half, 160 ) ) )
		{
			ImGui::Checkbox( "Enabled",    &cfg.triggerbot.enabled.value );
			ImGui::KeyBind(  "Key",        &cfg.triggerbot.key.value, &cfg.triggerbot.key.value );
			ImGui::SliderFloat( "Hitchance", &cfg.triggerbot.hitchance.value, 0, 100, "%.0f%%" );
			ImGui::SliderInt(   "Delay ms",  &cfg.triggerbot.delay.value, 0, 500 );
			ImGui::Checkbox( "Autowall",   &cfg.triggerbot.autowall.value );
			if ( cfg.triggerbot.autowall )
				ImGui::SliderFloat( "Min Dmg##tb", &cfg.triggerbot.min_damage.value, 1, 100, "%.0f" );
			ImGui::Checkbox( "Predictive", &cfg.triggerbot.predictive.value );
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );
}

void menu::draw_rcs( )
{
	auto& cfg = settings::g_combat.groups[ s_weapon_group ];
	const float half = ImGui::GetWindowSize( ).x / 2 - 25;

	ImGui::SetCursorPos( ImVec2( 20, 0 ) );
	ImGui::BeginGroup( );
	{
		if ( ImGui::CustomChild( "RCS", ImVec2( half, 100 ) ) )
		{
			ImGui::Checkbox( "Enabled", &cfg.aimbot.rcs_enabled.value );
			if ( cfg.aimbot.rcs_enabled )
			{
				ImGui::KeyBind( "Key", &cfg.aimbot.rcs_key.value, &cfg.aimbot.rcs_key.value );
				ImGui::SliderFloat( "Strength##rcs", &cfg.aimbot.rcs_strength.value, 0, 100, "%.0f%%" );
			}
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );
}

void menu::draw_esp( )
{
	auto& p = settings::g_esp.m_player;
	const float half = ImGui::GetWindowSize( ).x / 2 - 25;

	ImGui::SetCursorPos( ImVec2( 20, 0 ) );
	ImGui::BeginGroup( );
	{
		if ( ImGui::CustomChild( "VISUALS", ImVec2( half, 340 ) ) )
		{
			ImGui::Checkbox( "Enable Options", &p.enabled.value );
			ImGui::Checkbox( "Skeleton",       &p.m_skeleton.enabled.value );
			ImGui::Checkbox( "Box",            &p.m_box.enabled.value );
			ImGui::Checkbox( "Life Bar",       &p.m_health_bar.enabled.value );
			ImGui::Checkbox( "Ammo Bar",       &p.m_ammo_bar.enabled.value );
			ImGui::Checkbox( "Weapon Icon",    &p.m_weapon.enabled.value );
			ImGui::Checkbox( "Player Name",    &p.m_name.enabled.value );
			ImGui::Checkbox( "Hitboxes",       &p.m_hitboxes.enabled.value );
			ImGui::Checkbox( "Info Flags",     &p.m_info_flags.enabled.value );
			if ( p.m_info_flags.enabled )
			{
				using flag = settings::esp::player::info_flags::flag;
				auto& f = p.m_info_flags;
				auto toggle_flag = [&]( const char* label, flag bit )
				{
					bool on = f.has( bit );
					if ( ImGui::Checkbox( label, &on ) )
					{
						if ( on ) f.flags.value |= static_cast<std::uint8_t>( bit );
						else      f.flags.value &= ~static_cast<std::uint8_t>( bit );
					}
				};
				ImGui::Indent( 12.f );
				toggle_flag( "Money##fl",    flag::money );
				toggle_flag( "Armor##fl",    flag::armor );
				toggle_flag( "Kit##fl",      flag::kit );
				toggle_flag( "Scoped##fl",   flag::scoped );
				toggle_flag( "Defusing##fl", flag::defusing );
				toggle_flag( "Flashed##fl",  flag::flashed );
				toggle_flag( "Ping##fl",     flag::ping );
				toggle_flag( "Distance##fl", flag::distance );
				ImGui::Checkbox( "Look Dir##fl",  &f.look_dir.value );
				ImGui::Checkbox( "Bot Label##fl", &f.show_bot_label.value );
				ImGui::Unindent( 12.f );
			}

			ImGui::Separator( );
			ImGui::Checkbox( "Dropped Items",  &settings::g_esp.m_item.enabled.value );
			if ( settings::g_esp.m_item.enabled )
			{
				auto& fi = settings::g_esp.m_item.m_filters;
				ImGui::Indent( 12.f );
				ImGui::Checkbox( "Rifles##fi",   &fi.rifles.value );
				ImGui::Checkbox( "SMGs##fi",     &fi.smgs.value );
				ImGui::Checkbox( "Shotguns##fi", &fi.shotguns.value );
				ImGui::Checkbox( "Snipers##fi",  &fi.snipers.value );
				ImGui::Checkbox( "Pistols##fi",  &fi.pistols.value );
				ImGui::Checkbox( "Heavy##fi",    &fi.heavy.value );
				ImGui::Checkbox( "Grenades##fi", &fi.grenades.value );
				ImGui::Checkbox( "Utility##fi",  &fi.utility.value );
				ImGui::Unindent( 12.f );
			}

			ImGui::Separator( );
			ImGui::Checkbox( "Projectiles",    &settings::g_esp.m_projectile.enabled.value );
			if ( settings::g_esp.m_projectile.enabled )
			{
				auto& pr = settings::g_esp.m_projectile;
				ImGui::Indent( 12.f );
				ImGui::Checkbox( "Show Icon##pr",       &pr.show_icon.value );
				ImGui::Checkbox( "Show Name##pr",       &pr.show_name.value );
				ImGui::Checkbox( "Show Timer Bar##pr",  &pr.show_timer_bar.value );
				ImGui::Checkbox( "Inferno Bounds##pr",  &pr.show_inferno_bounds.value );
				ImGui::Unindent( 12.f );
			}
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );

	ImGui::SetCursorPos( ImVec2( half + 35, 0 ) );
	ImGui::BeginGroup( );
	{
		if ( ImGui::CustomChild( "VISUALS SETTINGS", ImVec2( half, 340 ) ) )
		{
			ImGui::SliderFloat( "Skeleton Thickness", &p.m_skeleton.thickness.value, 0.5f, 4.f, "%.1f" );
			color_picker_inline( "Skeleton Color",    "skeletonColorPicker",   p.m_skeleton.visible_color.value );

			{
				constexpr const char* styles[]{ "Normal","Corner" };
				int s = static_cast<int>( p.m_box.style.value );
				if ( ImGui::Combo( "Box Type", &s, styles, 2 ) )
					p.m_box.style.value = static_cast<decltype(p.m_box.style.value)>( s );
			}
			color_picker_inline( "Box Color",         "BoxColorPicker",        p.m_box.visible_color.value );

			{
				constexpr const char* hbpos[]{ "Left","Top","Bottom" };
				int pos = static_cast<int>( p.m_health_bar.position.value );
				if ( ImGui::Combo( "Life Bar Position", &pos, hbpos, 3 ) )
					p.m_health_bar.position.value = static_cast<decltype(p.m_health_bar.position.value)>( pos );
			}

			{
				constexpr const char* abpos[]{ "Left","Top","Bottom" };
				int apos = static_cast<int>( p.m_ammo_bar.position.value );
				if ( ImGui::Combo( "Ammo Bar Position", &apos, abpos, 3 ) )
					p.m_ammo_bar.position.value = static_cast<decltype(p.m_ammo_bar.position.value)>( apos );
			}

			{
				constexpr const char* wdisp[]{ "Icon","Text","Text + Icon" };
				int d = static_cast<int>( p.m_weapon.display.value );
				if ( ImGui::Combo( "Weapon Display", &d, wdisp, 3 ) )
					p.m_weapon.display.value = static_cast<decltype(p.m_weapon.display.value)>( d );
			}
			color_picker_inline( "Weapon Icon Color", "WeaponIconColorPicker", p.m_weapon.icon_color.value );
			color_picker_inline( "Player Name Color", "PlayerNameColorPicker", p.m_name.color.value );
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );
}

void menu::draw_misc( )
{
	const float half = ImGui::GetWindowSize( ).x / 2 - 25;

	ImGui::SetCursorPos( ImVec2( 20, 0 ) );
	ImGui::BeginGroup( );
	{
		if ( ImGui::CustomChild( "EXPLOITS", ImVec2( half, 340 ) ) )
		{
			ImGui::Checkbox( "No Flash",           &settings::g_misc.m_no_flash.enabled.value );
			if ( settings::g_misc.m_no_flash.enabled )
			{
				ImGui::Indent( 12.f );
				ImGui::SliderFloat( "Opacity##nf", &settings::g_misc.m_no_flash.opacity.value, 0.f, 1.f, "%.2f" );
				ImGui::Unindent( 12.f );
			}

			ImGui::Checkbox( "Radar Hack",         &settings::g_misc.m_radar.enabled.value );
			ImGui::Checkbox( "Speed ESP",          &settings::g_misc.m_speed_esp.enabled.value );

			ImGui::Checkbox( "Bullet Impacts",     &settings::g_misc.m_impacts.enabled.value );
			if ( settings::g_misc.m_impacts.enabled )
			{
				ImGui::Indent( 12.f );
				ImGui::SliderFloat( "Lifetime##imp", &settings::g_misc.m_impacts.lifetime.value, 0.5f, 10.f, "%.1fs" );
				color_picker_inline( "Impact Color", "impactColorPicker", settings::g_misc.m_impacts.color.value );
				ImGui::Unindent( 12.f );
			}

			ImGui::Checkbox( "Grenade Prediction", &settings::g_misc.m_grenades.enabled.value );
			if ( settings::g_misc.m_grenades.enabled )
			{
				ImGui::Indent( 12.f );
				ImGui::Checkbox( "Local Only##gren",   &settings::g_misc.m_grenades.local_only.value );
				color_picker_inline( "Traj Color", "grenColorPicker", settings::g_misc.m_grenades.color.value );
				ImGui::Unindent( 12.f );
			}

			ImGui::Checkbox( "Wallbang Indicator", &settings::g_misc.m_wallbang.enabled.value );
			if ( settings::g_misc.m_wallbang.enabled )
			{
				ImGui::Indent( 12.f );
				color_picker_inline( "Can Pen Color",   "wallbangColorYes", settings::g_misc.m_wallbang.color_yes.value );
				color_picker_inline( "Can't Pen Color", "wallbangColorNo",  settings::g_misc.m_wallbang.color_no.value );
				ImGui::Unindent( 12.f );
			}

			ImGui::Checkbox( "Bomb Timer",         &settings::g_misc.m_bombtimer.enabled.value );
			if ( settings::g_misc.m_bombtimer.enabled )
			{
				ImGui::Indent( 12.f );
				ImGui::SliderFloat( "Pos X##bt",   &settings::g_misc.m_bombtimer.pos_x.value,      0.f, 1800.f, "%.0f px" );
				ImGui::SliderFloat( "Pos Y%%##bt", &settings::g_misc.m_bombtimer.pos_y_frac.value, 0.f,    1.f, "%.2f"    );
				ImGui::Unindent( 12.f );
			}

			ImGui::Checkbox( "Stream Mode",        &settings::g_misc.m_stream_mode.enabled.value );
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );

	// ── Coluna direita: Radar config ────────────────────────────────────────
	ImGui::SetCursorPos( ImVec2( half + 35, 0 ) );
	ImGui::BeginGroup( );
	{
		if ( ImGui::CustomChild( "RADAR", ImVec2( half, 220 ) ) )
		{
			auto& r = settings::g_misc.m_radar;
			ImGui::Checkbox( "Enable Radar Panel", &r.enabled.value );

			if ( r.enabled )
			{
				ImGui::SliderFloat( "Pos X##rad",  &r.pos_x.value,  0.f, 1800.f, "%.0f px" );
				ImGui::SliderFloat( "Pos Y##rad",  &r.pos_y.value,  0.f, 1000.f, "%.0f px" );
				ImGui::SliderFloat( "Size##rad",   &r.size.value,    80.f, 400.f, "%.0f px" );
				ImGui::SliderFloat( "Zoom##rad",   &r.zoom.value,    50.f, 2000.f, "%.0f u"  );
				color_picker_inline( "Enemy Color", "radarEnemyColor", r.enemy_color.value );
				ImGui::Checkbox( "Show Names##rad", &r.show_names.value );
			}
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );
}

// ============================================================================
// SETTINGS (Others)
// ============================================================================
void menu::draw_settings( )
{
	const float half = ImGui::GetWindowSize( ).x / 2 - 25;

	ImGui::SetCursorPos( ImVec2( 20, 0 ) );
	ImGui::BeginGroup( );
	{
		// ── Painel esquerdo: General ─────────────────────────────────────
		if ( ImGui::CustomChild( "GENERAL", ImVec2( half, 200 ) ) )
		{
			// Tecla do menu
			ImGui::KeyBind( "Menu Key", &s_menu_key, &s_menu_key_state );

			ImGui::Spacing( );

			// Seletor de grupo de arma
			if ( ImGui::Button( "Weapon Group Selector", { -1, 22 } ) )
				ImGui::OpenPopup( "##weapon_selector_popup" );

			if ( ImGui::BeginPopup( "##weapon_selector_popup" ) )
			{
				ImGui::PushFont( InterRegular );
				ImGui::TextDisabled( "Select active weapon group" );
				ImGui::Separator( );
				for ( int i = 0; i < 6; ++i )
				{
					bool sel = ( s_weapon_group == i );
					if ( ImGui::Selectable( k_groups[ i ], sel ) )
						s_weapon_group = i;
					if ( sel ) ImGui::SetItemDefaultFocus( );
				}
				ImGui::PopFont( );
				ImGui::EndPopup( );
			}

			// Mostra grupo ativo
			ImGui::PushFont( InterRegular );
			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 53/255.f, 147/255.f, 251/255.f, 1.f ) );
			ImGui::Text( "Active: %s", k_groups[ s_weapon_group ] );
			ImGui::PopStyleColor( );
			ImGui::PopFont( );

			ImGui::Spacing( );
			ImGui::Separator( );
			ImGui::Spacing( );

			// FPS limit
			ImGui::Checkbox( "Limit FPS", &settings::g_misc.limit_fps.value );
			if ( settings::g_misc.limit_fps )
				ImGui::SliderInt( "FPS Limit", &settings::g_misc.fps_limit.value, 30, 1000 );

			ImGui::Spacing( );
			ImGui::Separator( );
			ImGui::Spacing( );

			// Exit sem console
			ImGui::PushStyleColor( ImGuiCol_Button,        ImVec4( 0.6f, 0.1f, 0.1f, 1.f ) );
			ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.8f, 0.1f, 0.1f, 1.f ) );
			ImGui::PushStyleColor( ImGuiCol_ButtonActive,  ImVec4( 1.0f, 0.2f, 0.2f, 1.f ) );
			if ( ImGui::Button( "Exit", { -1, 26 } ) )
				ExitProcess( 0 );
			ImGui::PopStyleColor( 3 );
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );

	ImGui::SetCursorPos( ImVec2( half + 35, 0 ) );
	ImGui::BeginGroup( );
	{
		// ── Painel direito: Config Manager ───────────────────────────────
		if ( ImGui::CustomChild( "CONFIG", ImVec2( half, 340 ) ) )
		{
			// Quick save/load
			static bool save_flash = false;
			static float save_t = 0;
			save_t += ImGui::GetIO( ).DeltaTime;
			if ( save_flash && save_t > 1.5f ) save_flash = false;

			const float bw = ( half - ImGui::GetStyle( ).ItemSpacing.x ) * 0.5f;
			if ( ImGui::Button( save_flash ? "Saved!" : "Save Config", { bw, 24 } ) )
				if ( config_persist::save( ) ) { save_flash = true; save_t = 0; }
			ImGui::SameLine( );
			if ( ImGui::Button( "Load Config", { bw, 24 } ) )
				config_persist::load( );

			ImGui::Spacing( );
			ImGui::Separator( );
			ImGui::Spacing( );

			static char search[ 128 ]{};
			ImGui::SetNextItemWidth( -1 );
			ImGui::InputTextWithHint( "##cfg_search", "search or new name...", search, sizeof( search ) );

			static std::vector<std::wstring> cfg_list;
			static int  cfg_sel   = -1;
			static bool need_ref  = true;
			static bool c_del     = false, c_save = false;
			static float c_t      = 0;

			if ( need_ref )
			{
				cfg_list = config::registry::list( );
				need_ref = false;
				if ( cfg_sel >= static_cast<int>( cfg_list.size( ) ) ) cfg_sel = -1;
			}

			c_t += ImGui::GetIO( ).DeltaTime;
			if ( ( c_del || c_save ) && c_t > 3.f ) c_del = c_save = false;

			auto to_n = []( const std::wstring& w ) {
				char b[ 128 ]{};
				WideCharToMultiByte( CP_UTF8, 0, w.c_str( ), -1, b, 128, nullptr, nullptr );
				return std::string( b );
			};
			auto to_w = []( const char* s ) {
				wchar_t b[ 128 ]{};
				MultiByteToWideChar( CP_UTF8, 0, s, -1, b, 128 );
				return std::wstring( b );
			};

			bool exact = false;
			if ( search[ 0 ] )
			{
				const auto ws = to_w( search );
				for ( auto& c : cfg_list )
					if ( !_wcsicmp( c.c_str( ), ws.c_str( ) ) ) { exact = true; break; }
			}

			const bool has_sel = cfg_sel >= 0 && cfg_sel < static_cast<int>( cfg_list.size( ) );
			const bool can_cr  = search[ 0 ] && !exact;
			const float bw3    = ( half - ImGui::GetStyle( ).ItemSpacing.x * 2 ) / 3.f;

			if ( can_cr )
			{
				if ( ImGui::Button( "Create", { bw3, 22 } ) )
				{
					config::registry::save( to_w( search ) );
					need_ref = true;
					search[ 0 ] = '\0';
				}
			}
			else
			{
				if ( c_save )
				{
					ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.7f, 0.55f, 0.2f, 1.f ) );
					if ( ImGui::Button( "Confirm##sv", { bw3, 22 } ) && has_sel )
					{
						config::registry::save( cfg_list[ cfg_sel ] );
						c_save = false;
					}
					ImGui::PopStyleColor( );
				}
				else
				{
					if ( ImGui::Button( "Save", { bw3, 22 } ) && has_sel )
					{ c_save = true; c_t = 0; }
				}
			}

			ImGui::SameLine( );
			if ( ImGui::Button( "Load", { bw3, 22 } ) && has_sel )
				config::registry::load( cfg_list[ cfg_sel ] );

			ImGui::SameLine( );
			if ( c_del )
			{
				ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.6f, 0.1f, 0.1f, 1.f ) );
				if ( ImGui::Button( "Confirm##dl", { bw3, 22 } ) && has_sel )
				{
					config::registry::remove( cfg_list[ cfg_sel ] );
					cfg_sel = -1; search[ 0 ] = '\0'; need_ref = true; c_del = false;
				}
				ImGui::PopStyleColor( );
			}
			else
			{
				if ( ImGui::Button( "Delete", { bw3, 22 } ) && has_sel )
				{ c_del = true; c_t = 0; }
			}

			ImGui::Spacing( );

			for ( int i = 0; i < static_cast<int>( cfg_list.size( ) ); ++i )
			{
				bool s = ( cfg_sel == i );
				if ( ImGui::Selectable( to_n( cfg_list[ i ] ).c_str( ), s, ImGuiSelectableFlags_AllowDoubleClick ) )
				{
					cfg_sel = i; c_del = c_save = false;
					if ( ImGui::IsMouseDoubleClicked( 0 ) )
						config::registry::load( cfg_list[ i ] );
				}
			}
		}
		ImGui::EndCustomChild( );
	}
	ImGui::EndGroup( );
}
