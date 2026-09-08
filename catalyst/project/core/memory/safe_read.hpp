#pragma once

#include <optional>
#include <type_traits>
#include <cstdint>
#include <cstddef>
#include "../../utilities/memory/memory.hpp"

// Safety wrapper around g::memory.read for typed and raw reads.
// Use explicit template read and the buffer read signature defined in utilities.
namespace memutil {

	template<typename T>
	inline std::optional<T> safe_read_opt( std::uintptr_t addr )
	{
		if ( !addr )
			return std::nullopt;

		// Prefer the templated reader when available; for trivially copyable types
		// we conservatively use the buffer read signature to detect failures.
		if constexpr ( std::is_trivially_copyable_v<T> )
		{
			T out{};
			if ( !g::memory.read( addr, &out, sizeof( T ) ) )
				return std::nullopt;

			return out;
		}
		else
		{
			// Use templated read which typically returns a value.
			T value = g::memory.read<T>( addr );
			return value;
		}
	}

	inline bool safe_read_bytes( std::uintptr_t addr, void* dest, std::size_t size )
	{
		if ( !addr || !dest || size == 0 )
			return false;

		return g::memory.read( addr, dest, size );
	}

}
