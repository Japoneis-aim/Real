#pragma once
#include <core/context.h>
#include <core/dumper/structs.h>

namespace Dumper {
	using namespace Context;

	struct PatternEntry {
		uintptr_t* target;
		const char* name;
		const char* pattern;
		int dispOffset;
		int instrSize;
	};

	bool DumpOffsets();
	bool DumpSchemas();
	void ResolveSchemaOffsets();
	bool Dump();
	void PrintLeagueOffsets();
}
