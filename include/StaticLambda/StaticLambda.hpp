#pragma once

#include <utility>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <atomic>

struct _StaticLambda_MemBase
{
	char code[256];
	void(*destroy)(_StaticLambda_MemBase* _mem);
#ifndef NDEBUG
	std::atomic<intptr_t> called;
#endif // !NDEBUG
	size_t allocated_size;
};

template <typename T>
struct _StaticLambda_tag_type {};

_StaticLambda_MemBase* _StaticLambda_Alloc(size_t size, void* near_target);
void _StaticLambda_Destroy(_StaticLambda_MemBase* mem);


static constexpr uintptr_t _StaticLambda_TAG = 0x0123456789abcdef;

template <typename>
struct _StaticLambda_FuncUtils;

template <typename TRet, typename... TArgs>
struct _StaticLambda_FuncUtils<TRet(TArgs...)>
{
	using func_t = TRet(*)(TArgs...);

	template <typename TLambda, typename Base>
	struct Mem : Base
	{
		TLambda lambda;
		TRet (*call)(Mem* mem, TArgs... args);

		static TRet CallProxy(TArgs... args)
		{
			volatile auto mem = reinterpret_cast<Mem*>(_StaticLambda_TAG);
			return mem->call(mem, args...);
		}

		struct DebugCalledCounterExit
		{
			Mem* mem;
#ifndef NDEBUG
			DebugCalledCounterExit(Mem* _mem) : mem{ _mem } { mem->called += 1; }
			~DebugCalledCounterExit() { mem->called -= 1; }
#endif // !NDEBUG
		};

		__declspec(noinline)
		static TRet Call(Mem* mem, TArgs... args)
		{
			DebugCalledCounterExit e(mem);

			if constexpr (std::is_same_v<Base, _StaticLambda_MemBase>)
				return mem->lambda(args...);
			else
				return mem->lambda(args..., mem);
		}
	};
};

template <typename TSignature>
struct StaticLambda
{
	_StaticLambda_MemBase* _mem = nullptr;


	template <typename TLambda, typename TNearTarget = std::nullptr_t, typename TMemBase = _StaticLambda_MemBase>
	explicit StaticLambda(TLambda&& lambda, TNearTarget near_target = nullptr, _StaticLambda_tag_type<TMemBase> = _StaticLambda_tag_type<TMemBase>{})
	{
		using mem_t = _StaticLambda_FuncUtils<TSignature>::template Mem<TLambda, TMemBase>;
		auto mem = (mem_t*)_StaticLambda_Alloc(sizeof(mem_t), (void*)near_target);

		mem->allocated_size = sizeof(mem_t);

		auto code_size = uintptr_t(&mem->Call) - uintptr_t(&mem->CallProxy);
		memcpy(mem->code, &mem->CallProxy, code_size);

		for (size_t i = 0; i < code_size; ++i)
		{
			if (memcmp(&mem->code[i], &_StaticLambda_TAG, sizeof(_StaticLambda_TAG)) == 0)
			{
				memcpy(&mem->code[i], &mem, sizeof(&mem));
				break;
			}
		}

		mem->destroy = [](_StaticLambda_MemBase* _mem) {
			std::destroy_at(&((mem_t*)_mem)->lambda);
		};

		std::construct_at(&mem->lambda, std::move(lambda));

		mem->call = &mem->Call;

		_mem = mem;
	}

	~StaticLambda()
	{
		if (_mem)
			_StaticLambda_Destroy(_mem);
	}

	auto GetTarget() const { return (typename _StaticLambda_FuncUtils<TSignature>::func_t)(char*)_mem->code; }
};
