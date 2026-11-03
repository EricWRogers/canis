#include <Canis/Asset.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/App.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Yaml.hpp>
#include <chrono>
#include <Canis/Window.hpp>
#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>
#include <Canis/OpenGL.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Canis;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv)
{
    try {
        if (argc > 1) std::filesystem::current_path(argv[1]);
        Window window("SDF font tests",320,240,true);
        const std::string path="assets/fonts/Antonio-Bold.ttf";
        FT_Library library; FT_Face face;
        Check(FT_Init_FreeType(&library)==0,"FreeType init failed");
        Check(FT_New_Face(library,path.c_str(),0,&face)==0,"Reference font load failed");
        TextAsset small(13),large(55);
        Check(small.Load(path)&&large.Load(path),"SDF fonts failed to load");
        Check(small.IsSignedDistance()&&large.IsSignedDistance(),"Fonts fell back to bitmap rendering");
        Check(small.GetTexture()==large.GetTexture(),"Font sizes did not share their atlas");
        for (auto* asset : {&small,&large}) {
            FT_Set_Pixel_Sizes(face,0,asset->GetFontSize());
            for (unsigned char c=32;c<127;++c) {
                Check(FT_Load_Char(face,c,FT_LOAD_RENDER)==0,"Reference glyph load failed");
                const auto& ch=asset->characters[c];
                Check(ch.advance==face->glyph->advance.x&&ch.sizeX==int(face->glyph->bitmap.width)
                    &&ch.sizeY==int(face->glyph->bitmap.rows)&&ch.bearingX==face->glyph->bitmap_left
                    &&ch.bearingY==face->glyph->bitmap_top,"SDF changed existing layout metrics");
            }
        }
        Check(small.characters['W'].imageSize.x>small.characters['W'].sizeX,"SDF padding was clipped from the glyph quad");
        Check(small.characters[' '].imageSize.x==0&&small.characters[' '].advance>0,"Space must advance without drawing a padded square");
        std::vector<unsigned char> pixels(1024*1024*4);
        glBindTexture(GL_TEXTURE_2D,small.GetTexture());glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        size_t outside=0,inside=0,distances=0;
        for(size_t i=3;i<pixels.size();i+=4) {outside+=pixels[i]<128;inside+=pixels[i]>128;distances+=pixels[i]>0&&pixels[i]<128;}
        Check(outside>1000&&inside>1000&&distances>1000,"Atlas lacks signed inside/outside distances");
        SpriteRenderer2DSystem renderer;renderer.CreateVertexArray();renderer.Begin(GlyphSortType::NONE);
        const GLTexture texture{small.GetTexture(),0,0};
        renderer.DrawUI(Vector4(0,0,10,10),Vector4(0,0,1,1),texture,0,Color(1),0,Vector2(0),Vector2(0),true);
        renderer.Draw(Vector4(20,0,10,10),Vector4(0,0,1,1),texture,0,Color(1),0,Vector2(0));
        renderer.DrawUI(Vector4(40,0,10,10),Vector4(0,0,1,1),texture,0,Color(1),0,Vector2(0),Vector2(0),true);
        renderer.End();
        Check(renderer.spriteRenderBatch.size()==3&&renderer.spriteRenderBatch[0].sdfText
            &&!renderer.spriteRenderBatch[1].sdfText&&renderer.spriteRenderBatch[2].sdfText,"Sprite and SDF batches share the wrong shader mode");
        renderer.Begin(GlyphSortType::NONE);
        renderer.Draw(Vector4(0,0,10,10),Vector4(0,0,1,1),texture,0,Color(1),0,Vector2(0));renderer.End();
        Check(!renderer.spriteRenderBatch[0].sdfText,"Reused sprite retained the SDF flag");
        const auto shared=small.GetTexture();small.Free();Check(glIsTexture(shared),"Freeing one font size deleted a shared atlas");
        large.Free();Check(!glIsTexture(shared),"Last atlas owner leaked its texture");
        FT_Done_Face(face);FT_Done_FreeType(library);
        // A failed font load must not leave a cached, apparently valid asset.
        const auto retryPath = std::filesystem::temp_directory_path() /
            ("canis-font-retry-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".ttf");
        const auto assetCount = AssetManager::GetAssetLibrary().assets.size();
        Check(AssetManager::LoadText(retryPath.string(), 32) == -1 &&
              AssetManager::GetAssetLibrary().assets.size() == assetCount,
              "Failed font was cached as a valid text asset");
        std::filesystem::copy_file(path, retryPath);
        const int retried = AssetManager::LoadText(retryPath.string(), 32);
        auto* recovered = AssetManager::GetText(retried);
        Check(recovered && recovered->GetTexture() != 0 && recovered->characters['H'].advance > 0,
              "Restoring a missing font did not recover its glyphs");
        Check(AssetManager::LoadText(retryPath.string(), 32) == retried, "Valid font did not reuse its cached asset");
        AssetManager::Free<TextAsset>(retryPath.string() + "32");
        std::filesystem::remove(retryPath);
#if CANIS_EDITOR
        App app; Editor editor; app.scene.app = &app; app.RegisterDefaults(editor);
        auto textOwner = app.scene.CreateEntity("New UI Text");
        auto* conf = app.GetScriptConf(Text::ScriptName);
        Check(conf && conf->Add, "Text component was not registered");
        conf->Add(textOwner);
        const auto& text = textOwner.GetComponent<Text>();
        auto* defaultFont = AssetManager::GetText(text.assetId);
        Check(textOwner.HasComponent<RectLayout>() && defaultFont && defaultFont->GetTexture() != 0 &&
              defaultFont->GetPath() == path, "New Text did not receive a working Antonio Bold font");
        YAML::Node encoded; conf->Encode(encoded, textOwner);
        Check(encoded["Canis::Text"]["FontAsset"]["uuid"].as<uint64_t>() == uint64_t(AssetManager::GetMetaFile(path)->uuid),
              "Default font did not save its restored asset reference");
#endif
        Check(glGetError()==GL_NO_ERROR,"Font/atlas operations caused an OpenGL error");
        std::cout<<"FONT_SDF_PASS layout metrics, distance data, padded quads, shared atlas lifetime and mixed sprite batches\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
