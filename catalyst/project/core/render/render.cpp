#include <stdafx.hpp>

// Variáveis globais de fonte exigidas pelo imgui.cpp/imgui_widgets.cpp customizados
ImFont* InterBlack      = nullptr;
ImFont* InterBold       = nullptr;
ImFont* InterBold12     = nullptr;
ImFont* InterExtraBold  = nullptr;
ImFont* InterExtraLight = nullptr;
ImFont* InterLight      = nullptr;
ImFont* InterMedium     = nullptr;
ImFont* InterRegular    = nullptr;
ImFont* InterSemiBold   = nullptr;
ImFont* InterThin       = nullptr;
ImFont* FontAwesomeRegular  = nullptr;
ImFont* FontAwesomeSolid    = nullptr;
ImFont* FontAwesomeSolidBig = nullptr;

bool render::initialize( )
{
	if ( !this->register_window_class( ) )
		return false;

	const auto screen_w = ::GetSystemMetrics( SM_CXSCREEN );
	const auto screen_h = ::GetSystemMetrics( SM_CYSCREEN );

	this->m_hwnd = ::CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
		k_class_name, k_class_name, WS_POPUP,
		0, 0, screen_w, screen_h,
		nullptr, nullptr, ::GetModuleHandleW( nullptr ), nullptr
	);

	if ( !this->m_hwnd )
		return false;

	constexpr MARGINS margins{ -1, -1, -1, -1 };
	::DwmExtendFrameIntoClientArea( this->m_hwnd, &margins );
	::SetLayeredWindowAttributes( this->m_hwnd, 0, 255, LWA_ALPHA );

	if ( !this->setup_d3d( ) )
		return false;

	if ( !this->setup_imgui( ) )
		return false;

	::ShowWindow( this->m_hwnd, SW_SHOW );
	::UpdateWindow( this->m_hwnd );

	g::console.print( "render initialized." );

	this->run( );

	return true;
}

bool render::setup_imgui( )
{
	IMGUI_CHECKVERSION( );
	ImGui::CreateContext( );

	ImGuiIO& io = ImGui::GetIO( );
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.IniFilename = nullptr; // não salva imgui.ini

	// Inicializa backends
	if ( !ImGui_ImplWin32_Init( this->m_hwnd ) )
		return false;

	if ( !ImGui_ImplDX11_Init( this->m_device, this->m_context ) )
		return false;

	// Carrega fontes Inter — dados comprimidos do fonts.hpp
	ImFontConfig cfg{};
	cfg.FontDataOwnedByAtlas = false;

	// Inter Regular 14px — texto padrão do menu
	InterRegular = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterRegular_compressed_data, static_cast<int>( InterRegular_compressed_size ), 14.0f, &cfg
	);
	this->m_fonts.imgui_inter_regular = InterRegular;

	// Inter Medium 14px
	InterMedium = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterMedium_compressed_data, static_cast<int>( InterMedium_compressed_size ), 14.0f, &cfg
	);
	this->m_fonts.imgui_inter_medium = InterMedium;

	// Inter Bold 15px
	InterBold = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterBold_compressed_data, static_cast<int>( InterBold_compressed_size ), 15.0f, &cfg
	);
	this->m_fonts.imgui_inter_bold = InterBold;

	// Inter Bold 12px (exigido pelo imgui customizado)
	InterBold12 = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterBold_compressed_data, static_cast<int>( InterBold_compressed_size ), 12.0f, &cfg
	);

	// Demais variantes Inter (exigidas pelo linker)
	InterBlack = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterBlack_compressed_data, static_cast<int>( InterBlack_compressed_size ), 14.0f, &cfg
	);
	InterExtraBold = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterExtraBold_compressed_data, static_cast<int>( InterExtraBold_compressed_size ), 14.0f, &cfg
	);
	InterExtraLight = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterExtraLight_compressed_data, static_cast<int>( InterExtraLight_compressed_size ), 14.0f, &cfg
	);
	InterLight = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterLight_compressed_data, static_cast<int>( InterLight_compressed_size ), 14.0f, &cfg
	);
	InterSemiBold = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterSemiBold_compressed_data, static_cast<int>( InterSemiBold_compressed_size ), 14.0f, &cfg
	);
	InterThin = io.Fonts->AddFontFromMemoryCompressedTTF(
		InterThin_compressed_data, static_cast<int>( InterThin_compressed_size ), 14.0f, &cfg
	);

	// FontAwesome 6 Solid 13px — ícones das abas
	static const ImWchar fa_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig fa_cfg{};
	fa_cfg.FontDataOwnedByAtlas = false;
	fa_cfg.GlyphMinAdvanceX = 13.0f;
	FontAwesomeSolid = io.Fonts->AddFontFromMemoryCompressedTTF(
		FontAwesome6Solid_compressed_data, static_cast<int>( FontAwesome6Solid_compressed_size ), 13.0f, &fa_cfg, fa_ranges
	);
	this->m_fonts.imgui_fa_solid = FontAwesomeSolid;

	// FontAwesome big (18px) — exigido pelo linker
	FontAwesomeSolidBig = io.Fonts->AddFontFromMemoryCompressedTTF(
		FontAwesome6Solid_compressed_data, static_cast<int>( FontAwesome6Solid_compressed_size ), 18.0f, &fa_cfg, fa_ranges
	);

	// FontAwesome Regular — usar Solid como fallback
	FontAwesomeRegular = FontAwesomeSolid;

	io.Fonts->Build( );
	ImGui_ImplDX11_CreateDeviceObjects( );

	// Aplica o estilo do menu uma única vez aqui — após o contexto ImGui estar pronto.
	// Isso garante que as cores/parâmetros persistam mesmo se o contexto for recriado.
	g::menu.on_init( );

	return true;
}

void render::shutdown_imgui( )
{
	ImGui_ImplDX11_Shutdown( );
	ImGui_ImplWin32_Shutdown( );
	ImGui::DestroyContext( );
}

bool render::register_window_class( )
{
	WNDCLASSEXW wc{};
	if ( ::GetClassInfoExW( ::GetModuleHandleW( nullptr ), k_class_name, &wc ) )
		return true;

	wc.cbSize        = sizeof( WNDCLASSEXW );
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = wnd_proc;
	wc.hInstance     = ::GetModuleHandleW( nullptr );
	wc.hbrBackground = nullptr;
	wc.hCursor       = ::LoadCursorW( nullptr, IDC_ARROW );
	wc.lpszClassName = k_class_name;

	this->m_atom = ::RegisterClassExW( &wc );
	return this->m_atom != 0;
}

void render::run( )
{
	constexpr float clear[ 4 ]{ 0.0f, 0.0f, 0.0f, 0.0f };
	MSG msg{};

	while ( true )
	{
		while ( ::PeekMessageW( &msg, nullptr, 0, 0, PM_REMOVE ) )
		{
			if ( msg.message == WM_QUIT )
			{
				this->shutdown_imgui( );
				return;
			}

			::TranslateMessage( &msg );
			::DispatchMessageW( &msg );
		}

		this->update_input_window( );

		this->m_context->OMSetRenderTargets( 1, &this->m_rtv, nullptr );
		this->m_context->ClearRenderTargetView( this->m_rtv, clear );

		// ── zdraw: overlay (ESP, hitboxes, etc.) ──────────────────────────
		zdraw::begin_frame( );
		{
			core::render::g_stream_mode.toggle( static_cast<bool>( settings::g_misc.m_stream_mode.enabled ) );

			auto& draw_list = zdraw::get_draw_list( zdraw::draw_layer::background );

			if ( systems::g_local.valid( ) )
			{
				systems::g_view.update( );
				features::esp::g_player.on_render( draw_list );
				features::esp::g_item.on_render( draw_list );
				features::esp::g_projectile.on_render( draw_list );
				features::misc::g_grenades.on_render( draw_list );
				features::combat::g_legit.on_render( draw_list );
				features::misc::g_impacts.on_render( draw_list );
				features::misc::g_wallbang.on_render( draw_list );
				features::misc::g_bomb_timer.on_render( draw_list );
				features::misc::g_speed_esp.on_render( draw_list );
				features::misc::g_radar.on_render( draw_list );
			}
		}
		zdraw::end_frame( );

		// ── ImGui: menu ───────────────────────────────────────────────────
		ImGui_ImplDX11_NewFrame( );
		ImGui_ImplWin32_NewFrame( );
		ImGui::NewFrame( );

		g::menu.draw( );

		ImGui::Render( );
		ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );

		const auto hr_present = this->m_swap_chain->Present( 0, 0 );
		if ( FAILED( hr_present ) )
		{
			g::console.print( "render: Present failed (0x{:08X}) — shutting down.", static_cast<unsigned>( hr_present ) );
			break;
		}

		{
			constexpr std::uint32_t k_uncapped_floor_fps = 2000u;
			const auto target = settings::g_misc.limit_fps
				? static_cast<std::uint32_t>( settings::g_misc.fps_limit )
				: k_uncapped_floor_fps;
			this->m_fps_limiter.set_target( target );
			this->m_fps_limiter.limit( );
		}
	}

	this->shutdown_imgui( );
	threads::shutdown( );
	::PostQuitMessage( 0 );
}

void render::update_input_window( )
{
	const auto open = g::menu.is_open( );
	const auto style = ::GetWindowLongW( this->m_hwnd, GWL_EXSTYLE );

	if ( open )
		::SetWindowLongW( this->m_hwnd, GWL_EXSTYLE, ( style & ~WS_EX_TRANSPARENT ) | WS_EX_LAYERED );
	else
		::SetWindowLongW( this->m_hwnd, GWL_EXSTYLE, style | WS_EX_TRANSPARENT | WS_EX_LAYERED );

	if ( open && !this->m_was_open )
	{
		this->m_prev_foreground = ::GetForegroundWindow( );

		const auto fg_thread = ::GetWindowThreadProcessId( this->m_prev_foreground, nullptr );
		const auto my_thread = ::GetCurrentThreadId( );

		if ( fg_thread && fg_thread != my_thread )
		{
			::AttachThreadInput( my_thread, fg_thread, TRUE );
			::SetForegroundWindow( this->m_hwnd );
			::AttachThreadInput( my_thread, fg_thread, FALSE );
		}
		else
		{
			::SetForegroundWindow( this->m_hwnd );
		}
	}
	else if ( !open && this->m_was_open )
	{
		if ( this->m_prev_foreground )
		{
			const auto my_thread = ::GetCurrentThreadId( );
			const auto prev_thread = ::GetWindowThreadProcessId( this->m_prev_foreground, nullptr );

			if ( prev_thread && prev_thread != my_thread )
			{
				::AttachThreadInput( my_thread, prev_thread, TRUE );
				::SetForegroundWindow( this->m_prev_foreground );
				::AttachThreadInput( my_thread, prev_thread, FALSE );
			}
			else
			{
				::SetForegroundWindow( this->m_prev_foreground );
			}

			this->m_prev_foreground = nullptr;
		}
	}

	this->m_was_open = open;
}

bool render::setup_d3d( )
{
	DXGI_SWAP_CHAIN_DESC desc{};
	desc.BufferCount          = 1;
	desc.BufferDesc.Format    = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow         = this->m_hwnd;
	desc.SampleDesc.Count     = 1;
	desc.Windowed             = TRUE;
	desc.SwapEffect           = DXGI_SWAP_EFFECT_DISCARD;
	desc.Flags                = 0;

	D3D_FEATURE_LEVEL levels[]{ D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL selected{};

	if ( FAILED( D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
		levels, 2, D3D11_SDK_VERSION,
		&desc, &this->m_swap_chain, &this->m_device, &selected, &this->m_context
	) ) )
	{
		return false;
	}

	ID3D11Texture2D* back_buffer{};
	if ( FAILED( this->m_swap_chain->GetBuffer( 0, IID_PPV_ARGS( &back_buffer ) ) ) )
		return false;

	const auto hr = this->m_device->CreateRenderTargetView( back_buffer, nullptr, &this->m_rtv );

	D3D11_TEXTURE2D_DESC bb_desc{};
	back_buffer->GetDesc( &bb_desc );
	back_buffer->Release( );

	if ( FAILED( hr ) )
		return false;

	D3D11_VIEWPORT vp{};
	vp.Width    = static_cast<float>( bb_desc.Width );
	vp.Height   = static_cast<float>( bb_desc.Height );
	vp.MaxDepth = 1.0f;
	this->m_context->RSSetViewports( 1, &vp );

	if ( !zdraw::initialize( this->m_device, this->m_context ) )
		return false;

	zui::initialize( this->m_hwnd );

	{
		this->m_fonts.mochi_12   = zdraw::add_font_from_memory( std::span( reinterpret_cast<const std::byte*>( resources::fonts::mochi ),   sizeof( resources::fonts::mochi ) ),   12.0f, 512, 512 );
		this->m_fonts.pretzel_12 = zdraw::add_font_from_memory( std::span( reinterpret_cast<const std::byte*>( resources::fonts::pretzel ), sizeof( resources::fonts::pretzel ) ), 12.0f, 512, 512 );
		this->m_fonts.pixel7_10  = zdraw::add_font_from_memory( std::span( reinterpret_cast<const std::byte*>( resources::fonts::pixel7 ),  sizeof( resources::fonts::pixel7 ) ),  10.0f, 512, 512 );
	}

	return true;
}

// Forward declare para o ImGui processar mensagens do Win32
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

LRESULT CALLBACK render::wnd_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	// ImGui processa input quando o menu está aberto
	if ( g::menu.is_open( ) )
	{
		if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wp, lp ) )
			return true;
	}

	zui::process_wndproc_message( msg, wp, lp );

	if ( msg == WM_DESTROY )
	{
		::PostQuitMessage( 0 );
		return 0;
	}

	return ::DefWindowProcW( hwnd, msg, wp, lp );
}
