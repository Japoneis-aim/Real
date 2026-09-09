#pragma once
#include <LazyDlls/Lazyimporter.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <Windows.h>
#include <ui/fonts.h>

namespace UI {

    namespace Colors {
        inline constexpr ImU32 Background       = IM_COL32(17, 17, 17, 255);
        inline constexpr ImU32 Sidebar          = IM_COL32(20, 20, 20, 255);
        inline constexpr ImU32 TopBar           = IM_COL32(25, 25, 25, 255);
        inline constexpr ImU32 Card             = IM_COL32(20, 20, 20, 255);
        inline constexpr ImU32 CardBorder       = IM_COL32(30, 30, 30, 255);
        inline constexpr ImU32 WidgetBG         = IM_COL32(23, 23, 23, 255);
        inline constexpr ImU32 WidgetHover      = IM_COL32(33, 33, 33, 255);
        inline constexpr ImU32 Border           = IM_COL32(40, 40, 40, 255);
        inline constexpr ImU32 BorderSubtle     = IM_COL32(30, 30, 30, 255);
        inline constexpr ImU32 Accent           = IM_COL32(140, 60, 220, 255);
        inline constexpr ImU32 AccentHover      = IM_COL32(170, 90, 245, 255);
        inline constexpr ImU32 AccentDim        = IM_COL32(140, 60, 220, 100);
        inline constexpr ImU32 TextPrimary      = IM_COL32(225, 225, 225, 255);
        inline constexpr ImU32 TextSecondary    = IM_COL32(200, 200, 200, 255);
        inline constexpr ImU32 TextMuted        = IM_COL32(80, 80, 80, 255);
        inline constexpr ImU32 TextDisabled     = IM_COL32(60, 60, 60, 255);
    }

    inline float SmoothLerp(float current, float target, float speed, float dt) {
        float factor = 1.0f - expf(-speed * dt);
        return current + (target - current) * factor;
    }

    struct Anim {
        float v1 = 0.f;
        float v2 = 0.f;
        float v3 = 0.f;
    };

    struct AnimEntry { ImGuiID id; Anim anim; };

    inline Anim& GetAnim(ImGuiID id) {
        static std::vector<AnimEntry> cache;
        for (auto& e : cache)
            if (e.id == id) return e.anim;
        cache.push_back({ id, {} });
        return cache.back().anim;
    }

    inline void SetupStyle() {
        ImGuiStyle* s = &ImGui::GetStyle();
        s->WindowRounding = 0.f;
        s->WindowBorderSize = 0.f;
        s->WindowPadding = ImVec2(0, 0);
        s->FrameBorderSize = 0.f;
        s->FrameRounding = 6.f;
        s->ScrollbarSize = 4.f;
        s->ScrollbarRounding = 4.f;
        s->GrabRounding = 4.f;
        s->PopupRounding = 8.f;
        s->ItemSpacing = ImVec2(0, 0);
        s->ChildRounding = 0.f;
        s->AntiAliasedLines = true;
        s->AntiAliasedLinesUseTex = true;
        s->AntiAliasedFill = true;
        s->CircleTessellationMaxError = 0.25f;
        s->Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(Colors::Background);
        s->Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        s->Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(Colors::Border);
        s->Colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(Colors::WidgetBG);
        s->Colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(Colors::WidgetHover);
        s->Colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(Colors::WidgetHover);
        s->Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(Colors::TextPrimary);
        s->Colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(Colors::Card);
        s->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        s->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3f, 0.3f, 0.3f, 0.4f);
        s->Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(Colors::AccentDim);
        s->Colors[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(Colors::Accent);
        s->Colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(Colors::WidgetBG);
        s->Colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(Colors::WidgetHover);
        s->Colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(Colors::WidgetHover);
    }

    inline bool SidebarTab(const char* icon, const char* label, bool selected, ImFont* iconFont = nullptr, ImFont* textFont = nullptr) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushID(label);
        ImGuiID id = window->GetID("##stab");
        ImDrawList* dl = window->DrawList;
        float dt = ImGui::GetIO().DeltaTime;

        float w = 90.f;
        float h = 55.f;
        ImVec2 pos = window->DC.CursorPos;
        ImRect bb(pos, pos + ImVec2(w, h));

        ImGui::ItemSize(ImVec2(w, h));
        if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        auto& a = GetAnim(id);
        a.v1 = SmoothLerp(a.v1, selected ? 1.f : hovered ? 0.5f : 0.f, 12.f, dt);
        a.v2 = SmoothLerp(a.v2, selected ? 1.f : 0.f, 12.f, dt);

        ImU32 iconCol = IM_COL32(
            (int)(80 + 145 * a.v1), (int)(80 + 145 * a.v1), (int)(80 + 145 * a.v1), 255);
        if (selected) iconCol = Colors::Accent;

        ImFont* iFont = iconFont ? iconFont : ImGui::GetFont();
        ImVec2 iconSize = iFont->CalcTextSizeA(iFont->FontSize, FLT_MAX, 0.f, icon);
        dl->AddText(iFont, iFont->FontSize,
            ImVec2(bb.GetCenter().x - iconSize.x * 0.5f, bb.Min.y + 10.f), iconCol, icon);

        ImU32 textCol = IM_COL32(
            (int)(80 + 120 * a.v1), (int)(80 + 120 * a.v1), (int)(80 + 120 * a.v1), 255);
        if (selected) textCol = Colors::TextPrimary;

        ImFont* tFont = textFont ? textFont : ImGui::GetFont();
        ImVec2 textSize = tFont->CalcTextSizeA(tFont->FontSize, FLT_MAX, 0.f, label);
        dl->AddText(tFont, tFont->FontSize,
            ImVec2(bb.GetCenter().x - textSize.x * 0.5f, bb.Min.y + 32.f), textCol, label);

        if (a.v2 > 0.01f) {
            float barW = 24.f * a.v2;
            dl->AddRectFilled(
                ImVec2(bb.GetCenter().x - barW * 0.5f, bb.Max.y - 3.f),
                ImVec2(bb.GetCenter().x + barW * 0.5f, bb.Max.y - 1.f),
                Colors::Accent, 1.f);
        }

        ImGui::PopID();
        return pressed;
    }

    inline bool SubTab(const char* label, bool selected) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushID(label);
        ImGuiID id = window->GetID("##sub");
        ImDrawList* dl = window->DrawList;
        float dt = ImGui::GetIO().DeltaTime;

        ImVec2 textSize = ImGui::CalcTextSize(label);
        float w = textSize.x + 20.f;
        float h = 32.f;
        ImVec2 pos = window->DC.CursorPos;
        ImRect bb(pos, pos + ImVec2(w, h));

        ImGui::ItemSize(ImVec2(w, h));
        if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        auto& a = GetAnim(id);
        a.v1 = SmoothLerp(a.v1, selected ? 1.f : hovered ? 0.4f : 0.f, 10.f, dt);

        ImU32 textCol = IM_COL32(
            (int)(80 + 145 * a.v1), (int)(80 + 145 * a.v1), (int)(80 + 145 * a.v1), 255);
        if (selected) textCol = Colors::TextPrimary;
        dl->AddText(ImVec2(bb.GetCenter().x - textSize.x * 0.5f, bb.GetCenter().y - textSize.y * 0.5f),
            textCol, label);

        if (selected) {
            float barW = textSize.x;
            dl->AddRectFilled(
                ImVec2(bb.GetCenter().x - barW * 0.5f, bb.Max.y - 2.f),
                ImVec2(bb.GetCenter().x + barW * 0.5f, bb.Max.y),
                Colors::Accent, 1.f);
        }

        ImGui::PopID();
        return pressed;
    }

    // Selectable portado do amigo (Custom.hpp:129 — `Tab`). Botao horizontal
    // 130x30 com icone esquerda + divider vertical + label, accent fill com
    // alpha animado quando selected/hovered.
    inline bool Selectable(const char* icon, const char* label, bool selected,
                           ImFont* iconFont = nullptr, ImFont* textFont = nullptr,
                           ImVec2 sizeOverride = ImVec2(0.f, 0.f)) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushID(label);
        ImGuiID id = window->GetID("##sel");
        ImDrawList* dl = window->DrawList;
        ImGuiStyle& style = ImGui::GetStyle();
        float dt = ImGui::GetIO().DeltaTime;

        // Espaco vertical antes do botao pra nao grudar no item de cima
        // (user reclamou que ficava colado). Tambem evita o card ficar
        // tocando linhas de divider de checkbox/slider anterior.
        const float topPad = 8.f;
        const float botPad = 4.f;

        // Default 36 (amigo usa 30, +6px porque user achou pequeno).
        ImVec2 visSz = sizeOverride.x > 0.f
            ? sizeOverride
            : ImVec2(ImGui::GetContentRegionAvail().x, 36.f);
        ImVec2 totalSz(visSz.x, visSz.y + topPad + botPad);
        ImVec2 cursor = window->DC.CursorPos;
        ImRect bb(cursor + ImVec2(0.f, topPad),
                  cursor + ImVec2(visSz.x, topPad + visSz.y));

        ImGui::ItemSize(totalSz, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        static std::unordered_map<ImGuiID, float> values;
        auto& v = values[id];
        v = ImLerp(v, selected ? 1.f : (hovered ? 0.5f : 0.f), 8.f * dt);

        ImU32 accent = (Colors::Accent & 0x00FFFFFF) | ((unsigned)(v * 64.f) << 24);
        dl->AddRectFilled(bb.Min, bb.Max, accent, 4.f);

        ImFont* iFont = iconFont ? iconFont : ImGui::GetFont();
        ImVec2 iconSize = iFont->CalcTextSizeA(iFont->FontSize, FLT_MAX, 0.f, icon);
        float iconAlpha = v > 0.25f ? v : 0.25f;
        ImU32 iconCol = (Colors::Accent & 0x00FFFFFF) | ((unsigned)(iconAlpha * 255.f) << 24);
        dl->AddText(iFont, iFont->FontSize,
            ImVec2(bb.Min.x + 10.f, bb.GetCenter().y - iconSize.y * 0.5f),
            iconCol, icon);

        ImFont* tFont = textFont ? textFont : ImGui::GetFont();
        ImVec2 labelSize = tFont->CalcTextSizeA(tFont->FontSize, FLT_MAX, 0.f, label);
        ImU32 textCol = IM_COL32(235, 235, 240, (int)(iconAlpha * 255.f));
        dl->AddText(tFont, tFont->FontSize,
            ImVec2(bb.Min.x + 40.f, bb.GetCenter().y - labelSize.y * 0.5f),
            textCol, label);

        ImU32 divCol = (Colors::Accent & 0x00FFFFFF) | ((unsigned)((v * 0.5f) * 255.f) << 24);
        dl->AddLine(
            ImVec2(bb.Min.x + 34.f, bb.Min.y + 8.f),
            ImVec2(bb.Min.x + 34.f, bb.Max.y - 8.f),
            divCol);

        ImGui::PopID();
        return pressed;
    }

    // BeginCard portado do BeginChild do amigo (Custom.hpp:178) — header com bg,
    // borda rounded top/bottom, e 2 linhas-neon (gradient transparent->accent->
    // transparent) no topo e fundo. Scrollbar fina em accent. Icone+label do
    // cs2 mantidos. EndCard fecha simetricamente.
    inline void BeginCard(const char* icon, const char* label, ImVec2 size) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* dl = window->DrawList;
        ImVec2 origin = ImGui::GetCursorScreenPos();

        const float headerH = 35.f;
        const float rounding = 5.f;

        dl->AddRectFilled(origin, origin + ImVec2(size.x, headerH),
            Colors::TopBar, rounding, ImDrawFlags_RoundCornersTop);

        ImVec2 iconSize(0.f, 0.f);
        if (Fonts::Icons && icon && icon[0]) {
            iconSize = Fonts::Icons->CalcTextSizeA(Fonts::Icons->FontSize, FLT_MAX, 0.f, icon);
            dl->AddText(Fonts::Icons, Fonts::Icons->FontSize,
                ImVec2(origin.x + 10.f, origin.y + headerH * 0.5f - iconSize.y * 0.5f),
                Colors::Accent, icon);
        }

        ImFont* hFont = Fonts::Big ? Fonts::Big : ImGui::GetFont();
        ImVec2 labelSize = hFont->CalcTextSizeA(hFont->FontSize, FLT_MAX, 0.f, label);
        float labelX = origin.x + (iconSize.x > 0.f ? (10.f + iconSize.x + 8.f) : 12.f);
        dl->AddText(hFont, hFont->FontSize,
            ImVec2(labelX, origin.y + headerH * 0.5f - labelSize.y * 0.5f),
            IM_COL32(255, 255, 255, 255), label);

        dl->AddRect(origin, origin + ImVec2(size.x, headerH + 1.f),
            Colors::Border, rounding, ImDrawFlags_RoundCornersTop);

        ImU32 accent = (Colors::Accent & 0x00FFFFFF) | 0xFF000000;
        ImU32 trans  = (Colors::Accent & 0x00FFFFFF);
        dl->AddRectFilledMultiColor(
            origin, origin + ImVec2(size.x * 0.5f, 1.f),
            trans, accent, accent, trans);
        dl->AddRectFilledMultiColor(
            origin + ImVec2(size.x * 0.5f, 0.f),
            origin + ImVec2(size.x, 1.f),
            accent, trans, trans, accent);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + headerH);

        // Scrollbar com fade-neon do amigo (custom em ScrollbarEx no
        // imgui_widgets.cpp). Largura suficiente pra grab ser visivel e clicavel,
        // mas o gradient deixa as bordas "esmaecidas" — visual leve em vez de
        // bloco solido.
        ImGui::PushStyleColor(ImGuiCol_ChildBg,              ImGui::ColorConvertU32ToFloat4(Colors::Card));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        ImGui::ColorConvertU32ToFloat4(Colors::Accent));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImGui::ColorConvertU32ToFloat4(Colors::Accent));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  ImGui::ColorConvertU32ToFloat4(Colors::Accent));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,     rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,     5.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 2.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,     ImVec2(12.f, 10.f));

        ImGui::BeginChild(label, ImVec2(size.x, size.y - headerH), false,
            ImGuiWindowFlags_AlwaysUseWindowPadding);
    }

    inline void EndCard() {
        ImGuiWindow* child = ImGui::GetCurrentWindow();
        ImDrawList* dl = child->DrawList;
        ImVec2 cpos = child->Pos;
        ImVec2 csz  = child->Size;

        ImU32 accent = (Colors::Accent & 0x00FFFFFF) | 0xFF000000;
        ImU32 trans  = (Colors::Accent & 0x00FFFFFF);
        dl->AddRectFilledMultiColor(
            ImVec2(cpos.x,                 cpos.y + csz.y - 1.f),
            ImVec2(cpos.x + csz.x * 0.5f,  cpos.y + csz.y),
            trans, accent, accent, trans);
        dl->AddRectFilledMultiColor(
            ImVec2(cpos.x + csz.x * 0.5f,  cpos.y + csz.y - 1.f),
            ImVec2(cpos.x + csz.x,         cpos.y + csz.y),
            accent, trans, trans, accent);

        dl->AddRect(
            ImVec2(cpos.x,           cpos.y - 1.f),
            ImVec2(cpos.x + csz.x,   cpos.y + csz.y),
            Colors::Border, 5.f, ImDrawFlags_RoundCornersBottom);

        ImGui::EndChild();
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(5);
    }

    // CheckBox portada do amigo (Custom.hpp:224) — square 20px slide-in fill +
    // checkmark renderizado por cima. AnimA = slide offset (vai de SQ a 0
    // quando ativa), AnimB = text alpha (0.4 inativo, 0.8 ativo).
    struct ZmCheckboxAnim { float A = 18.f; float B = 0.4f; };

    // Tooltip "?" badge: bolinha cinza com "?" centralizado a direita do label.
    // No hover, abre um popup proximo do cursor com a explicacao formatada.
    inline void HelpBadge(const char* helpText, ImVec2 center, float radius = 8.5f) {
        if (!helpText || !helpText[0]) return;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* dl = window->DrawList;

        ImRect bb(center - ImVec2(radius, radius), center + ImVec2(radius, radius));
        bool hovered = ImGui::IsMouseHoveringRect(bb.Min, bb.Max);

        ImU32 bg = hovered ? Colors::Accent : IM_COL32(70, 70, 78, 220);
        ImU32 fg = hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 210, 230);
        dl->AddCircleFilled(center, radius, bg, 24);
        dl->AddCircle(center, radius, IM_COL32(0, 0, 0, 100), 24, 1.f);

        ImFont* f = Fonts::Normal ? Fonts::Normal : ImGui::GetFont();
        ImVec2 qSz = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.f, "?");
        dl->AddText(f, f->FontSize,
            ImVec2(center.x - qSz.x * 0.5f, center.y - qSz.y * 0.5f + 0.5f),
            fg, "?");

        if (hovered) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(10.f, 8.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   6.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(Colors::Card));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImGui::ColorConvertU32ToFloat4(Colors::Accent));
            ImGui::PushStyleColor(ImGuiCol_Text,    ImVec4(0.92f, 0.92f, 0.94f, 1.f));
            ImGui::SetNextWindowSizeConstraints(ImVec2(160.f, 0.f), ImVec2(260.f, FLT_MAX));
            ImGui::BeginTooltip();
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            ImGui::PushTextWrapPos(240.f);
            ImGui::TextUnformatted(helpText);
            ImGui::PopTextWrapPos();
            if (Fonts::Small) ImGui::PopFont();
            ImGui::EndTooltip();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(3);
        }
    }

    inline bool CheckBox(const char* label, bool* v, bool lastItem = false, const char* helpText = nullptr) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushID(label);
        ImGuiID id = window->GetID("##cb");
        ImDrawList* dl = window->DrawList;
        ImGuiStyle& style = ImGui::GetStyle();
        float dt = ImGui::GetIO().DeltaTime;

        // Square 22 (amigo usa 20, +2px porque user achou pequeno).
        float sq = 22.f;
        float rowH = 38.f;
        float availW = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = window->DC.CursorPos;
        ImRect bb(cursor, cursor + ImVec2(availW, rowH));

        ImGui::ItemSize(bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) { *v = !(*v); ImGui::MarkItemEdited(id); }

        static std::unordered_map<ImGuiID, ZmCheckboxAnim> states;
        auto& a = states[id];
        a.A = ImLerp(a.A, *v ? 0.f : sq, 8.f * dt);
        a.B = ImLerp(a.B, *v ? 0.9f : (hovered ? 0.7f : 0.45f), 8.f * dt);

        float checkY = cursor.y + (rowH - sq) * 0.5f;
        ImRect cb(ImVec2(cursor.x, checkY), ImVec2(cursor.x + sq, checkY + sq));

        dl->AddRectFilled(cb.Min, cb.Max, Colors::WidgetBG, 5.f);

        ImVec2 labelSize = ImGui::CalcTextSize(label);
        ImU32 txt = IM_COL32(220, 220, 225, (int)(255 * a.B));
        float labelX = cb.Min.x + sq + 8.f;
        dl->AddText(ImVec2(labelX, checkY + (sq - labelSize.y) * 0.5f), txt, label);

        ImGui::PushClipRect(cb.Min, cb.Max, true);
        {
            ImU32 fillCol = (Colors::Accent & 0x00FFFFFF) | 0xFF000000;
            dl->AddRectFilled(ImVec2(cb.Min.x + 1.f - a.A, cb.Min.y + 1.f),
                              ImVec2(cb.Max.x - 1.f - a.A, cb.Max.y - 1.f),
                              fillCol, 5.f);
            ImGui::RenderCheckMark(dl, cb.GetCenter() - ImVec2(sq * 0.25f, sq * 0.25f) + ImVec2(a.A, 0.f),
                                   IM_COL32(255, 255, 255, 255), sq * 0.5f);
        }
        ImGui::PopClipRect();

        dl->AddRect(cb.Min, cb.Max, Colors::Border, 5.f);

        if (helpText && helpText[0]) {
            ImVec2 badgeC(labelX + labelSize.x + 16.f, cursor.y + rowH * 0.5f);
            HelpBadge(helpText, badgeC, 8.5f);
        }

        if (!lastItem)
            dl->AddLine(ImVec2(cursor.x, bb.Max.y - 1), ImVec2(cursor.x + availW, bb.Max.y - 1), Colors::BorderSubtle);

        ImGui::PopID();
        return pressed;
    }

    // Slider portado do amigo (Custom.hpp:342) — label esquerda + valor direita
    // no topo, bar embaixo com fill animado em accent e thumb knob de 3px.
    struct ZmSliderAnim { float PercentAnim = 0.f; };

    inline bool SliderScalar(const char* label, ImGuiDataType dataType, void* pData, const void* pMin, const void* pMax, const char* fmt, bool lastItem = false) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushID(label);
        ImGuiID id = window->GetID("##sl");
        ImDrawList* dl = window->DrawList;
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiContext& g = *GImGui;

        ImFont* tinyFont = UI::Fonts::Small ? UI::Fonts::Small : ImGui::GetFont();
        float tinySize = tinyFont->FontSize;

        float availW = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = window->DC.CursorPos;
        ImVec2 labelSize = tinyFont->CalcTextSizeA(tinySize, FLT_MAX, 0.f, label);

        // Padding vertical: 8px no topo, 8px embaixo (user reclamou de
        // widgets grudados verticalmente). Conteudo visivel comeca em
        // cursor + topPad.
        const float topPad = 8.f;
        const float botPad = 8.f;

        // Bar height 14 (amigo usa 10, +4px porque user achou pequeno).
        const ImRect frame_bb(
            cursor + ImVec2(0.f,    topPad + labelSize.y + 5.f),
            cursor + ImVec2(availW, topPad + labelSize.y + 19.f));
        const ImRect total_bb(cursor, frame_bb.Max + ImVec2(0.f, botPad));

        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id)) { ImGui::PopID(); return false; }

        if (!fmt) fmt = ImGui::DataTypeGetInfo(dataType)->PrintFmt;

        const bool hovered = ImGui::ItemHoverable(frame_bb, id, ImGuiItemFlags_None);
        if (hovered && g.IO.MouseClicked[0]) {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }

        ImRect grab_bb;
        bool changed = ImGui::SliderBehavior(frame_bb, id, dataType, pData, pMin, pMax, fmt, 0, &grab_bb);
        if (changed) ImGui::MarkItemEdited(id);

        char valueBuf[64];
        const char* valueBufEnd = valueBuf + ImGui::DataTypeFormatString(valueBuf, IM_ARRAYSIZE(valueBuf), dataType, pData, fmt);

        static std::unordered_map<ImGuiID, ZmSliderAnim> states;
        auto& s = states[id];

        float percent = (grab_bb.Max.x - frame_bb.Min.x) / frame_bb.GetWidth();
        percent = ImClamp(percent, 0.f, 1.f);
        s.PercentAnim = ImLerp(s.PercentAnim, percent * frame_bb.GetWidth(), 10.f * g.IO.DeltaTime);

        dl->AddRectFilled(frame_bb.Min, frame_bb.Max, Colors::WidgetBG, 5.f);

        ImU32 accent = (Colors::Accent & 0x00FFFFFF) | 0xFF000000;
        dl->AddRectFilled(frame_bb.Min, ImVec2(frame_bb.Min.x + s.PercentAnim, frame_bb.Max.y), accent, 5.f);

        dl->AddRectFilled(
            ImVec2(frame_bb.Min.x + s.PercentAnim - 3.f, frame_bb.Min.y - 1.f),
            ImVec2(frame_bb.Min.x + s.PercentAnim + 3.f, frame_bb.Max.y + 1.f),
            IM_COL32(235, 235, 240, 255), 5.f);

        // Label/valor desenhados acima da bar com o topPad ja aplicado.
        ImVec2 textTop(cursor.x, cursor.y + topPad);
        dl->AddText(tinyFont, tinySize, textTop, Colors::TextSecondary, label);

        ImVec2 valSize = tinyFont->CalcTextSizeA(tinySize, FLT_MAX, 0.f, valueBuf, valueBufEnd);
        dl->AddText(tinyFont, tinySize,
            ImVec2(textTop.x + availW - valSize.x, textTop.y),
            accent, valueBuf, valueBufEnd);

        if (!lastItem)
            dl->AddLine(ImVec2(cursor.x, total_bb.Max.y - 1.f), ImVec2(cursor.x + availW, total_bb.Max.y - 1.f), Colors::BorderSubtle);

        ImGui::PopID();
        return changed;
    }

    inline bool SliderFloat(const char* label, float* v, float vMin, float vMax, const char* fmt = "%.1f", bool lastItem = false) {
        return SliderScalar(label, ImGuiDataType_Float, v, &vMin, &vMax, fmt, lastItem);
    }

    inline bool SliderInt(const char* label, int* v, int vMin, int vMax, const char* fmt = "%d", bool lastItem = false) {
        return SliderScalar(label, ImGuiDataType_S32, v, &vMin, &vMax, fmt, lastItem);
    }

    inline const char* VKKeyName(int vk) {
        switch (vk) {
        case 0: return "None";
        case VK_LBUTTON: return "LMB"; case VK_RBUTTON: return "RMB";
        case VK_MBUTTON: return "MMB"; case VK_XBUTTON1: return "Mouse4";
        case VK_XBUTTON2: return "Mouse5"; case VK_BACK: return "Back";
        case VK_TAB: return "Tab"; case VK_RETURN: return "Enter";
        case VK_SHIFT: return "Shift"; case VK_CONTROL: return "Ctrl";
        case VK_MENU: return "Alt"; case VK_CAPITAL: return "Caps";
        case VK_ESCAPE: return "Esc"; case VK_SPACE: return "Space";
        case VK_LEFT: return "Left"; case VK_UP: return "Up";
        case VK_RIGHT: return "Right"; case VK_DOWN: return "Down";
        case VK_DELETE: return "Del"; case VK_INSERT: return "Ins";
        case VK_HOME: return "Home"; case VK_END: return "End";
        case VK_PRIOR: return "PgUp"; case VK_NEXT: return "PgDn";
        default:
            if (vk >= '0' && vk <= '9') { static char b[2]; b[0] = (char)vk; b[1] = 0; return b; }
            if (vk >= 'A' && vk <= 'Z') { static char b[2]; b[0] = (char)vk; b[1] = 0; return b; }
            if (vk >= VK_F1 && vk <= VK_F12) {
                static char b[4]; int n = vk - VK_F1 + 1;
                b[0] = 'F';
                if (n >= 10) { b[1] = '1'; b[2] = '0' + n - 10; b[3] = 0; }
                else { b[1] = '0' + n; b[2] = 0; }
                return b;
            }
            return "?";
        }
    }

    // KeyBind portado do amigo (Custom.hpp:611) — botao animado a direita com
    // largura lerping pelo tamanho do nome da key. ActiveId = "Press" mode.
    struct ZmKeyBindAnim { float Width = 40.f; float Alpha = 0.6f; };

    inline bool KeyBind(const char* label, int& key, bool lastItem = false) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushID(label);
        ImGuiID id = window->GetID("##kb");
        ImDrawList* dl = window->DrawList;
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiContext& g = *GImGui;
        ImGuiIO& io = g.IO;
        float dt = io.DeltaTime;

        static struct { ImGuiID id; int count; } fcBuf[32];
        static int fcCount = 0;
        auto getFC = [&](ImGuiID fid) -> int& {
            for (int j = 0; j < fcCount; j++) if (fcBuf[j].id == fid) return fcBuf[j].count;
            if (fcCount < 32) { fcBuf[fcCount] = { fid, 0 }; return fcBuf[fcCount++].count; }
            return fcBuf[0].count;
        };
        int& frameCount = getFC(id);

        static std::unordered_map<ImGuiID, ZmKeyBindAnim> states;
        auto& s = states[id];

        bool isActive = (g.ActiveId == id);
        const char* displayText = isActive ? "Press" : VKKeyName(key);
        ImFont* tinyFont = UI::Fonts::Small ? UI::Fonts::Small : ImGui::GetFont();
        float tinySize = tinyFont->FontSize;
        ImVec2 textSize = tinyFont->CalcTextSizeA(tinySize, FLT_MAX, 0.f, displayText);

        float minBtnW = 70.f;
        float wanted = textSize.x + 24.f;
        if (wanted < minBtnW) wanted = minBtnW;
        s.Width = ImLerp(s.Width, wanted, 10.f * dt);

        float rowH = 42.f;
        float btnH = 30.f;
        float availW = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = window->DC.CursorPos;
        ImRect totalBB(cursor, cursor + ImVec2(availW, rowH));
        ImRect btnBB(
            ImVec2(cursor.x + availW - s.Width, cursor.y + rowH * 0.5f - btnH * 0.5f),
            ImVec2(cursor.x + availW,           cursor.y + rowH * 0.5f + btnH * 0.5f));

        ImGui::ItemSize(totalBB, style.FramePadding.y);
        if (!ImGui::ItemAdd(totalBB, id)) { ImGui::PopID(); return false; }

        bool hovered = ImGui::ItemHoverable(btnBB, id, ImGuiItemFlags_None);
        if (hovered) {
            ImGui::SetHoveredID(id);
            g.MouseCursor = ImGuiMouseCursor_TextInput;
        }

        bool bound = (key != 0 && !isActive);
        s.Alpha = ImLerp(s.Alpha, bound ? 0.85f : 0.55f, 4.f * dt);

        ImVec2 labelSize = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(cursor.x, cursor.y + rowH * 0.5f - labelSize.y * 0.5f), Colors::TextSecondary, label);

        dl->AddRectFilled(btnBB.Min, btnBB.Max, Colors::WidgetBG, 5.f);
        ImGui::PushClipRect(btnBB.Min, btnBB.Max, true);
        {
            ImU32 txt = IM_COL32(235, 235, 240, (int)(255.f * s.Alpha));
            dl->AddText(tinyFont, tinySize,
                ImVec2(btnBB.GetCenter().x - textSize.x * 0.5f,
                       btnBB.GetCenter().y - textSize.y * 0.5f),
                txt, displayText);
        }
        ImGui::PopClipRect();
        dl->AddRect(btnBB.Min, btnBB.Max, isActive ? Colors::Accent : Colors::Border, 5.f);

        bool userClicked = hovered && io.MouseClicked[0];
        if (userClicked) {
            if (!isActive) {
                memset(io.MouseDown, 0, sizeof(io.MouseDown));
                key = 0;
                frameCount = 0;
            }
            ImGui::SetActiveID(id, window);
            ImGui::FocusWindow(window);
        } else if (io.MouseClicked[0] && isActive) {
            ImGui::ClearActiveID();
        }

        bool changed = false;
        if (g.ActiveId == id) {
            frameCount++;
            if (frameCount > 2) {
                for (int i = 1; i < 5; i++) {
                    if (io.MouseClicked[i]) {
                        key = (i == 1) ? VK_RBUTTON : (i == 2) ? VK_MBUTTON : (i == 3) ? VK_XBUTTON1 : VK_XBUTTON2;
                        changed = true; ImGui::ClearActiveID();
                    }
                }
                if (!changed) {
                    for (int vk = 0x08; vk <= 0xA5; vk++) {
                        if (LI_CACHED(GetAsyncKeyState)(vk) & 0x8000) {
                            key = vk; changed = true; ImGui::ClearActiveID(); break;
                        }
                    }
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { key = 0; ImGui::ClearActiveID(); }
        }

        if (!lastItem)
            dl->AddLine(ImVec2(cursor.x, totalBB.Max.y - 1), ImVec2(cursor.x + availW, totalBB.Max.y - 1), Colors::BorderSubtle);

        ImGui::PopID();
        return changed;
    }

    // Combo portado do BeginCombo customizado do amigo (imgui_widgets.cpp:1859).
    // Label no topo, frame WidgetBG + border accent, preview esquerda, arrow
    // direita. Popup BG=Card, items via ImGui::Selectable padrao.
    inline bool Combo(const char* label, int* current, const char* const items[], int count, bool lastItem = false) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushID(label);
        ImGuiID id = window->GetID("##combo");
        ImDrawList* dl = window->DrawList;
        ImGuiStyle& style = ImGui::GetStyle();

        ImFont* tiny = Fonts::Small ? Fonts::Small : ImGui::GetFont();
        float tinySize = tiny->FontSize;

        float availW = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = window->DC.CursorPos;
        ImVec2 labelSize = tiny->CalcTextSizeA(tinySize, FLT_MAX, 0.f, label);

        // Padding vertical 8/8 igual slider/selectable. User reclamou que o
        // label "Osso Alvo" estava muito proximo da linha do widget de cima.
        const float topPad = 8.f;
        const float botPad = 8.f;

        // Frame height 34 (amigo usa 29, +5px porque user achou pequeno).
        const ImRect frame_bb(
            cursor + ImVec2(0.f,    topPad + labelSize.y + 5.f),
            cursor + ImVec2(availW, topPad + labelSize.y + 5.f + 34.f));
        const ImRect total_bb(cursor, frame_bb.Max + ImVec2(0.f, botPad));

        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id, &frame_bb)) { ImGui::PopID(); return false; }

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);

        const char* popup_name = "##cs2_combo_popup";
        if (pressed) ImGui::OpenPopup(popup_name);

        dl->AddRectFilled(frame_bb.Min, frame_bb.Max, Colors::WidgetBG, 5.f);
        dl->AddRect(frame_bb.Min, frame_bb.Max, Colors::Border, 5.f);

        dl->AddText(tiny, tinySize, ImVec2(cursor.x, cursor.y + topPad), Colors::TextSecondary, label);

        const char* preview = (*current >= 0 && *current < count) ? items[*current] : "";
        ImVec2 prevSize = tiny->CalcTextSizeA(tinySize, FLT_MAX, 0.f, preview);
        dl->AddText(tiny, tinySize,
            ImVec2(frame_bb.Min.x + 10.f, frame_bb.GetCenter().y - prevSize.y * 0.5f),
            IM_COL32(235, 235, 240, 255), preview);

        ImGui::RenderArrow(dl,
            ImVec2(frame_bb.Max.x - 22.f, frame_bb.GetCenter().y - 6.f),
            Colors::TextSecondary, ImGuiDir_Down, 1.f);

        bool changed = false;

        ImGui::SetNextWindowPos(ImVec2(frame_bb.Min.x, frame_bb.Max.y + 2.f));
        ImGui::SetNextWindowSize(ImVec2(frame_bb.GetWidth(), 0.f));

        ImVec4 accentV  = ImGui::ColorConvertU32ToFloat4(Colors::Accent);
        ImVec4 accentLo = accentV; accentLo.w = 0.35f;
        ImVec4 accentHi = accentV; accentHi.w = 0.65f;
        ImGui::PushStyleColor(ImGuiCol_PopupBg,       ImGui::ColorConvertU32ToFloat4(Colors::Card));
        ImGui::PushStyleColor(ImGuiCol_Border,        ImGui::ColorConvertU32ToFloat4(Colors::Border));
        ImGui::PushStyleColor(ImGuiCol_Header,        accentLo);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, accentLo);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  accentHi);
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.92f, 0.92f, 0.94f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(4.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  3.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(0.f, 2.f));

        if (ImGui::BeginPopup(popup_name, ImGuiWindowFlags_NoMove)) {
            // Items custom-rendered pra texto ficar centralizado verticalmente
            // dentro do slot — ImGui::Selectable padrao desenha o texto no topo
            // do slot quando size.y > fontSize.
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            float itemW = ImGui::GetContentRegionAvail().x;
            float itemH = 28.f;
            for (int i = 0; i < count; i++) {
                ImGui::PushID(i);
                ImGuiID itemId = ImGui::GetID("##item");
                ImVec2 ip = ImGui::GetCursorScreenPos();
                ImRect ibb(ip, ip + ImVec2(itemW, itemH));
                ImGui::ItemSize(ImVec2(itemW, itemH));
                if (ImGui::ItemAdd(ibb, itemId)) {
                    bool ih, ihd;
                    bool ipressed = ImGui::ButtonBehavior(ibb, itemId, &ih, &ihd);
                    bool sel = (i == *current);
                    ImU32 accentHiU = ImGui::ColorConvertFloat4ToU32(accentHi);
                    ImU32 accentLoU = ImGui::ColorConvertFloat4ToU32(accentLo);
                    if (sel) pdl->AddRectFilled(ibb.Min, ibb.Max, accentHiU, 3.f);
                    else if (ih) pdl->AddRectFilled(ibb.Min, ibb.Max, accentLoU, 3.f);

                    ImVec2 ts = tiny->CalcTextSizeA(tinySize, FLT_MAX, 0.f, items[i]);
                    pdl->AddText(tiny, tinySize,
                        ImVec2(ibb.Min.x + 8.f, ibb.GetCenter().y - ts.y * 0.5f),
                        IM_COL32(235, 235, 240, 255), items[i]);

                    if (ipressed) {
                        *current = i;
                        changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(6);

        if (!lastItem)
            dl->AddLine(ImVec2(cursor.x, total_bb.Max.y - 1.f),
                        ImVec2(cursor.x + availW, total_bb.Max.y - 1.f), Colors::BorderSubtle);

        ImGui::PopID();
        return changed;
    }

    // Versao zero-terminated igual ao ImGui::Combo padrao ("Head\0Neck\0Chest\0\0").
    inline bool Combo(const char* label, int* current, const char* items_sep_by_zeros, bool lastItem = false) {
        const char* items[32]; int count = 0;
        const char* p = items_sep_by_zeros;
        while (*p && count < 32) { items[count++] = p; p += strlen(p) + 1; }
        return Combo(label, current, items, count, lastItem);
    }

    // ColorEdit portado do amigo (Custom.hpp:603 + imgui_widgets.cpp:5377):
    // Swatch quadrado pequeno a direita, popup picker com header accent de
    // 30px no topo (Secondary BG + linha border + label em accent), depois
    // ColorPicker4 com flags do amigo.
    inline void ColorEdit4(const char* label, float* col, bool lastItem = false) {
        ImGui::PushID(label);
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* dl = window->DrawList;
        float rowH = 38.f;
        float availW = ImGui::GetContentRegionAvail().x;
        ImVec2 pos = window->DC.CursorPos;

        ImFont* lFont = Fonts::Normal ? Fonts::Normal : ImGui::GetFont();
        float lSize = lFont->FontSize;
        ImVec2 labelSize = lFont->CalcTextSizeA(lSize, FLT_MAX, 0.f, label);
        dl->AddText(lFont, lSize,
            ImVec2(pos.x, pos.y + rowH * 0.5f - labelSize.y * 0.5f),
            Colors::TextSecondary, label);

        const float swatchSz = 22.f;
        ImGui::SetCursorPos(ImVec2(
            ImGui::GetCursorPosX() + availW - swatchSz,
            ImGui::GetCursorPosY() + rowH * 0.5f - swatchSz * 0.5f));

        ImVec2 swatchPos = ImGui::GetCursorScreenPos();
        ImRect swatchBB(swatchPos, swatchPos + ImVec2(swatchSz, swatchSz));

        ImGuiID swatchId = ImGui::GetID("##swatch");
        ImGui::ItemSize(ImVec2(swatchSz, swatchSz));
        bool addedSwatch = ImGui::ItemAdd(swatchBB, swatchId);
        bool swatchHover = false, swatchHeld = false, swatchPressed = false;
        if (addedSwatch)
            swatchPressed = ImGui::ButtonBehavior(swatchBB, swatchId, &swatchHover, &swatchHeld);

        ImU32 colU = IM_COL32(
            (int)(col[0] * 255.f), (int)(col[1] * 255.f),
            (int)(col[2] * 255.f), (int)(col[3] * 255.f));
        ImU32 colOpaque = (colU | 0xFF000000);
        if (col[3] < 0.999f) {
            dl->AddRectFilled(swatchBB.Min, ImVec2(swatchBB.Min.x + swatchSz * 0.5f, swatchBB.Max.y),
                colOpaque, 5.f, ImDrawFlags_RoundCornersLeft);
            dl->AddRectFilled(ImVec2(swatchBB.Min.x + swatchSz * 0.5f, swatchBB.Min.y), swatchBB.Max,
                colU, 5.f, ImDrawFlags_RoundCornersRight);
        } else {
            dl->AddRectFilled(swatchBB.Min, swatchBB.Max, colU, 5.f);
        }
        dl->AddRect(swatchBB.Min, swatchBB.Max,
            swatchHover ? Colors::Accent : Colors::Border, 5.f);

        const char* popupName = "##cs2_color_popup";
        if (swatchPressed) ImGui::OpenPopup(popupName);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(15.f, 15.f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,   3.f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(Colors::Card));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImGui::ColorConvertU32ToFloat4(Colors::Border));

        if (ImGui::BeginPopup(popupName, ImGuiWindowFlags_NoMove)) {
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            ImVec2 pwp = ImGui::GetWindowPos();
            float pww = ImGui::GetWindowSize().x;
            const float headerH = 30.f;

            // Header em Secondary (TopBar do cs2), accent label centralizado.
            pdl->AddRectFilled(pwp - ImVec2(15.f, 15.f),
                pwp + ImVec2(pww - 15.f, headerH - 15.f),
                Colors::TopBar, 3.f, ImDrawFlags_RoundCornersTop);
            pdl->AddLine(
                pwp + ImVec2(-15.f, headerH - 15.f),
                pwp + ImVec2(pww - 15.f, headerH - 15.f),
                Colors::Border);

            ImVec2 hSz = lFont->CalcTextSizeA(lSize, FLT_MAX, 0.f, label);
            pdl->AddText(lFont, lSize,
                ImVec2(pwp.x - 10.f, pwp.y - 15.f + (headerH - hSz.y) * 0.5f),
                Colors::Accent, label);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + headerH - 15.f);

            ImGuiColorEditFlags pickerFlags =
                ImGuiColorEditFlags_NoLabel        |
                ImGuiColorEditFlags_NoInputs       |
                ImGuiColorEditFlags_NoSidePreview  |
                ImGuiColorEditFlags_AlphaPreviewHalf |
                ImGuiColorEditFlags_AlphaBar       |
                ImGuiColorEditFlags_PickerHueBar   |
                ImGuiColorEditFlags_DisplayRGB     |
                ImGuiColorEditFlags_InputRGB       |
                ImGuiColorEditFlags_Float          |
                ImGuiColorEditFlags_NoBorder;

            ImGui::SetNextItemWidth(swatchSz * 9.f);
            ImGui::ColorPicker4("##picker", col, pickerFlags);
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);

        ImGui::SetCursorPosY(pos.y - window->Pos.y + ImGui::GetScrollY() + rowH);

        if (!lastItem)
            dl->AddLine(ImVec2(pos.x, pos.y + rowH - 1), ImVec2(pos.x + availW, pos.y + rowH - 1), Colors::BorderSubtle);

        ImGui::PopID();
    }

    inline bool Button(const char* label, const ImVec2& sizeArg = ImVec2(0, 0)) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGui::PushID(label);
        ImGuiID id = window->GetID("##btn");
        ImDrawList* dl = window->DrawList;
        float dt = ImGui::GetIO().DeltaTime;

        ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, true);
        ImVec2 sz = ImGui::CalcItemSize(sizeArg, labelSize.x + 20.f, 30.f);
        ImVec2 pos = window->DC.CursorPos;
        ImRect bb(pos, pos + sz);

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        auto& a = GetAnim(id);
        a.v1 = SmoothLerp(a.v1, held ? 0.6f : hovered ? 1.f : 0.f, 12.f, dt);

        ImU32 bgCol = IM_COL32(
            (int)(23 + 117 * a.v1), (int)(23 + 37 * a.v1), (int)(23 + 197 * a.v1), 255);
        dl->AddRectFilled(bb.Min, bb.Max, bgCol, 8.f);
        dl->AddRect(bb.Min, bb.Max, IM_COL32((int)(40 + 100 * a.v1), (int)(40 + 20 * a.v1), (int)(40 + 180 * a.v1), 255), 8.f);

        ImU32 textCol = IM_COL32((int)(200 + 25 * a.v1), (int)(200 + 25 * a.v1), (int)(200 + 25 * a.v1), 255);
        dl->AddText(bb.GetCenter() - labelSize * 0.5f, textCol, label);

        ImGui::PopID();
        return pressed;
    }

    inline void Spacing(float y = 4.f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y);
    }

}
