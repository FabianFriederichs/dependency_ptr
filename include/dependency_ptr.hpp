#ifndef _DPTR_DEPENDENCY_PTR_H_
#define _DPTR_DEPENDENCY_PTR_H_

#include <type_traits>
#include <atomic>
#include <ostream>
#include <utility>
#include <cstddef>

#include <iostream>
#ifndef NDEBUG
#define DPTR_ASSERT(condition, message)\
	(!(condition) ?\
		(std::cerr << "Assertion failed: (" << #condition << ")" << std::endl <<\
		"\tfile: " << __FILE__ << std::endl <<\
		"\tline: " << __LINE__ << std::endl <<\
		"\tmessage:" << message << std::endl, std::abort()) : (void)0)
#else
#define DPTR_ASSERT(condition, message) 0
#endif

#ifdef NDEBUG
#define DEBUG_CHOICE(debug, release) release
#else
#define DEBUG_CHOICE(debug, release) debug
#endif

namespace dptr
{
	namespace detail
	{
		#pragma region intrusive_ptr

		// Mostly equivalent to boost::intrusive_ptr. Avoids a dependency on boost.
		template <typename T>
		class intrusive_ptr
		{
		public:
			using element_type = T;
			using pointer = T*;
			using difference_type = std::ptrdiff_t;

			intrusive_ptr() noexcept;
			intrusive_ptr(T* ptr, bool add_ref = true);

			intrusive_ptr(const intrusive_ptr& other);
			template <typename U>
			intrusive_ptr(const intrusive_ptr<U>& other);
			intrusive_ptr& operator=(const intrusive_ptr& other);
			template <typename U>
			intrusive_ptr& operator=(const intrusive_ptr<U> &other);

			intrusive_ptr(intrusive_ptr&& other) noexcept;
			template <typename U>
			intrusive_ptr(intrusive_ptr<U>&& other) noexcept;
			intrusive_ptr& operator=(intrusive_ptr&& other);
			template <typename U>
			intrusive_ptr& operator=(intrusive_ptr<U>&& other);

			intrusive_ptr& operator=(T* ptr);

			void reset();
			void reset(T* ptr);
			void reset(T* ptr, bool add_ref);

			T& operator*() const noexcept;
			T* operator->() const noexcept;
			T* get() const noexcept;
			T* detach() noexcept;

			explicit operator bool() const noexcept;

			void swap(intrusive_ptr& rhs) noexcept;

			~intrusive_ptr();
		private:
			T* m_ptr;
		};

		template <typename T, typename U>
		bool operator==(const intrusive_ptr<T>& lhs, const intrusive_ptr<U>& rhs) noexcept;
		template <typename T, typename U>
		bool operator!=(const intrusive_ptr<T>& lhs, const intrusive_ptr<U>& rhs) noexcept;
		template <typename T, typename U>
		bool operator==(const intrusive_ptr<T>& lhs, U* rhs) noexcept;
		template <typename T, typename U>
		bool operator!=(const intrusive_ptr<T>& lhs, U* rhs) noexcept;
		template <typename T, typename U>
		bool operator==(U* lhs, const intrusive_ptr<T>& rhs) noexcept;
		template <typename T, typename U>
		bool operator!=(U* lhs, const intrusive_ptr<T>& rhs) noexcept;

		template <typename T>
		void swap(intrusive_ptr<T>& lhs, intrusive_ptr<T>& rhs) noexcept;
		template <typename T>
		T* get_pointer(const intrusive_ptr<T>& iptr) noexcept;
		template <typename T, typename U>
		intrusive_ptr<T> static_pointer_cast(const intrusive_ptr<U>& iptr) noexcept;
		template <typename T, typename U>
		intrusive_ptr<T> const_pointer_cast(const intrusive_ptr<U>& iptr) noexcept;
		template <typename T, typename U>
		intrusive_ptr<T> dynamic_pointer_cast(const intrusive_ptr<U>& iptr) noexcept;
		template <typename Elem, typename Traits, typename T>
		std::basic_ostream<Elem, Traits>& operator<<(std::basic_ostream<Elem, Traits>&, const intrusive_ptr<T>& iptr);

		#pragma endregion

		#pragma region guarded_dependency
		// TODO: add a set of rule parameters, specifying which operations are illegal on still-referenced dependencies, e.g. copy, move assignment, move from...
		// --- empty dummy class for release. should enable empty base optimization
		class guarded_dependency_nop {};

		// --- implemented variant for debug
		template <bool atomic = false>
		class guarded_dependency_impl;
		template<bool atomic>
		void intrusive_ptr_add_ref(const guarded_dependency_impl<atomic>* dep) noexcept;
		template<bool atomic>
		void intrusive_ptr_release(const guarded_dependency_impl<atomic>* dep) noexcept;
		template<>
		class guarded_dependency_impl<true>
		{
			friend void intrusive_ptr_add_ref<>(const guarded_dependency_impl*) noexcept;
			friend void intrusive_ptr_release<>(const guarded_dependency_impl*) noexcept;
		protected:
			guarded_dependency_impl() noexcept;
			guarded_dependency_impl(const guarded_dependency_impl& other) noexcept;
			guarded_dependency_impl(guarded_dependency_impl&& other) noexcept;
			guarded_dependency_impl& operator=(const guarded_dependency_impl& other) noexcept;
			guarded_dependency_impl& operator=(guarded_dependency_impl&& other) noexcept;
			~guarded_dependency_impl();
		private:
			void inc() const noexcept;
			void dec() const noexcept;

			using counter_t = std::atomic<std::size_t>;
			mutable counter_t m_counter;
		};
		template<>
		class guarded_dependency_impl<false>
		{
			friend void intrusive_ptr_add_ref<>(const guarded_dependency_impl*) noexcept;
			friend void intrusive_ptr_release<>(const guarded_dependency_impl*) noexcept;
		protected:
			guarded_dependency_impl() noexcept;
			guarded_dependency_impl(const guarded_dependency_impl& other) noexcept;
			guarded_dependency_impl(guarded_dependency_impl&& other) noexcept;
			guarded_dependency_impl& operator=(const guarded_dependency_impl& other) noexcept;
			guarded_dependency_impl& operator=(guarded_dependency_impl&& other) noexcept;
			~guarded_dependency_impl();
		private:
			void inc() const noexcept;
			void dec() const noexcept;

			using counter_t = std::size_t;
			mutable counter_t m_counter;
		};		

		template<bool atomic>
		using guarded_dependency_type = DEBUG_CHOICE(guarded_dependency_impl<atomic>, guarded_dependency_nop);
		#pragma endregion

		#pragma region dependency_pointer_impl
		template <typename T>
		class dependency_pointer_impl : public intrusive_ptr<T>
		{
			static_assert(std::is_base_of_v<guarded_dependency_type<false>, std::remove_cv_t<T>> || std::is_base_of_v<guarded_dependency_type<true>, std::remove_cv_t<T>>, "[dptr::detail::dependency_pointer_impl]: dependency_ptr can only be used with pointees deriving guarded_dependency<bool>.");
		public:
			using intrusive_ptr<T>::intrusive_ptr;
			operator typename intrusive_ptr<T>::pointer() const noexcept { return intrusive_ptr<T>::get(); }
		};
		#pragma endregion
	}	

	template <typename T>
	using dependency_ptr = DEBUG_CHOICE(detail::dependency_pointer_impl<T>, T*);
	template <bool atomic>
	using guarded_dependency = detail::guarded_dependency_type<atomic>;
}

#pragma region implementation
template<typename T>
inline dptr::detail::intrusive_ptr<T>::intrusive_ptr() noexcept :
	m_ptr(nullptr)
{
}
template<typename T>
inline dptr::detail::intrusive_ptr<T>::intrusive_ptr(T* ptr, bool add_ref) :
	m_ptr(ptr)
{
	if(ptr && add_ref) intrusive_ptr_add_ref(m_ptr);
}
template<typename T>
inline dptr::detail::intrusive_ptr<T>::intrusive_ptr(const intrusive_ptr& other) :
	m_ptr(other.m_ptr)
{
	if(m_ptr) intrusive_ptr_add_ref(m_ptr);
}
template <typename T>
template <typename U>
inline dptr::detail::intrusive_ptr<T>::intrusive_ptr(const intrusive_ptr<U>& other) :
	m_ptr(other.m_ptr)
{
	if(m_ptr) intrusive_ptr_add_ref(m_ptr);
}
template<typename T>
inline dptr::detail::intrusive_ptr<T>& dptr::detail::intrusive_ptr<T>::operator=(const intrusive_ptr& other)
{
	intrusive_ptr(other).swap(*this);
	return *this;
}
template <typename T>
template <typename U>
inline dptr::detail::intrusive_ptr<T>& dptr::detail::intrusive_ptr<T>::operator=(const intrusive_ptr<U>& other)
{
	intrusive_ptr(other).swap(*this);
	return *this;
}
template<typename T>
inline dptr::detail::intrusive_ptr<T>::intrusive_ptr(intrusive_ptr&& other) noexcept :
	m_ptr(std::exchange(other.m_ptr, nullptr))
{
}
template <typename T>
template <typename U>
inline dptr::detail::intrusive_ptr<T>::intrusive_ptr(intrusive_ptr<U>&& other) noexcept :
	m_ptr(std::exchange(other.m_ptr, nullptr))
{
}
template<typename T>
inline dptr::detail::intrusive_ptr<T>& dptr::detail::intrusive_ptr<T>::operator=(intrusive_ptr&& other)
{
	intrusive_ptr(std::move(other)).swap(*this);
	return *this;
}
template <typename T>
template <typename U>
inline dptr::detail::intrusive_ptr<T>& dptr::detail::intrusive_ptr<T>::operator=(intrusive_ptr<U>&& other)
{
	intrusive_ptr(std::move(other)).swap(*this);
	return *this;
}
template<typename T>
inline dptr::detail::intrusive_ptr<T>& dptr::detail::intrusive_ptr<T>::operator=(T* ptr)
{
	intrusive_ptr(ptr).swap(*this);
	return *this;
}
template<typename T>
inline void dptr::detail::intrusive_ptr<T>::reset()
{
	intrusive_ptr().swap(*this);
}
template<typename T>
inline void dptr::detail::intrusive_ptr<T>::reset(T* ptr)
{
	intrusive_ptr(ptr).swap(*this);
}
template<typename T>
inline void dptr::detail::intrusive_ptr<T>::reset(T* ptr, bool add_ref)
{
	intrusive_ptr(ptr, add_ref).swap(*this);
}
template<typename T>
inline T& dptr::detail::intrusive_ptr<T>::operator*() const noexcept
{
	DPTR_ASSERT(m_ptr, "[dptr::detail::intrusive_ptr::operator*]: nullptr access.");
	return *m_ptr;
}
template<typename T>
inline T* dptr::detail::intrusive_ptr<T>::operator->() const noexcept
{
	DPTR_ASSERT(m_ptr, "[dptr::detail::intrusive_ptr::operator->]: nullptr access.");
	return m_ptr;
}
template<typename T>
inline T* dptr::detail::intrusive_ptr<T>::get() const noexcept
{
	return m_ptr;
}
template<typename T>
inline T* dptr::detail::intrusive_ptr<T>::detach() noexcept
{
	return std::exchange(m_ptr, nullptr);
}
template<typename T>
inline dptr::detail::intrusive_ptr<T>::operator bool() const noexcept
{
	return m_ptr;
}
template<typename T>
inline void dptr::detail::intrusive_ptr<T>::swap(intrusive_ptr& rhs) noexcept
{
	using std::swap;
	swap(m_ptr, rhs.m_ptr);
}
template<typename T>
inline dptr::detail::intrusive_ptr<T>::~intrusive_ptr()
{
	if(m_ptr) intrusive_ptr_release(m_ptr);
}
template<typename T, typename U>
inline bool dptr::detail::operator==(const intrusive_ptr<T>& lhs, const intrusive_ptr<U>& rhs) noexcept
{
	return lhs.get() != rhs.get();
}
template<typename T, typename U>
inline bool dptr::detail::operator!=(const intrusive_ptr<T>& lhs, const intrusive_ptr<U>& rhs) noexcept
{
	return lhs.get() != rhs.get();
}
template<typename T, typename U>
inline bool dptr::detail::operator==(const intrusive_ptr<T>& lhs, U* rhs) noexcept
{
	return lhs.get() == rhs;
}
template<typename T, typename U>
inline bool dptr::detail::operator!=(const intrusive_ptr<T>& lhs, U* rhs) noexcept
{
	return lhs.get() != rhs;
}
template<typename T, typename U>
inline bool dptr::detail::operator==(U* lhs, const intrusive_ptr<T>& rhs) noexcept
{
	return lhs == rhs.get();
}
template<typename T, typename U>
inline bool dptr::detail::operator!=(U* lhs, const intrusive_ptr<T>& rhs) noexcept
{
	return lhs != rhs.get();
}
template<typename T>
inline void dptr::detail::swap(intrusive_ptr<T>& lhs, intrusive_ptr<T>& rhs) noexcept
{
	lhs.swap(rhs);
}
template<typename T>
inline T* dptr::detail::get_pointer(const intrusive_ptr<T>& iptr) noexcept
{
	return iptr.get();
}
template<typename T, typename U>
inline dptr::detail::intrusive_ptr<T> dptr::detail::static_pointer_cast(const intrusive_ptr<U>& iptr) noexcept
{
	return intrusive_ptr(static_cast<typename intrusive_ptr<T>::element_type*>(iptr.get()));
}
template<typename T, typename U>
inline dptr::detail::intrusive_ptr<T> dptr::detail::const_pointer_cast(const intrusive_ptr<U>& iptr) noexcept
{
	return intrusive_ptr(const_cast<typename intrusive_ptr<T>::element_type*>(iptr.get()));
}
template<typename T, typename U>
inline dptr::detail::intrusive_ptr<T> dptr::detail::dynamic_pointer_cast(const intrusive_ptr<U>& iptr) noexcept
{
	return intrusive_ptr(dynamic_cast<typename intrusive_ptr<T>::element_type*>(iptr.get()));
}
template<typename Elem, typename Traits, typename T>
inline std::basic_ostream<Elem, Traits>& dptr::detail::operator<<(std::basic_ostream<Elem, Traits>& strm, const intrusive_ptr<T>& iptr)
{
	return strm << iptr.get();
}

// --- atomic version
inline dptr::detail::guarded_dependency_impl<true>::guarded_dependency_impl() noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
}
inline dptr::detail::guarded_dependency_impl<true>::guarded_dependency_impl(const guarded_dependency_impl& other) noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
}
inline dptr::detail::guarded_dependency_impl<true>::guarded_dependency_impl(guarded_dependency_impl&& other) noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
	// if there are still dependents, moving from the object gives undefined behaviour
	DPTR_ASSERT(other.m_counter.load(std::memory_order_relaxed) == 0ull, "[dptr::guarded_dependency_impl::guarded_dependency_impl(move ctor)]: There were still (now invalid!) pointers referencing the moved-from object.");
}
inline dptr::detail::guarded_dependency_impl<true>& dptr::detail::guarded_dependency_impl<true>::operator=(const guarded_dependency_impl& other) noexcept
{
	// object stays at the same address => do not modify counter
	return *this;
}
inline dptr::detail::guarded_dependency_impl<true>& dptr::detail::guarded_dependency_impl<true>::operator=(guarded_dependency_impl&& other) noexcept
{
	// object stays at the same address => do not modify counter
	// if there are still dependents, moving from the object gives undefined behaviour
	DPTR_ASSERT(other.m_counter.load(std::memory_order_relaxed) == 0ull, "[dptr::guarded_dependency_impl::operator=(move)]: There were still (now invalid!) pointers referencing the moved-from object.");
	return *this;
}
inline dptr::detail::guarded_dependency_impl<true>::~guarded_dependency_impl()
{
	DPTR_ASSERT(m_counter.load(std::memory_order_relaxed) == 0ull, "[dptr::guarded_dependency_impl::~guarded_dependency_impl]: There were still (now dangling!) pointers referencing this object.");
}
inline void dptr::detail::guarded_dependency_impl<true>::inc() const noexcept
{
	m_counter.fetch_add(1ull, std::memory_order::memory_order_relaxed);
}
inline void dptr::detail::guarded_dependency_impl<true>::dec() const noexcept
{
	m_counter.fetch_sub(1ull, std::memory_order::memory_order_relaxed);
}

// --- non atomic version
inline dptr::detail::guarded_dependency_impl<false>::guarded_dependency_impl() noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
}
inline dptr::detail::guarded_dependency_impl<false>::guarded_dependency_impl(const guarded_dependency_impl& other) noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
}
inline dptr::detail::guarded_dependency_impl<false>::guarded_dependency_impl(guarded_dependency_impl&& other) noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
	// if there are still dependents, moving from the object gives undefined behaviour
	DPTR_ASSERT(other.m_counter == 0ull, "[dptr::guarded_dependency_impl::guarded_dependency_impl(move ctor)]: There were still (now invalid!) pointers referencing the moved-from object.");
}
inline dptr::detail::guarded_dependency_impl<false>& dptr::detail::guarded_dependency_impl<false>::operator=(const guarded_dependency_impl& other) noexcept
{
	// object stays at the same address => do not modify counter
	return *this;
}
inline dptr::detail::guarded_dependency_impl<false>& dptr::detail::guarded_dependency_impl<false>::operator=(guarded_dependency_impl&& other) noexcept
{
	// object stays at the same address => do not modify counter
	// if there are still dependents, moving from the object gives undefined behaviour
	DPTR_ASSERT(other.m_counter == 0ull, "[dptr::guarded_dependency_impl::operator=(move)]: There were still (now invalid!) pointers referencing the moved-from object.");
	return *this;
}
inline dptr::detail::guarded_dependency_impl<false>::~guarded_dependency_impl()
{
	DPTR_ASSERT(m_counter == 0ull, "[dptr::guarded_dependency_impl::~guarded_dependency_impl]: There were still (now dangling!) pointers referencing this object.");
}
inline void dptr::detail::guarded_dependency_impl<false>::inc() const noexcept
{
	++m_counter;
}
inline void dptr::detail::guarded_dependency_impl<false>::dec() const noexcept
{
	--m_counter;
}

// --- inc/dec functions
template<bool atomic>
void dptr::detail::intrusive_ptr_add_ref(const guarded_dependency_impl<atomic>* dep) noexcept
{
	dep->inc();
}
template<bool atomic>
void dptr::detail::intrusive_ptr_release(const guarded_dependency_impl<atomic>* dep) noexcept
{
	dep->dec();
}
#pragma endregion

#endif