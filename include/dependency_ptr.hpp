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

namespace dptr
{
	// --- bitmask describing forbidden oprations on the dependency while the reference counter is > 0
	enum class dependency_op : std::uint8_t
	{
		destroy = std::uint8_t{1u} << std::uint8_t{0u},
		move_from = std::uint8_t{1u} << std::uint8_t{1u},
		copy_from = std::uint8_t{1u} << std::uint8_t{2u},		
		move_assign = std::uint8_t{1u} << std::uint8_t{3u},
		copy_assign = std::uint8_t{1u} << std::uint8_t{4u},
		assign = (std::uint8_t{1u} << std::uint8_t{3u}) | (std::uint8_t{1u} << std::uint8_t{4u})
	};
	using dependency_op_flags = std::underlying_type_t<dependency_op>;
	constexpr dependency_op_flags operator~(dependency_op op) noexcept;
	constexpr dependency_op_flags operator|(dependency_op lhs, dependency_op rhs) noexcept;
	constexpr dependency_op_flags operator|(dependency_op_flags lhs, dependency_op rhs) noexcept;
	constexpr dependency_op_flags operator|(dependency_op lhs, dependency_op_flags rhs) noexcept;
	constexpr dependency_op_flags operator&(dependency_op lhs, dependency_op rhs) noexcept;
	constexpr dependency_op_flags operator&(dependency_op_flags lhs, dependency_op rhs) noexcept;
	constexpr dependency_op_flags operator&(dependency_op lhs, dependency_op_flags rhs) noexcept;
	constexpr dependency_op_flags operator^(dependency_op lhs, dependency_op rhs) noexcept;
	constexpr dependency_op_flags operator^(dependency_op_flags lhs, dependency_op rhs) noexcept;
	constexpr dependency_op_flags operator^(dependency_op lhs, dependency_op_flags rhs) noexcept;
	constexpr dependency_op_flags& operator|=(dependency_op_flags& lhs, dependency_op rhs) noexcept;
	constexpr dependency_op_flags& operator&=(dependency_op_flags& lhs, dependency_op rhs) noexcept;
	constexpr dependency_op_flags& operator^=(dependency_op_flags& lhs, dependency_op rhs) noexcept;

	namespace detail
	{
		#ifdef NDEBUG
		template <typename debug_type, typename release_type>
		using debug_type_choice_t = release_type;
		#else
		template <typename debug_type, typename release_type>
		using debug_type_choice_t = debug_type;
		#endif

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
		// --- empty dummy class for release. should enable empty base optimization
		template <bool atomic = false, dptr::dependency_op_flags forbidden_ops = dependency_op::destroy | dependency_op::move_from | dependency_op::assign>
		class guarded_dependency_nop
		{
			static constexpr bool is_dep_ref_counter_atomic = atomic;
			static constexpr dptr::dependency_op_flags dep_forbidden_op_flags = forbidden_ops;
		};

		// --- implemented variant for debug
		template <bool atomic = false, dptr::dependency_op_flags forbidden_ops = dependency_op::destroy | dependency_op::move_from | dependency_op::assign>
		class guarded_dependency_impl;
		
		template<bool atomic, dptr::dependency_op_flags forbidden_ops>
		void intrusive_ptr_add_ref(const guarded_dependency_impl<atomic, forbidden_ops>* dep) noexcept;
		template<bool atomic, dptr::dependency_op_flags forbidden_ops>
		void intrusive_ptr_release(const guarded_dependency_impl<atomic, forbidden_ops>* dep) noexcept;
		
		template<dptr::dependency_op_flags forbidden_ops>
		class guarded_dependency_impl<true, forbidden_ops>
		{
			friend void intrusive_ptr_add_ref<>(const guarded_dependency_impl*) noexcept;
			friend void intrusive_ptr_release<>(const guarded_dependency_impl*) noexcept;
		public:
			static constexpr bool is_dep_ref_counter_atomic = true;
			static constexpr dptr::dependency_op_flags dep_forbidden_op_flags = forbidden_ops;
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
		template<dptr::dependency_op_flags forbidden_ops>
		class guarded_dependency_impl<false, forbidden_ops>
		{
			friend void intrusive_ptr_add_ref<>(const guarded_dependency_impl*) noexcept;
			friend void intrusive_ptr_release<>(const guarded_dependency_impl*) noexcept;
		public:
			static constexpr bool is_dep_ref_counter_atomic = false;
			static constexpr dptr::dependency_op_flags dep_forbidden_op_flags = forbidden_ops;
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

		template <typename T, typename = void>
		struct is_guarded_dependency : std::false_type {};
		template <typename T>
		struct is_guarded_dependency<
			T,
			std::void_t<
				std::enable_if_t<std::is_same_v<std::remove_cv_t<decltype(T::is_dep_ref_counter_atomic)>, bool>>,
				std::enable_if_t<std::is_same_v<std::remove_cv_t<decltype(T::dep_forbidden_op_flags)>, dptr::dependency_op_flags>>,
				std::enable_if_t<std::is_base_of_v<guarded_dependency_impl<T::is_dep_ref_counter_atomic, T::dep_forbidden_op_flags>, std::remove_cv_t<T>>>
			>
		> : std::true_type {};		
		#pragma endregion

		#pragma region dependency_ptr_impl
		template <typename T>
		class dependency_pointer_impl : public intrusive_ptr<T>
		{
			static_assert(is_guarded_dependency<T>::value, "[dptr::detail::dependency_pointer_impl]: dependency_ptr can only be used with types deriving guarded_dependency.");
		public:
			using intrusive_ptr<T>::intrusive_ptr;
			operator typename intrusive_ptr<T>::pointer() const noexcept { return intrusive_ptr<T>::get(); }
		};
		#pragma endregion
	}
	template <typename T>
	using dependency_ptr = detail::debug_type_choice_t<detail::dependency_pointer_impl<T>, T*>;
	template <bool atomic = false, dependency_op_flags forbidden_ops = dependency_op::destroy | dependency_op::move_from | dependency_op::assign>
	using guarded_dependency = detail::debug_type_choice_t<detail::guarded_dependency_impl<atomic, forbidden_ops>, detail::guarded_dependency_nop<atomic, forbidden_ops>>;	
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
	GTS_ASSERT(m_ptr, "[dptr::detail::intrusive_ptr::operator*]: nullptr access.");
	return *m_ptr;
}
template<typename T>
inline T* dptr::detail::intrusive_ptr<T>::operator->() const noexcept
{
	GTS_ASSERT(m_ptr, "[dptr::detail::intrusive_ptr::operator->]: nullptr access.");
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

inline constexpr dptr::dependency_op_flags dptr::operator~(dependency_op op) noexcept
{
	return ~static_cast<dependency_op_flags>(op);
}

inline constexpr dptr::dependency_op_flags dptr::operator|(dependency_op lhs, dependency_op rhs) noexcept
{
	return static_cast<dependency_op_flags>(lhs) | static_cast<dependency_op_flags>(rhs);
}

constexpr dptr::dependency_op_flags dptr::operator|(dependency_op_flags lhs, dependency_op rhs) noexcept
{
	return lhs | static_cast<dependency_op_flags>(rhs);
}

constexpr dptr::dependency_op_flags dptr::operator|(dependency_op lhs, dependency_op_flags rhs) noexcept
{
	return static_cast<dependency_op_flags>(lhs) | rhs;
}

inline constexpr dptr::dependency_op_flags dptr::operator&(dependency_op lhs, dependency_op rhs) noexcept
{
	return static_cast<dependency_op_flags>(lhs) & static_cast<dependency_op_flags>(rhs);
}

constexpr dptr::dependency_op_flags dptr::operator&(dependency_op_flags lhs, dependency_op rhs) noexcept
{
	return lhs & static_cast<dependency_op_flags>(rhs);
}

constexpr dptr::dependency_op_flags dptr::operator&(dependency_op lhs, dependency_op_flags rhs) noexcept
{
	return static_cast<dependency_op_flags>(lhs) & rhs;
}

inline constexpr dptr::dependency_op_flags dptr::operator^(dependency_op lhs, dependency_op rhs) noexcept
{
	return static_cast<dependency_op_flags>(lhs) ^ static_cast<dependency_op_flags>(rhs);
}

constexpr dptr::dependency_op_flags dptr::operator^(dependency_op_flags lhs, dependency_op rhs) noexcept
{
	return lhs ^ static_cast<dependency_op_flags>(rhs);
}

constexpr dptr::dependency_op_flags dptr::operator^(dependency_op lhs, dependency_op_flags rhs) noexcept
{
	return static_cast<dependency_op_flags>(lhs) ^ rhs;
}

inline constexpr dptr::dependency_op_flags& dptr::operator|=(dependency_op_flags& lhs, dependency_op rhs) noexcept
{
	return lhs |= static_cast<dependency_op_flags>(rhs);
}

inline constexpr dptr::dependency_op_flags& dptr::operator&=(dependency_op_flags& lhs, dependency_op rhs) noexcept
{
	return lhs &= static_cast<dependency_op_flags>(rhs);
}

inline constexpr dptr::dependency_op_flags& dptr::operator^=(dependency_op_flags& lhs, dependency_op rhs) noexcept
{
	return lhs ^= static_cast<dependency_op_flags>(rhs);
}

// --- atomic version
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<true, forbidden_ops>::guarded_dependency_impl() noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<true, forbidden_ops>::guarded_dependency_impl(const guarded_dependency_impl& other) noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<true, forbidden_ops>::guarded_dependency_impl(guarded_dependency_impl&& other) noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
	// if there are still dependees, moving from the object gives undefined behaviour
	if constexpr(forbidden_ops & dependency_op::move_from)
		DPTR_ASSERT(other.m_counter.load(std::memory_order_relaxed) == 0ull, "[dptr::detail::guarded_dependency_impl::guarded_dependency_impl(move ctor)]: There were still (now invalid!) pointers referencing the moved-from object.");
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<true, forbidden_ops>& dptr::detail::guarded_dependency_impl<true, forbidden_ops>::operator=(const guarded_dependency_impl& other) noexcept
{
	// object stays at the same address => do not modify counter
	if constexpr(forbidden_ops & dependency_op::copy_assign)
		DPTR_ASSERT(m_counter.load(std::memory_order_relaxed) == 0ull, "[dptr::detail::guarded_dependency_impl::operator=(copy)]: There were still (now possibly invalid!) pointers referencing the assigned object.");
	return *this;
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<true, forbidden_ops>& dptr::detail::guarded_dependency_impl<true, forbidden_ops>::operator=(guarded_dependency_impl&& other) noexcept
{
	// object stays at the same address => do not modify counter
	// if there are still dependees, moving from the object gives undefined behaviour
	if constexpr(forbidden_ops & dependency_op::move_from)
		DPTR_ASSERT(other.m_counter.load(std::memory_order_relaxed) == 0ull, "[dptr::detail::guarded_dependency_impl::operator=(move)]: There were still (now invalid!) pointers referencing the moved-from object.");
	if constexpr(forbidden_ops & dependency_op::move_assign)
		DPTR_ASSERT(m_counter.load(std::memory_order_relaxed) == 0ull, "[dptr::detail::guarded_dependency_impl::operator=(move)]: There were still (now possibly invalid!) pointers referencing the assigned object.");
	return *this;
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<true, forbidden_ops>::~guarded_dependency_impl()
{
	if constexpr(forbidden_ops & dependency_op::destroy)
		DPTR_ASSERT(m_counter.load(std::memory_order_relaxed) == 0ull, "[dptr::detail::guarded_dependency_impl::~guarded_dependency_impl]: There were still (now dangling!) pointers referencing this object.");
}
template <dptr::dependency_op_flags forbidden_ops>
inline void dptr::detail::guarded_dependency_impl<true, forbidden_ops>::inc() const noexcept
{
	m_counter.fetch_add(1ull, std::memory_order::memory_order_relaxed);
}
template <dptr::dependency_op_flags forbidden_ops>
inline void dptr::detail::guarded_dependency_impl<true, forbidden_ops>::dec() const noexcept
{
	m_counter.fetch_sub(1ull, std::memory_order::memory_order_relaxed);
}

// --- non atomic version
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<false, forbidden_ops>::guarded_dependency_impl() noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<false, forbidden_ops>::guarded_dependency_impl(const guarded_dependency_impl& other) noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<false, forbidden_ops>::guarded_dependency_impl(guarded_dependency_impl&& other) noexcept :
	m_counter(0ull)
{
	// new object at new address, init counter to 0
	// if there are still dependees, moving from the object gives undefined behaviour
	if constexpr(forbidden_ops & dependency_op::move_from)
		DPTR_ASSERT(other.m_counter == 0ull, "[dptr::detail::guarded_dependency_impl::guarded_dependency_impl(move ctor)]: There were still (now invalid!) pointers referencing the moved-from object.");
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<false, forbidden_ops>& dptr::detail::guarded_dependency_impl<false, forbidden_ops>::operator=(const guarded_dependency_impl& other) noexcept
{
	// object stays at the same address => do not modify counter
	if constexpr(forbidden_ops & dependency_op::copy_assign)
		DPTR_ASSERT(m_counter == 0ull, "[dptr::detail::guarded_dependency_impl::operator=(copy)]: There were still (now possibly invalid!) pointers referencing the assigned object.");
	return *this;
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<false, forbidden_ops>& dptr::detail::guarded_dependency_impl<false, forbidden_ops>::operator=(guarded_dependency_impl&& other) noexcept
{
	// object stays at the same address => do not modify counter
	// if there are still dependees, moving from the object gives undefined behaviour
	if constexpr(forbidden_ops & dependency_op::move_from)
		DPTR_ASSERT(other.m_counter == 0ull, "[dptr::detail::guarded_dependency_impl::operator=(move)]: There were still (now invalid!) pointers referencing the moved-from object.");
	if constexpr(forbidden_ops & dependency_op::move_assign)
		DPTR_ASSERT(m_counter == 0ull, "[dptr::detail::guarded_dependency_impl::operator=(move)]: There were still (now possibly invalid!) pointers referencing the assigned object.");
	return *this;
}
template <dptr::dependency_op_flags forbidden_ops>
inline dptr::detail::guarded_dependency_impl<false, forbidden_ops>::~guarded_dependency_impl()
{
	if constexpr(forbidden_ops & dependency_op::destroy)
		DPTR_ASSERT(m_counter == 0ull, "[dptr::detail::guarded_dependency_impl::~guarded_dependency_impl]: There were still (now dangling!) pointers referencing this object.");
}
template <dptr::dependency_op_flags forbidden_ops>
inline void dptr::detail::guarded_dependency_impl<false, forbidden_ops>::inc() const noexcept
{
	++m_counter;
}
template <dptr::dependency_op_flags forbidden_ops>
inline void dptr::detail::guarded_dependency_impl<false, forbidden_ops>::dec() const noexcept
{
	--m_counter;
}

// --- inc/dec functions
template<bool atomic, dptr::dependency_op_flags forbidden_ops>
void dptr::detail::intrusive_ptr_add_ref(const guarded_dependency_impl<atomic, forbidden_ops>* dep) noexcept
{
	dep->inc();
}
template<bool atomic, dptr::dependency_op_flags forbidden_ops>
void dptr::detail::intrusive_ptr_release(const guarded_dependency_impl<atomic, forbidden_ops>* dep) noexcept
{
	dep->dec();
}
#pragma endregion
#endif