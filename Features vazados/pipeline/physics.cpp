#include <pipeline/physics.hpp>
#include <console.h>
#include <xorstr.h>

namespace PhysicsLoader {
	using namespace Context;

	uint64_t FindVtable(const std::string& className) {
		auto mod = Memory->GetModule(std::string(xorstr_("vphysics2.dll").c_str()));
		if (!mod.base || !mod.size) return 0;

		auto cached = Memory->CacheModule(std::string(xorstr_("vphysics2.dll").c_str()));
		if (!cached.Valid()) return 0;

		std::string descriptor = std::string(xorstr_(".?AV")) + className + std::string(xorstr_("@@"));
		const uint8_t* data = cached.Data();
		size_t sz = cached.Size();
		uint64_t base = cached.Base();

		uint64_t typeDesc = 0;
		for (size_t i = 0; i + descriptor.size() < sz; i++) {
			if (memcmp(data + i, descriptor.c_str(), descriptor.size() + 1) == 0) {
				typeDesc = base + i - 0x10;
				break;
			}
		}
		if (!typeDesc) return 0;

		uint32_t tdRva = static_cast<uint32_t>(typeDesc - base);
		uint64_t colAddr = 0;
		for (size_t i = 0; i + 24 < sz; i += 4) {
			uint32_t val = *reinterpret_cast<const uint32_t*>(data + i);
			if (val == tdRva) { colAddr = base + i - 12; break; }
		}
		if (!colAddr) return 0;

		for (size_t i = 0; i + 8 < sz; i += 8) {
			uint64_t val = *reinterpret_cast<const uint64_t*>(data + i);
			if (val == colAddr) return base + i + 8;
		}
		return 0;
	}

	bool ExtractMesh(uint64_t meshData, float scale[3], float worldPos[3],
		std::vector<Physics::Triangle>& out) {
		uint8_t md[0xA0]{};
		if (!Memory->ReadRaw(meshData, md, sizeof(md))) return false;

		uint64_t bvhPtr = *reinterpret_cast<uint64_t*>(md + 0x20);
		uint64_t vertPtr = *reinterpret_cast<uint64_t*>(md + 0x38);
		uint64_t triPtr = *reinterpret_cast<uint64_t*>(md + 0x50);

		uint32_t nodeCount = 0;
		for (auto off : {0x28, 0x30, 0x48, 0x58}) {
			int32_t c = *reinterpret_cast<int32_t*>(md + off);
			if (c > 0 && c < 0x1000000) { nodeCount = (uint32_t)c; break; }
		}
		if (!bvhPtr || !vertPtr || !triPtr || nodeCount == 0) return false;

		constexpr size_t INNER_NODE_SIZE = 32;
		std::vector<uint8_t> bvhBuf(nodeCount * INNER_NODE_SIZE);
		if (!Memory->ReadRaw(bvhPtr, bvhBuf.data(), bvhBuf.size())) return false;

		uint32_t minTri = UINT32_MAX, maxTri = 0;
		std::vector<std::pair<uint32_t, uint32_t>> ranges;
		std::vector<uint32_t> stack;
		stack.reserve(256);
		uint32_t cursor = 0;

		while (true) {
			if (cursor >= nodeCount) {
				if (stack.empty()) break;
				cursor = stack.back(); stack.pop_back(); continue;
			}
			auto* node = reinterpret_cast<const uint32_t*>(bvhBuf.data() + cursor * INNER_NODE_SIZE);
			uint32_t packed0 = node[3];
			uint32_t packed1 = node[7];
			uint32_t type = packed0 >> 30;
			uint32_t payload = packed0 & 0x3FFFFFFFu;

			if (type == 3) {
				if (payload > 0 && payload < 0x1000000) {
					ranges.push_back({ packed1, payload });
					if (packed1 < minTri) minTri = packed1;
					if (packed1 + payload > maxTri) maxTri = packed1 + payload;
				}
				if (stack.empty()) break;
				cursor = stack.back(); stack.pop_back();
			} else {
				if (payload == 0) {
					if (stack.empty()) break;
					cursor = stack.back(); stack.pop_back(); continue;
				}
				if (cursor + payload < nodeCount) stack.push_back(cursor + payload);
				cursor++;
			}
		}

		if (ranges.empty() || maxTri <= minTri) return false;
		uint32_t totalTris = maxTri - minTri;
		if (totalTris > 0x1000000) return false;

		std::vector<int32_t> indices(totalTris * 3);
		if (!Memory->ReadRaw(triPtr + (uint64_t)minTri * 12, indices.data(), indices.size() * 4)) return false;

		int32_t maxVert = 0;
		for (auto idx : indices) if (idx > maxVert) maxVert = idx;
		if (maxVert <= 0 || maxVert > 0x1000000) return false;

		uint32_t vertCount = (uint32_t)(maxVert + 1);
		std::vector<float> vertices(vertCount * 3);
		if (!Memory->ReadRaw(vertPtr, vertices.data(), vertices.size() * 4)) return false;

		size_t before = out.size();
		for (auto& [start, count] : ranges) {
			for (uint32_t i = 0; i < count; i++) {
				uint32_t li = start - minTri + i;
				if (li >= totalTris) continue;
				uint32_t b = li * 3;
				int32_t i0 = indices[b], i1 = indices[b + 1], i2 = indices[b + 2];
				if (i0 < 0 || i1 < 0 || i2 < 0) continue;
				if ((uint32_t)i0 >= vertCount || (uint32_t)i1 >= vertCount || (uint32_t)i2 >= vertCount) continue;

				auto xf = [&](int32_t vi) -> Vec3 {
					return { vertices[vi * 3] * scale[0] + worldPos[0],
							 vertices[vi * 3 + 1] * scale[1] + worldPos[1],
							 vertices[vi * 3 + 2] * scale[2] + worldPos[2] };
				};
				out.push_back({ xf(i0), xf(i1), xf(i2) });
			}
		}
		return out.size() > before;
	}

	bool ExtractHull(uint64_t hullData, float uniformScale,
		std::vector<Physics::Triangle>& out) {
		uint8_t hd[0x100]{};
		if (!Memory->ReadRaw(hullData, hd, sizeof(hd))) return false;

		int32_t vertCount = *reinterpret_cast<int32_t*>(hd + 0x88);
		uint64_t vertPtr = *reinterpret_cast<uint64_t*>(hd + 0x90);
		int32_t hedgeCount = *reinterpret_cast<int32_t*>(hd + 0xa0);
		uint64_t hedgePtr = *reinterpret_cast<uint64_t*>(hd + 0xa8);
		int32_t faceCount = *reinterpret_cast<int32_t*>(hd + 0xb8);
		uint64_t facePtr = *reinterpret_cast<uint64_t*>(hd + 0xc0);

		if (vertCount <= 0 || vertCount > 0xFFFF) return false;
		if (hedgeCount <= 0 || hedgeCount > 0xFFFF) return false;
		if (faceCount <= 0 || faceCount > 0xFFFF) return false;

		std::vector<float> verts(vertCount * 3);
		Memory->ReadRaw(vertPtr, verts.data(), verts.size() * 4);

		std::vector<HalfEdge> hedges(hedgeCount);
		Memory->ReadRaw(hedgePtr, hedges.data(), hedgeCount * 4);

		std::vector<uint8_t> faces(faceCount);
		Memory->ReadRaw(facePtr, faces.data(), faceCount);

		size_t before = out.size();
		for (int fi = 0; fi < faceCount; fi++) {
			uint8_t startHe = faces[fi];
			if (startHe >= hedgeCount) continue;

			std::vector<int> fv;
			fv.reserve(8);
			auto he = startHe;
			int safety = 0;
			do {
				if (he >= hedgeCount) break;
				fv.push_back(hedges[he].vert);
				he = hedges[he].next;
			} while (he != startHe && ++safety < 64);

			if (fv.size() < 3) continue;
			auto vert = [&](int vi) -> Vec3 {
				if (vi < 0 || vi >= vertCount) return {};
				return { verts[vi * 3] * uniformScale, verts[vi * 3 + 1] * uniformScale, verts[vi * 3 + 2] * uniformScale };
			};
			Vec3 v0 = vert(fv[0]);
			for (size_t i = 1; i + 1 < fv.size(); i++)
				out.push_back({ v0, vert(fv[i]), vert(fv[i + 1]) });
		}
		return out.size() > before;
	}

	void ProcessShape(uint64_t shape, std::vector<Physics::Triangle>& out) {
		uint64_t vt = Memory->Read<uint64_t>(shape);
		if (vt == hullVtable) {
			uint64_t hullData = Memory->Read<uint64_t>(shape + 0xb8);
			if (hullData > 0x10000 && hullData < 0x7fffffffffff) {
				float scale = Memory->Read<float>(shape + 0xb0);
				if (scale <= 0.0f || !std::isfinite(scale)) scale = 1.0f;
				ExtractHull(hullData, scale, out);
			}
		} else if (vt == meshVtable) {
			uint64_t meshData = Memory->Read<uint64_t>(shape + 0xc0);
			if (!meshData) return;
			float damage = Memory->Read<float>(shape + 0x2c);
			if (damage < 0.0f) return;
			float scale[3]{};
			Memory->ReadRaw(shape + 0xB0, scale, 12);
			if (!std::isfinite(scale[0]) || !std::isfinite(scale[1]) || !std::isfinite(scale[2])) return;
			float worldPos[3]{};
			Memory->ReadRaw(shape + 0x100, worldPos, 12);
			ExtractMesh(meshData, scale, worldPos, out);
		}
	}

	bool Parse(std::vector<Physics::Triangle>& triangles) {
		if (!hullVtable) hullVtable = FindVtable(std::string(xorstr_("CRnHullShape")));
		if (!meshVtable) meshVtable = FindVtable(std::string(xorstr_("CRnMeshShape")));
		if (!hullVtable && !meshVtable) {
			Console::Error(xorstr_("PhysWorld: vtables not found"));
			return false;
		}

		auto client = Memory->CacheModule(std::string(xorstr_("client.dll").c_str()));
		if (!client.Valid()) return false;

		auto patAddr = client.Scan(xorstr_("E8 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8D 54 24 ?? 48 8B CF"));
		if (!patAddr) {
			Console::Error(xorstr_("PhysWorld: pattern not found"));
			return false;
		}

		uint64_t ripAddr = patAddr - 9;
		int32_t ripDisp = Memory->Read<int32_t>(ripAddr + 3);
		uint64_t globalAddr = ripAddr + 7 + ripDisp;
		uint64_t vphys2Global = Memory->Read<uint64_t>(globalAddr);
		uint64_t vphys2World = Memory->Read<uint64_t>(vphys2Global);
		if (!vphys2World) return false;

		uint64_t innerWorld = Memory->Read<uint64_t>(vphys2World + 0x30);
		if (!innerWorld) return false;

		uint64_t bodyArray = Memory->Read<uint64_t>(innerWorld + 0x110);
		if (!bodyArray) return false;

		int32_t bodyCount = Memory->Read<int32_t>(bodyArray + 0x268);
		if (bodyCount <= 0 || bodyCount > 100000) return false;

		Console::Info(xorstr_("PhysWorld: %d bodies"), bodyCount);

		for (int32_t bi = 0; bi < bodyCount; bi++) {
			uint64_t body = bodyArray + (uint64_t)bi * 88;
			uint32_t bodyType = Memory->Read<uint32_t>(body + 0x40);
			if (bodyType != 2) continue;

			int32_t bvhRoot = Memory->Read<int32_t>(body);
			uint64_t bvhNodesPtr = Memory->Read<uint64_t>(body + 0x18);
			if (!bvhNodesPtr) continue;

			if (bvhRoot >= 0) {
				uint32_t outerCount = (uint32_t)(bvhRoot + 1);
				uint32_t cb = (uint32_t)Memory->Read<int32_t>(body + 0x08);
				uint32_t cc = (uint32_t)Memory->Read<int32_t>(body + 0x10);
				if (cb > outerCount) outerCount = cb;
				if (cc > outerCount) outerCount = cc;
				if (outerCount > 0x100000) continue;

				constexpr size_t OUTER_SIZE = 48;
				std::vector<uint8_t> outerBuf(outerCount * OUTER_SIZE);
				Memory->ReadRaw(bvhNodesPtr, outerBuf.data(), outerBuf.size());

				std::vector<int32_t> outerStack;
				outerStack.push_back(bvhRoot);
				while (!outerStack.empty()) {
					int32_t idx = outerStack.back(); outerStack.pop_back();
					if (idx < 0 || (uint32_t)idx >= outerCount) continue;
					auto* node = outerBuf.data() + (uint64_t)idx * OUTER_SIZE;
					int32_t left = *reinterpret_cast<int32_t*>(node + 12);
					if (left == -1) {
						uint64_t shapePtr = *reinterpret_cast<uint64_t*>(node + 0x28);
						if (shapePtr) ProcessShape(shapePtr, triangles);
					} else {
						int32_t right = *reinterpret_cast<int32_t*>(node + 28);
						if (left >= 0) outerStack.push_back(left);
						if (right >= 0) outerStack.push_back(right);
					}
				}
			} else {
				uint64_t shape = Memory->Read<uint64_t>(body + 0x28);
				if (shape) ProcessShape(shape, triangles);
			}
		}

		Console::Success(xorstr_("PhysWorld: %zu triangles"), triangles.size());
		return !triangles.empty();
	}

	void Work() {
		while (true) {
			Sys::Sleep(3000);
			if (ClientDLL.load() == 0 || !Memory->IsAttached()) return;

			std::string mapName = CleanMapName(MapName);

			if (mapName.empty() || mapName == std::string(xorstr_("<empty>"))) {
				if (!currentMap.empty()) {
					currentMap.clear();
					MapBVHReady.store(false);
				}
				continue;
			}

			if (mapName == currentMap) continue;

			Console::Info(xorstr_("Loading collision: %s"), mapName.c_str());
			MapBVHReady.store(false);
			hullVtable = 0;
			meshVtable = 0;

			while (true) {
				if (ClientDLL.load() == 0 || !Memory->IsAttached()) return;

				std::string check = CleanMapName(MapName);
				if (check.empty() || check == std::string(xorstr_("<empty>")) || check != mapName) break;

				std::vector<Physics::Triangle> tris;
				tris.reserve(500000);

				if (!Parse(tris)) continue;

				{
					std::lock_guard<std::mutex> lock(MapBVHMutex);
					MapBVH.Build(std::move(tris));
				}

				currentMap = mapName;
				MapBVHReady.store(true);
				Console::Success(xorstr_("BVH ready: %zu tris, %zu nodes"),
					MapBVH.TriangleCount(), MapBVH.NodeCount());
				break;
			}
		}
	}
}
