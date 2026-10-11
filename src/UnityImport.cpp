#include <Canis/UnityImport.hpp>
#include <Canis/AssetPipeline.hpp>
#include <Canis/Yaml.hpp>
#include "../vendor/ufbx/ufbx.h"
#include <stb_image.h>
#include <stb_image_write.h>
#if CANIS_HAS_TIFF
#include <tiffio.h>
#endif
#include <fstream>
#include <sstream>
#include <map>
#include <regex>
#include <set>
#include <iostream>
namespace Canis
{
    namespace
    {
        using Pipeline::Json;
        using ID = int64_t;
        struct Doc
        {
            ID id = 0;
            int kind = 0;
            YAML::Node node;
            bool stripped = false;
        };
        struct Asset
        {
            std::filesystem::path path;
            std::string guid;
            std::vector<Doc> docs;
            std::map<ID, std::string> meshes;
        };
        static std::string Read(const std::filesystem::path& p)
        {
            std::ifstream f(p, std::ios::binary);
            if (!f)
                throw std::runtime_error("Missing Unity asset: " + p.string());
            return {std::istreambuf_iterator<char>(f), {}};
        }
        static std::vector<Doc> Documents(const std::filesystem::path& path)
        {
            std::istringstream input(Read(path));
            std::string line, body;
            std::vector<Doc> docs;
            Doc current;
            std::regex marker(R"(^--- !u!(\d+) &(-?\d+)(.*)$)");
            std::smatch match;
            auto flush = [&]
            {
                if (current.kind && body.size())
                {
                    auto parsed = YAML::Load(body);
                    current.node.reset(parsed.begin()->second);
                    docs.push_back(current);
                }
                body.clear();
            };
            while (std::getline(input, line))
            {
                if (std::regex_match(line, match, marker))
                {
                    flush();
                    current.id = std::stoll(match[2]);
                    current.kind = std::stoi(match[1]);
                    current.node.reset();
                    current.stripped = match[3].str().find("stripped") != std::string::npos;
                }
                else if (!line.starts_with('%'))
                    body += line + '\n';
            }
            flush();
            return docs;
        }
        static Vector3 V3(YAML::Node n, Vector3 fallback = Vector3(0))
        {
            if (!n)
                return fallback;
            return {n["x"].as<float>(fallback.x), n["y"].as<float>(fallback.y), n["z"].as<float>(fallback.z)};
        }
        static Vector4 RGBA(YAML::Node n, Vector4 fallback = Vector4(1))
        {
            if (!n)
                return fallback;
            return {n["r"].as<float>(fallback.x), n["g"].as<float>(fallback.y), n["b"].as<float>(fallback.z),
                    n["a"].as<float>(fallback.w)};
        }
        static YAML::Node Vec(Vector3 p)
        {
            YAML::Node n;
            n.push_back(p.x);
            n.push_back(p.y);
            n.push_back(p.z);
            return n;
        }
        static YAML::Node Vec(Vector4 p)
        {
            YAML::Node n;
            n.push_back(p.x);
            n.push_back(p.y);
            n.push_back(p.z);
            n.push_back(p.w);
            return n;
        }
        static Vector3 Mirror(Vector3 v)
        {
            v.z = -v.z;
            return v;
        }
        static std::string Slug(std::string s)
        {
            for (auto& c : s)
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
                    c = '_';
            return s;
        }
        struct Importer
        {
            std::filesystem::path source, dest;
            std::function<void(const std::string&)> progress;
            std::map<std::string, Asset> assets;
            std::map<std::string, YAML::Node> materials;
            std::map<std::string, std::string> textures;
            std::vector<std::string> warnings;
            UnityImportResult result;
            uint64_t next = 905100000000000000ULL;
            struct Expanded
            {
                std::vector<YAML::Node> nodes;
                std::map<std::pair<std::string, ID>, size_t> refs;
                std::map<ID, uint64_t> transforms;
                std::map<ID, ID> strippedParents;
                std::map<ID, std::shared_ptr<Expanded>> instances;
                std::vector<size_t> roots;
            };
            void Log(std::string s)
            {
                if (progress)
                    progress(s);
            }
            std::string Relative(std::filesystem::path p)
            {
                return p.generic_string();
            }
            void Scan()
            {
                for (auto it = std::filesystem::recursive_directory_iterator(source);
                     it != std::filesystem::recursive_directory_iterator(); ++it)
                    if (it->is_regular_file() && it->path().extension() == ".meta")
                    {
                        auto text = Read(it->path());
                        std::smatch m;
                        if (!std::regex_search(text, m, std::regex("guid: ([a-fA-F0-9]{32})")))
                            continue;
                        auto p = it->path();
                        p.replace_extension();
                        assets[m[1]] = {p, m[1], {}, {}};
                    }
            }
            Asset& Get(const std::string& guid)
            {
                auto it = assets.find(guid);
                if (it == assets.end())
                    throw std::runtime_error("Unresolved Unity asset GUID " + guid);
                return it->second;
            }
            std::string Texture(std::string guid)
            {
                if (guid.empty() || guid == std::string(32, '0') || guid.starts_with("0000000000000000"))
                    return "";
                if (textures.contains(guid))
                    return textures[guid];
                auto& a = Get(guid);
                auto path =
                    dest / "textures" / (Slug(a.path.stem().string()) + "-" + guid.substr(0, 8) + ".png");
                std::filesystem::create_directories(path.parent_path());
                std::string ext = a.path.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (ext == ".png")
                    std::filesystem::copy_file(a.path, path,
                                               std::filesystem::copy_options::overwrite_existing);
                else
                {
                    int w = 0, h = 0, c = 0;
                    std::vector<uint8_t> pixels;
#if CANIS_HAS_TIFF
                    if (ext == ".tif" || ext == ".tiff")
                    {
                        TIFF* tif = TIFFOpen(a.path.c_str(), "r");
                        if (!tif)
                            throw std::runtime_error("Cannot decode TIFF");
                        uint32_t width, height;
                        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
                        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
                        w = width;
                        h = height;
                        std::vector<uint32_t> raster(size_t(w) * h);
                        if (!TIFFReadRGBAImageOriented(tif, w, h, raster.data(), ORIENTATION_TOPLEFT, 0))
                        {
                            TIFFClose(tif);
                            throw std::runtime_error("Invalid TIFF pixels");
                        }
                        TIFFClose(tif);
                        pixels.resize(size_t(w) * h * 4);
                        for (size_t i = 0; i < raster.size(); i++)
                        {
                            pixels[i * 4] = TIFFGetR(raster[i]);
                            pixels[i * 4 + 1] = TIFFGetG(raster[i]);
                            pixels[i * 4 + 2] = TIFFGetB(raster[i]);
                            pixels[i * 4 + 3] = TIFFGetA(raster[i]);
                        }
                    }
                    else
#endif
                    {
                        auto* data = stbi_load(a.path.c_str(), &w, &h, &c, 4);
                        if (!data)
                            throw std::runtime_error("Unsupported referenced texture " + a.path.string());
                        pixels.assign(data, data + size_t(w) * h * 4);
                        stbi_image_free(data);
                    }
                    if (!stbi_write_png(path.c_str(), w, h, 4, pixels.data(), w * 4))
                        throw std::runtime_error("Texture write failed");
                }
                Pipeline::WriteMeta(path, "TEXTURE");
                return textures[guid] = Relative(path);
            }
            YAML::Node Material(std::string guid)
            {
                if (materials.contains(guid))
                    return materials[guid];
                YAML::Node out;
                out["shader"]["path"] = "assets/shaders/model3d";
                out["color"] = Vec(Vector4(1));
                out["roughnessValue"] = .6f;
                out["metallicValue"] = 0;
                out["specularValue"] = .3f;
                out["backFaceCulling"] = true;
                if (guid.empty() || guid == std::string(32, '0') || guid.starts_with("0000000000000000"))
                    return out;
                auto& a = Get(guid);
                if (a.docs.empty())
                    a.docs = Documents(a.path);
                auto document =
                    std::find_if(a.docs.begin(), a.docs.end(), [](const Doc& doc) { return doc.kind == 21; });
                if (document == a.docs.end())
                    throw std::runtime_error("Unity material document missing: " + a.path.string());
                auto n = document->node["m_SavedProperties"];
                std::map<std::string, YAML::Node> props;
                for (auto group : {"m_TexEnvs", "m_Floats", "m_Colors"})
                    if (n[group])
                        for (auto p : n[group])
                            for (auto it : p)
                                props[it.first.as<std::string>()] = it.second;
                auto color = RGBA(props.contains("_BaseColor")  ? props["_BaseColor"]
                                  : props.contains("_MainTint") ? props["_MainTint"]
                                                                : props["_Color"]);
                out["color"] = Vec(color);
                if (props.contains("_Metallic"))
                    out["metallicValue"] = props["_Metallic"].as<float>();
                float smooth = props.contains("_Smoothness")   ? props["_Smoothness"].as<float>()
                               : props.contains("_Glossiness") ? props["_Glossiness"].as<float>()
                                                               : .5f;
                out["roughnessValue"] = 1 - smooth;
                for (auto mapping :
                     std::vector<std::pair<std::string, std::string>>{{"_BaseMap", "albedo"},
                                                                      {"_MainTex", "albedo"},
                                                                      {"_BumpMap", "normal"},
                                                                      {"_NormalMap", "normal"},
                                                                      {"_EmissionMap", "emission"},
                                                                      {"_Illum", "emission"}})
                    if (props.contains(mapping.first))
                    {
                        auto ref = props[mapping.first]["m_Texture"];
                        if (ref && ref["guid"])
                        {
                            auto textureGuid = ref["guid"].as<std::string>();
                            if (!assets.contains(textureGuid))
                            {
                                auto stem = a.path.stem().string();
                                auto at = stem.find("_Norm");
                                if (at != std::string::npos)
                                    stem.resize(at);
                                std::string suffix = mapping.second == "emission" ? "Illum"
                                                     : mapping.second == "normal" ? "Norm"
                                                                                  : "Alb";
                                auto expected = stem + "_Dif_Style_02_" + suffix;
                                std::string replacement;
                                for (const auto& [id, candidate] : assets)
                                    if (candidate.path.stem() == expected &&
                                        candidate.path.extension() == ".png")
                                        replacement = id;
                                if (replacement.empty())
                                    throw std::runtime_error("Missing texture " + textureGuid + " for " +
                                                             a.path.string());
                                warnings.push_back("Recovered missing texture GUID " + textureGuid +
                                                   " using " + Get(replacement).path.filename().string());
                                textureGuid = replacement;
                            }
                            auto p = Texture(textureGuid);
                            if (mapping.first == "_Illum" && !p.empty())
                            {
                                int width, height, channels;
                                auto* rgba = stbi_load(p.c_str(), &width, &height, &channels, 4);
                                if (!rgba)
                                    throw std::runtime_error("Cannot convert illumination texture: " + p);
                                std::vector<uint8_t> rgb(size_t(width) * height * 3);
                                for (size_t pixel = 0; pixel < size_t(width) * height; pixel++)
                                    for (int channel = 0; channel < 3; channel++)
                                        rgb[pixel * 3 + channel] = uint8_t(
                                            (unsigned(rgba[pixel * 4 + channel]) * rgba[pixel * 4 + 3] +
                                             127) /
                                            255);
                                stbi_image_free(rgba);
                                auto path = dest / "textures" / (textureGuid + "-emission.png");
                                if (!stbi_write_png(path.c_str(), width, height, 3, rgb.data(), width * 3))
                                    throw std::runtime_error("Cannot write illumination texture");
                                Pipeline::WriteMeta(path, "TEXTURE");
                                p = Relative(path);
                            }
                            if (!p.empty())
                                out[mapping.second]["path"] = p;
                        }
                    }
                if (props.contains("_MetallicGlossMap"))
                    if (auto ref = props["_MetallicGlossMap"]["m_Texture"]; ref && ref["guid"])
                    {
                        auto p = Texture(ref["guid"].as<std::string>());
                        int w, h, c;
                        auto* rgba = stbi_load(p.c_str(), &w, &h, &c, 4);
                        if (!rgba)
                            throw std::runtime_error("Metallic conversion failed");
                        std::vector<uint8_t> rough(size_t(w) * h), metal(rough.size());
                        for (size_t i = 0; i < rough.size(); i++)
                        {
                            rough[i] = 255 - rgba[i * 4 + 3];
                            metal[i] = rgba[i * 4];
                        }
                        stbi_image_free(rgba);
                        for (auto [suffix, data] : std::vector<std::pair<std::string, std::vector<uint8_t>*>>{
                                 {"roughness", &rough}, {"metallic", &metal}})
                        {
                            auto target = dest / "textures" / (guid + "-" + suffix + ".png");
                            stbi_write_png(target.c_str(), w, h, 1, data->data(), w);
                            Pipeline::WriteMeta(target, "TEXTURE");
                            out[suffix]["path"] = Relative(target);
                        }
                        out["roughnessValue"] = 1;
                        out["metallicValue"] = 1;
                    }
                auto path = dest / "materials" /
                            (Slug(a.path.stem().string()) + "-" + guid.substr(0, 8) + ".material");
                std::filesystem::create_directories(path.parent_path());
                std::ofstream f(path);
                f << out;
                Pipeline::WriteMeta(path, "MATERIAL");
                out["nativePath"] = Relative(path);
                materials[guid] = out;
                return out;
            }
            Json GltfMaterial(std::string guid, Pipeline::Mesh& mesh)
            {
                auto mat = Material(guid);
                auto color = RGBA(YAML::Node());
                if (mat["color"])
                    for (int i = 0; i < 4; i++)
                        color[i] = mat["color"][i].as<float>();
                Json m = {{"name", guid},
                          {"pbrMetallicRoughness",
                           {{"baseColorFactor", {color.x, color.y, color.z, color.w}},
                            {"metallicFactor", mat["metallicValue"].as<float>(0)},
                            {"roughnessFactor", mat["roughnessValue"].as<float>(.6f)}}}};
                if (color.a < .999f)
                    m["alphaMode"] = "BLEND";
                auto texture = [&](const char* key)
                {
                    if (!mat[key] || !mat[key]["path"])
                        return -1;
                    std::string path = mat[key]["path"].as<std::string>();
                    mesh.images.push_back({{"uri", std::filesystem::absolute(path).string()}});
                    mesh.textures.push_back({{"source", mesh.images.size() - 1}});
                    return int(mesh.textures.size() - 1);
                };
                int tex = texture("albedo");
                if (tex >= 0)
                    m["pbrMetallicRoughness"]["baseColorTexture"] = {{"index", tex}};
                tex = texture("normal");
                if (tex >= 0)
                    m["normalTexture"] = {{"index", tex}};
                tex = texture("emission");
                if (tex >= 0)
                {
                    m["emissiveTexture"] = {{"index", tex}};
                    m["emissiveFactor"] = {1, 1, 1};
                }
                if (mat["nativePath"])
                    m["extras"]["canisMaterialPath"] = mat["nativePath"].as<std::string>();
                return m;
            }
            // Implemented below: FBX model prefabs, nested Unity prefab expansion and native scene output.
            void Fbx(Asset& a);
            std::shared_ptr<Expanded> Expand(Asset& a, int depth = 0);
            void Component(Doc& d, const std::string& guid, Expanded& e);
            void Modifications(YAML::Node list, Expanded& e);
            UnityImportResult Run(const std::filesystem::path& scene);
        };
        void Importer::Fbx(Asset& a)
        {
            if (!a.meshes.empty())
                return;
            ufbx_load_opts opts{};
            opts.target_unit_meters = 1;
            opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
            opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
            opts.generate_missing_normals = true;
            ufbx_error error{};
            ufbx_scene* raw = ufbx_load_file(a.path.c_str(), &opts, &error);
            if (!raw)
            {
                char text[1024];
                ufbx_format_error(text, sizeof(text), &error);
                throw std::runtime_error(text);
            }
            std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> scene(raw, ufbx_free_scene);
            auto meta = YAML::LoadFile(a.path.string() + ".meta");
            auto table = meta["ModelImporter"]["fileIDToRecycleName"];
            std::map<std::string, std::string> materialGuid;
            std::map<ufbx_mesh*, std::vector<uint32_t>> materialOrders;
            for (const auto& [guid, asset] : assets)
                if (asset.path.extension() == ".mat")
                    materialGuid[asset.path.stem().string()] = guid;
            auto str = [](ufbx_string n) { return std::string(n.data, n.length); };
            for (auto entry : table)
            {
                ID meshId = entry.first.as<ID>();
                if (meshId / 100000 != 43)
                    continue;
                std::string name = entry.second.as<std::string>();
                ufbx_mesh* mesh = nullptr;
                for (auto* candidate : scene->meshes)
                    if (str(candidate->name) == name)
                    {
                        mesh = candidate;
                        break;
                    }
                if (!mesh)
                    for (auto* node : scene->nodes)
                        if (str(node->name) == name && node->mesh)
                        {
                            mesh = node->mesh;
                            break;
                        }
                if (!mesh && scene->meshes.count == 1)
                    mesh = scene->meshes.data[0];
                if (!mesh)
                {
                    warnings.push_back("Unused recycled FBX mesh name: " + a.path.filename().string() +
                                       " / " + name);
                    continue;
                }
                Pipeline::Mesh output;
                for (auto* mat : mesh->materials)
                {
                    auto found = materialGuid.find(str(mat->name));
                    output.materials.push_back(
                        GltfMaterial(found == materialGuid.end() ? "" : found->second, output));
                }
                if (output.materials.empty())
                    output.materials.push_back(GltfMaterial("", output));
                // Unity assigns submesh/material slots in polygon encounter order,
                // which can differ from the FBX material connection order. Keep
                // that order so MeshRenderer's authored material array still fits.
                std::vector<uint32_t> slotOrder;
                for (size_t f = 0; f < mesh->faces.count; ++f)
                    if (mesh->faces[f].num_indices >= 3)
                    {
                        auto slot = mesh->face_material.count ? mesh->face_material.data[f] : 0;
                        slot = std::min(slot, uint32_t(output.materials.size()-1));
                        if (std::find(slotOrder.begin(),slotOrder.end(),slot)==slotOrder.end())
                            slotOrder.push_back(slot);
                    }
                for (uint32_t slot = 0; slot < output.materials.size(); ++slot)
                    if (std::find(slotOrder.begin(),slotOrder.end(),slot)==slotOrder.end())
                        slotOrder.push_back(slot);
                std::vector<uint32_t> unitySlot(slotOrder.size());
                auto sourceMaterials = output.materials;
                for (size_t slot = 0; slot < slotOrder.size(); ++slot)
                {
                    unitySlot[slotOrder[slot]] = uint32_t(slot);
                    output.materials[slot] = sourceMaterials[slotOrder[slot]];
                }
                materialOrders[mesh] = slotOrder;
                std::map<uint32_t, Pipeline::Primitive> parts;
                std::vector<uint32_t> tris(mesh->max_face_triangles * 3);
                for (size_t f = 0; f < mesh->faces.count; f++)
                {
                    auto face = mesh->faces.data[f];
                    if (face.num_indices < 3)
                        continue;
                    uint32_t count = ufbx_triangulate_face(tris.data(), tris.size(), mesh, face);
                    uint32_t material = mesh->face_material.count ? mesh->face_material.data[f] : 0;
                    material = unitySlot[std::min(material,uint32_t(unitySlot.size()-1))];
                    auto& p = parts[material];
                    p.material = std::min(int(material), int(output.materials.size()) - 1);
                    // Unity imports FBX mesh coordinates with X reflected. The authored
                    // prefab already contains its FBX axis rotation; keep that rotation
                    // and reflect Z separately for Canis's right-handed scene space.
                    // Two reflections preserve the source triangle winding.
                    for (uint32_t t = 0; t < count; t++)
                        for (int corner : {0, 1, 2})
                        {
                            uint32_t index = tris[t * 3 + corner];
                            auto pos = ufbx_get_vertex_vec3(&mesh->vertex_position, index),
                                 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
                            Pipeline::Vertex v;
                            v.position = Vector3(-pos.x, pos.y, -pos.z);
                            v.normal = glm::normalize(Vector3(-normal.x, normal.y, -normal.z));
                            if (mesh->vertex_uv.exists)
                            {
                                auto uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
                                v.uv = {uv.x, 1 - uv.y};
                            }
                            p.indices.push_back(uint32_t(p.vertices.size()));
                            p.vertices.push_back(v);
                        }
                }
                for (auto& [id, p] : parts)
                    output.primitives.push_back(std::move(p));
                auto path =
                    dest / "models" / (Slug(a.path.stem().string()) + "-" + std::to_string(meshId) + ".gltf");
                Pipeline::WriteGltf(output, path);
                Pipeline::WriteMeta(path, "MODEL");
                a.meshes[meshId] = Relative(path);
                result.models++;
            }
            // Unity model-prefab IDs are named explicitly in legacy FBX importer metadata.
            std::map<std::string, ID> goIds, trIds, meshIds, rendererIds, filterIds;
            for (auto item : table)
            {
                auto id = item.first.as<ID>();
                auto name = item.second.as<std::string>();
                if (id / 100000 == 1)
                    goIds[name] = id;
                else if (id / 100000 == 4)
                    trIds[name] = id;
                else if (id / 100000 == 43)
                    meshIds[name] = id;
                else if (id / 100000 == 23)
                    rendererIds[name] = id;
                else if (id / 100000 == 33)
                    filterIds[name] = id;
            }
            auto record = [&](ID id, int kind, YAML::Node n) { a.docs.push_back({id, kind, n, false}); };
            for (auto [name, go] : goIds)
            {
                ufbx_node* node = nullptr;
                for (auto* candidate : scene->nodes)
                    if (str(candidate->name) == name)
                    {
                        node = candidate;
                        break;
                    }
                if (!node && name == "//RootNode")
                    node = scene->root_node->children.count ? scene->root_node->children.data[0]
                                                            : scene->root_node;
                YAML::Node g;
                g["m_Name"] = name == "//RootNode" ? a.path.stem().string() : name;
                g["m_IsActive"] = 1;
                record(go, 1, g);
                ID tr = trIds.contains(name) ? trIds[name] : 400000 + go - 100000;
                YAML::Node transform;
                transform["m_GameObject"]["fileID"] = go;
                if (node)
                {
                    auto x = node->local_transform;
                    transform["m_LocalPosition"]["x"] = -x.translation.x;
                    transform["m_LocalPosition"]["y"] = x.translation.y;
                    transform["m_LocalPosition"]["z"] = x.translation.z;
                    transform["m_LocalScale"]["x"] = x.scale.x;
                    transform["m_LocalScale"]["y"] = x.scale.y;
                    transform["m_LocalScale"]["z"] = x.scale.z;
                    transform["m_LocalRotation"]["x"] = x.rotation.x;
                    transform["m_LocalRotation"]["y"] = -x.rotation.y;
                    transform["m_LocalRotation"]["z"] = -x.rotation.z;
                    transform["m_LocalRotation"]["w"] = x.rotation.w;
                    std::string parent = node->parent ? str(node->parent->name) : "";
                    transform["m_Father"]["fileID"] = trIds.contains(parent) ? trIds[parent]
                                                      : name != "//RootNode" && trIds.contains("//RootNode")
                                                          ? trIds["//RootNode"]
                                                          : 0;
                }
                else
                    transform["m_Father"]["fileID"] = 0;
                record(tr, 4, transform);
                ID mi = meshIds.contains(name)                      ? meshIds[name]
                        : name == "//RootNode" && !a.meshes.empty() ? a.meshes.begin()->first
                                                                    : 0;
                if (mi && a.meshes.contains(mi))
                {
                    YAML::Node filter;
                    filter["m_GameObject"]["fileID"] = go;
                    filter["m_Mesh"]["guid"] = a.guid;
                    filter["m_Mesh"]["fileID"] = mi;
                    record(filterIds.contains(name) ? filterIds[name] : 3300000 + go - 100000, 33, filter);
                    YAML::Node renderer;
                    renderer["m_GameObject"]["fileID"] = go;
                    renderer["m_Enabled"] = 1;
                    if (node && node->mesh && materialOrders.contains(node->mesh))
                        for (auto slot : materialOrders.at(node->mesh))
                        {
                            if (slot >= node->materials.count)
                                continue;
                            auto* material = node->materials[slot];
                            auto it = materialGuid.find(str(material->name));
                            YAML::Node ref;
                            if (it != materialGuid.end())
                            {
                                ref["guid"] = it->second;
                                ref["fileID"] = 2100000;
                            }
                            else
                                ref["fileID"] = 0;
                            renderer["m_Materials"].push_back(ref);
                        }
                    record(rendererIds.contains(name) ? rendererIds[name] : 2300000 + go - 100000, 23,
                           renderer);
                }
            }
            Log("Converted FBX " + a.path.filename().string() + " (" + std::to_string(a.meshes.size()) +
                " mesh assets)");
        }
        void Importer::Component(Doc& d, const std::string& guid, Expanded& e)
        {
            auto n = d.node;
            ID go = n["m_GameObject"]["fileID"].as<ID>(0);
            auto it = e.refs.find({guid, go});
            if (it == e.refs.end())
                return;
            size_t index = it->second;
            e.refs[{guid, d.id}] = index;
            auto out = e.nodes[index];
            if (d.kind == 4)
            {
                auto t = out["Canis::Transform"];
                t["position"] = Vec(Mirror(V3(n["m_LocalPosition"])));
                t["scale"] = Vec(V3(n["m_LocalScale"], Vector3(1)));
                auto q = n["m_LocalRotation"];
                t["rotation"] = Vec(Vector4(-q["x"].as<float>(0), -q["y"].as<float>(0), q["z"].as<float>(0),
                                            q["w"].as<float>(1)));
                e.transforms[d.id] = out["Entity"].as<uint64_t>();
                ID father = n["m_Father"]["fileID"].as<ID>(0);
                e.strippedParents[d.id] = father;
            }
            if (d.kind == 33)
            {
                auto ref = n["m_Mesh"];
                std::string id = ref["guid"].as<std::string>("");
                if (id.empty())
                    return;
                ID mesh = ref["fileID"].as<ID>();
                if (id.starts_with("0000000000000000"))
                {
                    Pipeline::Mesh builtin;
                    Pipeline::Primitive part;
                    std::vector<std::array<Vector3, 4>> faces;
                    if (mesh == 10210)
                        faces.push_back({Vector3(-.5f, -.5f, 0), Vector3(.5f, -.5f, 0), Vector3(.5f, .5f, 0),
                                         Vector3(-.5f, .5f, 0)});
                    else if (mesh == 10209)
                        faces.push_back(
                            {Vector3(-5, 0, -5), Vector3(-5, 0, 5), Vector3(5, 0, 5), Vector3(5, 0, -5)});
                    else if (mesh == 10202)
                    {
                        const Vector3 v[8] = {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f},
                                              {-.5f, .5f, -.5f},  {-.5f, -.5f, .5f}, {.5f, -.5f, .5f},
                                              {.5f, .5f, .5f},    {-.5f, .5f, .5f}};
                        const int q[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
                                             {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5}};
                        for (const auto& face : q)
                            faces.push_back({v[face[0]], v[face[1]], v[face[2]], v[face[3]]});
                    }
                    else
                        throw std::runtime_error("Unsupported Unity builtin mesh " + std::to_string(mesh));
                    for (auto face : faces)
                    {
                        auto normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
                        for (int k : {0, 1, 2, 0, 2, 3})
                        {
                            Pipeline::Vertex v;
                            v.position = face[k];
                            v.normal = normal;
                            v.uv = {float(k == 2 || k == 3), float(k >= 1 && k <= 2)};
                            part.indices.push_back(part.vertices.size());
                            part.vertices.push_back(v);
                        }
                    }
                    builtin.primitives.push_back(part);
                    auto path = dest / "models" / ("unity-builtin-" + std::to_string(mesh) + ".gltf");
                    Pipeline::WriteGltf(builtin, path);
                    Pipeline::WriteMeta(path, "MODEL");
                    auto model = out["Canis::Model"];
                    model["ModelAsset"]["path"] = Relative(path);
                    model["static"] = true;
                    model["applyNodeTransform"] = true;
                    model["nodeIndex"] = -1;
                    return;
                }
                auto& asset = Get(id);
                Fbx(asset);
                if (!asset.meshes.contains(mesh))
                    throw std::runtime_error("Missing FBX mesh ID " + std::to_string(mesh));
                auto model = out["Canis::Model"];
                model["ModelAsset"]["path"] = asset.meshes[mesh];
                model["static"] = true;
                model["castShadow"] = true;
                model["applyNodeTransform"] = true;
                model["nodeIndex"] = -1;
            }
            if (d.kind == 23)
            {
                if (!n["m_Enabled"].as<bool>(true))
                    out["Active"] = false;
                YAML::Node ids(YAML::NodeType::Sequence);
                for (auto ref : n["m_Materials"])
                {
                    std::string id = ref["guid"].as<std::string>("");
                    auto mat = Material(id);
                    if (mat["nativePath"])
                    {
                        YAML::Node ref;
                        ref["path"] = mat["nativePath"].as<std::string>();
                        ids.push_back(ref);
                    }
                    else
                        ids.push_back(YAML::Node());
                }
                if (ids.size())
                    out["Canis::Material"]["MaterialSlots"] = ids;
            }
            if (d.kind == 65)
            {
                out["Canis::BoxCollider"]["active"] = n["m_Enabled"].as<bool>(true);
                out["Canis::BoxCollider"]["size"] = Vec(V3(n["m_Size"], Vector3(1)));
                out["Canis::BoxCollider"]["offset"] = Vec(Mirror(V3(n["m_Center"])));
                out["Canis::Rigidbody"]["motionType"] = 0;
                out["Canis::Rigidbody"]["useGravity"] = false;
                out["Canis::Rigidbody"]["isSensor"] = n["m_IsTrigger"].as<bool>(false);
            }
            if (d.kind == 64 && out["Canis::Model"])
            {
                out["Canis::MeshCollider"]["active"] = n["m_Enabled"].as<bool>(true);
                out["Canis::MeshCollider"]["useAttachedModel"] = true;
                out["Canis::Rigidbody"]["motionType"] = 0;
                out["Canis::Rigidbody"]["useGravity"] = false;
            }
            if (d.kind == 108)
            {
                auto light = out["Canis::PointLight"];
                light["enabled"] = n["m_Enabled"].as<bool>(true);
                light["color"] = Vec(RGBA(n["m_Color"]));
                light["intensity"] = n["m_Intensity"].as<float>(1);
                light["range"] = n["m_Range"].as<float>(10);
                // Native bake metadata preserves spot cones and all static point lights.
                auto bake = out["Canis::BakeLight"];
                bake["type"] = n["m_Type"].as<int>(2);
                bake["spotAngle"] = n["m_SpotAngle"].as<float>(30);
                bake["innerAngle"] = n["m_InnerSpotAngle"].as<float>(20);
                result.lights++;
            }
            if (d.kind == 20)
            {
                auto cam = out["Canis::Camera"];
                cam["primary"] = true;
                cam["fovDegrees"] = n["field of view"].as<float>(60);
                cam["nearClip"] = n["near clip plane"].as<float>(.1f);
                cam["farClip"] = n["far clip plane"].as<float>(1000);
            }
            if (d.kind == 215)
            {
                out["UnityReflectionProbe"] = YAML::Clone(n);
            }
        }
        void Importer::Modifications(YAML::Node list, Expanded& e)
        {
            for (auto change : list)
            {
                auto target = change["target"];
                auto it = e.refs.find({target["guid"].as<std::string>(""), target["fileID"].as<ID>(0)});
                if (it == e.refs.end())
                    continue;
                auto out = e.nodes[it->second];
                std::string p = change["propertyPath"].as<std::string>(""),
                            value = change["value"].as<std::string>("");
                if (p == "m_Name")
                    out["Name"] = value;
                else if (p == "m_IsActive")
                    out["Active"] = value != "0";
                else if (p == "m_Enabled")
                {
                    if (out["Canis::PointLight"])
                        out["Canis::PointLight"]["enabled"] = value != "0";
                    else
                        out["Active"] = value != "0";
                }
                else if (p.starts_with("m_LocalPosition.") || p.starts_with("m_LocalScale.") ||
                         p.starts_with("m_LocalRotation."))
                {
                    int axis = p.back() == 'x' ? 0 : p.back() == 'y' ? 1 : p.back() == 'z' ? 2 : 3;
                    float v = std::stof(value);
                    auto tr = out["Canis::Transform"];
                    if (p.starts_with("m_LocalPosition"))
                        tr["position"][axis] = (axis == 2 ? -v : v);
                    else if (p.starts_with("m_LocalScale"))
                        tr["scale"][axis] = v;
                    else
                        tr["rotation"][axis] = (axis < 2 ? -v : v);
                }
                else if (p == "m_Intensity" && out["Canis::PointLight"])
                    out["Canis::PointLight"]["intensity"] = std::stof(value);
                else if (p == "m_Range" && out["Canis::PointLight"])
                    out["Canis::PointLight"]["range"] = std::stof(value);
                else if (p.starts_with("m_Color.") && out["Canis::PointLight"])
                {
                    int axis = p.back() == 'r' ? 0 : p.back() == 'g' ? 1 : p.back() == 'b' ? 2 : 3;
                    out["Canis::PointLight"]["color"][axis] = std::stof(value);
                }
                else if (p.starts_with("m_Materials.Array.data["))
                {
                    int slot = std::stoi(p.substr(p.find('[') + 1));
                    auto ref = change["objectReference"];
                    auto mat = Material(ref["guid"].as<std::string>(""));
                    auto ids = out["Canis::Material"]["MaterialSlots"];
                    while (ids.size() <= size_t(slot))
                        ids.push_back(YAML::Node());
                    if (mat["nativePath"])
                    {
                        YAML::Node ref;
                        ref["path"] = mat["nativePath"].as<std::string>();
                        ids[slot] = ref;
                    }
                    else
                        ids[slot] = YAML::Node();
                }
            }
        }
        std::shared_ptr<Importer::Expanded> Importer::Expand(Asset& a, int depth)
        {
            if (depth > 24)
                throw std::runtime_error("Unity prefab reference cycle");
            if (a.docs.empty())
            {
                std::string ext = a.path.extension().string();
                if (ext == ".FBX" || ext == ".fbx")
                    Fbx(a);
                else
                    a.docs = Documents(a.path);
            }
            auto e = std::make_shared<Expanded>();
            for (auto& d : a.docs)
                if (d.kind == 1 && !d.stripped)
                {
                    YAML::Node n;
                    n["Entity"] = next++;
                    n["Name"] = d.node["m_Name"].as<std::string>("Object");
                    n["Active"] = d.node["m_IsActive"].as<bool>(true);
                    auto t = n["Canis::Transform"];
                    t["active"] = true;
                    t["position"] = Vec(Vector3(0));
                    t["rotation"] = Vec(Vector4(0, 0, 0, 1));
                    t["scale"] = Vec(Vector3(1));
                    t["parent"] = uint64_t(0);
                    t["children"] = YAML::Node(YAML::NodeType::Sequence);
                    e->refs[{a.guid, d.id}] = e->nodes.size();
                    e->nodes.push_back(n);
                }
            // Expand nested instances before resolving stripped transform aliases.
            for (auto& d : a.docs)
                if (d.kind == 1001 && d.node["m_SourcePrefab"] && d.node["m_SourcePrefab"]["guid"])
                {
                    auto ref = d.node["m_SourcePrefab"];
                    auto& child = Get(ref["guid"].as<std::string>());
                    auto instance = Expand(child, depth + 1);
                    Modifications(d.node["m_Modification"]["m_Modifications"], *instance);
                    size_t base = e->nodes.size();
                    for (auto [key, index] : instance->refs)
                        e->refs.try_emplace(key, index + base);
                    for (auto node : instance->nodes)
                        e->nodes.push_back(node);
                    e->instances[d.id] = instance;
                    for (auto root : instance->roots)
                    {
                        auto node = e->nodes[base + root];
                        ID parent = d.node["m_Modification"]["m_TransformParent"]["fileID"].as<ID>(0);
                        node["UnityParentTransform"] = parent;
                    }
                    for (auto removal : d.node["m_Modification"]["m_RemovedGameObjects"])
                    {
                        auto key =
                            std::make_pair(removal["guid"].as<std::string>(""), removal["fileID"].as<ID>(0));
                        auto it = instance->refs.find(key);
                        if (it != instance->refs.end())
                            e->nodes[base + it->second]["Active"] = false;
                    }
                }
            for (auto& d : a.docs)
                if (d.stripped)
                {
                    ID instanceId = d.node["m_PrefabInstance"]["fileID"].as<ID>(0);
                    auto inst = e->instances.find(instanceId);
                    if (inst == e->instances.end())
                        continue;
                    auto ref = d.node["m_CorrespondingSourceObject"];
                    auto key = std::make_pair(ref["guid"].as<std::string>(""), ref["fileID"].as<ID>(0));
                    auto it = inst->second->refs.find(key);
                    if (it == inst->second->refs.end())
                        throw std::runtime_error("Unresolved stripped prefab transform");
                    auto uid = inst->second->nodes[it->second]["Entity"].as<uint64_t>();
                    size_t index = 0;
                    while (index < e->nodes.size() && e->nodes[index]["Entity"].as<uint64_t>() != uid)
                        ++index;
                    e->refs[{a.guid, d.id}] = index;
                    if (d.kind == 4)
                        e->transforms[d.id] = uid;
                }
            // Transform/component references share a GameObject's native identity.
            for (auto& d : a.docs)
                if (!d.stripped && d.kind != 1 && d.kind != 1001)
                    Component(d, a.guid, *e);
            for (auto& d : a.docs)
                if (d.kind == 4 && !d.stripped)
                {
                    auto it = e->refs.find({a.guid, d.id});
                    if (it == e->refs.end())
                        continue;
                    auto n = e->nodes[it->second];
                    ID father = d.node["m_Father"]["fileID"].as<ID>(0);
                    if (father && e->transforms.contains(father))
                        n["Canis::Transform"]["parent"] = e->transforms[father];
                }
            for (size_t i = 0; i < e->nodes.size(); i++)
            {
                auto n = e->nodes[i];
                if (n["UnityParentTransform"])
                {
                    ID parent = n["UnityParentTransform"].as<ID>();
                    if (parent && !e->transforms.contains(parent))
                        throw std::runtime_error("Prefab parent transform missing: " +
                                                 std::to_string(parent));
                    n["Canis::Transform"]["parent"] = parent ? e->transforms[parent] : uint64_t(0);
                    n.remove("UnityParentTransform");
                }
            }
            std::map<uint64_t, size_t> byId;
            for (size_t i = 0; i < e->nodes.size(); i++)
            {
                byId[e->nodes[i]["Entity"].as<uint64_t>()] = i;
                e->nodes[i]["Canis::Transform"]["children"] = YAML::Node(YAML::NodeType::Sequence);
            }
            for (size_t i = 0; i < e->nodes.size(); i++)
            {
                auto n = e->nodes[i];
                uint64_t parent = n["Canis::Transform"]["parent"].as<uint64_t>();
                if (parent)
                {
                    if (!byId.contains(parent))
                        throw std::runtime_error("Native parent not in prefab");
                    e->nodes[byId[parent]]["Canis::Transform"]["children"].push_back(
                        n["Entity"].as<uint64_t>());
                }
                else
                    e->roots.push_back(i);
            }
            return e;
        }
        UnityImportResult Importer::Run(const std::filesystem::path& scenePath)
        {
            Scan();
            std::filesystem::create_directories(dest);
            Asset scene{scenePath, "scene", Documents(scenePath), {}};
            // Export every kit prefab, not only the ones appearing in Example_01.
            std::vector<std::string> prefabGuids;
            for (const auto& [guid, a] : assets)
                if (a.path.extension() == ".prefab")
                    prefabGuids.push_back(guid);
            for (const auto& guid : prefabGuids)
            {
                auto& a = Get(guid);
                auto e = Expand(a);
                YAML::Node doc;
                doc["Environment"] = YAML::Node(YAML::NodeType::Map);
                for (auto n : e->nodes)
                {
                    n.remove("UnityReflectionProbe");
                    doc["Entities"].push_back(n);
                }
                auto path =
                    dest / "prefabs" / (Slug(a.path.stem().string()) + "-" + guid.substr(0, 8) + ".scene");
                std::filesystem::create_directories(path.parent_path());
                std::ofstream f(path);
                f << doc;
                f.close();
                Pipeline::WriteMeta(path, "SCENE");
                result.prefabs++;
            }
            // Import otherwise unused FBX and material assets so the kit remains reusable.
            for (auto& [guid, a] : assets)
                if (a.path.extension() == ".FBX" || a.path.extension() == ".fbx")
                    Fbx(a);
            for (auto& [guid, a] : assets)
                if (a.path.extension() == ".mat")
                    Material(guid);
            for (auto& [guid, a] : assets)
            {
                auto extension = a.path.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (extension == ".png" || extension == ".tif" || extension == ".tiff")
                    Texture(guid);
                if (extension == ".wav")
                {
                    auto path = dest / "audio" / a.path.filename();
                    std::filesystem::create_directories(path.parent_path());
                    std::filesystem::copy_file(a.path, path,
                                               std::filesystem::copy_options::overwrite_existing);
                    Pipeline::WriteMeta(path, "AUDIO");
                }
            }
            result.lights = 0;
            auto expanded = Expand(scene);
            YAML::Node out;
            auto env = out["Environment"];
            env["ClearColor"] = Vec(Vector4(.025f, .03f, .04f, 1));
            env["AmbientLight"] = Vec(Vector4(.16f, .18f, .22f, 1));
            env["AmbientLightIntensity"] = .4f;
            for (const auto& document : scene.docs)
                if (document.kind == 104 && document.node["m_SkyboxMaterial"])
                {
                    auto guid = document.node["m_SkyboxMaterial"]["guid"].as<std::string>("");
                    if (guid.empty() || !assets.contains(guid))
                        continue;
                    auto& sky = Get(guid);
                    if (sky.docs.empty())
                        sky.docs = Documents(sky.path);
                    YAML::Node cubemap;
                    const std::map<std::string, std::string> faces = {
                        {"_RightTex", "right"}, {"_LeftTex", "left"},  {"_UpTex", "top"},
                        {"_DownTex", "bottom"}, {"_FrontTex", "back"}, {"_BackTex", "front"}};
                    auto materialDocument = std::find_if(sky.docs.begin(), sky.docs.end(),
                                                         [](const Doc& doc) { return doc.kind == 21; });
                    if (materialDocument == sky.docs.end())
                        continue;
                    for (auto item : materialDocument->node["m_SavedProperties"]["m_TexEnvs"])
                        for (auto property : item)
                        {
                            auto name = property.first.as<std::string>();
                            if (!faces.contains(name))
                                continue;
                            auto ref = property.second["m_Texture"]["guid"];
                            if (ref)
                                cubemap[faces.at(name)]["path"] = Texture(ref.as<std::string>());
                        }
                    if (cubemap.size() == 6)
                    {
                        // The side faces already follow Canis's cube convention.
                        // Swapping Unity's +/-Z faces reverses both axes at each pole.
                        cubemap["top"]["rotation"] = 180;
                        cubemap["bottom"]["rotation"] = 180;
                        auto path = dest / "space.skybox";
                        std::ofstream file(path);
                        file << cubemap;
                        file.close();
                        Pipeline::WriteMeta(path, "SKYBOX");
                        env["SkyboxAsset"]["path"] = Relative(path);
                    }
                }
            env["Baking"]["Lightmaps"] = true;
            env["Baking"]["StaticShadows"] = true;
            env["Baking"]["Occlusion"] = true;
            std::map<uint64_t, YAML::Node> nodes;
            for (auto n : expanded->nodes)
                nodes[n["Entity"].as<uint64_t>()] = n;
            auto world = [&](auto&& self, uint64_t id, std::set<uint64_t>& visited) -> Matrix4
            {
                if (!visited.insert(id).second)
                    throw std::runtime_error("Imported transform cycle");
                auto t = nodes.at(id)["Canis::Transform"];
                Matrix4 m = glm::translate(Matrix4(1), t["position"].as<Vector3>()) *
                            glm::mat4_cast(t["rotation"].as<Quaternion>()) *
                            glm::scale(Matrix4(1), t["scale"].as<Vector3>());
                uint64_t parent = t["parent"].as<uint64_t>(0);
                return parent ? self(self, parent, visited) * m : m;
            };
            for (auto n : expanded->nodes)
            {
                if (n["UnityReflectionProbe"])
                {
                    std::set<uint64_t> visited;
                    auto matrix = world(world, n["Entity"].as<uint64_t>(), visited);
                    auto original = n["UnityReflectionProbe"];
                    YAML::Node p;
                    p["Enabled"] = original["m_Enabled"].as<bool>(true);
                    p["CachePath"] = (dest / "baked" /
                                      ("probe-" + std::to_string(env["LightingProbes"].size()) + ".probe"))
                                         .generic_string();
                    p["Position"] = Vec(Vector3(matrix * Vector4(Mirror(V3(original["m_BoxOffset"])), 1)));
                    p["HalfExtents"] = Vec(V3(original["m_BoxSize"], Vector3(20)) * .5f);
                    p["BlendDistance"] = original["m_BlendDistance"].as<float>(1);
                    p["DiffuseIntensity"] = .2f;
                    p["SpecularIntensity"] = original["m_IntensityMultiplier"].as<float>(1);
                    p["NearClip"] = original["m_NearClip"].as<float>(.1f);
                    p["FarClip"] = original["m_FarClip"].as<float>(100);
                    env["LightingProbes"].push_back(p);
                    n.remove("UnityReflectionProbe");
                }
                if (n["Canis::Model"] && n["Canis::Model"]["ModelAsset"])
                {
                    result.renderers++;
                    if (!n["Canis::MeshCollider"] && !n["Canis::BoxCollider"])
                    {
                        n["Canis::MeshCollider"]["active"] = true;
                        n["Canis::MeshCollider"]["useAttachedModel"] = true;
                        n["Canis::Rigidbody"]["motionType"] = 0;
                        n["Canis::Rigidbody"]["useGravity"] = false;
                    }
                }
                out["Entities"].push_back(n);
            }
            if (!env["LightingProbes"] || env["LightingProbes"].size() == 0)
            {
                YAML::Node p;
                p["Enabled"] = true;
                p["Position"] = Vec(Vector3(1.9f, 1.89f, -5.391f));
                p["HalfExtents"] = Vec(Vector3(12));
                p["DiffuseIntensity"] = .2f;
                p["SpecularIntensity"] = 1;
                env["LightingProbes"].push_back(p);
            }
            result.scene = dest / "Example_01.scene";
            std::ofstream f(result.scene);
            f << out;
            f.close();
            Pipeline::WriteMeta(result.scene, "SCENE");
            result.entities = expanded->nodes.size();
            Json report = {{"source", scenePath.string()},  {"models", result.models},
                           {"prefabs", result.prefabs},     {"entities", result.entities},
                           {"renderers", result.renderers}, {"pointLights", result.lights},
                           {"warnings", warnings},          {"textureCount", textures.size()}};
            std::ofstream(dest / "import-report.json") << report.dump(2);
            return result;
        }
    } // namespace
    UnityImportResult ImportUnityScene(const std::filesystem::path& root, const std::filesystem::path& scene,
                                       const std::filesystem::path& dest,
                                       const std::function<void(const std::string&)>& progress)
    {
        Importer importer;
        importer.source = root;
        importer.dest = dest;
        importer.progress = progress;
        return importer.Run(scene);
    }
} // namespace Canis
