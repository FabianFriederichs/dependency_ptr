# dependency_ptr
*dependency_ptr.hpp* is a small utility header file which helps validating dependency pointers in hirarchies of interdependent objects.

The type `dependency_ptr<T>` is used as a drop-in replacement for non-owning raw pointers to dependencies. In debug builds it evaluates to a smart-pointer
based on boost's *intrusive_ptr*, which is used for reference counting `dependency_ptr<T>` instances referring to an instance of `T`.
In debug builds `dependency_ptr<T>` is implicitly convertible to `T*`.

The other type, `guarded_dependency<bool, dependency_op_flags>` is used as a base class for dependency types. In debug builds it adds
a reference counter (atomic if the first template parameter is `true`) to the respective derived class. Depending on the `dependency_op_flag`s
passed via the second template parameter, assertions are triggered if any operation specified by this bit mask is applied to the dependency instance while the reference count
is > 0.

In release builds, where `NDEBUG` is defined, `dependency_ptr<T>` evaluates to a plain pointer `T*` and `guarded_dependency<bool, dependency_op_flags>`
to an empty class, respectively. Hence, there are no additional runtime costs in release builds, provided that empty base optimization is applied.

The following forbidden operations on instances with reference count > 0 (enum `dependency_op`) can be specified and combined via binary-or:
| dependency_op:: | Operation                               |
| --------------- |---------------------------------------- |
| destroy         | Invocation of dependency's destructor   |
| move_from       | Moving from a dependency instance       |
| copy_from       | Copying from a dependency instance      |
| move_assign     | Move-assigning to a dependency instance |
| copy_assign     | Copy-assigning to a dependency instance |
| assign          | move_assign \| copy_assign              |

Default template arguments are:
```c++
guarded_dependency<false, dependency_op::destroy | dependency_op::move_from | dependency_op::assign>
```

## (Very) minimal example
```c++
#include <iostream>
#include <dependency_ptr.hpp>

// derive any dependency class from guarded_dependency
class dependency : public dptr::guarded_dependency<false>
{
public:
  void do_stuff() const
  {
    // ...
    std::cout << "--- dependency access ---" << std::endl;
  }
};

class dependant
{
public:
  // using dependency_ptr parametrized with a dependency type automagically counts reference in debug builds
  dependant(dptr::dependency_ptr<dependency> dep) : m_dep(dep) {}
  void use_dep() const { m_dep->do_stuff(); }
private:
  dptr::dependency_ptr<dependency> m_dep;
};

int main()
{
  dependency dep;                         // dependency instance, refcount = 0
  dependant da(&dep);                     // refcount = 1
  da.use_dep();
  // ...
  dependency other_dep = std::move(dep);  // moving from dep is illegal because refcount > 0! assertion triggered.
  return 0;
}
```

## When is it useful?
In general, if shared ownership of dependencies is not required, we can avoid the runtime and memory overhead of `std::shared_ptr`
by using raw pointers.

Obviously, we have to guarantee that the dependency outlives the dependant. `dependency_ptr` lets us validate this
in debug builds without introducing additional costs in release.

## Installing
The header requires a C++17 compatible compiler.
It may be either used as-is, or installed via the provided *CMakeLists.txt* file,
after which it can be included in other CMake projects:
```CMake
# ...
find_package(dependency_ptr)
target_link_libraries(target dependency_ptr)
```

