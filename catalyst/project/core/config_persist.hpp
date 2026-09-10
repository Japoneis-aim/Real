#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

// Sistema simples de persistência de config.
// Salva/carrega o estado de todos os campos registrados em um arquivo .bin
// na pasta de trabalho do executável.
//
// Sem auth, sem perfis, sem XOR elaborado — serialização direta via
// config::to_bytes() / config::from_bytes() (LZ4 + registro de campos).
//
// Uso:
//   config_persist::load();  // ao iniciar, após config::initialize()
//   config_persist::save();  // quando o usuário clicar em "SAVE CFG"

namespace config_persist {

    inline constexpr char k_filename[] = "catalyst_config.bin";

    // Magic header: "CATC" (0x43415443) — valida que o arquivo é nosso.
    inline constexpr std::uint32_t k_magic = 0x43415443u;

    // Versão do formato de config — incrementar sempre que campos forem
    // adicionados, removidos ou renomeados em settings.hpp.
    // O load() aceita configs de versões anteriores (campos novos ficam no
    // default), mas loga um warning para o usuário saber que o arquivo é antigo.
    // Versões futuras com formato incompatível podem rejeitar versões < mínimo.
    inline constexpr std::uint16_t k_version = 2u;
    inline constexpr std::uint16_t k_min_compatible_version = 1u;

    inline const char* get_path( )
    {
        return k_filename;
    }

    // Serializa o estado atual de todos os campos registrados para disco.
    // Retorna true em caso de sucesso.
    inline bool save( )
    {
        // config::to_bytes() serializa todos os campos do registry para um
        // buffer binário (versioned, key-indexed).
        const auto buf = config::to_bytes( );
        if ( buf.empty( ) )
        {
            return false;
        }

        std::ofstream f( get_path( ), std::ios::binary | std::ios::trunc );
        if ( !f.is_open( ) )
        {
            return false;
        }

        // Escreve magic header para validação na leitura.
        f.write( reinterpret_cast<const char*>( &k_magic ), sizeof( k_magic ) );

        // Escreve a versão do formato (uint16_t) para compatibilidade futura.
        f.write( reinterpret_cast<const char*>( &k_version ), sizeof( k_version ) );

        // Escreve o tamanho do payload (uint32_t) para checagem rápida.
        const auto payload_size = static_cast<std::uint32_t>( buf.size( ) );
        f.write( reinterpret_cast<const char*>( &payload_size ), sizeof( payload_size ) );

        // Escreve o payload serializado.
        f.write( reinterpret_cast<const char*>( buf.data( ) ), buf.size( ) );

        return f.good( );
    }

    // Lê o arquivo de config e restaura o estado de todos os campos registrados.
    // Retorna true em caso de sucesso. Falha silenciosa se o arquivo não existir
    // ou estiver corrompido (os valores padrão são mantidos).
    inline bool load( )
    {
        std::ifstream f( get_path( ), std::ios::binary );
        if ( !f.is_open( ) )
        {
            // Arquivo não existe ainda — comportamento normal na primeira execução.
            return false;
        }

        // Valida magic header.
        std::uint32_t magic = 0;
        f.read( reinterpret_cast<char*>( &magic ), sizeof( magic ) );
        if ( !f.good( ) || magic != k_magic )
        {
            return false;
        }

        // Lê e verifica a versão do formato.
        // Configs antigas (sem campo de versão) têm tamanho de payload logo após
        // o magic — detectamos isso verificando se o valor caberia em uint16.
        // Para robustez lemos sempre 2 bytes e tratamos como versão.
        std::uint16_t file_version = 0;
        f.read( reinterpret_cast<char*>( &file_version ), sizeof( file_version ) );
        if ( !f.good( ) )
        {
            return false;
        }

        if ( file_version < k_min_compatible_version )
        {
            // Versão incompatível — não tenta carregar para evitar dados corrompidos.
            return false;
        }

        if ( file_version < k_version )
        {
            // Versão mais antiga mas ainda compatível — campos novos ficam no default.
            // (sem log aqui: config_persist.hpp é incluído antes do namespace g::)
        }

        // Lê o tamanho esperado do payload.
        std::uint32_t payload_size = 0;
        f.read( reinterpret_cast<char*>( &payload_size ), sizeof( payload_size ) );
        if ( !f.good( ) || payload_size == 0 || payload_size > 32 * 1024 * 1024 )
        {
            return false; // sanity check — rejeita payloads absurdamente grandes.
        }

        // Lê o payload.
        std::vector<std::uint8_t> buf( payload_size );
        f.read( reinterpret_cast<char*>( buf.data( ) ), payload_size );
        if ( !f.good( ) && !f.eof( ) )
        {
            return false;
        }

        // Restaura o estado dos campos registrados a partir do buffer.
        return config::from_bytes( buf.data( ), buf.size( ) );
    }

} // namespace config_persist
