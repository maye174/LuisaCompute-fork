#pragma once
#include <VEngineConfig.h>
#include <stdint.h>
#include <Common/Hash.h>
#include <Common/Memory.h>
#include <type_traits>
#include <new>
template<typename T>
struct RunnableHash;
template<class T>
class Runnable;
template<class _Ret,
		 class... _Types>
class Runnable<_Ret(_Types...)> {
	/////////////////////Define
	friend class RunnableHash<_Ret(_Types...)>;
	static constexpr size_t PLACEHOLDERSIZE = 40;
	using PlaceHolderType = std::aligned_storage_t<PLACEHOLDERSIZE, alignof(size_t)>;
	struct IProcessFunctor {
		virtual void Delete(void* ptr) const = 0;
		virtual void CopyConstruct(void*& dst, void const* src, PlaceHolderType* holder) const = 0;
		virtual void MoveConstruct(void*& dst, void* src, PlaceHolderType* holder) const = 0;
		virtual _Ret Run(void*, _Types&&...) const = 0;
	};
	using ProcessorHolder = std::aligned_storage_t<sizeof(IProcessFunctor), alignof(IProcessFunctor)>;
	/////////////////////Data
	void* placePtr;
	ProcessorHolder logicPlaceHolder;
	PlaceHolderType funcPtrPlaceHolder;

public:
	operator bool() const noexcept {
		size_t const* logicHolder = reinterpret_cast<size_t const*>(&logicPlaceHolder);
		return *logicHolder != 0;
	}

	void Dispose() noexcept {
		size_t* logicHolder = reinterpret_cast<size_t*>(&logicPlaceHolder);
		if (*logicHolder != 0) {
			reinterpret_cast<IProcessFunctor const*>(&logicPlaceHolder)->Delete(placePtr);
			*logicHolder = 0;
		}
		placePtr = nullptr;
	}

	Runnable() noexcept
		: placePtr(nullptr) {
		size_t* logicHolder = reinterpret_cast<size_t*>(&logicPlaceHolder);
		*logicHolder = 0;
	}

	Runnable(std::nullptr_t) noexcept
		: Runnable() {
	}

	Runnable(funcPtr_t<_Ret(_Types...)> p) noexcept {
		struct FuncPtrLogic final : public IProcessFunctor {
			void Delete(void* ptr) const override {
			}
			void CopyConstruct(void*& dest, void const* source, PlaceHolderType* placeHolder) const override {
				dest = placeHolder;
				*reinterpret_cast<size_t*>(dest) = *reinterpret_cast<size_t const*>(source);
			}
			void MoveConstruct(void*& dst, void* src, PlaceHolderType* placeHolder) const override {
				*reinterpret_cast<size_t*>(dst) = *reinterpret_cast<size_t const*>(src);
			}
			_Ret Run(void* pp, _Types&&... tt) const override {
				funcPtr_t<_Ret(_Types...)> fp = reinterpret_cast<funcPtr_t<_Ret(_Types...)>>(*(void**)pp);
				return fp(std::forward<_Types>(tt)...);
			}
		};
		new (&logicPlaceHolder) FuncPtrLogic();
		*reinterpret_cast<size_t*>(placePtr) = *(reinterpret_cast<size_t const*>(&p));
	}

	Runnable(const Runnable<_Ret(_Types...)>& f) noexcept
		: logicPlaceHolder(f.logicPlaceHolder) {
		size_t const* logicHolder = reinterpret_cast<size_t const*>(&logicPlaceHolder);
		if (*logicHolder != 0) {
			reinterpret_cast<IProcessFunctor const*>(&logicPlaceHolder)->CopyConstruct(placePtr, f.placePtr, &funcPtrPlaceHolder);
		}
	}
	Runnable(Runnable<_Ret(_Types...)>&& f) noexcept
		: logicPlaceHolder(f.logicPlaceHolder) {
		size_t const* logicHolder = reinterpret_cast<size_t const*>(&logicPlaceHolder);
		if (*logicHolder != 0) {
			reinterpret_cast<IProcessFunctor const*>(&logicPlaceHolder)->MoveConstruct(placePtr, f.placePtr, &funcPtrPlaceHolder);
		}
		f.placePtr = nullptr;
		size_t* flogicHolder = reinterpret_cast<size_t*>(&f.logicPlaceHolder);
		*flogicHolder = 0;
	}

	template<typename Functor>
	Runnable(Functor&& f) noexcept {
		if constexpr (std::is_same_v<Functor&&, Runnable<_Ret(_Types...)>&>) {
			logicPlaceHolder = f.logicPlaceHolder;
			size_t const* logicHolder = reinterpret_cast<size_t const*>(&logicPlaceHolder);
			if (*logicHolder != 0) {
				reinterpret_cast<IProcessFunctor const*>(&logicPlaceHolder)->CopyConstruct(placePtr, f.placePtr, &funcPtrPlaceHolder);
			}
		} else {

			using PureType = std::remove_cvref_t<Functor>;
			constexpr bool USE_HEAP = (sizeof(PureType) > PLACEHOLDERSIZE);
			struct FuncPtrLogic final : public IProcessFunctor {
				void Delete(void* pp) const override {
					PureType* ff = (PureType*)pp;
					ff->~PureType();
					if constexpr (USE_HEAP) {
						vengine_free(pp);
					}
				}
				void CopyConstruct(void*& dest, void const* source, PlaceHolderType* placeHolder) const override {
					if constexpr (USE_HEAP) {
						dest = vengine_malloc(sizeof(PureType));
					} else {
						dest = placeHolder;
					}
					new (dest) PureType(*(PureType const*)source);
				}
				void MoveConstruct(void*& dest, void* source, PlaceHolderType* placeHolder) const override {
					if constexpr (USE_HEAP) {
						dest = source;
					} else {
						dest = placeHolder;
						new (dest) PureType(
							std::move(*reinterpret_cast<PureType*>(source)));
					}
				}
				_Ret Run(void* pp, _Types&&... tt) const override {
					PureType* ff = (PureType*)pp;
					return (*ff)(std::forward<_Types>(tt)...);
				}
			};
			new (&logicPlaceHolder) FuncPtrLogic();
			if constexpr (USE_HEAP) {
				placePtr = vengine_malloc(sizeof(PureType));
			} else {
				placePtr = &funcPtrPlaceHolder;
			}
			new (placePtr) PureType(std::forward<Functor>(f));
		}
	}

	void operator=(std::nullptr_t) noexcept {
		Dispose();
	}
	template<typename Functor>
	void operator=(Functor&& f) noexcept {
		~Runnable();
		new (this) Runnable(std::forward<Functor>(f));
	}

	_Ret operator()(_Types... t) const noexcept {
		return reinterpret_cast<IProcessFunctor const*>(&logicPlaceHolder)->Run(placePtr, std::forward<_Types>(t)...);
	}
	~Runnable() noexcept {
		size_t const* logicHolder = reinterpret_cast<size_t const*>(&logicPlaceHolder);
		if (*logicHolder != 0) {
			reinterpret_cast<IProcessFunctor const*>(&logicPlaceHolder)->Delete(placePtr);
		}
	}
};

template<typename T>
struct RunnableHash {
	size_t operator()(const Runnable<T>& runnable) const noexcept {
		vengine::hash<size_t> h;
		return h(*reinterpret_cast<size_t const*>(&runnable.logicPlaceHolder));
	}
};