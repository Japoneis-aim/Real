#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>

// Simple offsets loader: reads a map file with lines "KEY 0xVALUE" and exposes get(key).
// Place a generated offsets file at project_root/output/offsets.map before compiling.
// Namespace renamed to 'offsets_map' to avoid collision with utilities::offsets class.
namespace offsets_map {

	inline std::unordered_map<std::string, std::uintptr_t> table{};

	inline bool load_from_file( const std::string& path = "output/offsets.map" )
	{
		table.clear();

		std::ifstream f( path );
		if ( !f )
			return false;

		std::string line;
		while ( std::getline( f, line ) )
		{
			std::istringstream ss( line );
			std::string key, val;
			if ( !( ss >> key >> val ) )
				continue;

			try
			{
				const auto v = std::stoull( val, nullptr, 0 );
				table[ key ] = static_cast< std::uintptr_t >( v );
			}
			catch ( ... )
			{
				continue;
			}
		}

		return !table.empty();
	}

	inline std::uintptr_t get( const std::string& key )
	{
		const auto it = table.find( key );
		if ( it == table.end() )
			return 0;
		return it->second;
	}

}
