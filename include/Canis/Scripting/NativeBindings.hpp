#pragma once
#include <Canis/Math.hpp>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace Canis::Scripting
{
    // Stable ABI value; no C++ object or ownership crosses the managed boundary.
    enum class NativeKind : int32_t { Void, Bool, Int, Float, Double, Handle, Vector3, Quaternion, Vector4, String };
    struct NativeValue
    {
        NativeKind kind = NativeKind::Void;
        int32_t length = 0;
        uint64_t data = 0;
        double x = 0, y = 0, z = 0, w = 0;
    };
    static_assert(sizeof(NativeValue) == 48);
    template<class T> constexpr NativeKind Kind()
    {
        using V = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<V, void>) return NativeKind::Void;
        else if constexpr (std::is_same_v<V, bool>) return NativeKind::Bool;
        else if constexpr (std::is_same_v<V, int>) return NativeKind::Int;
        else if constexpr (std::is_same_v<V, float>) return NativeKind::Float;
        else if constexpr (std::is_same_v<V, double>) return NativeKind::Double;
        else if constexpr (std::is_same_v<V, uint64_t>) return NativeKind::Handle;
        else if constexpr (std::is_same_v<V, Vector3>) return NativeKind::Vector3;
        else if constexpr (std::is_same_v<V, Quaternion>) return NativeKind::Quaternion;
        else if constexpr (std::is_same_v<V, Vector4>) return NativeKind::Vector4;
        else if constexpr (std::is_same_v<V, std::string>) return NativeKind::String;
        else static_assert(!sizeof(T), "Unsupported C# binding type");
    }
    template<class T> std::remove_cvref_t<T> Decode(const NativeValue& value)
    {
        using V = std::remove_cvref_t<T>;
        if (value.kind != Kind<V>()) throw std::invalid_argument("Native argument type mismatch");
        if constexpr (std::is_same_v<V, std::string>)
        {
            if (value.length < 0 || value.length > 1024 * 1024 || (!value.data && value.length)) throw std::invalid_argument("Invalid UTF-8 buffer");
            return value.length ? std::string(reinterpret_cast<const char*>(value.data), value.length) : std::string();
        }
        else if constexpr (std::is_same_v<V, uint64_t>) return value.data;
        else if constexpr (std::is_same_v<V, Quaternion>) return Quaternion(value.w, value.x, value.y, value.z);
        else if constexpr (std::is_same_v<V, Vector3>) return Vector3(value.x, value.y, value.z);
        else if constexpr (std::is_same_v<V, Vector4>) return Vector4(value.x, value.y, value.z, value.w);
        else return static_cast<V>(value.x);
    }
    template<class T> NativeValue Encode(const T& value)
    {
        NativeValue result; result.kind = Kind<T>();
        if constexpr (std::is_same_v<T, std::string>)
        {
            // Valid until the next native call on this main thread; C# copies before returning.
            thread_local std::string text; text = value;
            result.data = reinterpret_cast<uint64_t>(text.data()); result.length = static_cast<int32_t>(text.size());
        }
        else if constexpr (std::is_same_v<T, uint64_t>) result.data = value;
        else if constexpr (std::is_same_v<T, Vector3>) { result.x = value.x; result.y = value.y; result.z = value.z; }
        else if constexpr (std::is_same_v<T, Vector4> || std::is_same_v<T, Quaternion>)
        { result.x = value.x; result.y = value.y; result.z = value.z; result.w = value.w; }
        else result.x = value;
        return result;
    }
    template<class F> struct FunctionTraits : FunctionTraits<decltype(&F::operator())> {};
    template<class R, class... A> struct FunctionTraits<R(*)(A...)> { using Function = std::function<R(A...)>; };
    template<class C, class R, class... A> struct FunctionTraits<R(C::*)(A...) const> : FunctionTraits<R(*)(A...)> {};
    template<class C, class R, class... A> struct FunctionTraits<R(C::*)(A...)> : FunctionTraits<R(*)(A...)> {};

    class NativeBindingRegistry
    {
    public:
        struct Entry
        {
            std::string owner, group, member, operation;
            NativeKind result;
            std::vector<NativeKind> arguments;
            std::function<NativeValue(std::span<const NativeValue>)> invoke;
        };
        template<class F> void Method(const std::string& owner, const std::string& group, const std::string& name, F function)
        { Register(owner, group, name, "", typename FunctionTraits<F>::Function(std::move(function))); }
        template<class T> void Variable(const std::string& owner, const std::string& group, const std::string& name, T* variable, bool readOnly = false)
        {
            if (!variable) throw std::invalid_argument("Cannot bind null variable");
            Register(owner, group, name, "get", std::function<T()>([variable] { return *variable; }));
            if (!readOnly) Register(owner, group, name, "set", std::function<void(T)>([variable](T value) { *variable = value; }));
        }
        template<class T, class G, class S> void Property(const std::string& owner, const std::string& group, const std::string& name, G get, S set)
        {
            Register(owner, group, name, "get", std::function<T()>(std::move(get)));
            Register(owner, group, name, "set", std::function<void(T)>(std::move(set)));
        }
        void RemoveOwner(const std::string& owner);
        NativeValue Invoke(const std::string& name, std::span<const NativeValue> arguments) const;
        std::string GenerateCSharp() const;
        static NativeBindingRegistry& Get();
    private:
        void Add(Entry entry);
        template<class R, class... A> void Register(const std::string& owner, const std::string& group, const std::string& name,
            const std::string& operation, std::function<R(A...)> function)
        {
            Entry entry{owner, group, name, operation, Kind<R>(), {Kind<A>()...}, {}};
            entry.invoke = [function = std::move(function)](std::span<const NativeValue> values)
            {
                if (values.size() != sizeof...(A)) throw std::invalid_argument("Native argument count mismatch");
                return [&]<size_t... I>(std::index_sequence<I...>) -> NativeValue
                {
                    if constexpr (std::is_void_v<R>) { function(Decode<A>(values[I])...); return {}; }
                    else return Encode<R>(function(Decode<A>(values[I])...));
                }(std::index_sequence_for<A...>{});
            };
            Add(std::move(entry));
        }
        std::map<std::string, Entry> entries;
    };
#ifdef _WIN32
#define CANIS_NATIVE_CALL __cdecl
#else
#define CANIS_NATIVE_CALL
#endif
    int CANIS_NATIVE_CALL DispatchNative(const unsigned char* name, int length, const NativeValue* args, int count,
        NativeValue* result, unsigned char* error, int capacity) noexcept;
}
