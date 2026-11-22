#pragma once
/// @file nnh/detail/small_function.hpp
/// A callable stored inline in a fixed buffer: never allocates (unlike std::function). Holds function
/// pointers and lambdas whose captures fit kSmallFunctionCapacity bytes and are trivially copyable
/// (pointers, numbers, `this`) — e.g. `[this](const Event& e) { ... }`. Used for event handlers.

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

#include "nnh/assert.hpp"

namespace nnh::detail {

/// Inline storage for captures: room for `this` plus one more pointer on any platform.
inline constexpr std::size_t kSmallFunctionCapacity = 16;

template <typename Signature>
class SmallFunction;

template <typename R, typename... Args>
class SmallFunction<R(Args...)> {
public:
    SmallFunction() = default;

    // Only callables convert: overloads taking a SmallFunction or a value (a tween's `after(t, fn)` and
    // `after(t, event)`) are not ambiguous.
    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, SmallFunction> &&
                                                      std::is_invocable_v<std::decay_t<F>&, Args...>>>
    SmallFunction(F&& callable) { // NOLINT: implicit on purpose (pass lambdas directly)
        using Fn = std::decay_t<F>;
        static_assert(sizeof(Fn) <= kSmallFunctionCapacity,
                      "nnh: event handler capture too large (max 16 bytes): capture `this` or a pointer to your "
                      "data instead of copies");
        static_assert(alignof(Fn) <= alignof(std::max_align_t), "nnh: handler capture is over-aligned");
        static_assert(std::is_trivially_copyable_v<Fn> && std::is_trivially_destructible_v<Fn>,
                      "nnh: event handler captures must be trivially copyable (pointers, numbers, `this`), so the "
                      "handler never allocates: capture a pointer instead of strings or containers");
        new (storage_) Fn(std::forward<F>(callable));
        invoke_ = [](const void* storage, Args... args) -> R {
            return (*static_cast<Fn*>(const_cast<void*>(storage)))(std::forward<Args>(args)...);
        };
    }

    explicit operator bool() const { return invoke_ != nullptr; }

    R operator()(Args... args) const {
        NNH_VERIFY(invoke_ != nullptr, "call of an empty nnh::detail::SmallFunction");
        return invoke_(storage_, std::forward<Args>(args)...);
    }

private:
    alignas(std::max_align_t) unsigned char storage_[kSmallFunctionCapacity] = {};
    R (*invoke_)(const void*, Args...) = nullptr;
};

} // namespace nnh::detail
