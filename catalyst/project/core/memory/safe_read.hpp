#pragma once

#include <optional>
#include <type_traits>
#include <cstdint>
#include <cstddef>
#include "../../utilities/memory/memory.hpp"

// safe_read.hpp — utilitários de leitura de memória com retorno opcional.
// Usado principalmente por schemas.cpp para walkthrough de estruturas internas.
//
// Para leitura simples prefira g::memory.read<T>(addr) diretamente.
// Use safe_read_opt quando precisar distinguir falha de leitura de valor zero.
namespace memutil {

	template<typename T>
	inline std::optional<T> safe_read_opt( std::uintptr_t addr )
	{
		if ( !addr )
			return std::nullopt;

		// Apenas tipos trivially copyable são suportados com garantia de detecção de falha.
		// Para tipos não-triviais (ex: std::string) a leitura via RPM não faz sentido —
		// use g::memory.read_string() ou APIs específicas.
		static_assert(
			std::is_trivially_copyable_v<T>,
			"safe_read_opt<T>: T deve ser trivially copyable. "
			"Para strings use g::memory.read_string(). "
			"Para tipos complexos use g::memory.read<T>() diretamente."
		);

		T out{};
		if ( !g::memory.read( addr, &out, sizeof( T ) ) )
			return std::nullopt;

		return out;
	}

	inline bool safe_read_bytes( std::uintptr_t addr, void* dest, std::size_t size )
	{
		if ( !addr || !dest || size == 0 )
			return false;

		return g::memory.read( addr, dest, size );
	}

}
