#include <Canis/PlayerPrefs.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>

namespace P = Canis::PlayerPrefs;
static void Check(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
int main()
{
    const auto root = std::filesystem::temp_directory_path() / ("canis-prefs-" + std::to_string(SDL_GetTicksNS()));
    const auto path = root / "prefs.yaml";
    try
    {
        P::Init("Canis", "Test", path.string());
        Check(P::GetInt("missing", 42) == 42 && !P::HasKey("missing"), "Reading a default mutated preferences");
        P::SetString("text|key", "Chef é😀|value\nsecond line");
        P::SetInt("int", std::numeric_limits<int>::min());
        P::SetFloat("float", .0031234567f);
        P::SetBool("bool", true);
        P::SaveToFile();
        P::Init("Canis", "Test", path.string());
        Check(P::GetString("text|key") == "Chef é😀|value\nsecond line", "String serialization failed");
        Check(P::GetInt("int") == std::numeric_limits<int>::min(), "Integer roundtrip failed");
        Check(P::GetFloat("float") == .0031234567f && P::GetBool("bool"), "Typed roundtrip failed");
        for (auto text : {"", "12x", "99999999999999999999999"})
        {
            P::SetString("bad", text);
            Check(P::GetInt("bad", 17) == 17, "Malformed integer did not return default");
        }
        P::SetString("bad", "nan");
        Check(P::GetFloat("bad", .5f) == .5f && P::GetBool("bad", true), "Malformed scalar did not return default");
        for (auto text : {"", "1.25x", " 1.25", "1.25 ", "+1.25", "1e1000", "inf", "1,25"})
        {
            P::SetString("bad-float", text);
            Check(P::GetFloat("bad-float", 17.f) == 17.f, "Malformed float did not return default");
        }
        struct CommaDecimal : std::numpunct<char>
        {
            char do_decimal_point() const override { return ','; }
        };
        const auto originalLocale = std::locale();
        std::locale::global(std::locale(originalLocale, new CommaDecimal));
        for (float value : {0.f, -0.f, .0031234567f, std::numeric_limits<float>::max(),
                            std::numeric_limits<float>::min(), std::numeric_limits<float>::denorm_min()})
        {
            P::SetFloat("locale-float", value);
            Check(P::GetFloat("locale-float", -1.f) == value, "Locale-independent float roundtrip failed");
        }
        std::locale::global(originalLocale);
        bool rejected = false;
        try { P::SetFloat("bad", std::numeric_limits<float>::infinity()); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Nonfinite preference accepted");
        P::DeleteKey("int");
        P::SaveToFile();
        P::LoadFromFile();
        Check(!P::HasKey("int"), "Deletion did not persist");
        std::ofstream(path) << "[broken";
        rejected = false;
        try { P::LoadFromFile(); }
        catch (const std::exception&) { rejected = true; }
        Check(rejected && P::GetBool("bool"), "Failed load damaged live preferences");
        P::DeleteAll();
        P::SaveToFile();
        P::LoadFromFile();
        Check(!P::HasKey("bool"), "DeleteAll did not persist");
        P::Init("Canis", "Other", (root / "other.yaml").string());
        Check(!P::HasKey("text|key"), "Preference stores leaked into each other");
        std::filesystem::remove_all(root);
        std::cout << "PASS: PlayerPrefs persistence, defaults, malformed data, Unicode and deletion\n";
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
