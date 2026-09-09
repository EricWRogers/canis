#pragma once
#include <memory>
#include <cstddef>
#include <stdexcept>
namespace Canis {
class Entity;
class ScriptableEntity;
struct ScriptLifetime { ScriptableEntity* script = nullptr; Entity* owner = nullptr; };
template<class T> class ScriptHandle {
public:
    ScriptHandle() = default;
    ScriptHandle(std::nullptr_t) {}
    ScriptHandle(T* script);
    T* TryGet() const;
    explicit operator bool() const { return TryGet() != nullptr; }
    operator T*() const { return TryGet(); }
    bool operator==(std::nullptr_t) const { return TryGet() == nullptr; }
    T* operator->() const {
        if (auto* script = TryGet()) return script;
        throw std::runtime_error("Access to a removed script");
    }
private:
    std::weak_ptr<ScriptLifetime> m_lifetime;
};
}
