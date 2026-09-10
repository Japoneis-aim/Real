Português Br 🇧🇷
CS2 External Cheat — Open Source

Um projeto open source de cheat para Counter-Strike 2, focado em features de combat (aimbot, RCS, triggerbot), ESP (players, itens, projéteis) e misc (radar, bombtimer, grenades, wallbang indicator, impacts, speed ESP).

Qualquer pessoa pode baixar o .exe pronto ou clonar o código-fonte para estudar, modificar e compilar a sua própria versão.

📦 O que está incluído

⚔️ Combat

Feature	Descrição
Aimbot (legit)	Seleção de alvo por FOV + distância normalizada, com penalty para alvos ocultos (wallbang). Suporte a multipoint, visible-only, autowall e head-only.
Aim Assist	Pull suave em direção ao alvo com dead-zone por histerese (evita chattering).
Smoothing	Exponential smoothing frame-rate independente (com floor de dt para resistir a jitter do scheduler do Windows).
Humanize	Jitter proporcional ao delta com amplitude constante (não escala com distância).
AimController	Pipeline centralizado: assist → smooth → humanize → hard-lock. Mantém sub-pixel error entre frames.
RCS standalone	Compensação de recuo vertical e horizontal via m_vecCsViewPunchAngle, com toggle por tecla, cache de deg_per_count e step suave de mouse.
Triggerbot	Trace crosshair → cápsulas de hitbox, autowall opcional, hitchance dinâmica com delay adaptativo, gates de cursor visível / velocidade Z.
Penetration	Sistema completo de autowall: superfícies especiais, compute_damage_loss, scale_damage com escalas de time (CT/T) cacheadas no shared::tick().
Hitchance	32 samples com early-exit, cache de ângulo/posição e RNG valve-style.
👁️ ESP

Feature	Descrição
Player ESP	Box (full/corner), skeleton, hitboxes (capsule 2D merge via poly2d), health/ammo bars, name, weapon (ícone + texto), flags (money, armor, kit, zoom, defusing, flashed, ping, distance), look-direction arrow.
Item ESP	Ícone, nome e munição com filtros por categoria (rifles, smgs, shotguns, snipers, pistols, heavy, grenades, utility).
Projectile ESP	Ícones, nomes, timer bar (molotov, smoke, decoy) e bounds do inferno (convex hull das fire points).
Radar	Painel 2D custom + opcionalmente seta m_bSpotted em inimigos.
Grenades	Simulação física de trajetória (gravity, bounce, clip velocity, detonação por superfície para molotov).
Impacts	Rastreia m_vecBulletImpacts de cada player e renderiza com fade por tempo.
Wallbang Indicator	Mostra se a linha de tiro atual penetra parede e dano estimado.
Bomb Timer	Timer da C4, defuse, defuser name, dano estimado por distância.
Speed ESP	Velocidade XY atual com cor por faixa (normal / walk / bhop).
🏗️ Arquitetura

features::combat::g_shared — contexto compartilhado (arma ativa, inaccuracy, spread, recoil, escalas de dano) atualizado uma vez por tick com shared_mutex. Todas as features leem cópia thread-safe via ctx().
features::combat::g_rcs — RCS standalone; offsets de punch centralizados em g_shared (lazy-init thread-safe).
features::combat::g_legit — aimbot / triggerbot / FOV circle.
features::combat::AimController — pipeline de mira persistente entre frames.
features::esp::* — players, itens, projéteis.
features::misc::* — radar, grenades, impacts, wallbang, bombtimer, speed.
Sistema de coleta (systems::g_collector) com batches (read_batch_fixed<N>) para reduzir ReadProcessMemory por tick. Bones e bounds são cacheados no collector com dirty-flag.

🔧 Como usar

Opção 1 — Baixar o .exe pronto

Vá em Releases e baixe a versão mais recente.
Execute como administrador (necessário para leitura de memória externa).
Abra o CS2 e configure as features pelo menu.
Opção 2 — Compilar do código-fonte

bash
git clone https://github.com/<seu-user>/<seu-repo>.git
cd <seu-repo>
# Abra a solution no Visual Studio (C++20) e compile em Release x64
Requisitos:

Visual Studio 2022 (C++20)
Windows SDK
DiretX 11 (para o overlay zdraw)
Dependências já inclusas em external/
⚠️ Aviso

Este projeto é distribuído apenas para fins educacionais — estudo de reversing, matemática de penetração, simulação física e renderização 2D.

Não me responsabilizo por bans, uso indevido ou violação dos Termos de Serviço da Valve.
Use em offline / bots / servidores próprios.
Se for usar online, é por sua conta e risco.
📜 Licença

Open source — sinta-se livre para forkar, modificar e redistribuir. Contribuições são bem-vindas via PR.
English 🇺🇸 
CS2 Cheat — Open Source

An open source cheat project for Counter-Strike 2, focused on combat features (aimbot, RCS, triggerbot), ESP (players, items, projectiles) and misc features (radar, bomb timer, grenades, wallbang indicator, impacts, speed ESP).

Anyone can download the ready-to-use .exe or clone the source code to study, modify and build their own version.

📦 What's included

⚔️ Combat

Feature	Description
Aimbot (legit)	Target selection by FOV + normalized distance, with penalty for occluded targets (wallbang). Supports multipoint, visible-only, autowall and head-only.
Aim Assist	Smooth pull toward the target with hysteresis-based dead-zone (prevents chattering).
Smoothing	Frame-rate independent exponential smoothing (with dt floor to resist Windows scheduler jitter).
Humanize	Jitter proportional to the delta with constant amplitude (does not scale with distance).
AimController	Centralized pipeline: assist → smooth → humanize → hard-lock. Keeps sub-pixel error between frames.
Standalone RCS	Vertical and horizontal recoil compensation via m_vecCsViewPunchAngle, with key toggle, deg_per_count cache and smooth mouse stepping.
Triggerbot	Crosshair trace → hitbox capsules, optional autowall, dynamic hitchance with adaptive delay, cursor-visible / Z-velocity gates.
Penetration	Full autowall system: special surfaces, compute_damage_loss, scale_damage with team-based (CT/T) scales cached in shared::tick().
Hitchance	32 samples with early-exit, angle/position cache and valve-style RNG.
👁️ ESP

Feature	Description
Player ESP	Box (full/corner), skeleton, hitboxes (2D capsule merge via poly2d), health/ammo bars, name, weapon (icon + text), flags (money, armor, kit, zoom, defusing, flashed, ping, distance), look-direction arrow.
Item ESP	Icon, name and ammo with per-category filters (rifles, smgs, shotguns, snipers, pistols, heavy, grenades, utility).
Projectile ESP	Icons, names, timer bar (molotov, smoke, decoy) and inferno bounds (convex hull of fire points).
Radar	Custom 2D panel + optional m_bSpotted write on enemies.
Grenades	Trajectory physics simulation (gravity, bounce, clip velocity, molotov surface detonation).
Impacts	Tracks each player's m_vecBulletImpacts and renders with time-based fade.
Wallbang Indicator	Shows whether the current shot line penetrates the wall and estimated damage.
Bomb Timer	C4 timer, defuse, defuser name, distance-based estimated damage.
Speed ESP	Current XY velocity with color by tier (normal / walk / bhop).
🏗️ Architecture

features::combat::g_shared — shared context (active weapon, inaccuracy, spread, recoil, damage scales) updated once per tick with shared_mutex. All features read a thread-safe copy via ctx().
features::combat::g_rcs — standalone RCS; punch offsets centralized in g_shared (thread-safe lazy-init).
features::combat::g_legit — aimbot / triggerbot / FOV circle.
features::combat::AimController — persistent aim pipeline between frames.
features::esp::* — players, items, projectiles.
features::misc::* — radar, grenades, impacts, wallbang, bomb timer, speed.
Collector system (systems::g_collector) with batches (read_batch_fixed<N>) to minimize ReadProcessMemory calls per tick. Bones and bounds are cached in the collector with a dirty-flag.

🔧 How to use

Option 1 — Download the ready .exe

Go to Releases and download the latest version.
Run as administrator (required for external memory reading).
Launch CS2 and configure the features from the menu.
Option 2 — Build from source

bash
git clone https://github.com/<your-user>/<your-repo>.git
cd <your-repo>
# Open the solution in Visual Studio (C++20) and build in Release x64
Requirements:

Visual Studio 2022 (C++20)
Windows SDK
DirectX 11 (for the zdraw overlay)
Dependencies already included in external/
⚠️ Disclaimer

This project is distributed for educational purposes only — studying reversing, penetration math, physics simulation and 2D rendering.

I am not responsible for bans, misuse or violations of Valve's Terms of Service.
Use it offline / with bots / on your own servers.
If you use it online, you do so at your own risk.
📜 License

Open source — feel free to fork, modify and redistribute. Contributions are welcome via PR
