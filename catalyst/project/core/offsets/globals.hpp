#pragma once

#include <cstddef>

// globals.hpp — offsets de estruturas globais do CS2 que não são campos de schema.
//
// Estes offsets descrevem campos dentro de CGlobalVarsBase (apontada por g::offsets.global_vars)
// e são fixos na estrutura do engine — não variam por update de schema de entidade.
//
// NÃO colocar aqui offsets de entidades (C_BaseEntity, C_CSPlayerPawn, etc.).
// Para campos de entidade use SCHEMA("ClassName", "field"_hash).

namespace cs2 {

	// CGlobalVarsBase — offsets dentro da struct apontada por g::offsets.global_vars
	// Referência: CS2 SDK / reverse engineering confirmado em múltiplas versões.
	constexpr std::uintptr_t global_vars_map_name  = 0x188; // ptr para string com nome do mapa atual
	constexpr std::uintptr_t global_vars_cur_time  = 0x030; // float curtime (tempo de jogo em segundos)

} // namespace cs2
