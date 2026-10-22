#include <Canis/Asset.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/AssetPipeline.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/TextureImport.hpp>
#include <Canis/Window.hpp>
#include <Canis/Yaml.hpp>
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Canis;

namespace
{
    void Check(bool ok, const std::string &message)
    {
        if (!ok) throw std::runtime_error(message);
    }

    struct TemporaryDirectory
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("canis-texture-import-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TemporaryDirectory() { std::filesystem::create_directories(path); }
        ~TemporaryDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    };

    std::string Read(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    void Write(const std::filesystem::path &path, const std::string &data)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << data;
        Check(bool(file), "Could not write temporary texture fixture");
    }

    GLint Parameter(GLuint texture, GLenum name)
    {
        GLint previous = 0, value = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexParameteriv(GL_TEXTURE_2D, name, &value);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
        return value;
    }

    GLint MipWidth(GLuint texture, int level)
    {
        GLint previous = 0, width = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &width);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
        return width;
    }

    std::vector<unsigned char> Pixels(const GLTexture &texture)
    {
        GLint previous = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glBindTexture(GL_TEXTURE_2D, texture.id);
        std::vector<unsigned char> result(static_cast<size_t>(texture.width) * texture.height * 4);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
        return result;
    }

    void Save(const std::string &path, const TextureImportSettings &settings)
    {
        std::string error;
        Check(SaveTextureImportSettings(path, settings, &error), "Saving import settings failed: " + error);
        Check(error.empty(), "Successful settings save retained an error");
        Check(LoadTextureImportSettings(path) == settings, "Texture import settings did not round-trip");
    }

    void TestBleed()
    {
        DecodedImage image;
        image.width = 12;
        image.height = 3;
        image.pixels.resize(12 * 3 * 4);
        for (size_t pixel = 0; pixel < image.pixels.size() / 4; ++pixel)
        {
            image.pixels[pixel * 4] = 7;
            image.pixels[pixel * 4 + 1] = 11;
            image.pixels[pixel * 4 + 2] = 13;
        }
        const size_t red = (1 * 12 + 2) * 4;
        const size_t blue = (1 * 12 + 8) * 4;
        const std::array<unsigned char, 4> redColor{240, 40, 20, 255};
        const std::array<unsigned char, 4> blueColor{20, 80, 220, 77};
        std::copy(redColor.begin(), redColor.end(), image.pixels.begin() + red);
        std::copy(blueColor.begin(), blueColor.end(), image.pixels.begin() + blue);
        const auto original = image.pixels;
        ApplyTextureAlphaBleed(image, 1);
        for (size_t pixel = 0; pixel < image.pixels.size() / 4; ++pixel)
            Check(image.pixels[pixel * 4 + 3] == original[pixel * 4 + 3], "Alpha bleed changed source alpha");
        Check(std::equal(redColor.begin(), redColor.end(), image.pixels.begin() + red) &&
              std::equal(blueColor.begin(), blueColor.end(), image.pixels.begin() + blue),
              "Alpha bleed changed an opaque or partially transparent source pixel");
        Check(image.pixels[(1 * 12 + 3) * 4] == redColor[0] &&
              image.pixels[(1 * 12 + 7) * 4 + 2] == blueColor[2], "Edge RGB was not extended");
        Check(image.pixels[(1 * 12 + 4) * 4] == 7 && image.pixels[(1 * 12 + 11) * 4] == 7,
              "Alpha bleed exceeded its bounded radius");
        const auto oneStep = image.pixels;
        ApplyTextureAlphaBleed(image, 0);
        Check(image.pixels == oneStep, "Disabled alpha bleed modified pixels");
        image.pixels.pop_back();
        const auto malformed = image.pixels;
        ApplyTextureAlphaBleed(image);
        Check(image.pixels == malformed, "Invalid image dimensions were processed");
    }

    void TestMetadata(const std::string &path)
    {
        const std::string metadata = path + ".meta";
        Check(LoadTextureImportSettings(path) == TextureImportSettings{}, "Missing metadata changed legacy defaults");
        Write(metadata,
            "FileType: TEXTURE\nUUID: 983746182345\nname: atlas\nextension: png\nsize: 42\nmodified: 11\n"
            "SpriteSheet: {Columns: 12, Rows: 3, Future: keep}\n"
            "Custom: {Nested: [one, two]}\nTextureImport: {FutureOption: preserve}\n");
        TextureImportSettings settings;
        settings.filter = TextureFilter::Trilinear;
        settings.wrapU = TextureWrap::Clamp;
        settings.wrapV = TextureWrap::Mirror;
        settings.alphaBleed = true;
        settings.mipBias = -1.5f;
        const auto source = Read(path);
        Save(path, settings);
        auto node = YAML::LoadFile(metadata);
        Check(node["UUID"].as<uint64_t>() == 983746182345ull, "Applying import settings changed the asset UUID");
        Check(node["TextureImport"]["FutureOption"].as<std::string>() == "preserve" &&
              node["SpriteSheet"]["Future"].as<std::string>() == "keep" &&
              node["Custom"]["Nested"][1].as<std::string>() == "two", "Applying import settings discarded unknown metadata");
        Check(Read(path) == source, "Applying import settings modified the source image");

        MetaFileAsset refresh;
        Check(refresh.LoadExisting(path), "Metadata refresh failed to load valid import metadata");
        refresh.size = 73;
        refresh.modified = 23;
        refresh.Save();
        node = YAML::LoadFile(metadata);
        Check(node["size"].as<uint64_t>() == 73 && node["modified"].as<int64_t>() == 23,
              "Metadata refresh failed to update the scanner fields");
        Check(node["UUID"].as<uint64_t>() == 983746182345ull &&
              node["TextureImport"]["FutureOption"].as<std::string>() == "preserve" &&
              node["SpriteSheet"]["Columns"].as<int>() == 12 &&
              node["Custom"]["Nested"][0].as<std::string>() == "one" && LoadTextureImportSettings(path) == settings,
              "Metadata refresh discarded texture/sprite import fields");

        Pipeline::WriteMeta(path, "TEXTURE");
        node = YAML::LoadFile(metadata);
        Check(node["UUID"].as<uint64_t>() == 983746182345ull &&
              node["TextureImport"]["FutureOption"].as<std::string>() == "preserve" &&
              node["SpriteSheet"]["Columns"].as<int>() == 12 && LoadTextureImportSettings(path) == settings,
              "Native texture reimport discarded UUID or import settings");

        const auto valid = Read(metadata);
        for (const std::string malformed : {"TextureImport: [invalid yaml", "- non-map\n- metadata\n", "TextureImport: [not, a, map]\n", ""})
        {
            Write(metadata, malformed);
            std::string error;
            Check(!SaveTextureImportSettings(path, settings, &error) && !error.empty(), "Malformed metadata save did not fail with an error");
            Check(Read(metadata) == malformed, "Malformed metadata was overwritten");
            Check(LoadTextureImportSettings(path) == TextureImportSettings{}, "Malformed metadata did not fall back to legacy defaults");
        }
        Write(metadata, valid);
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(), 4.1f, -4.1f})
        {
            auto bad = settings;
            bad.mipBias = invalid;
            std::string error;
            Check(!SaveTextureImportSettings(path, bad, &error) && !error.empty() && Read(metadata) == valid,
                  "Invalid mip bias damaged existing metadata");
        }
        auto bad = settings;
        bad.filter = static_cast<TextureFilter>(999);
        Check(!SaveTextureImportSettings(path, bad) && Read(metadata) == valid, "Invalid filter was saved");

        // Saving before the scanner visits a new image must create usable UUID
        // metadata rather than a TextureImport-only sidecar.
        std::filesystem::remove(metadata);
        Save(path, settings);
        MetaFileAsset created;
        Check(created.LoadExisting(path) && created.type == MetaFileAsset::TEXTURE &&
              static_cast<uint64_t>(created.uuid) != 0 && created.extension == "png",
              "First import save did not create valid asset metadata");
        std::filesystem::remove(metadata);
    }

    void TestUpload(const std::string &path, const DecodedImage &source)
    {
        TextureAsset texture;
        Check(texture.Load(path), "Legacy texture failed to load");
        const auto original = texture.GetGLTexture();
        Check(original.id && original.width == source.width && original.height == source.height,
              "Texture dimensions were not decoded");
        Check(Parameter(original.id, GL_TEXTURE_MIN_FILTER) == GL_LINEAR_MIPMAP_NEAREST &&
              Parameter(original.id, GL_TEXTURE_MAG_FILTER) == GL_NEAREST &&
              Parameter(original.id, GL_TEXTURE_WRAP_S) == GL_REPEAT &&
              Parameter(original.id, GL_TEXTURE_WRAP_T) == GL_REPEAT && MipWidth(original.id, 1) == 4,
              "Unconfigured texture changed the legacy sampler/mipmap behavior");
        Check(Pixels(original) == source.pixels, "Synchronous image load changed pixel orientation or color");

        const std::array<TextureFilter, 4> filters{TextureFilter::Legacy, TextureFilter::Point,
                                                TextureFilter::Bilinear, TextureFilter::Trilinear};
        const std::array<TextureWrap, 3> wraps{TextureWrap::Repeat, TextureWrap::Clamp, TextureWrap::Mirror};
        const std::array<GLint, 3> wrapParameters{GL_REPEAT, GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT};
        for (size_t i = 0; i < filters.size(); ++i)
        {
            for (bool mipmaps : {false, true})
            {
                TextureImportSettings settings;
                settings.filter = filters[i];
                settings.mipmaps = mipmaps;
                settings.wrapU = wraps[i % 3];
                settings.wrapV = wraps[(i + 1) % 3];
                settings.mipBias = -0.75f;
                Save(path, settings);
                Check(texture.Reload(path) && texture.GetGLTexture().id == original.id,
                      "Reimport replaced the GL name cached by sprites");
                const GLint expectedMin = filters[i] == TextureFilter::Point ?
                    (mipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST) :
                    (mipmaps ? (filters[i] == TextureFilter::Trilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST) : GL_LINEAR);
                const GLint expectedMag = i < 2 ? GL_NEAREST : GL_LINEAR;
                Check(Parameter(original.id, GL_TEXTURE_MIN_FILTER) == expectedMin &&
                      Parameter(original.id, GL_TEXTURE_MAG_FILTER) == expectedMag,
                      "Requested filter did not reach the GL sampler");
                Check(Parameter(original.id, GL_TEXTURE_WRAP_S) == wrapParameters[i % 3] &&
                      Parameter(original.id, GL_TEXTURE_WRAP_T) == wrapParameters[(i + 1) % 3],
                      "Requested wrapping did not reach the GL sampler");
                Check((Parameter(original.id, GL_TEXTURE_MAX_LEVEL) > 0) == mipmaps,
                      "Mipmap disable left mip levels active");
                if (mipmaps) Check(MipWidth(original.id, 1) == 4 && MipWidth(original.id, 3) == 1, "Mipmap chain was not generated");
                Check(texture.GetImportSettings() == settings, "Loaded import settings were not exposed to the inspector");
            }
        }

        TextureImportSettings settings;
        settings.filter = TextureFilter::Trilinear;
        settings.wrapU = TextureWrap::Clamp;
        settings.wrapV = TextureWrap::Mirror;
        settings.mipmaps = false; // Trilinear must fall back to complete bilinear sampling.
        settings.alphaBleed = true;
        settings.mipBias = 1.25f;
        Save(path, settings);
        GLuint foreignTexture = 0, uploadBuffer = 0;
        glGenTextures(1, &foreignTexture);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, foreignTexture);
        glGenBuffers(1, &uploadBuffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, uploadBuffer);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 32, nullptr, GL_STREAM_DRAW);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 21);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 3);
        Check(texture.Reload(path), "Reload with unrelated GL unpack state failed");
        GLint active = 0, binding = 0, buffer = 0, alignment = 0, rowLength = 0, skipRows = 0, skipPixels = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
        Check(active == GL_TEXTURE3 && binding == static_cast<GLint>(foreignTexture) && buffer == static_cast<GLint>(uploadBuffer) &&
              alignment == 8 && rowLength == 21 && skipRows == 2 && skipPixels == 3,
              "Texture import leaked its GL binding or pixel unpack state");
        Check(glGetError() == GL_NO_ERROR, "Texture upload interpreted source pixels as a PBO offset");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glDeleteTextures(1, &foreignTexture);
        glDeleteBuffers(1, &uploadBuffer);

        DecodedImage expected = source;
        ApplyTextureAlphaBleed(expected);
        Check(Pixels(texture.GetGLTexture()) == expected.pixels, "GPU import did not apply the same alpha bleed as CPU import");
        Check(Parameter(original.id, GL_TEXTURE_MIN_FILTER) == GL_LINEAR && Parameter(original.id, GL_TEXTURE_MAG_FILTER) == GL_LINEAR,
              "Trilinear without mipmaps did not fall back to linear filtering");
        glBindTexture(GL_TEXTURE_2D, original.id);
        GLfloat bias = 0, maxBias = 0;
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &bias);
        glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &maxBias);
        Check(std::abs(bias - std::clamp(settings.mipBias, -maxBias, maxBias)) < 0.001f, "Mip bias did not reach the sampler");
        Check(texture.Reload(path) && Parameter(original.id, GL_TEXTURE_MAX_LEVEL) == 0,
              "Reimport failed while the asset itself was bound");
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        Check(binding == static_cast<GLint>(original.id), "Reload unbound the currently used texture");
        glBindTexture(GL_TEXTURE_2D, 0);

        // The CPU stage runs on a worker; only the decoded upload touches GL.
        auto decoded = std::async(std::launch::async, [path] { return DecodeImageRGBA(path); }).get();
        Check(decoded.pixels == source.pixels, "Worker decoding differed from synchronous decoding");
        TextureAsset asynchronous;
        Check(asynchronous.LoadDecoded(path, decoded), "Decoded worker upload failed");
        const auto asyncTexture = asynchronous.GetGLTexture();
        Check(Pixels(asyncTexture) == expected.pixels && asynchronous.GetImportSettings() == settings &&
              Parameter(asyncTexture.id, GL_TEXTURE_WRAP_S) == GL_CLAMP_TO_EDGE &&
              Parameter(asyncTexture.id, GL_TEXTURE_WRAP_T) == GL_MIRRORED_REPEAT &&
              Parameter(asyncTexture.id, GL_TEXTURE_MIN_FILTER) == GL_LINEAR && MipWidth(asyncTexture.id, 1) == 0,
              "Async texture import ignored metadata or generated unwanted mipmaps");

        const auto beforeFailure = Pixels(texture.GetGLTexture());
        const auto validSource = Read(path);
        Write(path, "This is not an image");
        Check(!texture.Reload(path), "Corrupt image reload reported success");
        Check(texture.GetGLTexture().id == original.id && texture.GetGLTexture().width == original.width &&
              texture.GetGLTexture().height == original.height && texture.GetImportSettings() == settings &&
              Pixels(texture.GetGLTexture()) == beforeFailure && glIsTexture(original.id),
              "Failed source decode damaged the previous visible texture");
        Check(!texture.Reload(path + ".missing") && texture.GetPath() == path && Pixels(texture.GetGLTexture()) == beforeFailure,
              "Missing source reload changed the asset path or texture");
        DecodedImage invalid;
        invalid.width = 8;
        invalid.height = 8;
        invalid.pixels.resize(3);
        Check(!texture.LoadDecoded(path, invalid) && texture.GetGLTexture().id == original.id &&
              Pixels(texture.GetGLTexture()) == beforeFailure, "Malformed decoded upload damaged the current texture");
        Write(path, validSource);
        Check(Read(path) == validSource, "Texture source was not restored after the failure fixture");
        texture.Free();
        asynchronous.Free();
        Check(!glIsTexture(original.id) && !glIsTexture(asyncTexture.id), "Texture Free leaked GL objects");
    }

    void TestMaterialSampling(const std::filesystem::path &directory)
    {
        const auto path = (directory / "material-checker.png").string();
        const auto legacyPath = (directory / "material-legacy.png").string();
        const std::array<unsigned char, 16> checker{0,0,0,255, 255,255,255,255,
                                                   0,0,0,255, 255,255,255,255};
        Check(stbi_write_png(path.c_str(), 2, 2, 4, checker.data(), 8) != 0 &&
              stbi_write_png(legacyPath.c_str(), 2, 2, 4, checker.data(), 8) != 0,
              "Could not write material sampling fixtures");
        const int textureId = AssetManager::LoadTexture(path);
        const int legacyId = AssetManager::LoadTexture(legacyPath);
        auto* texture = AssetManager::GetTexture(textureId);
        Check(texture && texture->GetGLTexture().id, "Material sampling fixture failed to load");

        Shader shader;
        shader.CompileSource(
            "[OPENGL VERSION]\nlayout(location=0)in vec3 position;layout(location=3)in mat4 instanceM;"
            "uniform mat4 M;uniform bool useInstanceMatrix;"
            "void main(){gl_Position=(useInstanceMatrix?instanceM:M)*vec4(position,1);}",
            "[OPENGL VERSION]\nuniform sampler2D sampleMap;uniform bool useNormalMap;"
            "uniform bool useEmbeddedEmissionMap;out vec4 result;void main(){"
            "float enabled=(useNormalMap?1.0:0.99)*(useEmbeddedEmissionMap?1.0:0.99);"
            "result=vec4(texture(sampleMap,vec2(0.49,0.25)).rgb*enabled,1);}",
            "Texture import material sampler regression");
        shader.Link();
        Check(shader.IsLinked(), "Material sampling test shader did not link");

        GLuint smooth = 0;
        glGenSamplers(1, &smooth);
        glSamplerParameteri(smooth, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(smooth, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(smooth, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glSamplerParameteri(smooth, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, 96, 32);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_FRAMEBUFFER_SRGB);

        auto quad = [](float left, float right, int albedo)
        {
            ModelAsset::PrimitiveBuild3D result;
            result.vertices={{{left,-1,0},{0,0,1},{}},{{right,-1,0},{0,0,1},{}},
                             {{right,1,0},{0,0,1},{}},{{left,1,0},{0,0,1},{}}};
            result.indices={0,1,2,0,2,3};
            result.textureId=albedo;
            result.materialSlot=0;
            return result;
        };
        auto read = [](int x)
        {
            std::array<unsigned char,4> color{};
            glReadPixels(x,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,color.data());
            return color[0];
        };
        auto draw = [&](ModelAsset &model, MaterialAsset *material, int unit, bool instanced)
        {
            for (GLuint i=0;i<8;++i) glBindSampler(i,smooth);
            // PBR maps are normally bound by MeshRenderer3DSystem before the
            // model draw; albedo, normal and imported emission bind in ModelAsset.
            glActiveTexture(GL_TEXTURE0+unit);
            glBindTexture(GL_TEXTURE_2D,texture->GetGLTexture().id);
            glActiveTexture(GL_TEXTURE0);
            shader.Use();
            shader.SetInt("sampleMap",unit);
            glClearColor(1,0,1,1);
            glClear(GL_COLOR_BUFFER_BIT);
            if (instanced)
                model.DrawInstanced(shader,{Matrix4(1)},-1,Color(1),-1,true,nullptr,material);
            else
            {
                const std::vector<MaterialAsset*> slots{material};
                model.Draw(shader,Matrix4(1),nullptr,-1,Color(1),&slots,-1,true,material);
            }
            for (GLuint i=0;i<8;++i)
            {
                GLint sampler=0;
                glGetIntegeri_v(GL_SAMPLER_BINDING,i,&sampler);
                Check(sampler==static_cast<GLint>(smooth),"Model draw leaked an imported sampler override into the next material");
            }
        };

        // Real shader readback covers all renderer-owned material map units,
        // plus imported emission. A renderer sampler must preserve old smooth
        // materials while an explicitly Point-imported map overrides it.
        for (int unit : {0,1,2,3,4,5,7})
        {
            MaterialAsset material;
            auto geometry=quad(-1,1,unit==0?textureId:-1);
            if(unit==1)material.specularId=textureId;
            if(unit==2)material.roughnessId=textureId;
            if(unit==3)material.metallicId=textureId;
            if(unit==4)material.emissionId=textureId;
            if(unit==5)material.normalId=textureId;
            if(unit==7)geometry.emissionTextureId=textureId;
            ModelAsset model;
            Check(model.SetRuntimePrimitives({geometry}),"Material sampling geometry creation failed");
            for (int mode : {0,1,2})
            {
                TextureImportSettings settings;
                settings.alphaBleed=mode==1;
                if(mode==2){settings.filter=TextureFilter::Point;settings.mipmaps=false;}
                Save(path,settings);
                Check(AssetManager::ReloadTexture(path),"Material texture reimport failed");
                for(bool instanced : {false,true})
                {
                    draw(model,&material,unit,instanced);
                    const int value=read(48);
                    if(mode==2)Check(value<10,"3D material sampler overrode authored Point filtering on unit "+std::to_string(unit));
                    else Check(value>80&&value<170,"Legacy/alpha-only material lost smooth 3D sampling on unit "+std::to_string(unit));
                }
            }
            model.Free();
        }

        TextureImportSettings point;
        point.filter=TextureFilter::Point;
        point.mipmaps=false;
        Save(path,point);
        Check(AssetManager::ReloadTexture(path),"Mixed primitive point texture reload failed");
        ModelAsset mixed;
        Check(mixed.SetRuntimePrimitives({quad(-1,-.3333f,textureId),quad(-.3333f,.3333f,textureId),quad(.3333f,1,legacyId)}),
              "Mixed primitive sampling geometry failed");
        MaterialAsset base;
        for(bool instanced : {false,true})
        {
            draw(mixed,&base,0,instanced);
            Check(read(16)<10&&read(48)<10&&read(80)>80&&read(80)<170,
                  "Per-primitive sampler switching broke cached Point maps or the following legacy map");
        }
        mixed.Free();
        shader.UnUse();
        for(GLuint i=0;i<8;++i)glBindSampler(i,0);
        glDeleteSamplers(1,&smooth);
        glActiveTexture(GL_TEXTURE0);
        AssetManager::Free<TextureAsset>(path);
        AssetManager::Free<TextureAsset>(legacyPath);
        Check(glGetError()==GL_NO_ERROR,"Material sampling integration caused an OpenGL error");
    }
}

int main(int argc, char **argv)
{
    try
    {
        if (argc > 1) std::filesystem::current_path(argv[1]);
        TestBleed();
        TemporaryDirectory directory;
        const auto path = (directory.path / "atlas.png").string();
        DecodedImage source;
        source.width = 8;
        source.height = 8;
        source.pixels.resize(8 * 8 * 4);
        for (int y = 0; y < 8; ++y)
        {
            for (int x = 0; x < 8; ++x)
            {
                const size_t pixel = static_cast<size_t>(y * 8 + x) * 4;
                source.pixels[pixel] = static_cast<unsigned char>(20 + x * 13);
                source.pixels[pixel + 1] = static_cast<unsigned char>(30 + y * 17);
                source.pixels[pixel + 2] = static_cast<unsigned char>(40 + x + y);
                source.pixels[pixel + 3] = x >= 2 && x <= 5 && y >= 2 && y <= 5 ? 255 : 0;
            }
        }
        source.pixels[(4 * 8 + 4) * 4 + 3] = 128;
        Check(stbi_write_png(path.c_str(), source.width, source.height, 4, source.pixels.data(), source.width * 4) != 0,
              "Could not write the PNG texture fixture");
        TestMetadata(path);
        Window window("Texture import tests", 320, 240, true);
        TestUpload(path, source);
        TestMaterialSampling(directory.path);
        Check(glGetError() == GL_NO_ERROR, "Texture import operations caused an OpenGL error");
        std::cout << "TEXTURE_IMPORT_PASS metadata merge, legacy/filter/mip/wrap samplers, bounded alpha bleed, "
                     "stable reload, failed decode retention, worker parity and GL state restoration\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
