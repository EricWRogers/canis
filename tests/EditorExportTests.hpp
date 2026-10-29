#pragma once
#include <Canis/Components.hpp>
#include <Canis/Signal.hpp>
struct PrivateFixture : Canis::ScriptableEntity
{
    static constexpr const char *ScriptName = "PrivateFixture";
    explicit PrivateFixture(Canis::Entity &e) : ScriptableEntity(e)
    {
    }
    float unmarkedPublic = 99;
    float Read() const
    {
        return value;
    }
    bool Looping() const
    {
        return loop;
    }

  private:
    CANIS_EDITOR_EXPORT float value = 1;
    CANIS_EDITOR_EXPORT bool loop = false;
    CANIS_EDITOR_EXPORT Canis::Signal<> changed;
    CANIS_EDITOR_EXPORT void Set()
    {
        value = 7;
    }
    CANIS_EDITOR_EXPORT void Set(float _amount)
    {
        value = _amount;
        changed.Emit();
    }
    CANIS_EDITOR_EXPORT void Configure(float _amount, bool _repeat)
    {
        value = _amount;
        loop = _repeat;
    }
};
void RegisterPrivateFixtureScript(Canis::App &app);
