#include <pipeline/game.hpp>
#include <vmp.h>
#include <console.h>
#include <xorstr.h>
#include <features/nade.h>

namespace Game {
	using namespace Context;

	void Initialize() {
		VMP_BEGIN("game_init");
		ClientDLL.store(0);
		Console::Info("[game] Initialize() — waiting for client.dll");

		int waitTicks = 0;
		while (ClientDLL.load() == 0) {
			if (!Memory->IsAttached()) {
				if (++waitTicks % 10 == 0)
					Console::Warn("[game] Memory not attached (tick %d)", waitTicks);
				Sys::Sleep(500);
				continue;
			}

			auto client = Memory->GetModule(std::string(xorstr_("client.dll").c_str()));
			if (client.base != 0) {
				Console::Success("[game] client.dll @ 0x%llX size=0x%zX", client.base, client.size);
				ClientDLL.store(client.base);

				Console::Info("[game] Dumper::Dump() ...");
				bool ok = Dumper::Dump();
				Console::Info("[game] Dumper::Dump() returned %s", ok ? "OK" : "FAILED");

				Console::Info("[game] Features::Start() ...");
				Features::Start();
				Console::Success("[game] Features started");

				Console::Info("[game] Spawning Data/VisCheck/Physics/Nade threads...");
				Sys::CreateDetachedThread([](void*) { Data::Work(); });
				Sys::CreateDetachedThread([](void*) { Data::VisCheckWork(); });
				Sys::CreateDetachedThread([](void*) { PhysicsLoader::Work(); });
				Sys::CreateDetachedThread([](void*) { Features::Nade::Work(); });
				Console::Success("[game] Game::Initialize() complete");
				break;
			}

			if (++waitTicks % 10 == 0) {
				auto mods = Memory->GetModules();
				Console::Warn("[game] client.dll not found, %zu modules visible (tick %d)", mods.size(), waitTicks);
				if (mods.empty()) {
					Console::Error("[game]   GetModules() returned EMPTY — PEB read may be blocked");
				} else if (waitTicks <= 30) {
					int matches = 0;
					Console::Info("[game]   Modules matching 'client'/'cs'/'csgo'/'.dll' (case-insensitive substring):");
					for (size_t i = 0; i < mods.size(); i++) {
						std::string lower = mods[i].name;
						for (auto& c : lower) if (c >= 'A' && c <= 'Z') c |= 0x20;
						if (lower.find("client") != std::string::npos ||
							lower.find("csgo") != std::string::npos ||
							lower.find("cs2") != std::string::npos) {
							Console::Info("[game]     [%zu] '%s' @ 0x%llX (size=0x%zX)",
								i, mods[i].name.c_str(), mods[i].base, mods[i].size);
							matches++;
						}
					}
					Console::Info("[game]   %d matches found of %zu total modules", matches, mods.size());

					if (waitTicks == 30) {
						Console::Info("[game]   FULL DUMP of all %zu modules:", mods.size());
						for (size_t i = 0; i < mods.size(); i++)
							Console::Info("[game]     [%zu] %s @ 0x%llX (size=0x%zX)",
								i, mods[i].name.c_str(), mods[i].base, mods[i].size);
					}
				}
			}
			Sys::Sleep(500);
		}
		VMP_END;
	}
}
