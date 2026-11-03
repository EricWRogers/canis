#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace Canis {
// Portable authored dialogue. Compatible with the legacy Dialog/Messages YAML layout.
struct DialogDocument {
    std::string id;
    bool allowEarlyContinue = true, waitForUserInput = true;
    float charactersPerSecond = 60, initialDelay = .1f;
    std::vector<std::string> messages;
    static DialogDocument Load(const std::string& path);
    void Validate() const;
    void Save(const std::string& path) const;
};
class DialogPlayback {
public:
    DialogDocument document;
    std::size_t index = 0;
    void Start(DialogDocument value);
    void Tick(float dt);
    // Reveal first; a subsequent press advances. Returns true after the last line.
    bool Continue();
    bool Finished() const { return index >= document.messages.size(); }
    bool Revealed() const;
    bool CanContinue() const { return !Finished() && (document.allowEarlyContinue || Revealed()); }
    std::string VisibleText() const;
private:
    float m_age = 0;
    bool m_revealed = false;
};
}
