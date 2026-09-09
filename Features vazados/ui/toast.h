#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <vector>
#include <cmath>
#include <ui/fonts.h>

namespace UI::Toast {

    enum class Type { Success, Info, Warning, Error };
    enum class State { In, Visible, Out, Expired };

    struct Entry {
        std::string text;
        Type type = Type::Info;
        float lifetime;
        float elapsed = 0.f;
        State state = State::In;
        float slideX = 200.f;
        float alpha = 0.f;
        float yPos = 0.f;
        float targetY = 0.f;
    };

    inline std::vector<Entry> entries;
    constexpr int MAX_VISIBLE = 5;

    inline void Send(const std::string& text, float duration = 4.f, Type type = Type::Info) {
        if (entries.size() >= MAX_VISIBLE)
            entries.erase(entries.begin());
        Entry e;
        e.text = text;
        e.lifetime = duration;
        e.type = type;
        entries.push_back(e);
    }

    inline void Success(const std::string& text, float d = 3.f) { Send(text, d, Type::Success); }
    inline void Info(const std::string& text, float d = 3.f) { Send(text, d, Type::Info); }
    inline void Warn(const std::string& text, float d = 4.f) { Send(text, d, Type::Warning); }
    inline void Error(const std::string& text, float d = 5.f) { Send(text, d, Type::Error); }

    inline ImU32 GetAccentColor(Type t, float a) {
        switch (t) {
        case Type::Success: return IM_COL32(50, 205, 50, (int)(255 * a));
        case Type::Warning: return IM_COL32(255, 180, 0, (int)(255 * a));
        case Type::Error:   return IM_COL32(255, 60, 60, (int)(255 * a));
        default:            return IM_COL32(140, 60, 220, (int)(255 * a));
        }
    }

    inline const char* GetIcon(Type t) {
        switch (t) {
        case Type::Success: return ICON_FA_CHECK_CIRCLE;
        case Type::Warning: return ICON_FA_EXCLAMATION_TRIANGLE;
        case Type::Error:   return ICON_FA_TIMES_CIRCLE;
        default:            return ICON_FA_INFO_CIRCLE;
        }
    }

    inline void Render() {
        if (entries.empty()) return;

        float dt = ImGui::GetIO().DeltaTime;
        float screenW = ImGui::GetIO().DisplaySize.x;
        float screenH = ImGui::GetIO().DisplaySize.y;
        float toastW = 300.f;
        float toastH = 48.f;
        float gap = 6.f;
        float margin = 16.f;

        float stackY = margin;
        for (int i = 0; i < (int)entries.size(); i++) {
            entries[i].targetY = stackY;
            stackY += toastH + gap;
        }

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        for (int i = 0; i < (int)entries.size(); i++) {
            auto& e = entries[i];
            e.elapsed += dt;

            switch (e.state) {
            case State::In:
                e.slideX += (0.f - e.slideX) * (1.f - expf(-10.f * dt));
                e.alpha += (1.f - e.alpha) * (1.f - expf(-10.f * dt));
                if (e.alpha > 0.95f) e.state = State::Visible;
                break;
            case State::Visible:
                e.slideX += (0.f - e.slideX) * (1.f - expf(-10.f * dt));
                e.alpha = 1.f;
                if (e.elapsed >= e.lifetime) e.state = State::Out;
                break;
            case State::Out:
                e.slideX += (200.f - e.slideX) * (1.f - expf(-8.f * dt));
                e.alpha += (0.f - e.alpha) * (1.f - expf(-8.f * dt));
                if (e.alpha < 0.02f) e.state = State::Expired;
                break;
            default: break;
            }

            e.yPos += (e.targetY - e.yPos) * (1.f - expf(-12.f * dt));
            if (e.state == State::Expired) continue;

            int a = (int)(255 * e.alpha);
            float x = screenW - toastW - margin + e.slideX;
            float y = e.yPos;
            ImU32 accent = GetAccentColor(e.type, e.alpha);

            ImVec2 p0(x, y);
            ImVec2 p1(x + toastW, y + toastH);

            dl->AddRectFilled(p0, p1, IM_COL32(22, 22, 28, (int)(240 * e.alpha)), 10.f);
            dl->AddRect(p0, p1, IM_COL32(40, 40, 50, (int)(150 * e.alpha)), 10.f);
            dl->AddRectFilled(ImVec2(x + 1, y + 1), ImVec2(x + 4, y + toastH - 1), accent, 2.f);

            if (Fonts::Icons) {
                const char* icon = GetIcon(e.type);
                ImVec2 iconSz = Fonts::Icons->CalcTextSizeA(Fonts::Icons->FontSize, FLT_MAX, 0.f, icon);
                dl->AddText(Fonts::Icons, Fonts::Icons->FontSize,
                    ImVec2(x + 14.f, y + toastH * 0.5f - iconSz.y * 0.5f), accent, icon);
            }

            if (Fonts::Normal) {
                float textX = x + 36.f;
                float maxW = toastW - 50.f;
                ImVec2 textSz = Fonts::Normal->CalcTextSizeA(Fonts::Normal->FontSize, maxW, maxW, e.text.c_str());
                dl->AddText(Fonts::Normal, Fonts::Normal->FontSize,
                    ImVec2(textX, y + toastH * 0.5f - textSz.y * 0.5f),
                    IM_COL32(220, 220, 225, a), e.text.c_str(), nullptr, maxW);
            }

            if (e.state == State::Visible) {
                float progress = 1.f - (e.elapsed / e.lifetime);
                if (progress > 0.f) {
                    float barW = (toastW - 20.f) * progress;
                    dl->AddRectFilled(
                        ImVec2(x + 10.f, y + toastH - 4.f),
                        ImVec2(x + 10.f + barW, y + toastH - 2.f),
                        accent, 1.f);
                }
            }
        }

        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [](const Entry& e) { return e.state == State::Expired; }),
            entries.end());
    }
}
