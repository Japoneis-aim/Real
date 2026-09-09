#include <Windows.h>
#include <imgui_engine.h>
#include <proc.h>
#include <console.h>
#include <nt/wrap.h>
#include <uiaccess.h>
#include <xorstr.h>
#include <cstdio>

#include <core/context.h>
#include <core/config.h>
#include <core/config_io.h>
#include <ui/render.h>
#include <pipeline/game.hpp>
#include <web/server.hpp>
#include <DiscordRPC.hpp>
#include <vmp.h>
#include <core/integrity.h>

// Driver embedded — descompactavel em runtime via Unpacker::Unpack().
// TODO: reativar quando driver novo com cert valido estiver disponivel.
// wdcore.sys atual (Zicheng Lin cert) revogado em 2013 — NtLoadDriver
// retorna STATUS_IMAGE_CERT_REVOKED (0xC0000603).
// #include <Unpacker.h>
// #include <packed_drivers/wdcore_packed.hpp>
// #include <loader/driver_loader.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	// USER32/GDI32/OPENGL32/DWMAPI saíram da IAT na migração pra LazyImporter
	// → loader não auto-carrega esses módulos. Force-load aqui ANTES de qualquer
	// LI_CACHED dessas DLLs (ShowWindow, GetConsoleWindow eh kernel32 then ok).
	// kernel32/ntdll/vmprotect ja sempre montados pelo loader.
	LI_CACHED(LoadLibraryW)(L"user32.dll");
	LI_CACHED(LoadLibraryW)(L"gdi32.dll");
	LI_CACHED(LoadLibraryW)(L"dwmapi.dll");
	LI_CACHED(LoadLibraryW)(L"opengl32.dll");

#ifdef OXYGEN_LOGS
	if (!LI_CACHED(GetConsoleWindow)()) {
		LI_CACHED(AllocConsole)();
	}
	LI_CACHED(SetConsoleTitleA)("cs2 - zimo");
	FILE* fpOut = nullptr; freopen_s(&fpOut, "CONOUT$", "w", stdout);
	FILE* fpErr = nullptr; freopen_s(&fpErr, "CONOUT$", "w", stderr);
	FILE* fpIn  = nullptr; freopen_s(&fpIn,  "CONIN$",  "r", stdin);
	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);
	HWND hCon = LI_CACHED(GetConsoleWindow)();
	if (hCon) { LI_CACHED(ShowWindow)(hCon, SW_SHOW); LI_CACHED(SetForegroundWindow)(hCon); }
	printf("[+] zimo-cs2 starting...\n");
	LI_CACHED(OutputDebugStringA)("[zimo-cs2] console block done\n");
#endif



	VMP_BEGIN_ULTRA("main");

	// Elevacao via winlogon (UI-Access). Se ja estiver UI-Access, retorna
	// imediato sem relaunch. Senao, duplica o token do winlogon, marca o
	// nosso token como UI-Access, relaunch via CreateProcessAsUser e o
	// processo original sai com ExitProcess(0). O SelfRename roda no
	// processo relaunched ja com privilegios.
	PrepareForUIAccess();

	Sys::SelfRename();
	// 1ms timer resolution pra Sleep dos threads (Nade preview/aimbot/etc)
	// realmente respeitar os ms pedidos em vez de arredondar pro tick default
	// de 15.6ms do Windows.
	Sys::SetTimerResolution1ms();
	LI_CACHED(SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	if (Config::GameSettings.fpsLimit <= 0) {
		DEVMODEW dm{};
		dm.dmSize = sizeof(dm);
		int hz = 60;
		if (LI_CACHED(EnumDisplaySettingsW)(nullptr, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 0)
			hz = static_cast<int>(dm.dmDisplayFrequency);
		Config::GameSettings.fpsLimit = hz;
		Console::Info(xorstr_("FPS limit = %d Hz"), hz);
	}

	LI_CACHED(OutputDebugStringA)("[zimo-cs2] spawning render thread\n");
	Console::Success(xorstr_("Starting..."));

	Config::CurrentTab().store(Config::Menu);
	Sys::CreateDetachedThread([](void*) { Render::Initialize(); });

	//Console::Success(xorstr_("Auth bypassed, searching for game..."));

	// Driver loader desativado — wdcore.sys atual tem cert revogado (Zicheng Lin
	// expirado 2013, na vulnerable driver blocklist). Reativar quando driver
	// novo com cert valido for empacotado em wdcore_packed.hpp.
	//{
	//	auto ds = DriverLoader::EnsureLoaded();
	//	Console::Info(xorstr_("[main] DriverLoader::EnsureLoaded -> %s"),
	//		DriverLoader::StatusName(ds));
	//}

	DiscordRPC::Tick(Config::GameSettings.discordRpc);
	Integrity::Start();

	if (Config::GameSettings.webRadar) {
		auto port = Config::GameSettings.webRadarPort;
		Sys::CreateDetachedThread([](void* arg) { Web::Start(static_cast<int>(reinterpret_cast<intptr_t>(arg))); },
			reinterpret_cast<void*>(static_cast<intptr_t>(port)));
		Console::Info(xorstr_("Web radar: http://127.0.0.1:%d"), Config::GameSettings.webRadarPort);
	}

	Window::Watcher watcher(xorstr_("Counter-Strike 2"), xorstr_("SDL_app"));

	watcher.OnFound([](HWND handle) {
		auto pid = Window::GetPID(handle);

		Context::Memory->Attach(pid);
		Sys::CreateDetachedThread([](void*) { Game::Initialize(); });

		Console::Success(xorstr_("Game found - PID %lu"), pid);
		Config::GameWindow().store(handle);
	});

	watcher.OnLost([]() {
		Context::Memory->Detach();
		Context::ClientDLL.store(0);

		// Limpa o estado visivel pra ESP/Radar nao desenharem fantasmas com
		// as ultimas posicoes lidas ate o data thread perceber que perdeu.
		{
			std::lock_guard<std::mutex> lock(Context::EntitiesMutex);
			Context::Entities.clear();
		}
		Context::LocalPlayerPawn.store(0);
		Context::LocalHealth.store(0);
		Context::AimTarget.valid.store(false);

		Console::Warn(xorstr_("Game closed, switching to global overlay"));
		Config::GameWindow().store(nullptr);
	});

	watcher.StartWatching();
	VMP_END;

	while (!Config::ShutdownRequested().load()) {
		watcher.Poll();
		// Re-aplica o estado a cada loop — checkbox em Misc liga/desliga.
		DiscordRPC::Tick(Config::GameSettings.discordRpc);
		Sys::Sleep(500);
	}

	Integrity::g_alive.store(false);
	DiscordRPC::Shutdown();
	Context::Memory->Detach();
	return 0;
}
