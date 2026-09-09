#pragma once
#include <LazyDlls/Lazyimporter.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_engine.h>
#include <string>
#include <atomic>
#include <ui/theme.h>
#include <ui/fonts.h>
#include <ui/toast.h>
#include <ui/login_auth.h>
#include <core/config.h>
#include <core/lang.h>
#include <core/credentials.h>
#include <core/url.h>
#include <xorstr.h>

namespace UI::Login {

    inline char username[64] = {};
    inline char password[64] = {};
    inline bool showPassword = false;
    inline bool rememberMe = false;
    inline std::atomic<bool> authenticating = false;
    inline std::atomic<bool> authenticated = false;
    inline std::string statusText;
    inline std::string expiresText;
    inline void (*toastCallback)(const std::string&, float) = [](const std::string& t, float d) {
        Toast::Send(t, d, Toast::Type::Error);
    };
    inline void (*toastSuccessCallback)(const std::string&, float) = [](const std::string& t, float d) {
        Toast::Send(t, d, Toast::Type::Success);
    };

    inline void Draw(float winW, float winH) {
        float W = 360.f;
        float H = 380.f;

        static bool credsLoaded = false;
        if (!credsLoaded) {
            credsLoaded = true;
            Credentials::Load(username, sizeof(username), password, sizeof(password), &rememberMe);
        }

        UI::SetupStyle();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
        ImGui::Begin("##Login", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBackground);

        ImVec2 wp = ImGui::GetWindowPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float ox = (winW - W) * 0.5f;
        float oy = (winH - H) * 0.5f;
        ImVec2 p0 = wp + ImVec2(ox, oy);
        ImVec2 p1 = p0 + ImVec2(W, H);

        bool customHovered = false;

        dl->AddRectFilled(p0, p1, Colors::Background, 12.f);
        dl->AddRect(p0, p1, Colors::Border, 12.f);

        {
            ImGui::PushFont(Fonts::Icons);
            const char* closeIcon = ICON_FA_TIMES;
            ImVec2 closeSz = ImGui::CalcTextSize(closeIcon);
            ImVec2 closePos = ImVec2(p1.x - 28.f, p0.y + 10.f);
            ImRect closeBB(closePos - ImVec2(6, 6), closePos + closeSz + ImVec2(6, 6));
            bool closeHover = ImGui::IsMouseHoveringRect(closeBB.Min, closeBB.Max);
            if (closeHover) customHovered = true;
            dl->AddText(closePos, closeHover ? IM_COL32(255, 80, 80, 255) : Colors::TextMuted, closeIcon);
            if (closeHover && ImGui::IsMouseClicked(0))
                Config::ShutdownRequested().store(true);
            ImGui::PopFont();
        }

        if (UI::LogoTexture != 0) {
            float logoSz = 56.f;
            ImGui::SetCursorPos(ImVec2(ox + W * 0.5f - logoSz * 0.5f, oy + 16.f));
            ImGui::Image(UI::LogoTexture, ImVec2(logoSz, logoSz));
        }

        {
            std::string titleText = xorstr_("ZIMO CS2").c_str();
            ImFont* tf = Fonts::Title ? Fonts::Title : Fonts::Big;
            float titleSize = tf->FontSize;
            ImVec2 titleSz = tf->CalcTextSizeA(titleSize, FLT_MAX, 0.f, titleText.c_str());
            dl->AddText(tf, titleSize,
                p0 + ImVec2(W * 0.5f - titleSz.x * 0.5f, 78.f),
                Colors::Accent, titleText.c_str());
        }

        dl->AddLine(p0 + ImVec2(40, 118), p0 + ImVec2(W - 40, 118), Colors::BorderSubtle);

        float fieldW = W - 60.f;
        float fieldX = ox + 30.f;
        bool disabled = authenticating.load();

        ImGui::PushFont(Fonts::Normal);
        if (disabled) ImGui::BeginDisabled();

        // FramePadY 10 (era 8) pra texto do InputText ficar visualmente
        // centrado dentro do box com a Stolzl Regular (baseline mais baixa).
        const float framePadY = 10.f;
        const float textboxH = Fonts::Normal->FontSize + framePadY * 2.f;
        const float userBoxY = oy + 148.f;
        const float passBoxY = oy + 208.f;

        dl->AddText(wp + ImVec2(fieldX, oy + 130.f), Colors::TextSecondary, L(xorstr_("Username"),xorstr_("Usuário")));
        ImGui::SetCursorPos(ImVec2(fieldX, userBoxY));
        ImGui::PushItemWidth(fieldW);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(Colors::WidgetBG));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(Colors::Border));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, framePadY));
        ImGui::InputText("##user", username, sizeof(username));

        dl->AddText(wp + ImVec2(fieldX, oy + 190.f), Colors::TextSecondary, L(xorstr_("Password"),xorstr_("Senha")));
        ImGui::SetCursorPos(ImVec2(fieldX, passBoxY));
        ImGuiInputTextFlags passFlags = showPassword ? 0 : ImGuiInputTextFlags_Password;
        ImGui::InputText("##pass", password, sizeof(password), passFlags);
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        ImGui::PopItemWidth();

        ImGui::PushFont(Fonts::Icons);
        const char* eyeIcon = showPassword ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
        ImVec2 eyeSz = ImGui::CalcTextSize(eyeIcon);
        // Centro Y exato do textbox de senha: top + h/2.
        float passCenterY = passBoxY + textboxH * 0.5f;
        ImVec2 eyePos = wp + ImVec2(fieldX + fieldW - 30.f, passCenterY - eyeSz.y * 0.5f);
        ImRect eyeBB(eyePos - ImVec2(6, 6), eyePos + eyeSz + ImVec2(6, 6));
        bool eyeHover = ImGui::IsMouseHoveringRect(eyeBB.Min, eyeBB.Max);
        if (eyeHover) customHovered = true;
        if (eyeHover && ImGui::IsMouseClicked(0))
            showPassword = !showPassword;
        dl->AddText(eyePos, eyeHover ? Colors::TextPrimary : Colors::TextMuted, eyeIcon);
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(fieldX, oy + 245.f));
        UI::CheckBox(L(xorstr_("Remember me"),xorstr_("Lembrar de mim")), &rememberMe, true);

        if (disabled) ImGui::EndDisabled();

        bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter);
        ImGui::SetCursorPos(ImVec2(fieldX, oy + 280.f));

        if (disabled) {
            ImVec2 btnPos = wp + ImVec2(fieldX, oy + 280.f);
            float btnH = 34.f;
            dl->AddRectFilled(btnPos, btnPos + ImVec2(fieldW, btnH), IM_COL32(60, 30, 100, 255), 8.f);
            float dotAnim = fmodf((float)ImGui::GetTime() * 2.f, 3.f);
            int dots = (int)dotAnim + 1;
            char loadText[32];
            snprintf(loadText, sizeof(loadText), "%s%.*s", L(xorstr_("Authenticating"),xorstr_("Autenticando")), dots, "...");
            ImVec2 loadSz = ImGui::CalcTextSize(loadText);
            dl->AddText(
                ImVec2(btnPos.x + fieldW * 0.5f - loadSz.x * 0.5f, btnPos.y + btnH * 0.5f - loadSz.y * 0.5f),
                Colors::TextMuted, loadText);
            ImGui::SetCursorPosY(oy + 280.f + btnH);
        } else {
            if (UI::Button(L(xorstr_("Sign In"),xorstr_("Entrar")), ImVec2(fieldW, 34.f)) || enterPressed) {
                if (strlen(username) > 0 && strlen(password) > 0) {
                    DoAuthAsync(std::string(username), std::string(password));
                } else {
                    Toast::Warn(std::string(L(xorstr_("Enter username and password"),xorstr_("Digite usuário e senha"))));
                }
            }
        }

        ImGui::PushFont(Fonts::Small);
        {
            std::string forgotText = L(xorstr_("Forgot your password?"),xorstr_("Esqueceu sua senha?"));
            ImVec2 forgotSz = ImGui::CalcTextSize(forgotText.c_str());
            float forgotX = ox + W * 0.5f - forgotSz.x * 0.5f;
            float forgotY = oy + 322.f;
            ImRect forgotBB(wp + ImVec2(forgotX, forgotY), wp + ImVec2(forgotX + forgotSz.x, forgotY + forgotSz.y));
            bool forgotHover = ImGui::IsMouseHoveringRect(forgotBB.Min, forgotBB.Max);
            if (forgotHover) customHovered = true;
            ImU32 forgotCol = forgotHover ? IM_COL32(220, 150, 255, 255) : IM_COL32(170, 120, 220, 220);
            dl->AddText(forgotBB.Min, forgotCol, forgotText.c_str());
            if (forgotHover) {
                dl->AddLine(ImVec2(forgotBB.Min.x, forgotBB.Max.y + 1.f),
                            ImVec2(forgotBB.Max.x, forgotBB.Max.y + 1.f),
                            forgotCol, 1.f);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    Url::Open(xorstr_w(L"https://zimobr.com/forgot-password"));
            }
        }
        ImGui::PopFont();

        if (!statusText.empty() && !authenticating.load()) {
            ImGui::PushFont(Fonts::Small);
            ImVec2 stSz = ImGui::CalcTextSize(statusText.c_str());
            ImU32 stCol = authenticated.load() ? IM_COL32(80, 255, 80, 255) : IM_COL32(255, 80, 80, 255);
            dl->AddText(wp + ImVec2(winW * 0.5f - stSz.x * 0.5f, oy + 345.f), stCol, statusText.c_str());
            ImGui::PopFont();
        }

        ImGui::PushFont(Fonts::Small);
        ImVec2 verSz = ImGui::CalcTextSize(xorstr_("v1.0.1"));
        dl->AddText(p0 + ImVec2(W * 0.5f - verSz.x * 0.5f, H - 22.f), Colors::TextDisabled, xorstr_("v1.0.1"));
        ImGui::PopFont();

        ImGui::PopFont();

        {
            static bool dragging = false;
            static POINT dragStart{};
            static POINT winStart{};

            bool canStartDrag = ImGui::IsMouseHoveringRect(p0, p1) &&
                                !customHovered &&
                                !ImGui::IsAnyItemHovered() &&
                                !ImGui::IsAnyItemActive();

            if (canStartDrag && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
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

        ImGui::End();

        Toast::Render();
    }
}
