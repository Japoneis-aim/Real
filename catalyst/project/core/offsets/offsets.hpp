#pragma once

class offsets
{
public:
	bool initialize( );

	// ─── client.dll ─────────────────────────────────────────────────────────
	std::uintptr_t csgo_input{ 0 };
	std::uintptr_t entity_list{ 0 };
	std::uintptr_t local_player_controller{ 0 };
	std::uintptr_t global_vars{ 0 };
	std::uintptr_t view_matrix{ 0 };

	// Novos offsets — carregados via offsets_map::get() em runtime
	// (pattern scan dinâmico, não hardcoded)
	std::uintptr_t planted_c4{ 0 };       // dwPlantedC4
	std::uintptr_t prediction{ 0 };        // dwPrediction
	std::uintptr_t sensitivity{ 0 };       // dwSensitivity
	std::uintptr_t view_angles{ 0 };       // dwViewAngles
	std::uintptr_t view_render{ 0 };       // dwViewRender
	std::uintptr_t weapon_c4{ 0 };         // dwWeaponC4

	// ─── engine2.dll ────────────────────────────────────────────────────────
	std::uintptr_t build_number{ 0 };              // dwBuildNumber
	std::uintptr_t network_game_client{ 0 };       // dwNetworkGameClient
};
