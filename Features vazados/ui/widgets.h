#pragma once
#include <imgui.h>
#include <Windows.h>

namespace UI {

	inline const char* KeyName(int vk) {
		switch (vk) {
		case VK_LBUTTON: return "LMB";
		case VK_RBUTTON: return "RMB";
		case VK_MBUTTON: return "MMB";
		case VK_XBUTTON1: return "MOUSE4";
		case VK_XBUTTON2: return "MOUSE5";
		case VK_INSERT: return "INSERT";
		case VK_DELETE: return "DELETE";
		case VK_HOME: return "HOME";
		case VK_END: return "END";
		case VK_PRIOR: return "PGUP";
		case VK_NEXT: return "PGDN";
		case VK_SHIFT: return "SHIFT";
		case VK_CONTROL: return "CTRL";
		case VK_MENU: return "ALT";
		case VK_CAPITAL: return "CAPS";
		case VK_TAB: return "TAB";
		case VK_SPACE: return "SPACE";
		case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3";
		case VK_F4: return "F4"; case VK_F5: return "F5"; case VK_F6: return "F6";
		default:
			if (vk >= 0x30 && vk <= 0x39) { static char buf[2]; buf[0] = (char)vk; buf[1] = 0; return buf; }
			if (vk >= 0x41 && vk <= 0x5A) { static char buf[2]; buf[0] = (char)vk; buf[1] = 0; return buf; }
			return "???";
		}
	}

	inline bool KeyBind(const char* label, int& key) {
		char buf[64];
		snprintf(buf, sizeof(buf), "[%s]##%s", KeyName(key), label);

		bool changed = false;
		ImGui::SameLine();
		if (ImGui::SmallButton(buf)) {
			ImGui::OpenPopup(label);
		}

		if (ImGui::BeginPopup(label)) {
			ImGui::Text("Press any key...");

			for (int vk = 1; vk < 256; vk++) {
				if (vk == VK_ESCAPE) continue;
				if (LI_CACHED(GetAsyncKeyState)(vk) & 1) {
					key = vk;
					changed = true;
					ImGui::CloseCurrentPopup();
					break;
				}
			}
			ImGui::EndPopup();
		}
		return changed;
	}

}
