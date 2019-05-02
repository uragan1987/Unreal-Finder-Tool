#include "pch.h"
#include "TArrayFinder.h"
#include "Color.h"
#include <vector>


TArrayFinder::TArrayFinder(Memory* memory) : _memory(memory)
{
	dwStart = 0;
	dwEnd = 0;
	ptr_size = !_memory->Is64Bit ? 0x4 : 0x8;
	dwBetweenObjects = ptr_size + 0x10;
	dwInternalIndex = ptr_size + 0x4;
}

void TArrayFinder::Find()
{
	Color green(LightGreen, Black);
	Color def(White, Black);
	Color yellow(LightYellow, Black);
	Color purple(LightPurple, Black);
	Color red(LightRed, Black);
	Color dgreen(Green, Black);

	dwStart = !_memory->Is64Bit ? 0x300000 : static_cast<uintptr_t>(0x7FF00000);
	dwEnd = !_memory->Is64Bit ? 0x7FEFFFFF : static_cast<uintptr_t>(0x7fffffffffff);

	std::cout << dgreen << "[!] " << def << "Start scan for TArrays. (at 0x" << std::hex << dwStart << ". to 0x" << std::hex << dwEnd << ")" << std::endl << def;

	// Start scan for TArrays
	SYSTEM_INFO si = { 0 };
	GetSystemInfo(&si);

	if (dwStart < reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress))
		dwStart = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);

	if (dwEnd > reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress))
		dwEnd = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);

	// dwStart = static_cast<uintptr_t>(0x250261B0000);
	// dwEnd = static_cast<uintptr_t>(0x7ff7ccc25c68);

	int found_count = 0;
	MEMORY_BASIC_INFORMATION info;
	FUObjectArray gObject;

	// Cycle through memory based on RegionSize
	for (uintptr_t i = dwStart; (VirtualQueryEx(_memory->ProcessHandle, LPVOID(i), &info, sizeof info) == sizeof info && i < dwEnd); i += info.RegionSize)
	{
		// Bad Memory
		if (info.State != MEM_COMMIT) continue;
		if (info.Protect != PAGE_EXECUTE_READWRITE && info.Protect != PAGE_READWRITE) continue;

		if (IsValidTArray(i, gObject))
		{
			std::cout << green << "[+] " << def << "\t" << red << "0x" << std::hex << i << std::dec << def << std::endl;
			// std::cout << " (" << yellow << "Max: " << gObject.Max << ", Num: " << gObject.Num << ", Data: " << std::hex << gObject.Data << def << ")" << std::dec << std::endl;
			found_count++;
		}
	}

	std::cout << purple << "[!] " << yellow << "Found " << found_count << " Address." << std::endl << def;
}

bool TArrayFinder::IsValidPointer(const uintptr_t address, uintptr_t& pointer, const bool checkIsAllocationBase)
{
	pointer = NULL;
	if (!_memory->Is64Bit)
		pointer = _memory->ReadUInt(address);
	else
		pointer = _memory->ReadUInt64(address);

	if (INVALID_POINTER_VALUE(pointer) /*|| pointer < dwStart || pointer > dwEnd*/ || pointer < 0x10000000)
		return false;

	// Check memory state, type and permission
	MEMORY_BASIC_INFORMATION info;

	//const uintptr_t pointerVal = _memory->ReadInt(pointer);
	if (VirtualQueryEx(_memory->ProcessHandle, LPVOID(pointer), &info, sizeof info) == sizeof info)
	{
		// Bad Memory
		if (info.State != MEM_COMMIT) return false;
		if (info.Protect == PAGE_NOACCESS) return false;
		if (checkIsAllocationBase && info.AllocationBase != LPVOID(pointer)) return false;

		return true;
	}

	return false;
}

bool TArrayFinder::IsValidTArray(const uintptr_t address, FUObjectArray& tArray)
{
	tArray = { 0, 0, 0 };

	// Check GObjects
	// ObjFirstGCIndex = (ObjLastNonGCIndex - 1) & MaxObjectsNotConsideredByGC = ObjFirstGCIndex, OpenForDisregardForGC = 0(not sure)
	//const int obj_first_gc_index = _memory->ReadInt(address - 0x10);
	//const int obj_last_non_gc_index = _memory->ReadInt(address - 0xC);
	//const int max_objects_not_considered_by_gc = _memory->ReadInt(address - 0x8);
	//const int openForDisregardForGC = _memory->ReadInt(address - 0x4);
	//if (obj_first_gc_index < 0xF) return false;
	//if (obj_last_non_gc_index < 0xF) return false;
	//if (max_objects_not_considered_by_gc < 0xF) return false;
	//if (openForDisregardForGC > 1 || openForDisregardForGC < 0) return false; // it's bool
	//// if (obj_first_gc_index != obj_last_non_gc_index + 1) return false;
	//// if (max_objects_not_considered_by_gc != obj_first_gc_index) return false;
	//// if (openForDisregardForGC != 0) return false;

	// Check (UObject*) (Object1) Is Valid Pointer
	uintptr_t ptrUObject0;
	if (!IsValidPointer(address, ptrUObject0, false)) return false;

	// Check (UObject*) (Object2) Is Valid Pointer
	uintptr_t ptrUObject1;
	if (!IsValidPointer(address + dwBetweenObjects, ptrUObject1, false)) return false;

	// Check vfTableObject Is Valid Pointer
	uintptr_t ptrVfTableObject0, ptrVfTableObject1;
	if (!IsValidPointer(ptrUObject0, ptrVfTableObject0, false)) return false;
	if (!IsValidPointer(ptrUObject1, ptrVfTableObject1, false)) return false;

	// Check Objects (InternalIndex)
	const int uObject1InternalIndex = _memory->ReadInt(ptrUObject0 + dwInternalIndex);
	const int uObject2InternalIndex = _memory->ReadInt(ptrUObject1 + dwInternalIndex);

	if (!(uObject1InternalIndex == 0 && (uObject2InternalIndex == 1 || uObject2InternalIndex == 3))) return false;

	tArray.Data = address;
	tArray.Max = 0;
	tArray.Num = 0;
	return true;
}