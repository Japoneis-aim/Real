#include <features/esp.hpp>

namespace Features::ESP {
	using namespace Context;

	ScreenBox EntityBox(const Entity& e) {
		ScreenBox r;
		float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
		int hits = 0;
		for (Bone b : BoneList) {
			Vec2 sp;
			if (!WorldToScreen(ViewMatrix, e.bones[static_cast<size_t>(b)],
				ScreenWidth, ScreenHeight, sp))
				continue;
			minX = (sp.x < minX) ? sp.x : minX;
			minY = (sp.y < minY) ? sp.y : minY;
			maxX = (sp.x > maxX) ? sp.x : maxX;
			maxY = (sp.y > maxY) ? sp.y : maxY;
			hits++;
		}
		if (hits < 3) return r;

		float h = maxY - minY;
		minY -= h * 0.10f;
		maxY += h * 0.02f;
		float w = (maxY - minY) * 0.45f;
		float cx = (minX + maxX) * 0.5f;
		r.mn = { cx - w, minY };
		r.mx = { cx + w, maxY };
		r.ok = true;
		return r;
	}

	void DrawHealth(ImDrawList* dl, const ScreenBox& b, int hp) {
		float h = b.mx.y - b.mn.y;
		float bx = b.mn.x - 5.0f;
		float by = b.mn.y;
		float bw = 3.0f;
		float fillH = h * (hp / 100.0f);
		dl->AddRectFilled({ bx, by }, { bx + bw, by + h },
			IM_COL32(0, 0, 0, 180));
		ImU32 c = hp > 60 ? IM_COL32(60, 230, 60, 230) :
			hp > 25 ? IM_COL32(230, 200, 30, 230) :
			IM_COL32(230, 60, 60, 230);
		dl->AddRectFilled({ bx, by + (h - fillH) }, { bx + bw, by + h }, c);
	}

	void DrawName(ImDrawList* dl, const ScreenBox& b, const Entity& e) {
		const char* n = e.name.c_str();
		ImVec2 sz = ImGui::CalcTextSize(n);
		ImVec2 pos{ (b.mn.x + b.mx.x) * 0.5f - sz.x * 0.5f, b.mn.y - sz.y - 2.0f };
		dl->AddText({ pos.x + 1, pos.y + 1 }, IM_COL32(0, 0, 0, 220), n);
		dl->AddText(pos, Config::GameSettings.colName.ToImU32(), n);
	}

	void DrawLookDir(ImDrawList* dl, const Entity& e) {
		Vec3 head = e.Head();
		if (head.x == 0 && head.y == 0 && head.z == 0) return;

		float pitch = e.eyeAngles.x * 0.01745329f;
		float yaw = e.eyeAngles.y * 0.01745329f;
		float cp = std::cos(pitch), sp = std::sin(pitch);
		float cy = std::cos(yaw), sy = std::sin(yaw);
		Vec3 dir = { cp * cy, cp * sy, -sp };
		Vec3 endPos = head + dir * 60.0f;

		Vec2 s1, s2;
		if (WorldToScreen(ViewMatrix, head, ScreenWidth, ScreenHeight, s1) &&
			WorldToScreen(ViewMatrix, endPos, ScreenWidth, ScreenHeight, s2))
			dl->AddLine({ s1.x, s1.y }, { s2.x, s2.y }, Config::GameSettings.colLookDir.ToImU32(), 1.0f);
	}

	void DrawSkeleton(ImDrawList* dl, const Entity& e, ImU32 col) {
		auto draw = [&](Bone a, Bone b) {
			Vec2 sa, sb;
			if (WorldToScreen(ViewMatrix, e.bones[static_cast<size_t>(a)],
					ScreenWidth, ScreenHeight, sa) &&
				WorldToScreen(ViewMatrix, e.bones[static_cast<size_t>(b)],
					ScreenWidth, ScreenHeight, sb))
				dl->AddLine({ sa.x, sa.y }, { sb.x, sb.y }, col, 1.2f);
		};
		draw(Bone::Head, Bone::Neck0);
		draw(Bone::Neck0, Bone::Spine1);
		draw(Bone::Spine1, Bone::Spine2);
		draw(Bone::Spine2, Bone::Pelvis);
		draw(Bone::Spine1, Bone::ArmUpperL);
		draw(Bone::ArmUpperL, Bone::ArmLowerL);
		draw(Bone::ArmLowerL, Bone::HandL);
		draw(Bone::Spine1, Bone::ArmUpperR);
		draw(Bone::ArmUpperR, Bone::ArmLowerR);
		draw(Bone::ArmLowerR, Bone::HandR);
		draw(Bone::Pelvis, Bone::LegUpperL);
		draw(Bone::LegUpperL, Bone::LegLowerL);
		draw(Bone::LegLowerL, Bone::AnkleL);
		draw(Bone::Pelvis, Bone::LegUpperR);
		draw(Bone::LegUpperR, Bone::LegLowerR);
		draw(Bone::LegLowerR, Bone::AnkleR);
	}

	void Draw() {
		if (ClientDLL.load() == 0) return;

		auto dl = ImGui::GetBackgroundDrawList();

		// FOV circle independente de ESP e do aimbot — basta o checkbox
		// "Show FOV" estar marcado.
		if (Config::GameSettings.aimFovCircle) {
			float cx = ScreenWidth * 0.5f;
			float cy = ScreenHeight * 0.5f;
			float fovPx = Config::GameSettings.aimFov;
			dl->AddCircle({ cx, cy }, fovPx, Config::GameSettings.colFovCircle.ToImU32(), 64, 1.0f);
		}

		if (!Config::GameSettings.esp) return;

		uint32_t aimIdx = AimTarget.valid.load() ? AimTarget.index.load() : 0;
		uint32_t spectatedIdx = LocalSpectatedIdx.load();

		std::lock_guard<std::mutex> lock(EntitiesMutex);
		for (const auto& e : Entities) {
			if (e.isLocal) continue;
			if (e.health <= 0) continue;
			// Pula quem voce esta telando (player morreu) — caixa em volta
			// da camera atrapalha a visao.
			if (spectatedIdx != 0 && e.index == spectatedIdx) continue;
			if (Config::GameSettings.teamCheck && e.isTeam) continue;
			if (e.distance > Config::GameSettings.espDistance * Config::UNITS_PER_METER) continue;

			ScreenBox b = EntityBox(e);
			if (!b.ok) continue;

			bool isTarget = (Config::GameSettings.aimHighlight && aimIdx && e.index == aimIdx);

			auto& cfg = Config::GameSettings;
			ImU32 col;
			if (isTarget)
				col = cfg.colTarget.ToImU32();
			else if (e.isTeam)
				col = cfg.colTeam.ToImU32();
			else if (e.visibleBones != 0)
				col = cfg.colEnemyVisible.ToImU32();
			else
				col = cfg.colEnemyHidden.ToImU32();

			if (Config::GameSettings.espBox) {
				// colBox com alpha > 0 sobrescreve a cor dinamica de team/vis
				// pra todos os boxes (override estatico do user).
				ImU32 boxCol = col;
				if (Config::GameSettings.colBox.a > 0.001f)
					boxCol = Config::GameSettings.colBox.ToImU32();
				if (e.isVisible && !e.isTeam) {
					ImU32 glowCol = (boxCol & 0x00FFFFFF) | 0x30000000;
					dl->AddRect({ b.mn.x - 1, b.mn.y - 1 }, { b.mx.x + 1, b.mx.y + 1 }, glowCol, 0.f, 0, 2.f);
				}
				DrawBox(dl, b, boxCol);
			}
			if (Config::GameSettings.espHealth) DrawHealth(dl, b, e.health);
			if (Config::GameSettings.espName) DrawName(dl, b, e);
			if (Config::GameSettings.espSkeleton) DrawSkeleton(dl, e,
				isTarget ? col : cfg.colSkeleton.ToImU32());

			if (Config::GameSettings.espWeapon && e.weaponId) {
				const char* wname = WeaponName(e.weaponId);
				if (wname) {
					ImVec2 sz = ImGui::CalcTextSize(wname);
					ImVec2 wp{ (b.mn.x + b.mx.x) * 0.5f - sz.x * 0.5f, b.mx.y + 3.0f };
					dl->AddText({ wp.x + 1, wp.y + 1 }, IM_COL32(0, 0, 0, 200), wname);
					dl->AddText(wp, Config::GameSettings.colWeapon.ToImU32(), wname);
				}
			}

			if (cfg.espLookDir)
				DrawLookDir(dl, e);

			if (Config::GameSettings.espFlags) {
				float flagX = b.mx.x + 4.0f;
				float flagY = b.mn.y;
				float spacing = 13.0f;
				int fi = 0;

				auto drawFlag = [&](const char* txt, ImU32 c) {
					ImVec2 fp{ flagX, flagY + spacing * fi };
					dl->AddText({ fp.x + 1, fp.y + 1 }, IM_COL32(0, 0, 0, 180), txt);
					dl->AddText(fp, c, txt);
					fi++;
				};

				if (e.isScoped) drawFlag("SCOPED", IM_COL32(200, 200, 255, 255));
				if (e.isFlashed) drawFlag("FLASHED", IM_COL32(255, 255, 100, 255));
			}

			if (isTarget) {
				Vec2 bp;
				if (WorldToScreen(ViewMatrix, AimTarget.bonePos, ScreenWidth, ScreenHeight, bp)) {
					// Pontinho do alvo: cor propria quando "Osso Dinamico" liga
					// (segue o bone mais proximo do crosshair em vez do fixo),
					// pra distinguir visualmente do modo bone fixo.
					ImU32 tc = (cfg.aimDynamic ? cfg.colDynamic : cfg.colTarget).ToImU32();
					dl->AddCircleFilled({ bp.x, bp.y }, 2.5f, tc, 8);
					dl->AddCircle({ bp.x, bp.y }, 5.0f, (tc & 0x00FFFFFF) | 0x66000000, 10, 1.0f);
				}
			}
		}
	}
}
