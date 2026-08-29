#include "StaticLambda/StaticLambda.hpp"

static size_t align_up_to_page(size_t size)
{
	return (size + 4095) & ~4095;
}

static void* align_down_to_page(void* val)
{
	return (void*)(uintptr_t(val) & ~uintptr_t(0x1000 - 1));
}

#if !defined(NDEBUG)
#include <vector>
#include <mutex>
static std::mutex g_mtx;
static std::vector<_StaticLambda_MemBase*> g_to_free;

static void _StaticLambda_Free(_StaticLambda_MemBase* mem);

static void _StaticLambda_CleanDebugMem()
{
	// NOTE: Don't care about data race
	if (g_to_free.empty())
		return;

	std::unique_lock lck(g_mtx);

	for (auto it = g_to_free.begin(); it != g_to_free.end();)
	{
		if ((*it)->called == 0)
		{
			_StaticLambda_Free(*it);
			it = g_to_free.erase(it);
		}
		else
		{
			++it;
		}
	}
}
#endif // !defined(NDEBUG)

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")

typedef enum _MEMORY_INFORMATION_CLASS
{
	MemoryBasicInformation,                     // q: MEMORY_BASIC_INFORMATION
	MemoryWorkingSetInformation,                // q: MEMORY_WORKING_SET_INFORMATION
	MemoryMappedFilenameInformation,            // q: UNICODE_STRING
	MemoryRegionInformation,                    // q: MEMORY_REGION_INFORMATION/MEMORY_REGION_INFORMATION_EX
	MemoryWorkingSetExInformation,              // q: MEMORY_WORKING_SET_EX_INFORMATION // since VISTA
	MemorySharedCommitInformation,              // q: MEMORY_SHARED_COMMIT_INFORMATION // since WIN8
	MemoryImageInformation,                     // q: MEMORY_IMAGE_INFORMATION
	MemoryRegionInformationEx,                  // q: MEMORY_REGION_INFORMATION/MEMORY_REGION_INFORMATION_EX
	MemoryPrivilegedBasicInformation,           // q: MEMORY_BASIC_INFORMATION
	MemoryEnclaveImageInformation,              // q: MEMORY_ENCLAVE_IMAGE_INFORMATION // since REDSTONE3
	MemoryBasicInformationCapped,               // q: 10
	MemoryPhysicalContiguityInformation,        // q: MEMORY_PHYSICAL_CONTIGUITY_INFORMATION // since 20H1
	MemoryBadInformation,                       // q: MEMORY_BAD_INFORMATION // since WIN11
	MemoryBadInformationAllProcesses,           // qs: not implemented // since 22H1
	MemoryImageExtensionInformation,            // q: MEMORY_IMAGE_EXTENSION_INFORMATION // since 24H2
	MaxMemoryInfoClass
} MEMORY_INFORMATION_CLASS;

extern "C"
NTSYSCALLAPI
NTSTATUS
NTAPI
NtAllocateVirtualMemory(
	_In_ HANDLE ProcessHandle,
	_Inout_ _At_(*BaseAddress, _Readable_bytes_(*RegionSize) _Writable_bytes_(*RegionSize) _Post_readable_byte_size_(*RegionSize)) PVOID *BaseAddress,
	_In_ ULONG_PTR ZeroBits,
	_Inout_ PSIZE_T RegionSize,
	_In_ ULONG AllocationType,
	_In_ ULONG PageProtection
	);

extern "C"
NTSYSCALLAPI
NTSTATUS
NTAPI
NtFreeVirtualMemory(
	_In_ HANDLE ProcessHandle,
	_Inout_ __drv_freesMem(Mem) PVOID *BaseAddress,
	_Inout_ PSIZE_T RegionSize,
	_In_ ULONG FreeType
	);

extern "C"
NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryVirtualMemory(
	_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress,
	_In_ MEMORY_INFORMATION_CLASS MemoryInformationClass,
	_Out_writes_bytes_(MemoryInformationLength) PVOID MemoryInformation,
	_In_ SIZE_T MemoryInformationLength,
	_Out_opt_ PSIZE_T ReturnLength
	);

static void _StaticLambda_Free(_StaticLambda_MemBase* mem)
{
	void* addr = mem;
	SIZE_T size = 0;
	if (NTSTATUS status = NtFreeVirtualMemory(HANDLE(-1), &addr, &size, MEM_RELEASE); !NT_SUCCESS(status))
		throw 1;
}

static _StaticLambda_MemBase* _StaticLambda_TryAllocAt(void* target, size_t size)
{
	void* result = align_down_to_page(target);
	NTSTATUS status = NtAllocateVirtualMemory(HANDLE(-1), &result, 0, &size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (NT_SUCCESS(status))
	{
		((_StaticLambda_MemBase*)result)->allocated_size = size;
		return (_StaticLambda_MemBase*)result;
	}

	return nullptr;
}

static _StaticLambda_MemBase* TryAlloc2gbUpSegmented(void* near_target, size_t size)
{
	uintptr_t target = uintptr_t(near_target);

	uintptr_t gb2 = uintptr_t(2) * 1024 * 1024 * 1024;

	while (target < uintptr_t(near_target) + gb2 - 4096)
	{
		MEMORY_BASIC_INFORMATION mbi;

		NTSTATUS status = NtQueryVirtualMemory(
			HANDLE(-1),
			(void*)target,
			MemoryBasicInformation,
			&mbi,
			sizeof(mbi),
			nullptr
		);

		if (NT_SUCCESS(status) && mbi.State == MEM_FREE && mbi.RegionSize >= size)
		{
			if (auto result = _StaticLambda_TryAllocAt((void*)target, size))
				return result;
		}

		target += mbi.RegionSize;
	}

	return nullptr;
}

static _StaticLambda_MemBase* TryAlloc2gbDownSegmented(void* near_target, size_t size)
{
	uintptr_t target = uintptr_t(near_target) - 4096;

	uintptr_t gb2 = uintptr_t(2) * 1024 * 1024 * 1024;

	while (target > uintptr_t(near_target) - gb2 + 4096)
	{
		MEMORY_BASIC_INFORMATION mbi;

		NTSTATUS status = NtQueryVirtualMemory(
			HANDLE(-1),
			(void*)target,
			MemoryBasicInformation,
			&mbi,
			sizeof(mbi),
			nullptr
		);

		if (NT_SUCCESS(status) && mbi.State == MEM_FREE && mbi.RegionSize >= size)
		{
			if (auto result = _StaticLambda_TryAllocAt((void*)target, size))
				return result;
		}

		target -= mbi.RegionSize;
	}

	return nullptr;
}
#else
#include <sys/mman.h>

static void _StaticLambda_Free(_StaticLambda_MemBase* mem)
{
	if (munmap(mem, mem->allocated_size) == -1)
		throw 1;
}

static _StaticLambda_MemBase* _StaticLambda_TryAllocAt(void* target, size_t size)
{
	// Must be aligned for mmap
	size = align_up_to_page(size);

	int flags = MAP_PRIVATE | MAP_ANONYMOUS;

	if (target)
		flags |= MAP_FIXED_NOREPLACE;

	void* aligned_target = align_down_to_page(target);

	void* result = mmap(aligned_target, size, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0);

	if (result == MAP_FAILED)
		return nullptr;

	((_StaticLambda_MemBase*)result)->allocated_size = size;
	return (_StaticLambda_MemBase*)result;
}

#endif // defined(_WIN32)

static _StaticLambda_MemBase* TryAlloc2gbUpStepped(void* near_target, size_t size, size_t step)
{
	uintptr_t target = uintptr_t(near_target);

	const uintptr_t gb2 = uintptr_t(2) * 1024 * 1024 * 1024;

	while (target > uintptr_t(near_target) - gb2 + 4096)
	{
		if (auto result = _StaticLambda_TryAllocAt((void*)target, size))
			return result;

		target += step;
	}

	return nullptr;
}

static _StaticLambda_MemBase* TryAlloc2gbDownStepped(void* near_target, size_t size, size_t step)
{
	uintptr_t target = uintptr_t(near_target);

	const uintptr_t gb2 = uintptr_t(2) * 1024 * 1024 * 1024;

	while (target > uintptr_t(near_target) - gb2 + 4096)
	{
		if (auto result = _StaticLambda_TryAllocAt((void*)target, size))
			return result;

		target -= step;
	}

	return nullptr;
}

_StaticLambda_MemBase* _StaticLambda_Alloc(size_t size, void* near_target)
{
#if !defined(NDEBUG)
	_StaticLambda_CleanDebugMem();
#endif // !defined(NDEBUG)

	if (!near_target)
		return _StaticLambda_TryAllocAt(nullptr, size);

#if defined(_WIN32)
	if (auto result = TryAlloc2gbUpSegmented(near_target, size))
		return result;

	if (auto result = TryAlloc2gbDownSegmented(near_target, size))
		return result;
#endif // defined(_WIN32)

	if (auto result = TryAlloc2gbUpStepped(near_target, size, 0x10000))
		return result;

	if (auto result = TryAlloc2gbDownStepped(near_target, size, 0x10000))
		return result;

	return nullptr;
}

void _StaticLambda_Destroy(_StaticLambda_MemBase* mem)
{
	mem->destroy(mem);

#if !defined(NDEBUG)
	if (mem->called > 0)
	{
		std::unique_lock lck(g_mtx);
		g_to_free.push_back(mem);
		return;
	}
#endif // !defined(NDEBUG)

	_StaticLambda_Free(mem);
}
