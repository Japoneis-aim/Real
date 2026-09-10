#pragma once
#include <string_view>

// offsets_map — carrega em runtime o arquivo output/offsets.map gerado pelo
// scanner externo.  Formato: linhas "KEY 0xVALOR" (hex), "#" = comentário.
//
// Uso:
//   offsets_map::load_from_file( "output/offsets.map" );
//   auto rva = offsets_map::get( "dwPlantedC4" );   // 0 se não encontrado

namespace offsets_map {

	// Carrega (ou recarrega) o arquivo .map.  Seguro chamar antes de qualquer
	// thread ser criada.  Retorna false se o arquivo não puder ser aberto.
	bool load_from_file( std::string_view path );

	// Retorna o valor associado à chave, ou 0 se a chave não existir.
	std::uintptr_t get( std::string_view key );

} // namespace offsets_map
