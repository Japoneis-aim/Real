#pragma once

#define NOMINMAX

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "lz4/lz4.h"
#include "fnv1a.hpp"
#include "zdraw/zdraw.hpp"

namespace config {

	enum class field_type : std::uint8_t
	{
		bool_val,
		int_val,
		uint8_val,
		float_val,
		color,
		float3,
		bool_array,
		string_val,
		custom
	};

	struct float3
	{
		float x{}, y{}, z{};
	};

	struct field
	{
		std::uint32_t key;
		field_type type;
		void* ptr;
		std::uint32_t count;
	};

	struct custom_field
	{
		virtual ~custom_field( ) = default;
		virtual void serialize( std::vector<std::uint8_t>& out ) const = 0;
		virtual void deserialize( const std::uint8_t* data, std::size_t len ) = 0;
	};

	namespace detail {

		struct config_registry
		{
			std::vector<field> fields{};
			std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> defaults{};
		};

		inline config_registry& get_registry( )
		{
			static config_registry r{};
			return r;
		}

		inline std::uint32_t make_key( std::string_view category, std::string_view name )
		{
			char buf[ 256 ]{};
			const auto cat_len = std::min( category.size( ), std::size_t{ 120 } );
			const auto name_len = std::min( name.size( ), std::size_t{ 120 } );

			std::memcpy( buf, category.data( ), cat_len );
			buf[ cat_len ] = '.';
			std::memcpy( buf + cat_len + 1, name.data( ), name_len );
			buf[ cat_len + 1 + name_len ] = '\0';

			return fnv1a::runtime_hash( buf );
		}

		inline void register_field( field f )
		{
			get_registry( ).fields.push_back( f );
		}

		inline void unregister_ptr( void* ptr )
		{
			auto& v = get_registry( ).fields;
			v.erase( std::remove_if( v.begin( ), v.end( ), [ ptr ]( const field& f ) { return f.ptr == ptr; } ), v.end( ) );
		}

	} // namespace detail

	template <typename T>
	struct val
	{
		T value{};

		val( ) = default;

		explicit val( T v ) : value{ v } {}

		val( T v, std::string_view category, std::string_view name ) : value{ v }
		{
			reg( category, name );
		}

		void reg( std::string_view category, std::string_view name )
		{
			constexpr auto ft = [ ]( )
				{
					if constexpr ( std::is_same_v<T, bool> )              return field_type::bool_val;
					else if constexpr ( std::is_same_v<T, int> )          return field_type::int_val;
					else if constexpr ( std::is_same_v<T, float> )        return field_type::float_val;
					else if constexpr ( std::is_same_v<T, std::uint8_t> ) return field_type::uint8_val;
					else static_assert( !sizeof( T ), "unsupported type for config::val" );
				}( );

			detail::register_field( { .key = detail::make_key( category, name ), .type = ft, .ptr = &this->value, .count = 1 } );
		}

		operator T& ( ) noexcept { return value; }
		operator const T& ( ) const noexcept { return value; }
		val& operator=( T v ) noexcept { value = v; return *this; }
	};

	struct col
	{
		zdraw::rgba value{};

		col( ) = default;

		explicit col( zdraw::rgba v ) : value{ v } {}

		col( zdraw::rgba v, std::string_view category, std::string_view name ) : value{ v }
		{
			reg( category, name );
		}

		void reg( std::string_view category, std::string_view name )
		{
			detail::register_field( { .key = detail::make_key( category, name ), .type = field_type::color, .ptr = &this->value, .count = 1 } );
		}

		operator zdraw::rgba& ( ) noexcept { return value; }
		operator const zdraw::rgba& ( ) const noexcept { return value; }
		col& operator=( const zdraw::rgba& v ) noexcept { value = v; return *this; }
	};

	struct vec3
	{
		float3 value{};

		vec3( ) = default;

		explicit vec3( float3 v ) : value{ v } {}

		vec3( float3 v, std::string_view category, std::string_view name ) : value{ v }
		{
			reg( category, name );
		}

		void reg( std::string_view category, std::string_view name )
		{
			detail::register_field( { .key = detail::make_key( category, name ), .type = field_type::float3, .ptr = &this->value, .count = 1 } );
		}

		operator float3& ( ) noexcept { return value; }
		operator const float3& ( ) const noexcept { return value; }
		vec3& operator=( const float3& v ) noexcept { value = v; return *this; }
	};

	template <typename E>
	struct enm
	{
		static_assert( std::is_enum_v<E>, "enm requires an enum type" );

		E value{};

		enm( ) = default;

		explicit enm( E v ) : value{ v } {}

		enm( E v, std::string_view category, std::string_view name ) : value{ v }
		{
			reg( category, name );
		}

		void reg( std::string_view category, std::string_view name )
		{
			constexpr auto ft = ( sizeof( E ) == 1 ) ? field_type::uint8_val : field_type::int_val;

			detail::register_field( { .key = detail::make_key( category, name ), .type = ft, .ptr = &this->value, .count = 1 } );
		}

		operator E& ( ) noexcept { return value; }
		operator const E& ( ) const noexcept { return value; }
		enm& operator=( E v ) noexcept { value = v; return *this; }
	};

	template <std::uint32_t N>
	struct bools
	{
		bool values[ N ]{};

		bools( ) = default;

		explicit bools( std::initializer_list<bool> init )
		{
			auto i{ 0u };

			for ( auto v : init )
			{
				if ( i >= N )
				{
					break;
				}

				values[ i++ ] = v;
			}
		}

		bools( std::initializer_list<bool> init, std::string_view category, std::string_view name )
		{
			auto i{ 0u };

			for ( auto v : init )
			{
				if ( i >= N )
				{
					break;
				}

				values[ i++ ] = v;
			}

			reg( category, name );
		}

		void reg( std::string_view category, std::string_view name )
		{
			detail::register_field( { .key = detail::make_key( category, name ), .type = field_type::bool_array, .ptr = this->values, .count = N } );
		}

		bool& operator[]( std::size_t i ) { return values[ i ]; }
		const bool& operator[]( std::size_t i ) const { return values[ i ]; }
		operator bool* ( ) noexcept { return values; }
		operator const bool* ( ) const noexcept { return values; }
	};

	struct str
	{
		std::string value{};

		str( ) = default;

		explicit str( std::string_view v ) : value{ v } {}

		str( std::string_view v, std::string_view category, std::string_view name ) : value{ v }
		{
			reg( category, name );
		}

		void reg( std::string_view category, std::string_view name )
		{
			detail::register_field( { .key = detail::make_key( category, name ), .type = field_type::string_val, .ptr = &this->value, .count = 1 } );
		}

		operator std::string& ( ) noexcept { return value; }
		operator const std::string& ( ) const noexcept { return value; }
		str& operator=( const std::string& v ) noexcept { value = v; return *this; }
		str& operator=( std::string_view v ) noexcept { value = v; return *this; }

		[[nodiscard]] const char* c_str( ) const noexcept { return value.c_str( ); }
		[[nodiscard]] bool empty( ) const noexcept { return value.empty( ); }
	};

	template <std::size_t N>
	struct fixed_string
	{
		char data[ N ]{};

		consteval fixed_string( const char( &s )[ N ] )
		{
			for ( std::size_t i = 0; i < N; ++i ) data[ i ] = s[ i ];
		}

		consteval std::string_view view( ) const noexcept { return { data, N - 1 }; }
	};

	template <std::size_t N>
	fixed_string( const char( & )[ N ] ) -> fixed_string<N>;

	template <typename T>
	consteval field_type type_to_field_type( )
	{
		if constexpr ( std::is_same_v<T, bool> )              return field_type::bool_val;
		else if constexpr ( std::is_same_v<T, int> )          return field_type::int_val;
		else if constexpr ( std::is_same_v<T, std::uint8_t> ) return field_type::uint8_val;
		else if constexpr ( std::is_same_v<T, float> )        return field_type::float_val;
		else if constexpr ( std::is_same_v<T, zdraw::rgba> )  return field_type::color;
		else if constexpr ( std::is_same_v<T, float3> )       return field_type::float3;
		else if constexpr ( std::is_same_v<T, std::string> )  return field_type::string_val;
		else if constexpr ( std::is_enum_v<T> )               return ( sizeof( T ) == 1 ) ? field_type::uint8_val : field_type::int_val;
		else static_assert( !sizeof( T ), "unsupported type for config::field" );
	}

	template <typename T, fixed_string Name>
	struct field_t
	{
		using value_type = T;
		static constexpr std::string_view name_view{ Name.view( ) };

		T value{};

		constexpr field_t( ) = default;
		constexpr field_t( T v ) : value{ v } {}

		operator T& ( ) noexcept { return value; }
		operator const T& ( ) const noexcept { return value; }
		field_t& operator=( T v ) noexcept { value = v; return *this; }
	};

	template <typename T, fixed_string Name>
	using f = field_t<T, Name>;

	template <std::uint32_t N, fixed_string Name>
	struct bools_t
	{
		static constexpr std::uint32_t count = N;
		static constexpr std::string_view name_view{ Name.view( ) };

		bool values[ N ]{};

		constexpr bools_t( ) = default;
		constexpr bools_t( std::initializer_list<bool> init )
		{
			std::size_t i{};
			for ( auto v : init )
			{
				if ( i >= N )
				{
					break;
				}

				values[ i++ ] = v;
			}
		}

		bool& operator[]( std::size_t i ) noexcept { return values[ i ]; }
		const bool& operator[]( std::size_t i ) const noexcept { return values[ i ]; }
		operator bool* ( ) noexcept { return values; }
		operator const bool* ( ) const noexcept { return values; }
	};

	template <std::uint32_t N, fixed_string Name>
	using b = bools_t<N, Name>;

	template <typename T, fixed_string Name>
	struct group_t : T
	{
		static constexpr std::string_view name_view{ Name.view( ) };
		using nested_config_marker = void;

		using T::T;
	};

	template <typename T, fixed_string Name>
	using g = group_t<T, Name>;

	template <typename Member>
	inline void register_one( Member& m, std::string_view category )
	{
		using M = std::remove_cvref_t<Member>;

		if constexpr ( requires { typename M::nested_config_marker; M::name_view; } )
		{
			std::string cat;
			cat.reserve( category.size( ) + 3 + M::name_view.size( ) );
			cat.append( category );
			cat.append( " - " );
			cat.append( M::name_view );
			m.register_config( cat );
		}
		else if constexpr ( requires { typename M::value_type; M::name_view; } && !requires { M::count; } )
		{
			using T = typename M::value_type;
			detail::register_field( { .key = detail::make_key( category, M::name_view ), .type = type_to_field_type<T>( ), .ptr = &m.value, .count = 1 } );
		}
		else if constexpr ( requires { M::count; M::name_view; } )
		{
			detail::register_field( { .key = detail::make_key( category, M::name_view ), .type = field_type::bool_array, .ptr = m.values, .count = M::count } );
		}
		else
		{
			static_assert( !sizeof( M ), "register_one: member type is not a config field" );
		}
	}

	template <typename... Members>
	inline void register_members( std::string_view category, Members&... members )
	{
		( register_one( members, category ), ... );
	}

#define CONFIG_MEMBERS(...)                                                    \
    void register_config( std::string_view category ) {                        \
        ::config::register_members( category, __VA_ARGS__ );                   \
    }

	template <typename T, std::size_t N>
	inline void register_array( std::array<T, N>& arr, std::string_view base, const std::array<std::string_view, N>& suffixes )
	{
		for ( std::size_t i = 0; i < N; ++i )
		{
			std::string cat;
			cat.reserve( base.size( ) + 3 + suffixes[ i ].size( ) );
			cat.append( base );
			cat.append( " - " );
			cat.append( suffixes[ i ] );
			arr[ i ].register_config( cat );
		}
	}

	namespace binary {

		inline void write_u8( std::vector<std::uint8_t>& b, std::uint8_t v )
		{
			b.push_back( v );
		}

		inline void write_u32( std::vector<std::uint8_t>& b, std::uint32_t v )
		{
			std::uint8_t buf[ 4 ];
			std::memcpy( buf, &v, 4 );
			b.insert( b.end( ), buf, buf + 4 );
		}

		inline void write_f32( std::vector<std::uint8_t>& b, float v )
		{
			std::uint8_t buf[ 4 ];
			std::memcpy( buf, &v, 4 );
			b.insert( b.end( ), buf, buf + 4 );
		}

		inline void write_bytes( std::vector<std::uint8_t>& b, const void* p, std::size_t n )
		{
			const auto s = static_cast< const std::uint8_t* >( p );
			b.insert( b.end( ), s, s + n );
		}

		inline void patch_u32( std::vector<std::uint8_t>& b, std::size_t pos, std::uint32_t v )
		{
			std::memcpy( b.data( ) + pos, &v, 4 );
		}

		struct reader
		{
			const std::uint8_t* p{};
			const std::uint8_t* end{};

			bool have( std::size_t n ) const noexcept { return p + n <= end; }

			std::uint8_t u8( )
			{
				const auto v = *p;
				p += 1;
				return v;
			}

			std::uint32_t u32( )
			{
				std::uint32_t v;
				std::memcpy( &v, p, 4 );
				p += 4;
				return v;
			}

			float f32( )
			{
				float v;
				std::memcpy( &v, p, 4 );
				p += 4;
				return v;
			}

			void skip( std::size_t n ) noexcept { p += n; }
		};

		inline void encode_payload( std::vector<std::uint8_t>& out, const field& f )
		{
			switch ( f.type )
			{
			case field_type::bool_val:
				write_u8( out, *static_cast< const bool* >( f.ptr ) ? 1 : 0 );
				break;
			case field_type::int_val:
				write_u32( out, static_cast< std::uint32_t >( *static_cast< const int* >( f.ptr ) ) );
				break;
			case field_type::uint8_val:
				write_u8( out, *static_cast< const std::uint8_t* >( f.ptr ) );
				break;
			case field_type::float_val:
				write_f32( out, *static_cast< const float* >( f.ptr ) );
				break;
			case field_type::color:
			{
				const auto& c = *static_cast< const zdraw::rgba* >( f.ptr );
				write_u8( out, c.r );
				write_u8( out, c.g );
				write_u8( out, c.b );
				write_u8( out, c.a );
				break;
			}
			case field_type::float3:
			{
				const auto& v = *static_cast< const config::float3* >( f.ptr );
				write_f32( out, v.x );
				write_f32( out, v.y );
				write_f32( out, v.z );
				break;
			}
			case field_type::bool_array:
			{
				const auto arr = static_cast< const bool* >( f.ptr );
				write_u32( out, f.count );
				for ( std::uint32_t i = 0; i < f.count; ++i )
				{
					write_u8( out, arr[ i ] ? 1 : 0 );
				}
				break;
			}
			case field_type::string_val:
			{
				const auto& s = *static_cast< const std::string* >( f.ptr );
				write_u32( out, static_cast< std::uint32_t >( s.size( ) ) );
				write_bytes( out, s.data( ), s.size( ) );
				break;
			}
			case field_type::custom:
			{
				const auto c = static_cast< const custom_field* >( f.ptr );
				c->serialize( out );
				break;
			}
			}
		}

		inline void encode_record( std::vector<std::uint8_t>& out, const field& f )
		{
			write_u32( out, f.key );
			write_u8( out, static_cast< std::uint8_t >( f.type ) );

			const auto len_pos = out.size( );
			write_u32( out, 0 );

			const auto payload_start = out.size( );
			encode_payload( out, f );
			const auto payload_len = static_cast< std::uint32_t >( out.size( ) - payload_start );

			patch_u32( out, len_pos, payload_len );
		}

		inline void decode_payload( reader& r, field& f, std::uint8_t wire_type, std::uint32_t payload_len )
		{
			if ( wire_type != static_cast< std::uint8_t >( f.type ) )
			{
				r.skip( payload_len );
				return;
			}

			const auto end_pos = r.p + payload_len;

			switch ( f.type )
			{
			case field_type::bool_val:
				if ( payload_len >= 1 ) *static_cast< bool* >( f.ptr ) = r.u8( ) != 0;
				break;
			case field_type::int_val:
				if ( payload_len >= 4 ) *static_cast< int* >( f.ptr ) = static_cast< int >( r.u32( ) );
				break;
			case field_type::uint8_val:
				if ( payload_len >= 1 ) *static_cast< std::uint8_t* >( f.ptr ) = r.u8( );
				break;
			case field_type::float_val:
				if ( payload_len >= 4 ) *static_cast< float* >( f.ptr ) = r.f32( );
				break;
			case field_type::color:
			{
				if ( payload_len >= 4 )
				{
					auto& c = *static_cast< zdraw::rgba* >( f.ptr );
					c.r = r.u8( );
					c.g = r.u8( );
					c.b = r.u8( );
					c.a = r.u8( );
				}
				break;
			}
			case field_type::float3:
			{
				if ( payload_len >= 12 )
				{
					auto& v = *static_cast< config::float3* >( f.ptr );
					v.x = r.f32( );
					v.y = r.f32( );
					v.z = r.f32( );
				}
				break;
			}
			case field_type::bool_array:
			{
				if ( payload_len < 4 ) break;

				const auto n = r.u32( );
				const auto remaining = static_cast< std::uint32_t >( end_pos - r.p );
				const auto avail = std::min( n, remaining );
				const auto copy = std::min( avail, f.count );

				auto arr = static_cast< bool* >( f.ptr );
				for ( std::uint32_t i = 0; i < copy; ++i )
				{
					arr[ i ] = r.u8( ) != 0;
				}
				break;
			}
			case field_type::string_val:
			{
				if ( payload_len < 4 ) break;

				const auto n = r.u32( );
				const auto remaining = static_cast< std::uint32_t >( end_pos - r.p );
				const auto take = std::min( n, remaining );

				auto& s = *static_cast< std::string* >( f.ptr );
				s.assign( reinterpret_cast< const char* >( r.p ), take );
				break;
			}
			case field_type::custom:
			{
				auto c = static_cast< custom_field* >( f.ptr );
				c->deserialize( r.p, payload_len );
				break;
			}
			}

			r.p = end_pos;
		}

	} // namespace binary

	namespace base64 {

		inline constexpr char k_table[ ]{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };

		inline std::string encode( const std::uint8_t* data, std::size_t len )
		{
			std::string out;
			out.reserve( ( len + 2 ) / 3 * 4 );

			for ( std::size_t i = 0; i < len; i += 3 )
			{
				const auto b0 = data[ i ];
				const auto b1 = ( i + 1 < len ) ? data[ i + 1 ] : 0;
				const auto b2 = ( i + 2 < len ) ? data[ i + 2 ] : 0;
				out.push_back( k_table[ b0 >> 2 ] );
				out.push_back( k_table[ ( ( b0 & 0x03 ) << 4 ) | ( b1 >> 4 ) ] );
				out.push_back( ( i + 1 < len ) ? k_table[ ( ( b1 & 0x0F ) << 2 ) | ( b2 >> 6 ) ] : '=' );
				out.push_back( ( i + 2 < len ) ? k_table[ b2 & 0x3F ] : '=' );
			}

			return out;
		}

		inline std::string encode( const std::string& s )
		{
			return encode( reinterpret_cast< const std::uint8_t* >( s.data( ) ), s.size( ) );
		}

		inline std::optional<std::vector<std::uint8_t>> decode( std::string_view s )
		{
			static constexpr auto make_rev = [ ]( ) { std::array<std::uint8_t, 256> t{}; t.fill( 0xff ); for ( auto i = 0; i < 64; ++i ) t[ static_cast< unsigned char >( k_table[ i ] ) ] = static_cast< std::uint8_t >( i ); t[ '=' ] = 0; return t; };
			static constexpr auto k_rev = make_rev( );

			if ( s.size( ) % 4 != 0 )
			{
				return std::nullopt;
			}

			std::vector<std::uint8_t> out;
			out.reserve( s.size( ) / 4 * 3 );

			for ( std::size_t i = 0; i < s.size( ); i += 4 )
			{
				const auto a = k_rev[ static_cast< unsigned char >( s[ i ] ) ];
				const auto b = k_rev[ static_cast< unsigned char >( s[ i + 1 ] ) ];
				const auto c = k_rev[ static_cast< unsigned char >( s[ i + 2 ] ) ];
				const auto d = k_rev[ static_cast< unsigned char >( s[ i + 3 ] ) ];

				if ( a == 0xff || b == 0xff || c == 0xff || d == 0xff )
				{
					return std::nullopt;
				}

				out.push_back( ( a << 2 ) | ( b >> 4 ) );

				if ( s[ i + 2 ] != '=' )
				{
					out.push_back( ( ( b & 0x0f ) << 4 ) | ( c >> 2 ) );
				}

				if ( s[ i + 3 ] != '=' )
				{
					out.push_back( ( ( c & 0x03 ) << 6 ) | d );
				}
			}

			return out;
		}

	} // namespace base64

	namespace compress {

		inline std::vector<std::uint8_t> deflate( const std::uint8_t* data, std::size_t len )
		{
			const auto bound = LZ4_compressBound( static_cast< int >( len ) );
			if ( bound <= 0 )
			{
				return {};
			}

			std::vector<std::uint8_t> buf( 4 + bound );

			const auto src_size = static_cast< std::uint32_t >( len );
			std::memcpy( buf.data( ), &src_size, 4 );

			const auto compressed = LZ4_compress_default( reinterpret_cast< const char* >( data ), reinterpret_cast< char* >( buf.data( ) + 4 ), static_cast< int >( len ), bound );
			if ( compressed <= 0 )
			{
				return {};
			}

			buf.resize( 4 + static_cast< std::size_t >( compressed ) );
			return buf;
		}

		inline std::optional<std::vector<std::uint8_t>> inflate( const std::uint8_t* data, std::size_t len )
		{
			if ( len < 4 )
			{
				return std::nullopt;
			}

			std::uint32_t original_size{};
			std::memcpy( &original_size, data, 4 );

			if ( original_size > 64 * 1024 * 1024 )
			{
				return std::nullopt;
			}

			std::vector<std::uint8_t> out( original_size );

			const auto result = LZ4_decompress_safe( reinterpret_cast< const char* >( data + 4 ), reinterpret_cast< char* >( out.data( ) ), static_cast< int >( len - 4 ), static_cast< int >( original_size ) );
			if ( result < 0 )
			{
				return std::nullopt;
			}

			out.resize( static_cast< std::size_t >( result ) );
			return out;
		}

	} // namespace compress

	inline constexpr std::uint8_t k_version{ 1 };
	inline constexpr char k_share_magic[ ]{ "CT" };

	inline std::vector<std::uint8_t> to_bytes( )
	{
		auto& reg = detail::get_registry( );

		std::vector<std::uint8_t> out;
		out.reserve( 5 + reg.fields.size( ) * 16 );

		binary::write_u8( out, k_version );
		binary::write_u32( out, static_cast< std::uint32_t >( reg.fields.size( ) ) );

		for ( const auto& f : reg.fields )
		{
			binary::encode_record( out, f );
		}

		return out;
	}

	inline bool from_bytes( const std::uint8_t* data, std::size_t len )
	{
		binary::reader r{ data, data + len };
		if ( !r.have( 5 ) )
		{
			return false;
		}

		( void )r.u8( );
		const auto count = r.u32( );

		auto& reg = detail::get_registry( );

		std::unordered_map<std::uint32_t, field*> lookup;
		lookup.reserve( reg.fields.size( ) );

		for ( auto& f : reg.fields )
		{
			lookup[ f.key ] = &f;
		}

		for ( std::uint32_t i = 0; i < count; ++i )
		{
			if ( !r.have( 9 ) )
			{
				return false;
			}

			const auto key = r.u32( );
			const auto type = r.u8( );
			const auto plen = r.u32( );

			if ( !r.have( plen ) )
			{
				return false;
			}

			auto it = lookup.find( key );
			if ( it == lookup.end( ) )
			{
				r.skip( plen );
				continue;
			}

			binary::decode_payload( r, *it->second, type, plen );
		}

		return true;
	}

	inline std::vector<std::uint8_t> to_bytes_delta( )
	{
		auto& reg = detail::get_registry( );

		struct entry { const field* f; std::vector<std::uint8_t> payload; };

		std::vector<entry> changed;
		changed.reserve( reg.fields.size( ) );

		std::vector<std::uint8_t> scratch;
		for ( const auto& f : reg.fields )
		{
			scratch.clear( );
			binary::encode_payload( scratch, f );

			auto it = reg.defaults.find( f.key );
			if ( it != reg.defaults.end( ) && it->second == scratch )
			{
				continue;
			}

			changed.push_back( { &f, scratch } );
		}

		std::vector<std::uint8_t> out;
		out.reserve( 5 + changed.size( ) * 16 );

		binary::write_u8( out, k_version );
		binary::write_u32( out, static_cast< std::uint32_t >( changed.size( ) ) );

		for ( const auto& e : changed )
		{
			binary::write_u32( out, e.f->key );
			binary::write_u8( out, static_cast< std::uint8_t >( e.f->type ) );
			binary::write_u32( out, static_cast< std::uint32_t >( e.payload.size( ) ) );
			binary::write_bytes( out, e.payload.data( ), e.payload.size( ) );
		}

		return out;
	}

	inline bool from_bytes_delta( const std::uint8_t* data, std::size_t len )
	{
		binary::reader r{ data, data + len };
		if ( !r.have( 5 ) )
		{
			return false;
		}

		( void )r.u8( );
		const auto count = r.u32( );

		auto& reg = detail::get_registry( );

		for ( auto& f : reg.fields )
		{
			auto it = reg.defaults.find( f.key );
			if ( it == reg.defaults.end( ) )
			{
				continue;
			}

			binary::reader dr{ it->second.data( ), it->second.data( ) + it->second.size( ) };
			binary::decode_payload( dr, f, static_cast< std::uint8_t >( f.type ), static_cast< std::uint32_t >( it->second.size( ) ) );
		}

		std::unordered_map<std::uint32_t, field*> lookup;
		lookup.reserve( reg.fields.size( ) );

		for ( auto& f : reg.fields )
		{
			lookup[ f.key ] = &f;
		}

		for ( std::uint32_t i = 0; i < count; ++i )
		{
			if ( !r.have( 9 ) )
			{
				return false;
			}

			const auto key = r.u32( );
			const auto type = r.u8( );
			const auto plen = r.u32( );

			if ( !r.have( plen ) )
			{
				return false;
			}

			auto it = lookup.find( key );
			if ( it == lookup.end( ) )
			{
				r.skip( plen );
				continue;
			}

			binary::decode_payload( r, *it->second, type, plen );
		}

		return true;
	}

	inline void initialize( )
	{
		auto& reg = detail::get_registry( );

		reg.defaults.clear( );
		reg.defaults.reserve( reg.fields.size( ) );

		std::vector<std::uint8_t> scratch;
		for ( const auto& f : reg.fields )
		{
			scratch.clear( );
			binary::encode_payload( scratch, f );
			reg.defaults.emplace( f.key, scratch );
		}
	}

	namespace registry {

		inline constexpr wchar_t k_root_key[ ]{ L"Software\\catalyst\\configs" };

		inline bool save( std::wstring_view name )
		{
			HKEY hkey{};
			if ( RegCreateKeyExW( HKEY_CURRENT_USER, k_root_key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hkey, nullptr ) != ERROR_SUCCESS )
			{
				return false;
			}

			const auto raw = to_bytes( );
			const auto compressed = compress::deflate( raw.data( ), raw.size( ) );
			if ( compressed.empty( ) )
			{
				RegCloseKey( hkey );
				return false;
			}

			const auto result = RegSetValueExW( hkey, name.data( ), 0, REG_BINARY, compressed.data( ), static_cast< DWORD >( compressed.size( ) ) );
			RegCloseKey( hkey );
			return result == ERROR_SUCCESS;
		}

		inline bool load( std::wstring_view name )
		{
			HKEY hkey{};
			if ( RegOpenKeyExW( HKEY_CURRENT_USER, k_root_key, 0, KEY_QUERY_VALUE, &hkey ) != ERROR_SUCCESS )
			{
				return false;
			}

			DWORD size{};
			if ( RegQueryValueExW( hkey, name.data( ), nullptr, nullptr, nullptr, &size ) != ERROR_SUCCESS || size == 0 )
			{
				RegCloseKey( hkey );
				return false;
			}

			std::vector<std::uint8_t> buf( size );

			if ( RegQueryValueExW( hkey, name.data( ), nullptr, nullptr, buf.data( ), &size ) != ERROR_SUCCESS )
			{
				RegCloseKey( hkey );
				return false;
			}

			RegCloseKey( hkey );

			const auto raw = compress::inflate( buf.data( ), buf.size( ) );
			if ( !raw )
			{
				return false;
			}

			return from_bytes( raw->data( ), raw->size( ) );
		}

		inline bool remove( std::wstring_view name )
		{
			HKEY hkey{};
			if ( RegOpenKeyExW( HKEY_CURRENT_USER, k_root_key, 0, KEY_SET_VALUE, &hkey ) != ERROR_SUCCESS )
			{
				return false;
			}

			const auto result = RegDeleteValueW( hkey, name.data( ) );
			RegCloseKey( hkey );
			return result == ERROR_SUCCESS;
		}

		inline std::vector<std::wstring> list( )
		{
			std::vector<std::wstring> names;

			HKEY hkey{};
			if ( RegOpenKeyExW( HKEY_CURRENT_USER, k_root_key, 0, KEY_QUERY_VALUE, &hkey ) != ERROR_SUCCESS )
			{
				return names;
			}

			wchar_t name_buf[ 256 ]{};
			auto index{ 0ul };

			while ( true )
			{
				auto name_len{ 256ul };
				if ( RegEnumValueW( hkey, index++, name_buf, &name_len, nullptr, nullptr, nullptr, nullptr ) != ERROR_SUCCESS )
				{
					break;
				}

				names.emplace_back( name_buf, name_len );
			}

			RegCloseKey( hkey );
			std::sort( names.begin( ), names.end( ) );
			return names;
		}

	} // namespace registry

	namespace share_detail {

		inline std::vector<std::uint8_t> make_payload( std::string_view name )
		{
			const auto delta_raw = to_bytes_delta( );
			const auto compressed = compress::deflate( delta_raw.data( ), delta_raw.size( ) );

			if ( compressed.empty( ) )
			{
				return {};
			}

			const auto name_len = static_cast< std::uint8_t >( std::min< std::size_t >( name.size( ), 255 ) );

			std::vector<std::uint8_t> payload;
			payload.reserve( 4ull + name_len + compressed.size( ) );
			payload.push_back( static_cast< std::uint8_t >( k_share_magic[ 0 ] ) );
			payload.push_back( static_cast< std::uint8_t >( k_share_magic[ 1 ] ) );
			payload.push_back( k_version );
			payload.push_back( name_len );
			payload.insert( payload.end( ), name.data( ), name.data( ) + name_len );
			payload.insert( payload.end( ), compressed.begin( ), compressed.end( ) );
			return payload;
		}

		struct import_result
		{
			bool success{};
			std::string name{};
		};

		inline import_result import_payload( const std::vector<std::uint8_t>& d )
		{
			if ( d.size( ) < 4 )
			{
				return {};
			}

			if ( d[ 0 ] != static_cast< std::uint8_t >( k_share_magic[ 0 ] ) || d[ 1 ] != static_cast< std::uint8_t >( k_share_magic[ 1 ] ) )
			{
				return {};
			}

			const auto name_len = d[ 3 ];
			const auto header = 4ull + name_len;

			if ( d.size( ) < header )
			{
				return {};
			}

			std::string name( reinterpret_cast< const char* >( d.data( ) + 4 ), name_len );

			const auto raw = compress::inflate( d.data( ) + header, d.size( ) - header );
			if ( !raw )
			{
				return {};
			}

			const bool ok = from_bytes_delta( raw->data( ), raw->size( ) );
			return { ok, std::move( name ) };
		}

	} // namespace share_detail

	inline std::string export_share( std::string_view name = {} )
	{
		const auto payload = share_detail::make_payload( name );
		if ( payload.empty( ) )
		{
			return {};
		}

		return base64::encode( payload.data( ), payload.size( ) );
	}

	inline share_detail::import_result import_share( std::string_view share_str )
	{
		std::string cleaned;
		cleaned.reserve( share_str.size( ) );

		for ( auto c : share_str )
		{
			if ( c != ' ' && c != '\n' && c != '\r' && c != '\t' )
			{
				cleaned.push_back( c );
			}
		}

		const auto decoded = base64::decode( cleaned );
		if ( !decoded )
		{
			return {};
		}

		return share_detail::import_payload( *decoded );
	}

} // namespace config
