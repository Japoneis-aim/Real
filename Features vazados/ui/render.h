#pragma once
#include <LazyDlls/Lazyimporter.hpp>
#include <Windows.h>
#include <cmath>
#include <imgui_engine.h>
#include "menu.h"
#include "login.h"
#include <core/config.h>
#include <core/context.h>
#include <core/lang.h>
#include <core/avatar.h>
#include <features/esp.hpp>
#include <ui/fonts.h>
#include <features/bombtimer.hpp>
#include <features/nade.h>
#include <ui/toast.h>
#include <DiscordRPC.hpp>
#include <core/integrity.h>
#include <xorstr.h>

// Forward decls do auth session (definidas em login_auth.cpp). Render chama
// pra checar expiry todo segundo e fazer cleanup no shutdown.
namespace UI::Login {
	void WatchdogTick();
	void StopSession();
}

namespace Render {

	static DWORD g_startTick = 0;
	static Config::Tab g_lastTab = (Config::Tab)-1;
	static RECT g_lastGameRect{};
	static HWND g_lastGameHwnd = nullptr;

	static void DrawConnectionStatus() {
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		ImVec2 sz = ImGui::GetIO().DisplaySize;

		bool connected = (Config::GameWindow().load() != nullptr);

		std::string label;
		if (connected)
			label = L(xorstr_("Connected to CS2").c_str(), xorstr_("Conectado ao CS2").c_str());
		else
			label = L(xorstr_("Waiting for CS2...").c_str(), xorstr_("Aguardando CS2...").c_str());

		ImFont* font = UI::Fonts::Small;
		float fontSize = font ? font->FontSize : 12.f;
		ImVec2 textSz = font
			? font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, label.c_str())
			: ImGui::CalcTextSize(label.c_str());

		float circleR = 6.f;
		float gap = 8.f;
		float padding = 16.f;
		float totalW = circleR * 2 + gap + textSz.x;

		float x = sz.x - padding - totalW;
		float y = sz.y - padding - textSz.y;

		float t = (float)ImGui::GetTime();
		float pulse = 0.5f + 0.5f * sinf(t * 4.f);

		ImU32 circleCol, glowCol, textCol;
		if (connected) {
			circleCol = IM_COL32(60, 220, 100, 255);
			glowCol   = IM_COL32(60, 220, 100, 80);
			textCol   = IM_COL32(220, 220, 225, 235);
		} else {
			int alpha = (int)(120.f + 135.f * pulse);
			circleCol = IM_COL32(255, 165, 50, alpha);
			glowCol   = IM_COL32(255, 165, 50, (int)(60.f * pulse));
			textCol   = IM_COL32(220, 220, 225, 220);
		}

		ImVec2 cc(x + circleR, y + textSz.y * 0.5f);
		dl->AddCircleFilled(cc, circleR + 3.f, glowCol, 16);
		dl->AddCircleFilled(cc, circleR, circleCol, 16);
		dl->AddCircle(cc, circleR, IM_COL32(0, 0, 0, 120), 16, 1.f);

		if (font)
			dl->AddText(font, fontSize, ImVec2(x + circleR * 2 + gap, y), textCol, label.c_str());
		else
			dl->AddText(ImVec2(x + circleR * 2 + gap, y), textCol, label.c_str());
	}

	static void OnFrame() {
		if (Config::ShutdownRequested().load()) {
			::UI::Login::StopSession();
			DiscordRPC::Shutdown();
			LI_CACHED(PostQuitMessage)(0);
			return;
		}
		Integrity::TickMain();
		Avatar::TickUpload();
		static int frameCounter = 0;
		if (++frameCounter % 300 == 0) Integrity::CheckCrcAlive();
		// Verifica expiracao da licenca a cada segundo (~165 frames a 165Hz).
		if (frameCounter % 165 == 0) ::UI::Login::WatchdogTick();

		auto size = ImGuiEngine::GetSize();
		auto tab = Config::CurrentTab().load();

		if (tab != g_lastTab) {
			if (tab == Config::Login) {
				int sw = LI_CACHED(GetSystemMetrics)(SM_CXSCREEN);
				int sh = LI_CACHED(GetSystemMetrics)(SM_CYSCREEN);
				ImGuiEngine::SetWindowRect((sw - 400) / 2, (sh - 380) / 2, 400, 380);
				ImGuiEngine::SetInteractive(true, true);
			} else {
				ImGuiEngine::SetFullscreen();
				ImGuiEngine::SetInteractive(true);
				g_lastGameHwnd = nullptr;
				g_lastGameRect = RECT{};
			}
			g_lastTab = tab;
		}

		// IIFE: 'return' de dentro pula o body de render mas NAO o Sleep
		// abaixo. Antes os 'return' de "menu fechado" tambem skipavam o
		// Sleep → frame loop rodava sem cap → ESP/Toast/Nade desenhavam a
		// centenas de FPS enquanto o menu (apenas) ficava a fpsLimit.
		[&]() {
			if (tab == Config::Login) {
				if (LI_CACHED(GetAsyncKeyState)(Config::MenuKey) & 1) {
					bool vis = !Config::MenuVisible().load();
					Config::MenuVisible().store(vis);
					ImGuiEngine::SetInteractive(vis, vis);
				}
				if (!Config::MenuVisible().load()) return;
				UI::Login::Draw(size.x, size.y);
			} else {
				HWND game = Config::GameWindow().load();
				if (game) {
					RECT r;
					LI_CACHED(GetWindowRect)(game, &r);
					if (r.left != g_lastGameRect.left || r.top != g_lastGameRect.top ||
						r.right != g_lastGameRect.right || r.bottom != g_lastGameRect.bottom) {
						ImGuiEngine::SetWindowRect(r.left, r.top,
							r.right - r.left, r.bottom - r.top);
						g_lastGameRect = r;
					}
					Context::ScreenWidth = r.right - r.left;
					Context::ScreenHeight = r.bottom - r.top;
					g_lastGameHwnd = game;

					Features::ESP::Draw();
					Features::BombTimer::Draw();
					Features::Nade::Draw();
				} else {
					if (g_lastGameHwnd != nullptr) {
						ImGuiEngine::SetFullscreen();
						g_lastGameHwnd = nullptr;
						g_lastGameRect = RECT{};
					}
					Context::ScreenWidth = (int)size.x;
					Context::ScreenHeight = (int)size.y;
				}

				if (LI_CACHED(GetAsyncKeyState)(Config::MenuKey) & 1) {
					bool vis = !Config::MenuVisible().load();
					Config::MenuVisible().store(vis);
					ImGuiEngine::SetInteractive(vis, !game);
					if (vis) LI_CACHED(ClipCursor)(nullptr);
				}
				// Toast renderiza independente do menu — notificacoes precisam
				// aparecer mesmo com o menu fechado (kill feed, status, etc).
				UI::Toast::Render();
				if (!Config::MenuVisible().load()) return;
				UI::DrawMenu();
			}
		}();

		// FPS cap — roda SEMPRE no fim do frame, sincroniza tudo (menu+ESP+
		// Toast+Nade+BombTimer) com o slider unico de "Menu FPS Limit".
		int fps = Config::GameSettings.fpsLimit;
		if (fps <= 0) {
			static int cachedHz = 0;
			if (cachedHz <= 0) {
				DEVMODEW dm{};
				dm.dmSize = sizeof(dm);
				if (LI_CACHED(EnumDisplaySettingsW)(nullptr, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 0)
					cachedHz = static_cast<int>(dm.dmDisplayFrequency);
				else
					cachedHz = 60;
			}
			fps = cachedHz;
		}
		Sys::Sleep(1000 / fps);
	}

	static void Initialize() {
		Console::Info("[render] Initialize() start");
		g_startTick = Sys::TickCount();

		ImGuiEngine::Config cfg;
		cfg.backend = ImGuiEngine::Backend::OpenGL3;

		Console::Info("[render] ImGuiEngine::Init() ...");
		if (!ImGuiEngine::Init(cfg)) {
			Console::Error("[render] ImGuiEngine::Init() FAILED — exiting");
			exit(0);
			return;
		}
		Console::Success("[render] ImGuiEngine::Init() OK, hwnd=0x%p", (void*)ImGuiEngine::GetHWND());

		Console::Info("[render] UI::LoadFonts() ...");
		UI::LoadFonts();
		ImGui::GetIO().Fonts->Build();
		Console::Success("[render] Fonts built (Normal=%p Big=%p Small=%p Icons=%p)",
			(void*)UI::Fonts::Normal, (void*)UI::Fonts::Big,
			(void*)UI::Fonts::Small, (void*)UI::Fonts::Icons);

		UI::LoadLogo();
		Console::Info("[render] LoadLogo done, calling SetInteractive(true) + Run...");

		ImGuiEngine::SetInteractive(true);
		ImGuiEngine::Run(Render::OnFrame);
		Console::Info("[render] Run() returned — Shutdown");
		ImGuiEngine::Shutdown();
	}
}
