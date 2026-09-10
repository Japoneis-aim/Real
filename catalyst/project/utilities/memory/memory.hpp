#pragma once

class memory
{
public:
	bool initialize( std::wstring_view process_name );
	bool read( std::uintptr_t address, void* buffer, std::size_t size ) const;
	bool write( std::uintptr_t address, const void* buffer, std::size_t size ) const;

	template <typename T>
	T read( std::uintptr_t address ) const
	{
		T value{};
		this->read( address, &value, sizeof( T ) );
		return value;
	}

	template <typename T>
	bool write( std::uintptr_t address, const T& value ) const
	{
		return this->write( address, &value, sizeof( T ) );
	}

	template <typename T = std::uintptr_t>
	T resolve_rip( std::uintptr_t address, std::int32_t offset = 3, std::int32_t length = 7 ) const
	{
		const auto rva = this->read<std::int32_t>( address + offset );
		const auto resolved = address + length + rva;

		if constexpr ( std::is_pointer_v<T> )
		{
			return reinterpret_cast< T >( resolved );
		}
		else
		{
			return static_cast< T >( resolved );
		}
	}

	template <typename T = std::uintptr_t>
	T resolve_call( std::uintptr_t address ) const
	{
		return this->resolve_rip<T>( address, 1, 5 );
	}

	std::string read_string( std::uintptr_t address, std::size_t max_len = 256 ) const;

	// -------------------------------------------------------------------------
	// Batch read helper — lê um bloco contíguo de memória uma única vez e
	// permite extrair campos individuais por offset relativo ao base_address.
	// Reduz de N syscalls para 1 ReadProcessMemory por região de struct.
	//
	// Uso:
	//   auto batch = g::memory.read_batch( pawn_addr, 0x1400 );
	//   const auto health = batch.get<int>( offset_health );
	//   const auto team   = batch.get<int>( offset_team );
	//
	// Para tamanhos conhecidos em tempo de compilação, preferir read_batch_fixed<N>()
	// que usa std::array inline — sem alocação heap, sem pressão no allocator por tick.
	// -------------------------------------------------------------------------
	struct batch_reader
	{
		std::uintptr_t base{};
		std::vector<std::uint8_t> buf{};
		bool valid{};

		template<typename T>
		[[nodiscard]] T get( std::uintptr_t offset ) const
		{
			const auto off = offset;
			if ( !valid || off + sizeof( T ) > buf.size( ) )
				return T{};
			T val{};
			std::memcpy( &val, buf.data( ) + off, sizeof( T ) );
			return val;
		}

		[[nodiscard]] bool in_range( std::uintptr_t offset, std::size_t size ) const
		{
			return valid && ( offset + size ) <= buf.size( );
		}
	};

	// -------------------------------------------------------------------------
	// static_batch_reader<N> — versão sem alocação heap para tamanhos fixos.
	// Armazena o buffer no stack/inline (como membro de struct).
	// Usar quando N é conhecido em compilação (weapon 0x1000, vdata 0x800, etc.)
	// -------------------------------------------------------------------------
	template<std::size_t N>
	struct static_batch_reader
	{
		std::uintptr_t base{};
		std::array<std::uint8_t, N> buf{};
		bool valid{};

		template<typename T>
		[[nodiscard]] T get( std::uintptr_t offset ) const
		{
			if ( !valid || offset + sizeof( T ) > N )
				return T{};
			T val{};
			std::memcpy( &val, buf.data( ) + offset, sizeof( T ) );
			return val;
		}

		[[nodiscard]] bool in_range( std::uintptr_t offset, std::size_t size ) const
		{
			return valid && ( offset + size ) <= N;
		}

		// Compatibilidade: permite ser passado onde batch_reader é esperado
		// (via conversão implícita para batch_reader com view do mesmo buffer).
		[[nodiscard]] batch_reader as_dynamic( ) const
		{
			batch_reader br{};
			br.base  = base;
			br.valid = valid;
			br.buf.assign( buf.begin( ), buf.end( ) );
			return br;
		}

		// Conversão implícita para batch_reader — copia o buffer uma única vez
		// ao passar para funções que esperam batch_reader const&.
		// Custo: 1 alocação + memcpy de N bytes (ocorre apenas no call site, não por tick
		// se a função for chamada raramente; para tick() hot path usar overloads template).
		operator batch_reader( ) const { return as_dynamic( ); }
	};

	[[nodiscard]] batch_reader read_batch( std::uintptr_t address, std::size_t size ) const
	{
		batch_reader br{};
		br.base = address;
		br.buf.resize( size );
		br.valid = this->read( address, br.buf.data( ), size );
		return br;
	}

	// Versão sem alocação heap — buffer inline de N bytes.
	// Preferir esta nos hot paths (tick de combat, collector) onde N é constante.
	template<std::size_t N>
	[[nodiscard]] static_batch_reader<N> read_batch_fixed( std::uintptr_t address ) const
	{
		static_batch_reader<N> br{};
		br.base  = address;
		br.valid = this->read( address, br.buf.data( ), N );
		return br;
	}

	std::uintptr_t find_pattern( std::uintptr_t module_base, std::string_view pattern ) const;
	std::uintptr_t find_vtable( std::uintptr_t module_base, std::string_view class_name ) const;
	std::uintptr_t find_vtable_instance( std::uintptr_t module_base, std::string_view class_name ) const;
	std::uintptr_t get_module( std::string_view name ) const;

	[[nodiscard]] void* handle( ) const { return this->m_handle; }

private:
	std::uint32_t m_id{};
	void* m_handle{};
	std::uintptr_t m_base{};

	std::size_t get_module_size( std::uintptr_t module_base ) const;
	std::uintptr_t find_qword_in_sections( std::uintptr_t module_base, std::uintptr_t value, std::uint32_t section_filter ) const;
};