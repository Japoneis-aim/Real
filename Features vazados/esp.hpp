#pragma once
#include <imgui.h>
#include <math/w2s.h>
#include <core/context.h>
#include <core/config.h>

namespace Features::ESP {
	using namespace Context;

	inline const char* WeaponName(uint16_t id) {
		switch (id) {
		case 1: return "DEAGLE"; case 2: return "ELITE"; case 3: return "FIVESEVEN";
		case 4: return "GLOCK"; case 7: return "AK47"; case 8: return "AUG";
		case 9: return "AWP"; case 10: return "FAMAS"; case 11: return "G3SG1";
		case 13: return "GALIL"; case 14: return "M249"; case 16: return "M4A4";
		case 17: return "MAC10"; case 19: return "P90"; case 23: return "MP5";
		case 24: return "UMP45"; case 25: return "XM1014"; case 26: return "BIZON";
		case 27: return "MAG7"; case 28: return "NEGEV"; case 29: return "SAWEDOFF";
		case 30: return "TEC9"; case 31: return "TASER"; case 32: return "HKP2000";
		case 33: return "MP7"; case 34: return "MP9"; case 35: return "NOVA";
		case 36: return "P250"; case 38: return "SCAR20"; case 39: return "SG556";
		case 40: return "SSG08"; case 60: return "M4A1-S"; case 61: return "USP-S";
		case 63: return "CZ75"; case 64: return "REVOLVER";
		case 43: return "FLASH"; case 44: return "HE"; case 45: return "SMOKE";
		case 46: return "MOLOTOV"; case 47: return "DECOY"; case 48: return "INC";
		case 49: return "C4";
		default: return nullptr;
		}
	}

	struct ScreenBox {
		bool ok = false;
		Vec2 mn, mx;
	};

	ScreenBox EntityBox(const Entity& e);
	inline void DrawBox(ImDrawList* dl, const ScreenBox& b, ImU32 col) {
		dl->AddRect({ b.mn.x, b.mn.y }, { b.mx.x, b.mx.y }, col, 0.0f, 0, 1.4f);
	}
	void DrawHealth(ImDrawList* dl, const ScreenBox& b, int hp);
	void DrawName(ImDrawList* dl, const ScreenBox& b, const Entity& e);
	void DrawLookDir(ImDrawList* dl, const Entity& e);
	void DrawSkeleton(ImDrawList* dl, const Entity& e, ImU32 col);
	void Draw();
}
