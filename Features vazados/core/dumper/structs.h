#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <core/context.h>

namespace Dumper {

#pragma pack(push, 1)

	struct UtlMemoryPool {
		int block_size;
		int blocks_per_blob;
		int grow_mode;
		int blocks_allocated;
		int peak_allocated;
		uint16_t alignment;
		uint16_t blob_count;
		uint8_t pad_0[0x8];
		uint8_t free_blocks[0x28];
		void* blob_head;
		int total_size;
		uint8_t pad_2[0xC];
	};

	struct HashEntry {
		uint64_t ui_key;
		void* next;
		void* data;
	};

	struct HashBucket {
		uint64_t add_lock;
		void* first;
		void* first_uncommitted;
	};

	struct UtlTsHash {
		UtlMemoryPool entry_mem;
		uint64_t buckets[256];
		bool needs_commit;
		uint8_t pad_0[0x3];
		int contention_check;
		uint8_t pad[0x8];

		std::vector<void*> Enumerate(uint64_t baseAddr) const {
			std::vector<void*> out;
			for (int i = 0; i < 256; i++) {
				uint64_t bucketAddr = baseAddr + 0x60 + (i * sizeof(HashBucket));
				auto bucket = Context::Memory->Read<HashBucket>(bucketAddr);
				uint64_t entryPtr = (uint64_t)bucket.first_uncommitted;
				while (entryPtr != 0) {
					auto entry = Context::Memory->Read<HashEntry>(entryPtr);
					out.push_back(entry.data);
					entryPtr = (uint64_t)entry.next;
				}
			}
			return out;
		}
	};

	struct SchemaSystem {
		uint8_t pad_0000[0x190];
		struct {
			int size;
			int _pad;
			void* mem;

			void* GetElementPtr(int index) const {
				return (uint8_t*)mem + (index * sizeof(void*));
			}
			void* GetElement(int index) const {
				return Context::Memory->Read<void*>((uint64_t)GetElementPtr(index));
			}
		} type_scopes;
		uint8_t pad_0198[0xE0];
		int num_registrations;
	};

	struct SchemaSystemTypeScope {
		uint8_t pad_0000[0x8];
		char name[256];
		void* global_scope;
		uint8_t pad_0110[0x450];
		UtlTsHash class_bindings;
	};

	struct SchemaClassBinding {
		void* baseClass;
		void* name;
		void* module_name;
		void* binary_name;
		int size;
		short field_count;
		short static_metadata_count;
		short pad_0020;
		uint8_t align_of;
		uint8_t has_base_class;
		short total_class_size;
		short derived_class_size;
		void* fields;
		uint8_t pad_0030[0x8];
		void* base_classes;
		void* static_metadata;
		void* type_scope;
		void* type;
		uint8_t pad_0060[0x10];
	};

	struct SchemaClassFieldData {
		void* name;
		void* type;
		int offset;
		int metadata_count;
		void* metadata;
	};

#pragma pack(pop)

}
