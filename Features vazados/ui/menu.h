#pragma once

#include <imgui.h>
#include <imgui_engine.h>
#include <string>
#include <cstdio>
#include <LazyDlls/Lazyimporter.hpp>
#include <core/config.h>
#include <core/config_io.h>
#include <core/context.h>
#include <core/url.h>
#include <core/profile.h>
#include <core/avatar.h>
#include <ui/theme.h>
#include <ui/fonts.h>
#include <ui/toast.h>
#include <core/lang.h>
#include <web/server.hpp>
#include <xorstr.h>

namespace UI {

    inline int ActiveTab = 0;
    inline int SubTabIndex = 0;

    inline void DrawRadarMiniMap(ImVec2 center, float radius) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddCircleFilled(center, radius, IM_COL32(20, 20, 28, 200), 64);
        dl->AddCircle(center, radius, Colors::CardBorder, 64, 1.5f);
        dl->AddCircle(center, radius * 0.66f, IM_COL32(60, 60, 70, 80), 48, 1.f);
        dl->AddCircle(center, radius * 0.33f, IM_COL32(60, 60, 70, 80), 32, 1.f);
        dl->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), IM_COL32(60, 60, 70, 80), 1.f);
        dl->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius), IM_COL32(60, 60, 70, 80), 1.f);

        // Yaw em radianos. Rotacao final: alpha = pi/2 - yaw faz a direcao
        // que o player encara apontar pra CIMA na tela (sentido natural de
        // radar).
        float yawDeg = Context::LocalAimAngles.y;
        float alpha  = 1.5707963f - yawDeg * 0.01745329f;
        float ca = cosf(alpha);
        float sa = sinf(alpha);

        // Marcador "N" do norte do mundo (yaw +Y mundial) — gira em volta
        // conforme o player vira a camera. Garante feedback visual de
        // rotacao mesmo quando nao tem inimigo no radar.
        {
            float nx_world = 0.f, ny_world = 1.f;
            float nx_r = nx_world * ca - ny_world * sa;
            float ny_r = nx_world * sa + ny_world * ca;
            float nr   = radius - 12.f;
            ImVec2 npos(center.x + nx_r * nr, center.y - ny_r * nr);
            ImVec2 ntextSz = ImGui::CalcTextSize("N");
            dl->AddText(ImVec2(npos.x - ntextSz.x * 0.5f, npos.y - ntextSz.y * 0.5f),
                IM_COL32(180, 180, 200, 220), "N");
        }

        // Player indicator: triangulo apontando pra cima (a rotacao ja foi
        // aplicada na world delta, entao "cima na tela" = "frente do player").
        ImVec2 p0(center.x,         center.y - 7.f);
        ImVec2 p1(center.x - 4.5f,  center.y + 4.f);
        ImVec2 p2(center.x + 4.5f,  center.y + 4.f);
        dl->AddTriangleFilled(p0, p1, p2, IM_COL32(80, 200, 255, 255));
        dl->AddTriangle(p0, p1, p2, IM_COL32(255, 255, 255, 220), 1.f);

        // Se o radar web estiver desligado, o preview nao mostra inimigos
        // (so a base do radar + N + triangulo do player). Mantem consistente
        // com o estado do servico.
        if (!Config::GameSettings.webRadar) return;

        Vec3 localPos = Context::LocalEyePos;
        float rangeMeters = 50.f;
        float scale = radius / (rangeMeters * Config::UNITS_PER_METER);

        std::lock_guard<std::mutex> lock(Context::EntitiesMutex);
        for (const auto& e : Context::Entities) {
            if (e.isLocal) continue;

            float wx = (e.position.x - localPos.x) * scale;
            float wy = (e.position.y - localPos.y) * scale;

            // Rotaciona a delta-mundial pra que +Y final = "frente do player".
            float dx = wx * ca - wy * sa;
            float dy = wx * sa + wy * ca;

            float distFromCenter = sqrtf(dx * dx + dy * dy);
            if (distFromCenter > radius - 2.f) {
                float angle = atan2f(dy, dx);
                dx = cosf(angle) * (radius - 4.f);
                dy = sinf(angle) * (radius - 4.f);
            }

            ImVec2 dot(center.x + dx, center.y - dy);
            ImU32 col = e.isTeam
                ? Config::GameSettings.colTeam.ToImU32()
                : (e.isVisible
                    ? Config::GameSettings.colEnemyVisible.ToImU32()
                    : Config::GameSettings.colEnemyHidden.ToImU32());
            dl->AddCircleFilled(dot, 3.5f, col, 12);
            dl->AddCircle(dot, 3.5f, IM_COL32(0, 0, 0, 180), 12, 1.f);
        }
    }

    inline void DrawMenu() {
        UI::SetupStyle();

        const float W = 700.f;
        const float H = 445.f;
        const float sidebarW = 90.f;
        // topBarH alinhado com o divider da logo do sidebar (y=50) pra ambas
        // bordas inferiores ficarem na mesma horizontal.
        const float topBarH = 50.f;
        const float subTabH = 45.f;
        {
            ImVec2 disp = ImGui::GetIO().DisplaySize;
            // Recentralizar quando o DisplaySize muda (Login -> Fullscreen ainda
            // tem o display antigo no primeiro frame, ImGuiCond_Appearing ja
            // disparou com display errado e a janela fica off-screen).
            static ImVec2 s_lastDisp(0.f, 0.f);
            ImGuiCond posCond = ImGuiCond_Appearing;
            if (s_lastDisp.x != disp.x || s_lastDisp.y != disp.y) {
                posCond = ImGuiCond_Always;
                s_lastDisp = disp;
            }
            ImGui::SetNextWindowPos(ImVec2((disp.x - W) * 0.5f, (disp.y - H) * 0.5f), posCond);
        }
        ImGui::SetNextWindowSize(ImVec2(W, H), ImGuiCond_Always);
        ImGui::Begin("##ZimoMenu", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 wp = ImGui::GetWindowPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(wp, wp + ImVec2(sidebarW, H), Colors::Sidebar);
        dl->AddLine(wp + ImVec2(sidebarW, 0), wp + ImVec2(sidebarW, H), Colors::Border);

        dl->AddRectFilled(wp + ImVec2(sidebarW, 0), wp + ImVec2(W, topBarH), Colors::TopBar);
        dl->AddLine(wp + ImVec2(sidebarW, topBarH), wp + ImVec2(W, topBarH), Colors::Border);

        {
            std::string uname = UI::Profile::Username.empty()
                ? std::string(L(xorstr_("Guest"),xorstr_("Convidado")))
                : UI::Profile::Username;
            std::string role = UI::Profile::Role.empty()
                ? std::string(L(xorstr_("User"),xorstr_("Usuário")))
                : UI::Profile::Role;

            {
                bool connected = (Config::GameWindow().load() != nullptr);
                std::string statusLabel = connected
                    ? std::string(L(xorstr_("Connected to CS2"),xorstr_("Conectado ao CS2")))
                    : std::string(L(xorstr_("Waiting for CS2"),xorstr_("Aguardando CS2")));

                ImVec2 statSz = Fonts::Small->CalcTextSizeA(Fonts::Small->FontSize, FLT_MAX, 0.f, statusLabel.c_str());
                float dotR = 3.f;
                float gapBetween = 5.f;

                float statX = sidebarW + 14.f;
                float statY = topBarH * 0.5f;

                ImU32 dotCol, txtCol;
                if (connected) {
                    dotCol = IM_COL32(60, 220, 100, 255);
                    txtCol = IM_COL32(140, 200, 150, 220);
                } else {
                    float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 4.f);
                    int alpha = (int)(160.f + 95.f * pulse);
                    dotCol = IM_COL32(255, 165, 50, alpha);
                    txtCol = IM_COL32(200, 170, 140, 200);
                }

                ImVec2 dotC = wp + ImVec2(statX, statY);
                dl->AddCircleFilled(dotC, dotR, dotCol, 12);

                dl->AddText(Fonts::Small, Fonts::Small->FontSize,
                    wp + ImVec2(statX + dotR + gapBetween, statY - statSz.y * 0.5f),
                    txtCol, statusLabel.c_str());
            }

            ImVec2 unameSz = Fonts::Normal->CalcTextSizeA(Fonts::Normal->FontSize, FLT_MAX, 0.f, uname.c_str());
            ImVec2 roleSz  = Fonts::Small->CalcTextSizeA(Fonts::Small->FontSize,  FLT_MAX, 0.f, role.c_str());
            float biggerW = unameSz.x > roleSz.x ? unameSz.x : roleSz.x;

            float avatarR = 14.f;
            float avatarCx = W - 28.f;
            float avatarCy = topBarH * 0.5f;
            float textRight = avatarCx - avatarR - 8.f;
            float textBlockX = textRight - biggerW;

            if (Avatar::HasAvatar()) {
                ImVec2 avMin = wp + ImVec2(avatarCx - avatarR, avatarCy - avatarR);
                ImVec2 avMax = wp + ImVec2(avatarCx + avatarR, avatarCy + avatarR);
                dl->AddCircleFilled(wp + ImVec2(avatarCx, avatarCy), avatarR + 1.5f, Colors::Accent, 32);
                dl->AddImageRounded(Avatar::GetTexture(), avMin, avMax,
                    ImVec2(0, 0), ImVec2(1, 1),
                    IM_COL32(255, 255, 255, 255), avatarR);
            } else {
                dl->AddCircleFilled(wp + ImVec2(avatarCx, avatarCy), avatarR, IM_COL32(60, 30, 100, 90), 32);
                dl->AddCircle(wp + ImVec2(avatarCx, avatarCy), avatarR, Colors::Accent, 32, 1.5f);
                ImGui::PushFont(Fonts::Icons);
                ImVec2 uIconSz = ImGui::CalcTextSize(ICON_FA_USER);
                dl->AddText(wp + ImVec2(avatarCx - uIconSz.x * 0.5f, avatarCy - uIconSz.y * 0.5f),
                    Colors::Accent, ICON_FA_USER);
                ImGui::PopFont();
            }

            dl->AddText(Fonts::Normal, Fonts::Normal->FontSize,
                wp + ImVec2(textBlockX, avatarCy - unameSz.y - 1.f),
                Colors::TextPrimary, uname.c_str());
            dl->AddText(Fonts::Small, Fonts::Small->FontSize,
                wp + ImVec2(textBlockX, avatarCy + 2.f),
                Colors::TextMuted, role.c_str());

            float divX = textBlockX - 12.f;
            dl->AddLine(wp + ImVec2(divX, 6.f), wp + ImVec2(divX, topBarH - 6.f), Colors::BorderSubtle, 1.f);
        }

        if (UI::LogoTexture != 0) {
            float logoSz = 40.f;
            float logoX = sidebarW * 0.5f - logoSz * 0.5f;
            ImGui::SetCursorPos(ImVec2(logoX, 5.f));
            ImGui::Image(UI::LogoTexture, ImVec2(logoSz, logoSz));
        }

        dl->AddLine(wp + ImVec2(10, 50), wp + ImVec2(sidebarW - 10, 50), Colors::BorderSubtle);

        const char* tabIcons[] = {
            ICON_FA_CROSSHAIRS, ICON_FA_BOLT, ICON_FA_EYE, ICON_FA_MAP, ICON_FA_COG
        };

        ImGui::SetCursorPos(ImVec2(0, 56));
        for (int i = 0; i < 5; i++) {
            ImGui::PushID(i);
            ImGui::SetCursorPosX(0);
            bool clicked = false;
            switch (i) {
                case 0: clicked = UI::SidebarTab(tabIcons[0], L(xorstr_("Aim"),xorstr_("Aim")),         ActiveTab == 0, Fonts::Icons, Fonts::Normal); break;
                case 1: clicked = UI::SidebarTab(tabIcons[1], L(xorstr_("Trigger"),xorstr_("Trigger")), ActiveTab == 1, Fonts::Icons, Fonts::Normal); break;
                case 2: clicked = UI::SidebarTab(tabIcons[2], L(xorstr_("ESP"),xorstr_("ESP")),         ActiveTab == 2, Fonts::Icons, Fonts::Normal); break;
                case 3: clicked = UI::SidebarTab(tabIcons[3], L(xorstr_("Radar"),xorstr_("Radar")),     ActiveTab == 3, Fonts::Icons, Fonts::Normal); break;
                case 4: clicked = UI::SidebarTab(tabIcons[4], L(xorstr_("Misc"),xorstr_("Misc")),       ActiveTab == 4, Fonts::Icons, Fonts::Normal); break;
            }
            if (clicked) { ActiveTab = i; SubTabIndex = 0; }
            ImGui::PopID();
        }


        float contentX = sidebarW;
        float contentY = topBarH;
        float contentW = W - sidebarW;
        float contentH = H - topBarH;

        bool hasSubTabs = false;

        float pad = 10.f;
        float cardGap = 10.f;
        float cardAreaW = contentW - pad * 2.f;
        float cardW = (cardAreaW - cardGap) * 0.5f;
        float cardH = contentH - pad * 2.f;

        ImGui::SetCursorPos(ImVec2(contentX + pad, contentY + pad));
        ImGui::BeginChild("##mainContent", ImVec2(cardAreaW, cardH), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        ImGui::PushFont(Fonts::Normal);

        if (ActiveTab == 0) {
            ImGui::BeginChild("##aim_l", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_CROSSHAIRS, L(xorstr_("General"),xorstr_("Geral")), ImVec2(cardW, cardH));
            UI::CheckBox(L(xorstr_("Enable Aimbot"),xorstr_("Ativar Aimbot")), &Config::GameSettings.aimbot, false,
                L(xorstr_("Main function responsible for making your crosshair follow and stick to enemies. While you hold the aim key, the cheat smoothly moves your mouse onto the closest target inside the FOV."),
                  xorstr_("Funcao principal responsavel por fazer sua mira seguir e grudar nos inimigos. Enquanto voce segura a tecla do aimbot, o cheat move sua mira suavemente pro alvo mais proximo dentro do FOV.")));
            UI::KeyBind(L(xorstr_("Aim Key"),xorstr_("Tecla do Aimbot")), Config::GameSettings.aimKey);
            UI::CheckBox(L(xorstr_("Visible Targets Only"),xorstr_("Apenas Alvos Visíveis")), &Config::GameSettings.aimRequireVisible, false,
                L(xorstr_("The aimbot will only pull onto enemies that are visible - it ignores anyone you can't see."),
                  xorstr_("O aimbot so vai puxar em inimigos que estao visiveis - ignora qualquer um que voce nao consegue ver.")));
            UI::CheckBox(L(xorstr_("Dynamic Bone"),xorstr_("Osso Dinâmico")), &Config::GameSettings.aimDynamic, false,
                L(xorstr_("Picks the bone closest to where you're aiming to be the target inside the enemy. This option overrides the Target Bone config when enabled."),
                  xorstr_("Escolhe o osso mais proximo da sua mira pra ser o alvo dentro do inimigo. Essa opcao sobrepoe a config de Osso Alvo quando ativada.")));
            UI::CheckBox(L(xorstr_("Highlight Target"),xorstr_("Destacar Alvo")), &Config::GameSettings.aimHighlight, false,
                L(xorstr_("Draws a marker on the enemy currently being targeted by the aimbot, so you can see who it's about to lock onto."),
                  xorstr_("Desenha um marcador no inimigo que o aimbot esta mirando no momento, pra voce saber em quem ele vai grudar.")));
            UI::CheckBox(L(xorstr_("Humanize"),xorstr_("Humanizar")), &Config::GameSettings.aimHumanize, false,
                L(xorstr_("Adds small random perturbations to the aim trajectory, simulating natural human imprecision and reducing the perfectly straight snap that gives away aimbots."),
                  xorstr_("Adiciona pequenas variacoes aleatorias na mira pra simular a imprecisao natural de um humano, evitando a puxada perfeitamente reta que denuncia o aimbot.")));
            UI::CheckBox(L(xorstr_("Assist Mode"),xorstr_("Modo Assist")), &Config::GameSettings.aimAssistMode, false,
                L(xorstr_("Instead of locking onto the target, slightly nudges your aim toward it. Keeps full mouse control and gives a subtle correction - much harder to detect."),
                  xorstr_("Em vez de travar no alvo, da um leve empurrao na mira pra perto dele. Voce mantem o controle do mouse e ganha uma correcao sutil - bem mais dificil de detectar.")));
            UI::CheckBox(L(xorstr_("Show FOV"),xorstr_("Mostrar FOV")), &Config::GameSettings.aimFovCircle, true,
                L(xorstr_("Draws a circle in the middle of your screen showing the area the aimbot watches. Any enemy that enters this circle becomes a valid target and the aim will follow them when you press the aim key."),
                  xorstr_("Desenha um circulo no meio da sua tela mostrando a area que o aimbot enxerga. Qualquer inimigo que entrar dentro desse circulo vira alvo valido e a mira vai seguir ele quando voce apertar a tecla do aimbot.")));
            UI::EndCard();
            ImGui::EndChild();

            ImGui::SameLine(0.f, cardGap);

            ImGui::BeginChild("##aim_r", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_COG, L(xorstr_("Configurations"),xorstr_("Configurações")), ImVec2(cardW, cardH));
            UI::SliderFloat(L(xorstr_("Aim FOV"),xorstr_("FOV do Aimbot")), &Config::GameSettings.aimFov, 5.f, 500.f, "%.0f px");
            UI::SliderFloat(L(xorstr_("Smoothness"),xorstr_("Suavidade")), &Config::GameSettings.aimSmooth, 0.f, 100.f, "%.0f%%");
            UI::SliderFloat(L(xorstr_("Max Distance"),xorstr_("Distância Máxima")), &Config::GameSettings.aimDistance, 1.f, 200.f, "%.0f m");
            {
                std::string boneNames[3] = { L(xorstr_("Head"),xorstr_("Cabeça")), L(xorstr_("Neck"),xorstr_("Pescoço")), L(xorstr_("Chest"),xorstr_("Peito")) };
                const char* bones[3] = { boneNames[0].c_str(), boneNames[1].c_str(), boneNames[2].c_str() };
                UI::Combo(L(xorstr_("Target Bone"),xorstr_("Osso Alvo")), &Config::GameSettings.aimBone, bones, 3);
            }
            UI::ColorEdit4(L(xorstr_("Target Highlight Color"),xorstr_("Cor do Alvo")), Config::GameSettings.colTarget.data());
            UI::ColorEdit4(L(xorstr_("Dynamic Bone Color"),xorstr_("Cor do Osso Dinâmico")), Config::GameSettings.colDynamic.data());
            UI::ColorEdit4(L(xorstr_("FOV Color"),xorstr_("Cor do FOV")), Config::GameSettings.colFovCircle.data(), true);
            UI::EndCard();
            ImGui::EndChild();
        }

        if (ActiveTab == 1) {
            ImGui::BeginChild("##trig_l", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_BOLT, L(xorstr_("Trigger"),xorstr_("Trigger")), ImVec2(cardW, cardH));
            UI::CheckBox(L(xorstr_("Enable Trigger"),xorstr_("Ativar Trigger")), &Config::GameSettings.triggerbot, false,
                L(xorstr_("With this option on and the selected key pressed, shots fire automatically the instant an enemy crosses your crosshair. Very useful with snipers."),
                  xorstr_("Com essa opcao ativa e a tecla selecionada pressionada, os disparos vao ser automaticos quando o inimigo passar na sua mira. Bastante util com snipers.")));
            UI::KeyBind(L(xorstr_("Trigger Key"),xorstr_("Tecla do Trigger")), Config::GameSettings.triggerKey);
            UI::SliderInt(L(xorstr_("Shot Delay"),xorstr_("Delay do Tiro")), &Config::GameSettings.triggerDelay, 0, 250, "%d ms", true);
            UI::EndCard();
            ImGui::EndChild();

            ImGui::SameLine(0.f, cardGap);

            ImGui::BeginChild("##trig_r", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_COG, L(xorstr_("RCS"),xorstr_("RCS")), ImVec2(cardW, cardH));
            UI::CheckBox(L(xorstr_("Enable RCS"),xorstr_("Ativar RCS")), &Config::GameSettings.rcs, false,
                L(xorstr_("Anti-recoil. When you fire in bursts, the rifle pulls upward - RCS automatically pulls your aim down by the same amount, keeping your crosshair on the enemy without you needing to drag the mouse down."),
                  xorstr_("Anti-recuo. Quando voce atira em rajada a arma sobe - o RCS puxa sua mira pra baixo sozinho na mesma proporcao, sua mira fica colada no inimigo sem voce precisar puxar o mouse pra baixo.")));
            UI::SliderFloat(L(xorstr_("RCS Strength"),xorstr_("Força do RCS")), &Config::GameSettings.rcsStrength, 0.f, 100.f, "%.0f%%", true);
            UI::EndCard();
            ImGui::EndChild();
        }

        if (ActiveTab == 2) {
            ImGui::BeginChild("##esp_l", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_EYE, L(xorstr_("Visual"),xorstr_("Visual")), ImVec2(cardW, cardH));
            UI::CheckBox(L(xorstr_("Enable ESP"),xorstr_("Ativar ESP")), &Config::GameSettings.esp);
            UI::CheckBox(L(xorstr_("Player Box"),xorstr_("Caixa do Jogador")), &Config::GameSettings.espBox);
            UI::CheckBox(L(xorstr_("Health Bar"),xorstr_("Barra de Vida")), &Config::GameSettings.espHealth);
            UI::CheckBox(L(xorstr_("Player Name"),xorstr_("Nome do Jogador")), &Config::GameSettings.espName);
            UI::CheckBox(L(xorstr_("Skeleton"),xorstr_("Esqueleto")), &Config::GameSettings.espSkeleton);
            UI::CheckBox(L(xorstr_("Weapon Name"),xorstr_("Nome da Arma")), &Config::GameSettings.espWeapon);
            UI::CheckBox(L(xorstr_("Status Flags"),xorstr_("Status do Jogador")), &Config::GameSettings.espFlags);
            UI::CheckBox(L(xorstr_("Look Direction"),xorstr_("Direção do Olhar")), &Config::GameSettings.espLookDir);
            UI::CheckBox(L(xorstr_("Nade Prediction"),xorstr_("Trajetória de Granada")), &Config::GameSettings.nadePrediction);
            UI::CheckBox(L(xorstr_("Bomb Timer"),xorstr_("Tempo da Bomba")), &Config::GameSettings.bombTimer, true);
            UI::EndCard();
            ImGui::EndChild();

            ImGui::SameLine(0.f, cardGap);

            ImGui::BeginChild("##esp_r", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_COG, L(xorstr_("Distance & Colors"),xorstr_("Distância & Cores")), ImVec2(cardW, cardH));
            UI::SliderFloat(L(xorstr_("Max Render Distance"),xorstr_("Distância Máxima")), &Config::GameSettings.espDistance, 1.f, 200.f, "%.0f m");
            UI::ColorEdit4(L(xorstr_("Enemy Visible"),xorstr_("Inimigo Visível")), Config::GameSettings.colEnemyVisible.data());
            UI::ColorEdit4(L(xorstr_("Enemy Hidden"),xorstr_("Inimigo Oculto")), Config::GameSettings.colEnemyHidden.data());
            UI::ColorEdit4(L(xorstr_("Teammate"),xorstr_("Aliado")), Config::GameSettings.colTeam.data());
            UI::ColorEdit4(L(xorstr_("Box"),xorstr_("Caixa")), Config::GameSettings.colBox.data());
            UI::ColorEdit4(L(xorstr_("Skeleton"),xorstr_("Esqueleto")), Config::GameSettings.colSkeleton.data());
            UI::ColorEdit4(L(xorstr_("Player Name"),xorstr_("Nome do Jogador")), Config::GameSettings.colName.data());
            UI::ColorEdit4(L(xorstr_("Weapon"),xorstr_("Arma")), Config::GameSettings.colWeapon.data());
            UI::ColorEdit4(L(xorstr_("Bomb Timer"),xorstr_("Tempo da Bomba")), Config::GameSettings.colBomb.data());
            UI::ColorEdit4(L(xorstr_("Nade Trajectory"),xorstr_("Trajetória da Granada")), Config::GameSettings.colNade.data());
            UI::ColorEdit4(L(xorstr_("Look Direction"),xorstr_("Direção do Olhar")), Config::GameSettings.colLookDir.data(), true);
            UI::EndCard();
            ImGui::EndChild();
        }

        if (ActiveTab == 3) {
            ImGui::BeginChild("##radar_l", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_GLOBE, L(xorstr_("Web Radar"),xorstr_("Radar Web")), ImVec2(cardW, cardH));
            UI::CheckBox(L(xorstr_("Enable Web Radar"),xorstr_("Ativar Radar Web")), &Config::GameSettings.webRadar);
            UI::SliderInt(L(xorstr_("Server Port"),xorstr_("Porta do Servidor")), &Config::GameSettings.webRadarPort, 1024, 65535, "%d");

            {
                static int  s_lastAppliedPort = -1;
                static bool s_lastEnabled     = false;
                static bool s_initialized     = false;
                static double s_pendingSince  = 0.0;
                bool sliderActive = ImGui::IsAnyItemActive();
                int  desiredPort = Config::GameSettings.webRadarPort;
                bool desiredEnabled = Config::GameSettings.webRadar;

                // Primeiro frame: sincroniza os statics com a config atual sem
                // disparar restart/stop ou toast. Senao quando o user entra na
                // aba pela primeira vez o port "muda" de -1 pra 8888 e abre
                // Web::Stop com toast "Servidor web parado" sem ter alterado
                // nada.
                if (!s_initialized) {
                    s_lastAppliedPort = desiredPort;
                    s_lastEnabled = desiredEnabled;
                    s_initialized = true;
                }

                bool portChanged    = (desiredPort != s_lastAppliedPort);
                bool enabledChanged = (desiredEnabled != s_lastEnabled);

                if ((portChanged || enabledChanged) && sliderActive) {
                    s_pendingSince = ImGui::GetTime();
                } else if ((portChanged || enabledChanged) && !sliderActive) {
                    double elapsed = ImGui::GetTime() - s_pendingSince;
                    if (s_pendingSince == 0.0 || elapsed > 0.35) {
                        if (desiredEnabled) {
                            Web::Restart(desiredPort);
                            Toast::Info(std::string(L(xorstr_("Web server applying..."),xorstr_("Aplicando servidor web..."))));
                        } else {
                            Web::Stop();
                            Toast::Info(std::string(L(xorstr_("Web server stopped"),xorstr_("Servidor web parado"))));
                        }
                        s_lastAppliedPort = desiredPort;
                        s_lastEnabled     = desiredEnabled;
                        s_pendingSince    = 0.0;
                    }
                } else {
                    s_pendingSince = 0.0;
                }
            }
            UI::Spacing(6.f);

            int activePort = Web::CurrentPort();
            bool running = Web::IsRunning();
            char urlBuf[64];
            snprintf(urlBuf, sizeof(urlBuf), "http://127.0.0.1:%d", activePort > 0 ? activePort : Config::GameSettings.webRadarPort);

            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::TextSecondary),
                running ? L(xorstr_("Server: Running"),xorstr_("Servidor: Ativo"))
                        : L(xorstr_("Server: Stopped"),xorstr_("Servidor: Parado")));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::Accent), "%s", urlBuf);
            UI::Spacing(6.f);

            if (UI::Button(L(xorstr_("Open in Browser"),xorstr_("Abrir no Navegador")), ImVec2(cardW - 28.f, 28.f))) {
                wchar_t wurl[128];
                int n = LI_CACHED(MultiByteToWideChar)(CP_UTF8, 0, urlBuf, -1, wurl, 128);
                if (n > 0) Url::Open(wurl);
            }
            UI::EndCard();
            ImGui::EndChild();

            ImGui::SameLine(0.f, cardGap);

            ImGui::BeginChild("##radar_r", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_MAP, L(xorstr_("Mini Map"),xorstr_("Mini Mapa")), ImVec2(cardW, cardH));
            {
                ImVec2 contentMin = ImGui::GetCursorScreenPos();
                ImVec2 contentAvail = ImGui::GetContentRegionAvail();
                float mapSize = (contentAvail.x < contentAvail.y ? contentAvail.x : contentAvail.y) - 10.f;
                if (mapSize < 60.f) mapSize = 60.f;
                ImVec2 center(contentMin.x + contentAvail.x * 0.5f, contentMin.y + contentAvail.y * 0.5f - 10.f);
                DrawRadarMiniMap(center, mapSize * 0.5f);

                ImGui::Dummy(ImVec2(contentAvail.x, mapSize + 4.f));
            }
            UI::EndCard();
            ImGui::EndChild();
        }

        if (ActiveTab == 4) {
            ImGui::BeginChild("##misc_l", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_COG, L(xorstr_("Features"),xorstr_("Funções")), ImVec2(cardW, cardH));
            UI::CheckBox(L(xorstr_("Ignore Teammates"),xorstr_("Ignorar Time")), &Config::GameSettings.teamCheck, false,
                L(xorstr_("Ignores teammates in every feature (Aimbot, Trigger, ESP). Recommended for competitive modes. WARNING: in Deathmatch and free-for-all the game flags everyone as your team, so leaving this on will hide enemies from the cheat - turn it off in those modes."),
                  xorstr_("Ignora os aliados em todas as funcoes (Aimbot, Trigger, ESP). Recomendado pra modos competitivos. ATENCAO: no Mata-Mata e no FFA o jogo marca todo mundo como do seu time, entao deixar essa opcao ligada esconde os inimigos do cheat - desligue nesses modos.")));
            {
                bool prev = Config::GameSettings.streamMode;
                UI::CheckBox(L(xorstr_("Stream Mode"),xorstr_("Stream Mode")), &Config::GameSettings.streamMode, false,
                    L(xorstr_("Makes the cheat panel and the ESP invisible to screen capture and screen recording tools (OBS, Discord screen share, Windows Game Bar, Print Screen, etc). You keep seeing everything normally."),
                      xorstr_("Deixa o painel do cheat e o ESP invisiveis em programas de captura e gravacao de tela (OBS, tela compartilhada do Discord, Game Bar do Windows, Print Screen, etc). Voce continua vendo tudo normalmente.")));
                if (Config::GameSettings.streamMode != prev) {
                    HWND hw = ImGuiEngine::GetHWND();
                    if (hw) LI_CACHED(SetWindowDisplayAffinity)(hw,
                        Config::GameSettings.streamMode ? 0x11u : 0x0u);
                }
            }
            UI::CheckBox(L(xorstr_("Discord RPC"),xorstr_("Discord RPC")), &Config::GameSettings.discordRpc, false,
                L(xorstr_("Shows your status on Discord (\"Playing CS2 with Zimo\") with the cheat's image and buttons to access support and buy. Toggling here turns it on/off live."),
                  xorstr_("Mostra seu status no Discord (\"Jogando CS2 com Zimo\") com a imagem do cheat e botoes pra suporte e compra. Ligar/desligar aqui aplica em tempo real.")));
            UI::CheckBox(L(xorstr_("No Flash"),xorstr_("Sem Flash")), &Config::GameSettings.noFlash, false,
                L(xorstr_("Cancels the blinding effect from flash grenades. You can still hear the bang, but your screen stays clean - no white flash, you see the enemy normally."),
                  xorstr_("Cancela o efeito de cegueira das granadas de luz. Voce ainda escuta o barulho, mas sua tela continua limpa - sem o branco da flash, voce continua enxergando o inimigo normalmente.")));
            {
                static const std::string kEN0 = xorstr_("English").c_str();
                static const std::string kPT0 = xorstr_("Português").c_str();
                const char* langItems[2] = { kEN0.c_str(), kPT0.c_str() };
                int langIdx = (Lang::CurrentLang == Lang::PT_BR) ? 1 : 0;
                int prevIdx = langIdx;
                UI::Combo(L(xorstr_("Language"),xorstr_("Idioma")), &langIdx, langItems, 2, true);
                if (langIdx != prevIdx)
                    Lang::CurrentLang = (langIdx == 1) ? Lang::PT_BR : Lang::EN;
            }
            UI::EndCard();
            ImGui::EndChild();

            ImGui::SameLine(0.f, cardGap);

            ImGui::BeginChild("##misc_r", ImVec2(cardW, cardH), false, ImGuiWindowFlags_NoBackground);
            UI::BeginCard(ICON_FA_COG, L(xorstr_("Config"),xorstr_("Config")), ImVec2(cardW, cardH));
            UI::KeyBind(L(xorstr_("Menu Toggle Key"),xorstr_("Tecla pra Abrir/Fechar Menu")), Config::MenuKey);
            UI::SliderInt(L(xorstr_("Menu FPS Limit"),xorstr_("Limite de FPS do Menu")), &Config::GameSettings.fpsLimit, 15, 240, "%d");
            UI::Spacing(8.f);
            if (UI::Button(L(xorstr_("Save Config"),xorstr_("Salvar Config")), ImVec2(cardW - 28.f, 30.f))) {
                Config::Save();
                Toast::Success(std::string(L(xorstr_("Config saved"),xorstr_("Config salva"))));
            }
            UI::Spacing(4.f);
            if (UI::Button(L(xorstr_("Load Config"),xorstr_("Carregar Config")), ImVec2(cardW - 28.f, 30.f))) {
                Config::Load();
                Toast::Info(std::string(L(xorstr_("Config loaded"),xorstr_("Config carregada"))));
            }
            UI::Spacing(8.f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.12f, 0.12f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.18f, 0.18f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.08f, 0.08f, 1.f));
            if (UI::Button(L(xorstr_("Unload Cheat"),xorstr_("Encerrar Cheat")), ImVec2(cardW - 28.f, 30.f))) {
                Config::ShutdownRequested().store(true);
            }
            ImGui::PopStyleColor(3);
            UI::EndCard();
            ImGui::EndChild();
        }

        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::End();
    }

}
