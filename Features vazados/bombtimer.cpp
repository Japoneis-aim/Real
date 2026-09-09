#include <features/bombtimer.hpp>

namespace Features::BombTimer {
	using namespace Context;

	void Tick() {
		if (ClientDLL.load() == 0 || !Memory->IsAttached()) return;

		uintptr_t base = ClientDLL.load();
		uintptr_t tempC4 = Memory->Read<uintptr_t>(base + Offsets.dwPlantedC4);
		uintptr_t plantedC4 = Memory->Read<uintptr_t>(tempC4);
		bool planted = Memory->Read<bool>(base + Offsets.dwPlantedC4 - 0x8);

		if (!planted || !plantedC4) {
			g_bomb = {};
			return;
		}

		uintptr_t globalVars = Memory->Read<uintptr_t>(base + Offsets.dwGlobalVars);
		if (!globalVars) return;

		float currentTime = Memory->Read<float>(globalVars + 0x30);

		bool defused = Memory->Read<bool>(plantedC4 + Schema.m_bBombDefused);
		if (defused) {
			g_bomb = {};
			g_bomb.defused = true;
			return;
		}

		float c4Blow = Memory->Read<float>(plantedC4 + Schema.m_flC4Blow);
		float defuseCountDown = Memory->Read<float>(plantedC4 + Schema.m_flDefuseCountDown);
		bool defusing = Memory->Read<bool>(plantedC4 + Schema.m_bBeingDefused);
		int site = Memory->Read<int>(plantedC4 + Schema.m_nBombSite);

		g_bomb.planted = true;
		g_bomb.defused = false;
		g_bomb.defusing = defusing;
		g_bomb.site = site;
		g_bomb.timeLeft = (std::max)(c4Blow - currentTime, 0.0f);
		g_bomb.defuseLeft = defusing ? (std::max)(defuseCountDown - currentTime, 0.0f) : 0.0f;
	}

	void Draw() {
		if (!Config::GameSettings.bombTimer) return;
		if (!g_bomb.planted && !g_bomb.defused) return;

		auto dl = ImGui::GetBackgroundDrawList();
		float x = 12.0f;
		float y = static_cast<float>(ScreenHeight) * 0.35f;

		float panelW = 220.0f;
		float panelH = g_bomb.defusing ? 105.0f : 75.0f;
		if (g_bomb.defused) panelH = 45.0f;

		dl->AddRectFilled({ x - 4, y - 4 }, { x + panelW, y + panelH },
			IM_COL32(10, 10, 15, 200), 6.0f);
		dl->AddRect({ x - 4, y - 4 }, { x + panelW, y + panelH },
			IM_COL32(60, 60, 80, 150), 6.0f);

		if (g_bomb.defused) {
			dl->AddText(ImGui::GetFont(), 18.0f, { x + 4, y + 8 },
				IM_COL32(80, 255, 80, 255), "BOMB DEFUSED");
			return;
		}

		const char* siteStr = g_bomb.site == 1 ? "B" : "A";
		ImU32 timeCol;
		if (g_bomb.timeLeft < 5.0f)
			timeCol = IM_COL32(255, 50, 50, 255);
		else if (g_bomb.timeLeft < 15.0f)
			timeCol = IM_COL32(255, 200, 50, 255);
		else
			timeCol = IM_COL32(230, 230, 230, 255);

		char buf[64];

		snprintf(buf, sizeof(buf), "BOMB  [%s]", siteStr);
		dl->AddText(ImGui::GetFont(), 16.0f, { x + 4, y + 2 }, Config::GameSettings.colBomb.ToImU32(), buf);

		float barY = y + 22.0f;
		float barW = panelW - 12.0f;
		float barH = 8.0f;
		float fill = g_bomb.timeLeft / 40.0f;
		if (fill > 1.0f) fill = 1.0f;

		dl->AddRectFilled({ x + 4, barY }, { x + 4 + barW, barY + barH },
			IM_COL32(30, 30, 40, 200), 3.0f);
		dl->AddRectFilled({ x + 4, barY }, { x + 4 + barW * fill, barY + barH },
			timeCol, 3.0f);

		snprintf(buf, sizeof(buf), "%.1fs", g_bomb.timeLeft);
		ImVec2 sz = ImGui::CalcTextSize(buf);
		dl->AddText({ x + 4 + barW - sz.x, barY + barH + 2 }, timeCol, buf);

		if (g_bomb.defusing) {
			float defY = barY + barH + 20.0f;
			dl->AddText(ImGui::GetFont(), 13.0f, { x + 4, defY },
				IM_COL32(100, 180, 255, 255), "DEFUSING");

			float defBarY = defY + 16.0f;
			float defFill = g_bomb.defuseLeft / 10.0f;
			if (defFill > 1.0f) defFill = 1.0f;

			bool canDefuse = g_bomb.defuseLeft <= g_bomb.timeLeft;
			ImU32 defCol = canDefuse ? IM_COL32(80, 200, 255, 255) : IM_COL32(255, 80, 80, 255);

			dl->AddRectFilled({ x + 4, defBarY }, { x + 4 + barW, defBarY + 6.0f },
				IM_COL32(30, 30, 40, 200), 2.0f);
			dl->AddRectFilled({ x + 4, defBarY }, { x + 4 + barW * defFill, defBarY + 6.0f },
				defCol, 2.0f);

			snprintf(buf, sizeof(buf), "%.1fs %s", g_bomb.defuseLeft,
				canDefuse ? "" : "(NO TIME)");
			dl->AddText({ x + 4, defBarY + 8 }, defCol, buf);
		}
	}

	void Work() {
		while (true) {
			Sys::Sleep(50);
			if (ClientDLL.load() == 0) return;
			Tick();
		}
	}
}
