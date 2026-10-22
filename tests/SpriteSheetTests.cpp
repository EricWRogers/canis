#include <Canis/SpriteSheet.hpp>
#include <yaml-cpp/yaml.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>

namespace
{
    void Check(bool passed, const char *message)
    {
        if (!passed) throw std::runtime_error(message);
    }

    bool Near(float a, float b) { return std::abs(a - b) < 0.00001f; }

    struct TemporaryDirectory
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("canis-sprites-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TemporaryDirectory() { std::filesystem::create_directories(path); }
        ~TemporaryDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    };

    std::string Read(const std::string &path)
    {
        std::ifstream input(path, std::ios::binary);
        Check(bool(input), "Cannot read fixture file");
        return std::string(std::istreambuf_iterator<char>(input), {});
    }

    void Write(const std::string &path, const std::string &text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
        output.close();
        Check(bool(output), "Cannot write fixture file");
    }

    void CheckRegion(const Canis::SpriteRegion &region, int x, int y, int width, int height, const char *message)
    {
        Check(region.x == x && region.y == y && region.width == width && region.height == height, message);
    }

    void Normalization()
    {
        Canis::SpriteSheet sheet{12, 8, {
            {"same", -50, 900, 0, std::numeric_limits<int>::max(),
                Canis::Vector2(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity())},
            {"same", 11, 7, 5, 5, Canis::Vector2(-1, 3)},
            {"same_2", 2, 3, 4, 2, Canis::Vector2(.25f, .75f)},
            {"", 0, 0, 12, 8, Canis::Vector2(.5f)}
        }};
        Canis::NormalizeSpriteSheet(sheet);
        CheckRegion(sheet.regions[0], 0, 7, 1, 1, "Bounds were not clamped to nonempty source pixels");
        Check(Near(sheet.regions[0].pivot.x, .5f) && Near(sheet.regions[0].pivot.y, .5f), "Nonfinite pivots were not repaired");
        CheckRegion(sheet.regions[1], 11, 7, 1, 1, "Oversized bounds escaped the final source pixel");
        Check(Near(sheet.regions[1].pivot.x, 0) && Near(sheet.regions[1].pivot.y, 1), "Pivots were not clamped");
        CheckRegion(sheet.regions[2], 2, 3, 4, 2, "Valid region changed while normalizing");
        Check(Near(sheet.regions[2].pivot.x, .25f) && Near(sheet.regions[2].pivot.y, .75f), "Valid pivot changed");
        std::set<std::string> names;
        for (const auto &region : sheet.regions)
            Check(!region.name.empty() && names.insert(region.name).second, "Sprite names are empty or duplicated");
        const auto normalized = sheet.regions;
        Canis::NormalizeSpriteSheet(sheet);
        for (size_t i = 0; i < normalized.size(); ++i)
            Check(normalized[i].name == sheet.regions[i].name, "Normalization renames already unique regions");
    }

    void UVCoordinates()
    {
        const Canis::SpriteRegion region{"asymmetric", 10, 5, 20, 15, Canis::Vector2(.5f)};
        const auto uv = Canis::SpriteRegionUV(region, 100, 80);
        Check(Near(uv.x, .1f) && Near(uv.y, .75f) && Near(uv.z, .2f) && Near(uv.w, .1875f), "Asymmetric pixel rectangle has wrong UV");
        // The runtime vertex shader computes fragmentUV.y = 1 - vertexUV.y.
        // Test source rows after that flip, rather than mirroring a helper's
        // implementation: top vertices must sample the source rectangle's top.
        Check(Near(1.f - (uv.y + uv.w), 5.f / 80), "Sprite top samples the wrong source row");
        Check(Near(1.f - uv.y, 20.f / 80), "Sprite bottom samples the wrong source row");
        const auto top = Canis::SpriteRegionUV({"top", 0, 0, 256, 85}, 256, 255);
        const auto middle = Canis::SpriteRegionUV({"middle", 0, 85, 256, 85}, 256, 255);
        const auto bottom = Canis::SpriteRegionUV({"bottom", 0, 170, 256, 85}, 256, 255);
        Check(Near(top.y, 2.f / 3) && Near(middle.y, 1.f / 3) && Near(bottom.y, 0), "Stacked frames are vertically reversed");
        Check(Near(1.f - top.y - top.w, 0) && Near(1.f - bottom.y, 1), "Stacked sprite edge orientation is incorrect");
        const auto clamped = Canis::SpriteRegionUV({"outside", 200, -1, std::numeric_limits<int>::max(), 0}, 100, 80);
        Check(Near(clamped.x, .99f) && Near(clamped.y, 79.f / 80) && Near(clamped.z, .01f) && Near(clamped.w, 1.f / 80),
              "Out-of-image UV bounds were not clamped");
        const auto invalid = Canis::SpriteRegionUV(region, 0, 80);
        Check(invalid == Canis::Vector4(0, 0, 1, 1), "Invalid dimensions produced unsafe UV coordinates");
    }

    void GridSlicing()
    {
        std::string error = "old error";
        Canis::SpriteGrid grid;
        grid.cellWidth = 6; grid.cellHeight = 3;
        grid.offsetX = 2; grid.offsetY = 1; grid.paddingX = 1; grid.paddingY = 2;
        auto regions = Canis::SliceSpriteGrid(20, 10, grid, &error);
        Check(error.empty() && regions.size() == 4, "Cell slicing did not skip incomplete border strips");
        CheckRegion(regions[0], 2, 1, 6, 3, "First offset cell is wrong");
        CheckRegion(regions[1], 9, 1, 6, 3, "Horizontal padding is wrong");
        CheckRegion(regions[2], 2, 6, 6, 3, "Vertical padding or source row order is wrong");
        Check(regions[3].name == "sprite_1_1", "Grid region names are not stable row/column names");

        grid.columns = 3; grid.rows = 2;
        regions = Canis::SliceSpriteGrid(23, 15, grid, &error);
        Check(error.empty() && regions.size() == 6, "Count slicing returned wrong number of regions");
        CheckRegion(regions[0], 2, 1, 6, 6, "Count slicing computed wrong cell dimensions");
        CheckRegion(regions[5], 16, 9, 6, 6, "Count slicing lost offset or padding");

        auto reject = [&](const Canis::SpriteGrid &settings, int width = 20, int height = 10)
        {
            error.clear();
            Check(Canis::SliceSpriteGrid(width, height, settings, &error).empty() && !error.empty(), "Invalid grid was accepted without explanation");
        };
        auto bad = grid; bad.columns = 0; reject(bad);
        bad = grid; bad.offsetX = -1; reject(bad);
        bad = grid; bad.offsetY = 10; reject(bad);
        bad = grid; bad.paddingX = -1; reject(bad);
        bad = grid; bad.paddingX = std::numeric_limits<int>::max(); bad.columns = std::numeric_limits<int>::max(); reject(bad);
        bad = {}; bad.cellWidth = 0; reject(bad);
        bad = {}; bad.cellHeight = -1; reject(bad);
        bad = {}; bad.cellWidth = 500; reject(bad);
        reject({}, 0, 10);
        bad = {}; bad.cellWidth = 1; bad.cellHeight = 1;
        regions = Canis::SliceSpriteGrid(64, 64, bad, &error);
        Check(regions.size() == 4096 && error.empty(), "Exact region limit was incorrectly rejected");
        reject(bad, 65, 64);
        reject(bad, std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    }

    void AlphaTrimming()
    {
        std::vector<unsigned char> pixels(10 * 8 * 4, 0);
        auto alpha = [&](int x, int y, unsigned char value) { pixels[(y * 10 + x) * 4 + 3] = value; };
        alpha(3, 2, 255); alpha(6, 4, 255); alpha(2, 1, 20);
        alpha(9, 7, 255); // Opaque pixels outside the selection must be ignored.
        Canis::SpriteRegion region{"trimmed", 1, 1, 7, 5, Canis::Vector2(.5f, .5f)};
        Check(Canis::TrimSpriteRegion(region, pixels, 10, 8, 20), "Visible region could not be trimmed");
        CheckRegion(region, 3, 2, 4, 3, "Trim ignored threshold or included pixels outside the region");
        // Original image-space pivot is (4.5,3.5), using top-left source pixels.
        Check(Near(region.pivot.x, .375f) && Near(region.pivot.y, .5f), "Trim changed the pivot's source-image position");
        Check(Near(region.x + region.width * region.pivot.x, 4.5f) &&
              Near(region.y + region.height * (1 - region.pivot.y), 3.5f), "Trimmed pivot no longer matches the original anchor");
        region = {"inclusive", 1, 1, 7, 5, Canis::Vector2(.5f)};
        Check(Canis::TrimSpriteRegion(region, pixels, 10, 8, 19), "Threshold below faint alpha failed");
        CheckRegion(region, 2, 1, 5, 4, "Pixels above alpha threshold were excluded");
        region = {"empty", 0, 0, 2, 2, Canis::Vector2(.2f, .8f)};
        const auto before = region;
        Check(!Canis::TrimSpriteRegion(region, pixels, 10, 8), "Empty selection was trimmed to an invented pixel");
        CheckRegion(region, before.x, before.y, before.width, before.height, "Failed trim mutated region bounds");
        Check(region.pivot == before.pivot, "Failed trim mutated pivot");
        Check(!Canis::TrimSpriteRegion(region, {}, 10, 8), "Incorrect RGBA buffer length was accepted");
        Check(!Canis::TrimSpriteRegion(region, pixels, 0, 8), "Invalid image dimensions were accepted for trim");
    }

    void MetadataRoundtrip(const std::filesystem::path &directory)
    {
        const auto source = (directory / "atlas.png").string();
        const auto metadata = source + ".meta";
        const std::string originalImage = "source pixels must never be rewritten";
        Write(source, originalImage);
        Write(metadata,
            "FileType: TEXTURE\nUUID: 19446744073709551\nCustom: {author: artist, values: [1, 2, 3]}\n"
            "TextureImport: {Filter: Point, Mipmaps: false, UnknownImportSetting: 71}\n"
            "SpriteSheet:\n  Version: 1\n  UnknownSheetSetting: keep\n  Regions:\n"
            "    - Name: top\n      Bounds: [0, 0, 256, 85]\n      Pivot: [0.2, 0.8]\n      CustomTag: hero\n");
        const auto beforeSelection = Read(metadata);
        Canis::SpriteSheet sheet;
        std::string error;
        Check(Canis::LoadSpriteSheet(source, 256, 255, sheet, &error) && error.empty(), "Valid metadata could not be loaded");
        Check(sheet.imageWidth == 256 && sheet.imageHeight == 255 && sheet.regions.size() == 1, "Loaded sheet has wrong source size or count");
        CheckRegion(sheet.regions[0], 0, 0, 256, 85, "Loaded region differs from metadata");
        Check(Read(metadata) == beforeSelection, "Merely loading the sprite sheet rewrote metadata");
        sheet.regions[0].width = 128;
        sheet.regions.push_back({"bottom", 4, 170, 120, 85, Canis::Vector2(0, 1)});
        Check(Canis::SaveSpriteSheet(source, sheet, &error) && error.empty(), "Sprite metadata could not be saved");
        auto node = YAML::LoadFile(metadata);
        Check(node["UUID"].as<std::string>() == "19446744073709551", "Sprite save changed the existing UUID");
        Check(node["FileType"].as<std::string>() == "TEXTURE", "Sprite save changed the metadata type");
        Check(node["Custom"]["author"].as<std::string>() == "artist" && node["Custom"]["values"][2].as<int>() == 3, "Sprite save discarded unknown root metadata");
        Check(node["TextureImport"]["Filter"].as<std::string>() == "Point" &&
              !node["TextureImport"]["Mipmaps"].as<bool>() && node["TextureImport"]["UnknownImportSetting"].as<int>() == 71,
              "Sprite save discarded or changed texture import settings");
        Check(node["SpriteSheet"]["UnknownSheetSetting"].as<std::string>() == "keep", "Sprite save discarded unknown sheet metadata");
        Check(node["SpriteSheet"]["Regions"][0]["CustomTag"].as<std::string>() == "hero", "Sprite save discarded retained region metadata");
        Check(node["SpriteSheet"]["Regions"][0]["Bounds"].size() == 4 && node["SpriteSheet"]["Regions"][0]["Pivot"].size() == 2,
              "Sprite save appended to previous bounds or pivot arrays");
        Canis::SpriteSheet reloaded;
        Check(Canis::LoadSpriteSheet(source, 256, 255, reloaded, &error), "Saved metadata could not be reloaded");
        Check(reloaded.regions.size() == 2 && reloaded.regions[1].name == "bottom", "Roundtrip lost named sprite regions");
        CheckRegion(reloaded.regions[0], 0, 0, 128, 85, "Roundtrip lost edited pixel bounds");
        Check(reloaded.regions[1].pivot == Canis::Vector2(0, 1), "Roundtrip lost custom pivot");
        Check(Read(source) == originalImage, "Sprite authoring changed source image bytes");
        for (const auto &entry : std::filesystem::directory_iterator(directory))
            Check(entry.path().filename().string().find(".sprite-tmp-") == std::string::npos, "Successful save leaked a temporary file");

        const auto missingMetaSource = (directory / "new-atlas.png").string();
        Write(missingMetaSource, originalImage);
        Check(Canis::SaveSpriteSheet(missingMetaSource, sheet, &error), "Sprite save cannot create first metadata file");
        node = YAML::LoadFile(missingMetaSource + ".meta");
        Check(node["UUID"].as<std::uint64_t>() != 0 && node["FileType"].as<std::string>() == "TEXTURE", "New metadata lacks asset identity");
        Check(Read(missingMetaSource) == originalImage, "Creating metadata changed source bytes");
        Canis::SpriteSheet noMetadata;
        const auto noMetaSource = (directory / "unseen.png").string();
        Check(Canis::LoadSpriteSheet(noMetaSource, 10, 20, noMetadata, &error) && noMetadata.regions.empty(), "Missing metadata is not an empty valid sheet");
        Check(!std::filesystem::exists(noMetaSource + ".meta"), "Loading an unregistered texture created metadata");
    }

    void RejectMalformedMetadata(const std::filesystem::path &directory)
    {
        const auto source = (directory / "malformed.png").string();
        Write(source, "unchanged image");
        const Canis::SpriteSheet sentinel{10, 10, {{"sentinel", 2, 3, 4, 5, Canis::Vector2(.25f, .75f)}}};
        const std::string invalidMetadata[] = {
            "UUID: [broken\n",
            "- this\n- is not a metadata map\n",
            "UUID: 17\nSpriteSheet: scalar\n",
            "UUID: 17\nSpriteSheet: {Version: 2, Regions: []}\n",
            "UUID: 17\nSpriteSheet: {Version: 1, Regions: scalar}\n",
            "UUID: 17\nSpriteSheet: {Regions: [{Name: broken, Bounds: [1, 2, 3]}]}\n",
            "UUID: 17\nSpriteSheet: {Regions: [{Name: broken, Bounds: [1, 2, 3, nope]}]}\n",
            "UUID: 17\nSpriteSheet: {Regions: [{Bounds: [1, 2, 3, 4], Pivot: [1]}]}\n",
            "UUID: 17\nSpriteSheet: {Regions: [{Bounds: [1, 2, 3, 4], Pivot: [.nan, 1]}]}\n"
        };
        for (const auto &text : invalidMetadata)
        {
            Write(source + ".meta", text);
            auto loaded = sentinel;
            std::string error;
            Check(!Canis::LoadSpriteSheet(source, 10, 10, loaded, &error) && !error.empty(), "Malformed metadata loaded successfully");
            Check(loaded.regions.size() == 1 && loaded.regions[0].name == "sentinel" && loaded.regions[0].pivot == sentinel.regions[0].pivot,
                  "Failed load partially replaced the caller's sheet");
            CheckRegion(loaded.regions[0], 2, 3, 4, 5, "Failed load changed the caller's pixel bounds");
            Check(!Canis::SaveSpriteSheet(source, sentinel, &error) && !error.empty(), "Save silently replaced malformed metadata");
            Check(Read(source + ".meta") == text, "Failed save changed malformed metadata bytes");
            Check(Read(source) == "unchanged image", "Failed save changed source bytes");
        }
        const std::string valid = "UUID: 17\nSpriteSheet: {Version: 1, Regions: []}\n";
        Write(source + ".meta", valid);
        auto invalidSheet = sentinel; invalidSheet.imageWidth = 0;
        std::string error;
        Check(!Canis::SaveSpriteSheet(source, invalidSheet, &error), "Save accepted invalid source dimensions");
        Check(Read(source + ".meta") == valid, "Invalid dimensions mutated valid metadata");
        invalidSheet = sentinel; invalidSheet.regions.resize(4097);
        Check(!Canis::SaveSpriteSheet(source, invalidSheet, &error), "Save accepted too many regions");
        Check(Read(source + ".meta") == valid, "Oversized sheet mutated valid metadata");
    }
}

int main()
{
    try
    {
        TemporaryDirectory directory;
        Normalization();
        UVCoordinates();
        GridSlicing();
        AlphaTrimming();
        MetadataRoundtrip(directory.path);
        RejectMalformedMetadata(directory.path);
        std::cout << "SpriteSheetTests passed: source-oriented UVs, grid slicing, alpha trim, metadata integrity.\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "SpriteSheetTests failed: " << exception.what() << '\n';
        return 1;
    }
}
