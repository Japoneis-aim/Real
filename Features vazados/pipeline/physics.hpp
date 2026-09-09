#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <core/context.h>
#include <nt/wrap.h>

namespace PhysicsLoader {
	using namespace Context;

	struct HalfEdge { uint8_t next, twin, vert, face; };

	inline uint64_t hullVtable = 0;
	inline uint64_t meshVtable = 0;
	inline std::string currentMap;

	uint64_t FindVtable(const std::string& className);
	bool ExtractMesh(uint64_t meshData, float scale[3], float worldPos[3],
		std::vector<Physics::Triangle>& out);
	bool ExtractHull(uint64_t hullData, float uniformScale,
		std::vector<Physics::Triangle>& out);
	void ProcessShape(uint64_t shape, std::vector<Physics::Triangle>& out);
	bool Parse(std::vector<Physics::Triangle>& triangles);

	inline std::string CleanMapName(const std::string& raw) {
		std::string name = raw;
		auto dot = name.find_last_of('.');
		if (dot != std::string::npos) name = name.substr(0, dot);
		auto slash = name.find_last_of("/\\");
		if (slash != std::string::npos) name = name.substr(slash + 1);
		return name;
	}

	void Work();
}
