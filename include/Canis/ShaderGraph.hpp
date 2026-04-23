#pragma once

#include <string>
#include <vector>

#include <Canis/Math.hpp>

namespace Canis
{
    enum class ShaderGraphValueType
    {
        UNKNOWN = 0,
        FLOAT,
        VEC2,
        VEC3,
        VEC4
    };

    enum class ShaderGraphPropertyType
    {
        FLOAT = 0,
        VEC2,
        VEC3,
        VEC4,
        COLOR,
        TEXTURE
    };

    struct ShaderGraphLink
    {
        int nodeId = -1;
        std::string slot = {};

        bool IsValid() const
        {
            return nodeId >= 0 && !slot.empty();
        }
    };

    struct ShaderGraphNode
    {
        int id = 0;
        std::string type = "Color";
        Vector2 position = Vector2(0.0f);

        float floatValue = 1.0f;
        Vector2 vec2Value = Vector2(1.0f);
        Vector3 vec3Value = Vector3(1.0f);
        Vector4 vec4Value = Vector4(1.0f);
        Color colorValue = Color(1.0f);
        Vector2 speedValue = Vector2(0.25f, 0.0f);
        std::string texturePath = {};
        int propertyId = -1;
        ShaderGraphPropertyType propertyType = ShaderGraphPropertyType::FLOAT;
        std::string title = {};
        std::string text = {};

        ShaderGraphLink inputA = {};
        ShaderGraphLink inputB = {};
        ShaderGraphLink inputT = {};
        ShaderGraphLink inputUV = {};
    };

    struct ShaderGraphProperty
    {
        int id = 0;
        std::string name = "New Property";
        ShaderGraphPropertyType type = ShaderGraphPropertyType::FLOAT;

        float floatValue = 1.0f;
        Vector2 vec2Value = Vector2(1.0f);
        Vector3 vec3Value = Vector3(1.0f);
        Vector4 vec4Value = Vector4(1.0f);
        Color colorValue = Color(1.0f);
        std::string texturePath = {};
    };

    struct ShaderGraphDocument
    {
        int nextNodeId = 1;
        int nextPropertyId = 1;
        std::vector<ShaderGraphProperty> properties = {};
        std::vector<ShaderGraphNode> nodes = {};
        ShaderGraphLink outputColor = {};
        ShaderGraphLink outputAlpha = {};
        ShaderGraphLink vertexPosition = {};
        ShaderGraphLink vertexNormal = {};
    };

    struct ShaderGraphPinInfo
    {
        const char *name = "";
        ShaderGraphValueType type = ShaderGraphValueType::UNKNOWN;
    };

    ShaderGraphDocument MakeDefaultShaderGraphDocument();
    ShaderGraphNode CreateShaderGraphNode(const std::string &_type, int _id, const Vector2 &_position);
    ShaderGraphProperty CreateShaderGraphProperty(ShaderGraphPropertyType _type, int _id, const std::string &_name);

    bool LoadShaderGraphDocument(const std::string &_path, ShaderGraphDocument &_document);
    bool SaveShaderGraphDocument(const std::string &_path, const ShaderGraphDocument &_document);
    bool GenerateShaderGraphAssets(const std::string &_graphPath, const ShaderGraphDocument &_document, std::string *_errorMessage = nullptr);

    std::string GetShaderGraphGeneratedVertexPath(const std::string &_graphPath);
    std::string GetShaderGraphGeneratedFragmentPath(const std::string &_graphPath);
    std::string GetShaderGraphGeneratedMaterialPath(const std::string &_graphPath);

    const char *GetShaderGraphNodeDisplayName(const std::string &_type);
    const char *GetShaderGraphPropertyTypeDisplayName(ShaderGraphPropertyType _type);
    const char *GetShaderGraphPropertyTypeMaterialName(ShaderGraphPropertyType _type);
    ShaderGraphValueType GetShaderGraphValueType(ShaderGraphPropertyType _type);
    std::string GetShaderGraphPropertyUniformName(const ShaderGraphProperty &_property);
    std::vector<ShaderGraphPinInfo> GetShaderGraphNodeInputPins(const ShaderGraphNode &_node);
    std::vector<ShaderGraphPinInfo> GetShaderGraphNodeOutputPins(const ShaderGraphNode &_node);
    ShaderGraphLink GetShaderGraphNodeInputLink(const ShaderGraphNode &_node, const std::string &_pinName);
    bool SetShaderGraphNodeInputLink(ShaderGraphNode &_node, const std::string &_pinName, const ShaderGraphLink &_link);
    bool ClearShaderGraphNodeInputLink(ShaderGraphNode &_node, const std::string &_pinName);

    ShaderGraphNode *FindShaderGraphNode(ShaderGraphDocument &_document, int _nodeId);
    const ShaderGraphNode *FindShaderGraphNode(const ShaderGraphDocument &_document, int _nodeId);
    ShaderGraphProperty *FindShaderGraphProperty(ShaderGraphDocument &_document, int _propertyId);
    const ShaderGraphProperty *FindShaderGraphProperty(const ShaderGraphDocument &_document, int _propertyId);
    bool RemoveShaderGraphNode(ShaderGraphDocument &_document, int _nodeId);
    bool RemoveShaderGraphProperty(ShaderGraphDocument &_document, int _propertyId);
}
