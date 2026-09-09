#pragma once
#include <LazyDlls/Lazyimporter.hpp>
#include <imgui.h>
#include <imgui_engine.h>
#include <ui/theme.h>
#include <ui/fonts.h>
#include <core/lang.h>
#include <xorstr.h>
#include <string>
#include <math.h>

namespace UI {

	inline void DrawWaiting(float width, float height, float elapsed) {
		UI::SetupStyle();
		ImGui::GetStyle().WindowRounding = 12.f;

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
		ImGui::Begin("##waiting", nullptr,
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar);

		{
			static bool dragging = false;
			static POINT dragStart{};
			static POINT winStart{};

			if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				dragging = true;
				LI_CACHED(GetCursorPos)(&dragStart);
				RECT r;
				LI_CACHED(GetWindowRect)(ImGuiEngine::GetHWND(), &r);
				winStart = { r.left, r.top };
			}

			if (dragging) {
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					POINT cur;
					LI_CACHED(GetCursorPos)(&cur);
					LI_CACHED(SetWindowPos)(ImGuiEngine::GetHWND(), HWND_TOPMOST,
						winStart.x + (cur.x - dragStart.x),
						winStart.y + (cur.y - dragStart.y),
						0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
				} else {
					dragging = false;
				}
			}
		}

		ImVec2 wp = ImGui::GetWindowPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(wp, wp + ImVec2(width, height), Colors::Background, 12.f);
		dl->AddRect(wp, wp + ImVec2(width, height), Colors::Border, 12.f);

		float cw = width;

		if (UI::LogoTexture != 0) {
			float logoSz = 28.f;
			ImGui::SetCursorPos(ImVec2(cw * 0.5f - logoSz * 0.5f, 12.f));
			ImGui::Image(UI::LogoTexture, ImVec2(logoSz, logoSz));
		}

		ImGui::PushFont(Fonts::Normal);
		ImGui::SetCursorPosY(46);
		{
			std::string waitText = L(xorstr_("Waiting for CS2..."),xorstr_("Aguardando CS2..."));
			float textW = ImGui::CalcTextSize(waitText.c_str()).x;
			ImGui::SetCursorPosX((cw - textW) / 2.0f);
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::TextSecondary), "%s", waitText.c_str());
		}
		ImGui::PopFont();

		ImGui::SetCursorPosY(76);
		float barW = cw - 40.0f;
		ImGui::SetCursorPosX((cw - barW) / 2.0f);
		float progress = (sinf(elapsed * 2.0f) + 1.0f) / 2.0f;

		ImVec2 barPos = wp + ImVec2((cw - barW) * 0.5f, 76.f);
		dl->AddRectFilled(barPos, barPos + ImVec2(barW, 4.f), Colors::WidgetBG, 2.f);
		dl->AddRectFilled(barPos, barPos + ImVec2(barW * progress, 4.f), Colors::Accent, 2.f);
		ImGui::SetCursorPosY(86);

		ImGui::End();
	}
}
