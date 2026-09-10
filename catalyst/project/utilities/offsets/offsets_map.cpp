#include <stdafx.hpp>
#include "offsets_map.hpp"

#include <fstream>
#include <unordered_map>
#include <charconv>

namespace offsets_map {

	namespace {
		static std::unordered_map<std::string, std::uintptr_t> s_map{};
	}

	bool load_from_file( std::string_view path )
	{
		s_map.clear( );

		std::ifstream file( path.data( ) );
		if ( !file.is_open( ) )
			return false;

		std::string line;
		while ( std::getline( file, line ) )
		{
			// Strip comments and empty lines
			const auto comment = line.find( '#' );
			if ( comment != std::string::npos )
				line.erase( comment );

			// Trim leading/trailing whitespace
			const auto first = line.find_first_not_of( " \t\r\n" );
			if ( first == std::string::npos )
				continue;
			const auto last = line.find_last_not_of( " \t\r\n" );
			line = line.substr( first, last - first + 1 );

			if ( line.empty( ) )
				continue;

			// Split on whitespace: KEY VALUE
			const auto sep = line.find_first_of( " \t" );
			if ( sep == std::string::npos )
				continue;

			auto key = line.substr( 0, sep );
			const auto val_start = line.find_first_not_of( " \t", sep );
			if ( val_start == std::string::npos )
				continue;

			auto val_str = line.substr( val_start );

			// Parse hex (0x...) or decimal
			std::uintptr_t value = 0;
			const char* begin = val_str.data( );
			const char* end   = begin + val_str.size( );

			int base = 10;
			if ( val_str.size( ) > 2 && val_str[ 0 ] == '0' && ( val_str[ 1 ] == 'x' || val_str[ 1 ] == 'X' ) )
			{
				base  = 16;
				begin += 2;
			}

			std::from_chars( begin, end, value, base );
			s_map.emplace( std::move( key ), value );
		}

		return true;
	}

	std::uintptr_t get( std::string_view key )
	{
		const auto it = s_map.find( std::string( key ) );
		return it != s_map.end( ) ? it->second : 0u;
	}

} // namespace offsets_map
