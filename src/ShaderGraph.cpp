#include <Canis/ShaderGraph.hpp>

#include <Canis/Yaml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Canis
{
    namespace
    {
        struct ExpressionResult
        {
            ShaderGraphValueType type = ShaderGraphValueType::UNKNOWN;
            std::string expression = {};
        };

        struct TextureUniformInfo
        {
            int nodeId = 0;
            std::string uniformName = {};
            std::string texturePath = {};
        };

        struct PropertyUniformInfo
        {
            int propertyId = 0;
            std::string uniformName = {};
            ShaderGraphProperty property = {};
        };

        enum class ShaderGraphBuildStage
        {
            Vertex = 0,
            Fragment
        };

        struct BuildContext
        {
            const ShaderGraphDocument *document = nullptr;
            std::unordered_map<std::string, ExpressionResult> cache = {};
            std::unordered_set<std::string> visiting = {};
            std::vector<TextureUniformInfo> textureUniforms = {};
            std::vector<PropertyUniformInfo> propertyUniforms = {};
            ShaderGraphBuildStage stage = ShaderGraphBuildStage::Fragment;
        };

        std::string FormatFloatLiteral(float _value)
        {
            std::ostringstream stream;
            stream.setf(std::ios::fixed, std::ios::floatfield);
            stream << std::setprecision(4) << _value;
            return stream.str();
        }

        std::string FormatVec2Literal(const Vector2 &_value)
        {
            return "vec2(" + FormatFloatLiteral(_value.x) + ", " + FormatFloatLiteral(_value.y) + ")";
        }

        std::string FormatVec3Literal(const Vector3 &_value)
        {
            return "vec3(" + FormatFloatLiteral(_value.x) + ", " + FormatFloatLiteral(_value.y) + ", " + FormatFloatLiteral(_value.z) + ")";
        }

        std::string FormatVec4Literal(const Vector4 &_value)
        {
            return "vec4(" + FormatFloatLiteral(_value.x) + ", " + FormatFloatLiteral(_value.y) + ", " +
                FormatFloatLiteral(_value.z) + ", " + FormatFloatLiteral(_value.w) + ")";
        }

        const char *PropertyTypeToString(ShaderGraphPropertyType _type)
        {
            switch (_type)
            {
                case ShaderGraphPropertyType::FLOAT: return "float";
                case ShaderGraphPropertyType::VEC2: return "vector2";
                case ShaderGraphPropertyType::VEC3: return "vector3";
                case ShaderGraphPropertyType::VEC4: return "vector4";
                case ShaderGraphPropertyType::COLOR: return "color";
                case ShaderGraphPropertyType::TEXTURE: return "texture";
                default: return "float";
            }
        }

        ShaderGraphPropertyType StringToPropertyType(const std::string &_type)
        {
            if (_type == "vector2" || _type == "vec2") return ShaderGraphPropertyType::VEC2;
            if (_type == "vector3" || _type == "vec3") return ShaderGraphPropertyType::VEC3;
            if (_type == "vector4" || _type == "vec4") return ShaderGraphPropertyType::VEC4;
            if (_type == "color") return ShaderGraphPropertyType::COLOR;
            if (_type == "texture" || _type == "sampler2d") return ShaderGraphPropertyType::TEXTURE;
            return ShaderGraphPropertyType::FLOAT;
        }

        std::string SanitizeUniformIdentifier(const std::string &_name)
        {
            std::string result;
            result.reserve(_name.size());

            for (char c : _name)
            {
                const unsigned char ch = static_cast<unsigned char>(c);
                if (std::isalnum(ch) || c == '_')
                    result.push_back(c);
                else if (!result.empty() && result.back() != '_')
                    result.push_back('_');
            }

            while (!result.empty() && result.back() == '_')
                result.pop_back();

            if (result.empty())
                result = "Property";

            if (std::isdigit(static_cast<unsigned char>(result.front())))
                result = "P_" + result;

            return result;
        }

        int ValueTypeRank(ShaderGraphValueType _type)
        {
            switch (_type)
            {
                case ShaderGraphValueType::FLOAT: return 1;
                case ShaderGraphValueType::VEC2: return 2;
                case ShaderGraphValueType::VEC3: return 3;
                case ShaderGraphValueType::VEC4: return 4;
                default: return 0;
            }
        }

        ShaderGraphValueType MaxValueType(ShaderGraphValueType _a, ShaderGraphValueType _b)
        {
            return (ValueTypeRank(_a) >= ValueTypeRank(_b)) ? _a : _b;
        }

        ExpressionResult MakeZeroValue(ShaderGraphValueType _type)
        {
            switch (_type)
            {
                case ShaderGraphValueType::FLOAT: return { ShaderGraphValueType::FLOAT, "0.0" };
                case ShaderGraphValueType::VEC2: return { ShaderGraphValueType::VEC2, "vec2(0.0)" };
                case ShaderGraphValueType::VEC3: return { ShaderGraphValueType::VEC3, "vec3(0.0)" };
                case ShaderGraphValueType::VEC4: return { ShaderGraphValueType::VEC4, "vec4(0.0)" };
                default: return { ShaderGraphValueType::FLOAT, "0.0" };
            }
        }

        ExpressionResult MakeOneValue(ShaderGraphValueType _type)
        {
            switch (_type)
            {
                case ShaderGraphValueType::FLOAT: return { ShaderGraphValueType::FLOAT, "1.0" };
                case ShaderGraphValueType::VEC2: return { ShaderGraphValueType::VEC2, "vec2(1.0)" };
                case ShaderGraphValueType::VEC3: return { ShaderGraphValueType::VEC3, "vec3(1.0)" };
                case ShaderGraphValueType::VEC4: return { ShaderGraphValueType::VEC4, "vec4(1.0)" };
                default: return { ShaderGraphValueType::FLOAT, "1.0" };
            }
        }

        std::string ConvertExpression(const ExpressionResult &_source, ShaderGraphValueType _targetType, bool _preferAlphaScalar = false)
        {
            if (_targetType == ShaderGraphValueType::UNKNOWN || _source.type == _targetType)
                return _source.expression;

            if (_source.type == ShaderGraphValueType::UNKNOWN)
                return MakeZeroValue(_targetType).expression;

            switch (_targetType)
            {
                case ShaderGraphValueType::FLOAT:
                {
                    switch (_source.type)
                    {
                        case ShaderGraphValueType::VEC2: return "(" + _source.expression + ").x";
                        case ShaderGraphValueType::VEC3: return "(" + _source.expression + ").x";
                        case ShaderGraphValueType::VEC4: return "(" + _source.expression + ")." + std::string(_preferAlphaScalar ? "a" : "x");
                        default: return _source.expression;
                    }
                }
                case ShaderGraphValueType::VEC2:
                {
                    switch (_source.type)
                    {
                        case ShaderGraphValueType::FLOAT: return "vec2(" + _source.expression + ")";
                        case ShaderGraphValueType::VEC3: return "(" + _source.expression + ").xy";
                        case ShaderGraphValueType::VEC4: return "(" + _source.expression + ").xy";
                        default: return _source.expression;
                    }
                }
                case ShaderGraphValueType::VEC3:
                {
                    switch (_source.type)
                    {
                        case ShaderGraphValueType::FLOAT: return "vec3(" + _source.expression + ")";
                        case ShaderGraphValueType::VEC2: return "vec3(" + _source.expression + ", 0.0)";
                        case ShaderGraphValueType::VEC4: return "(" + _source.expression + ").xyz";
                        default: return _source.expression;
                    }
                }
                case ShaderGraphValueType::VEC4:
                {
                    switch (_source.type)
                    {
                        case ShaderGraphValueType::FLOAT: return "vec4(" + _source.expression + ")";
                        case ShaderGraphValueType::VEC2: return "vec4(" + _source.expression + ", 0.0, 1.0)";
                        case ShaderGraphValueType::VEC3: return "vec4(" + _source.expression + ", 1.0)";
                        default: return _source.expression;
                    }
                }
                default:
                    return _source.expression;
            }
        }

        ShaderGraphLink DecodeLink(const YAML::Node &_node)
        {
            ShaderGraphLink link = {};
            if (!_node || !_node.IsMap())
                return link;

            link.nodeId = _node["node"].as<int>(-1);
            link.slot = _node["slot"].as<std::string>("");
            return link;
        }

        YAML::Node EncodeLink(const ShaderGraphLink &_link)
        {
            YAML::Node node(YAML::NodeType::Map);
            if (!_link.IsValid())
                return node;

            node["node"] = _link.nodeId;
            node["slot"] = _link.slot;
            return node;
        }

        std::string GetGeneratedBasePath(const std::string &_graphPath)
        {
            namespace fs = std::filesystem;

            fs::path path(_graphPath);
            fs::path base = path.parent_path() / (path.stem().string() + "_graph");
            return base.generic_string();
        }

        std::string ReadTextFile(const std::filesystem::path &_path)
        {
            std::ifstream input(_path);
            if (!input.is_open())
                return "";

            std::ostringstream stream;
            stream << input.rdbuf();
            return stream.str();
        }

        bool WriteTextFile(const std::string &_path, const std::string &_contents)
        {
            namespace fs = std::filesystem;

            std::error_code ec;
            fs::path path(_path);
            if (!path.parent_path().empty())
                fs::create_directories(path.parent_path(), ec);

            std::ofstream output(_path);
            if (!output.is_open())
                return false;

            output << _contents;
            return true;
        }

        const char *GetStageUvExpression(ShaderGraphBuildStage _stage)
        {
            return _stage == ShaderGraphBuildStage::Vertex ? "vertexUV" : "fragmentUV";
        }

        ExpressionResult GetStagePositionExpression(ShaderGraphBuildStage _stage)
        {
            if (_stage == ShaderGraphBuildStage::Vertex)
                return { ShaderGraphValueType::VEC3, "sgWorldPos.xyz" };

            return { ShaderGraphValueType::VEC3, "fragmentWorldPos" };
        }

        ExpressionResult GetStageNormalExpression(ShaderGraphBuildStage _stage)
        {
            if (_stage == ShaderGraphBuildStage::Vertex)
                return { ShaderGraphValueType::VEC3, "sgWorldNormal" };

            return { ShaderGraphValueType::VEC3, "normalize(fragmentNormal)" };
        }

        ExpressionResult GetAmbientLightExpression()
        {
            return { ShaderGraphValueType::VEC3, "ambientLightLinear" };
        }

        void AppendTextureUniforms(std::vector<TextureUniformInfo> &_target, const std::vector<TextureUniformInfo> &_source)
        {
            for (const TextureUniformInfo &uniform : _source)
            {
                const auto existingIt = std::find_if(
                    _target.begin(),
                    _target.end(),
                    [&](const TextureUniformInfo &_existing)
                    {
                        return _existing.nodeId == uniform.nodeId;
                    });

                if (existingIt == _target.end())
                    _target.push_back(uniform);
                else
                    *existingIt = uniform;
            }
        }

        void AppendPropertyUniforms(std::vector<PropertyUniformInfo> &_target, const std::vector<PropertyUniformInfo> &_source)
        {
            for (const PropertyUniformInfo &uniform : _source)
            {
                const auto existingIt = std::find_if(
                    _target.begin(),
                    _target.end(),
                    [&](const PropertyUniformInfo &_existing)
                    {
                        return _existing.propertyId == uniform.propertyId;
                    });

                if (existingIt == _target.end())
                    _target.push_back(uniform);
                else
                    *existingIt = uniform;
            }
        }

        void WritePropertyUniformDeclaration(std::ostringstream &_shader, const PropertyUniformInfo &_uniform)
        {
            switch (_uniform.property.type)
            {
                case ShaderGraphPropertyType::FLOAT:
                    _shader << "uniform float " << _uniform.uniformName << ";\n";
                    break;
                case ShaderGraphPropertyType::VEC2:
                    _shader << "uniform vec2 " << _uniform.uniformName << ";\n";
                    break;
                case ShaderGraphPropertyType::VEC3:
                    _shader << "uniform vec3 " << _uniform.uniformName << ";\n";
                    break;
                case ShaderGraphPropertyType::VEC4:
                case ShaderGraphPropertyType::COLOR:
                    _shader << "uniform vec4 " << _uniform.uniformName << ";\n";
                    break;
                case ShaderGraphPropertyType::TEXTURE:
                    _shader << "uniform sampler2D " << _uniform.uniformName << ";\n";
                    break;
            }
        }

        ExpressionResult BuildLinkExpression(BuildContext &_context, const ShaderGraphLink &_link, const ExpressionResult &_fallback);

        std::string BuildVertexShaderSource(
            const ShaderGraphDocument &_document,
            std::vector<TextureUniformInfo> &_outTextureUniforms,
            std::vector<PropertyUniformInfo> &_outPropertyUniforms)
        {
            BuildContext context;
            context.document = &_document;
            context.stage = ShaderGraphBuildStage::Vertex;

            ExpressionResult positionExpression = BuildLinkExpression(
                context,
                _document.vertexPosition,
                GetStagePositionExpression(context.stage));
            ExpressionResult normalExpression = BuildLinkExpression(
                context,
                _document.vertexNormal,
                GetStageNormalExpression(context.stage));

            _outTextureUniforms = context.textureUniforms;
            _outPropertyUniforms = context.propertyUniforms;

            std::ostringstream shader;
            shader
                << "[OPENGL VERSION]\n\n"
                << "layout (location = 0) in vec3 vertexPosition;\n"
                << "layout (location = 1) in vec3 vertexNormal;\n"
                << "layout (location = 2) in vec2 vertexUV;\n\n"
                << "uniform mat4 P;\n"
                << "uniform mat4 V;\n"
                << "uniform mat4 M;\n"
                << "uniform float TIME;\n";

            for (const TextureUniformInfo &uniform : context.textureUniforms)
                shader << "uniform sampler2D " << uniform.uniformName << ";\n";
            for (const PropertyUniformInfo &uniform : context.propertyUniforms)
                WritePropertyUniformDeclaration(shader, uniform);

            shader
                << "\n"
                << "out vec3 fragmentNormal;\n"
                << "out vec2 fragmentUV;\n"
                << "out vec3 fragmentWorldPos;\n\n"
                << "void main()\n"
                << "{\n"
                << "    vec4 sgWorldPos = M * vec4(vertexPosition, 1.0);\n"
                << "    mat3 sgNormalMatrix = mat3(transpose(inverse(M)));\n"
                << "    vec3 sgWorldNormal = normalize(sgNormalMatrix * vertexNormal);\n"
                << "    vec3 sgFinalWorldPos = " << ConvertExpression(positionExpression, ShaderGraphValueType::VEC3) << ";\n"
                << "    vec3 sgFinalWorldNormal = normalize(" << ConvertExpression(normalExpression, ShaderGraphValueType::VEC3) << ");\n"
                << "    fragmentNormal = sgFinalWorldNormal;\n"
                << "    fragmentUV = vertexUV;\n"
                << "    fragmentWorldPos = sgFinalWorldPos;\n"
                << "    gl_Position = P * V * vec4(sgFinalWorldPos, 1.0);\n"
                << "}\n";

            return shader.str();
        }

        void TrackTextureUniform(BuildContext &_context, int _nodeId, const std::string &_texturePath)
        {
            for (TextureUniformInfo &uniform : _context.textureUniforms)
            {
                if (uniform.nodeId == _nodeId)
                {
                    uniform.texturePath = _texturePath;
                    return;
                }
            }

            TextureUniformInfo info;
            info.nodeId = _nodeId;
            info.uniformName = "sg_texture_" + std::to_string(_nodeId);
            info.texturePath = _texturePath;
            _context.textureUniforms.push_back(info);
        }

        void TrackPropertyUniform(BuildContext &_context, const ShaderGraphProperty &_property)
        {
            for (PropertyUniformInfo &uniform : _context.propertyUniforms)
            {
                if (uniform.propertyId == _property.id)
                {
                    uniform.property = _property;
                    return;
                }
            }

            PropertyUniformInfo info;
            info.propertyId = _property.id;
            info.uniformName = GetShaderGraphPropertyUniformName(_property);
            info.property = _property;
            _context.propertyUniforms.push_back(info);
        }

        ExpressionResult BuildNodeExpression(BuildContext &_context, const ShaderGraphNode &_node, const std::string &_slot);

        ExpressionResult BuildLinkExpression(BuildContext &_context, const ShaderGraphLink &_link, const ExpressionResult &_fallback)
        {
            if (!_link.IsValid())
                return _fallback;

            const ShaderGraphNode *node = FindShaderGraphNode(*_context.document, _link.nodeId);
            if (node == nullptr)
                return _fallback;

            return BuildNodeExpression(_context, *node, _link.slot);
        }

        ShaderGraphValueType ResolveArithmeticType(const ExpressionResult &_a, const ExpressionResult &_b)
        {
            ShaderGraphValueType resultType = MaxValueType(_a.type, _b.type);
            if (resultType == ShaderGraphValueType::UNKNOWN)
                resultType = ShaderGraphValueType::FLOAT;
            return resultType;
        }

        ExpressionResult BuildNodeExpression(BuildContext &_context, const ShaderGraphNode &_node, const std::string &_slot)
        {
            const std::string cacheKey = std::to_string(_node.id) + "|" + _slot;
            if (auto it = _context.cache.find(cacheKey); it != _context.cache.end())
                return it->second;

            if (_context.visiting.contains(cacheKey))
                return { ShaderGraphValueType::VEC4, "vec4(1.0, 0.0, 1.0, 1.0)" };

            _context.visiting.insert(cacheKey);

            ExpressionResult result = {};

            if (_node.type == "Float")
            {
                result = { ShaderGraphValueType::FLOAT, FormatFloatLiteral(_node.floatValue) };
            }
            else if (_node.type == "Vector2")
            {
                result = { ShaderGraphValueType::VEC2, FormatVec2Literal(_node.vec2Value) };
            }
            else if (_node.type == "Vector3")
            {
                result = { ShaderGraphValueType::VEC3, FormatVec3Literal(_node.vec3Value) };
            }
            else if (_node.type == "Vector4")
            {
                result = { ShaderGraphValueType::VEC4, FormatVec4Literal(_node.vec4Value) };
            }
            else if (_node.type == "Color")
            {
                result = { ShaderGraphValueType::VEC4, FormatVec4Literal(_node.colorValue) };
            }
            else if (_node.type == "UV")
            {
                result = { ShaderGraphValueType::VEC2, GetStageUvExpression(_context.stage) };
            }
            else if (_node.type == "Time")
            {
                result = { ShaderGraphValueType::FLOAT, "TIME" };
            }
            else if (_node.type == "Position")
            {
                result = GetStagePositionExpression(_context.stage);
            }
            else if (_node.type == "Normal")
            {
                result = GetStageNormalExpression(_context.stage);
            }
            else if (_node.type == "AmbientLight")
            {
                result = GetAmbientLightExpression();
            }
            else if (_node.type == "Property")
            {
                const ShaderGraphProperty *property = FindShaderGraphProperty(*_context.document, _node.propertyId);
                if (property == nullptr)
                {
                    result = MakeZeroValue(GetShaderGraphValueType(_node.propertyType));
                }
                else if (property->type == ShaderGraphPropertyType::TEXTURE)
                {
                    TrackPropertyUniform(_context, *property);
                    const ExpressionResult uvExpression = BuildLinkExpression(
                        _context,
                        _node.inputUV,
                        { ShaderGraphValueType::VEC2, GetStageUvExpression(_context.stage) });
                    result =
                    {
                        ShaderGraphValueType::VEC4,
                        "texture(" + GetShaderGraphPropertyUniformName(*property) + ", " + ConvertExpression(uvExpression, ShaderGraphValueType::VEC2) + ")"
                    };
                }
                else
                {
                    TrackPropertyUniform(_context, *property);
                    result = { GetShaderGraphValueType(property->type), GetShaderGraphPropertyUniformName(*property) };
                }
            }
            else if (_node.type == "Texture2D")
            {
                TrackTextureUniform(_context, _node.id, _node.texturePath);
                const ExpressionResult uvExpression = BuildLinkExpression(
                    _context,
                    _node.inputUV,
                    { ShaderGraphValueType::VEC2, GetStageUvExpression(_context.stage) });
                const std::string uniformName = "sg_texture_" + std::to_string(_node.id);
                result =
                {
                    ShaderGraphValueType::VEC4,
                    "texture(" + uniformName + ", " + ConvertExpression(uvExpression, ShaderGraphValueType::VEC2) + ")"
                };
            }
            else if (_node.type == "Add")
            {
                ExpressionResult a = BuildLinkExpression(_context, _node.inputA, { ShaderGraphValueType::UNKNOWN, "" });
                ExpressionResult b = BuildLinkExpression(_context, _node.inputB, { ShaderGraphValueType::UNKNOWN, "" });
                ShaderGraphValueType resultType = ResolveArithmeticType(a, b);
                if (a.type == ShaderGraphValueType::UNKNOWN)
                    a = MakeZeroValue(resultType);
                if (b.type == ShaderGraphValueType::UNKNOWN)
                    b = MakeZeroValue(resultType);

                result =
                {
                    resultType,
                    "(" + ConvertExpression(a, resultType) + " + " + ConvertExpression(b, resultType) + ")"
                };
            }
            else if (_node.type == "Multiply")
            {
                ExpressionResult a = BuildLinkExpression(_context, _node.inputA, { ShaderGraphValueType::UNKNOWN, "" });
                ExpressionResult b = BuildLinkExpression(_context, _node.inputB, { ShaderGraphValueType::UNKNOWN, "" });
                ShaderGraphValueType resultType = ResolveArithmeticType(a, b);
                if (a.type == ShaderGraphValueType::UNKNOWN)
                    a = MakeOneValue(resultType);
                if (b.type == ShaderGraphValueType::UNKNOWN)
                    b = MakeOneValue(resultType);

                result =
                {
                    resultType,
                    "(" + ConvertExpression(a, resultType) + " * " + ConvertExpression(b, resultType) + ")"
                };
            }
            else if (_node.type == "Sine")
            {
                ExpressionResult input = BuildLinkExpression(_context, _node.inputA, { ShaderGraphValueType::FLOAT, "TIME" });
                ShaderGraphValueType resultType = input.type;
                if (resultType == ShaderGraphValueType::UNKNOWN)
                    resultType = ShaderGraphValueType::FLOAT;

                result =
                {
                    resultType,
                    "sin(" + ConvertExpression(input, resultType) + ")"
                };
            }
            else if (_node.type == "Lerp")
            {
                ExpressionResult a = BuildLinkExpression(_context, _node.inputA, { ShaderGraphValueType::UNKNOWN, "" });
                ExpressionResult b = BuildLinkExpression(_context, _node.inputB, { ShaderGraphValueType::UNKNOWN, "" });
                ShaderGraphValueType resultType = ResolveArithmeticType(a, b);
                if (a.type == ShaderGraphValueType::UNKNOWN)
                    a = MakeZeroValue(resultType);
                if (b.type == ShaderGraphValueType::UNKNOWN)
                    b = MakeOneValue(resultType);

                ExpressionResult t = BuildLinkExpression(_context, _node.inputT, { ShaderGraphValueType::FLOAT, "0.5" });
                result =
                {
                    resultType,
                    "mix(" + ConvertExpression(a, resultType) + ", " + ConvertExpression(b, resultType) + ", " +
                        ConvertExpression(t, ShaderGraphValueType::FLOAT) + ")"
                };
            }
            else if (_node.type == "Panner")
            {
                const ExpressionResult uvExpression = BuildLinkExpression(
                    _context,
                    _node.inputUV,
                    { ShaderGraphValueType::VEC2, GetStageUvExpression(_context.stage) });
                const ExpressionResult speedExpression = BuildLinkExpression(
                    _context,
                    _node.inputB,
                    { ShaderGraphValueType::VEC2, FormatVec2Literal(_node.speedValue) });

                result =
                {
                    ShaderGraphValueType::VEC2,
                    "(" + ConvertExpression(uvExpression, ShaderGraphValueType::VEC2) + " + " +
                        ConvertExpression(speedExpression, ShaderGraphValueType::VEC2) + " * TIME)"
                };
            }
            else
            {
                result = { ShaderGraphValueType::VEC4, "vec4(1.0, 0.0, 1.0, 1.0)" };
            }

            _context.visiting.erase(cacheKey);
            _context.cache[cacheKey] = result;
            return result;
        }

        std::string BuildFragmentShaderSource(
            const ShaderGraphDocument &_document,
            std::vector<TextureUniformInfo> &_outTextureUniforms,
            std::vector<PropertyUniformInfo> &_outPropertyUniforms)
        {
            BuildContext context;
            context.document = &_document;
            context.stage = ShaderGraphBuildStage::Fragment;

            ExpressionResult colorExpression = BuildLinkExpression(
                context,
                _document.outputColor,
                { ShaderGraphValueType::VEC4, "vec4(1.0)" });
            ExpressionResult alphaExpression = BuildLinkExpression(
                context,
                _document.outputAlpha,
                { ShaderGraphValueType::FLOAT, "1.0" });

            _outTextureUniforms = context.textureUniforms;
            _outPropertyUniforms = context.propertyUniforms;

            std::ostringstream shader;
            shader
                << "[OPENGL VERSION]\n\n"
                << "in vec3 fragmentNormal;\n"
                << "in vec2 fragmentUV;\n\n"
                << "in vec3 fragmentWorldPos;\n\n"
                << "uniform vec4 albedoValue;\n"
                << "uniform vec3 ambientLightColor;\n"
                << "uniform float ambientLightIntensity;\n"
                << "uniform float TIME;\n";

            for (const TextureUniformInfo &uniform : context.textureUniforms)
                shader << "uniform sampler2D " << uniform.uniformName << ";\n";
            for (const PropertyUniformInfo &uniform : context.propertyUniforms)
                WritePropertyUniformDeclaration(shader, uniform);

            shader
                << "\n"
                << "vec3 SRGBToLinear(vec3 value)\n"
                << "{\n"
                << "    return pow(max(value, vec3(0.0)), vec3(2.2));\n"
                << "}\n"
                << "\n"
                << "out vec4 color;\n\n"
                << "void main()\n"
                << "{\n"
                << "    vec3 ambientLightLinear = clamp(ambientLightColor, vec3(0.0), vec3(1.0)) * max(ambientLightIntensity, 0.0);\n"
                << "    vec4 sgSurfaceColor = " << ConvertExpression(colorExpression, ShaderGraphValueType::VEC4) << ";\n"
                << "    sgSurfaceColor *= albedoValue;\n"
                << "    float sgSurfaceAlpha = clamp(" << ConvertExpression(alphaExpression, ShaderGraphValueType::FLOAT, true) << ", 0.0, 1.0);\n"
                << "    color = vec4(sgSurfaceColor.rgb, clamp(sgSurfaceColor.a * sgSurfaceAlpha, 0.0, 1.0));\n"
                << "}\n";

            return shader.str();
        }

        void RemoveGeneratedTextureUniforms(YAML::Node &_uniformsNode)
        {
            if (!_uniformsNode || !_uniformsNode.IsMap())
                return;

            std::vector<std::string> keysToRemove = {};
            for (const auto &entry : _uniformsNode)
            {
                const std::string key = entry.first.as<std::string>("");
                if (key.rfind("sg_texture_", 0) == 0)
                    keysToRemove.push_back(key);
            }

            for (const std::string &key : keysToRemove)
                _uniformsNode.remove(key);
        }

        void RemoveStaleGeneratedPropertyUniforms(YAML::Node &_uniformsNode, const std::unordered_set<std::string> &_activeUniformNames)
        {
            if (!_uniformsNode || !_uniformsNode.IsMap())
                return;

            std::vector<std::string> keysToRemove = {};
            for (const auto &entry : _uniformsNode)
            {
                const std::string key = entry.first.as<std::string>("");
                if (key.rfind("sgp_", 0) == 0 && !_activeUniformNames.contains(key))
                    keysToRemove.push_back(key);
            }

            for (const std::string &key : keysToRemove)
                _uniformsNode.remove(key);
        }

        YAML::Node MakeAssetPathRefNode(const std::string &_path)
        {
            YAML::Node node(YAML::NodeType::Map);
            node["path"] = _path;
            return node;
        }

        YAML::Node MakePropertyDefaultValueNode(const ShaderGraphProperty &_property)
        {
            switch (_property.type)
            {
                case ShaderGraphPropertyType::FLOAT:
                    return YAML::Node(_property.floatValue);
                case ShaderGraphPropertyType::VEC2:
                    return YAML::Node(_property.vec2Value);
                case ShaderGraphPropertyType::VEC3:
                    return YAML::Node(_property.vec3Value);
                case ShaderGraphPropertyType::VEC4:
                    return YAML::Node(_property.vec4Value);
                case ShaderGraphPropertyType::COLOR:
                    return YAML::Node(_property.colorValue);
                case ShaderGraphPropertyType::TEXTURE:
                    return MakeAssetPathRefNode(_property.texturePath);
            }

            return YAML::Node();
        }

        YAML::Node MakePropertyUniformNode(const ShaderGraphProperty &_property, const YAML::Node &_existingNode)
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            const std::string materialType = GetShaderGraphPropertyTypeMaterialName(_property.type);
            uniformNode["type"] = materialType;

            bool keepExistingValue = false;
            if (_existingNode && _existingNode.IsMap())
            {
                const std::string existingType = _existingNode["type"].as<std::string>("");
                keepExistingValue = existingType == materialType && _existingNode["value"];
            }

            uniformNode["value"] = keepExistingValue ? _existingNode["value"] : MakePropertyDefaultValueNode(_property);
            return uniformNode;
        }
    }

    ShaderGraphDocument MakeDefaultShaderGraphDocument()
    {
        ShaderGraphDocument document;
        document.nextNodeId = 2;
        document.nextPropertyId = 1;
        document.nodes.push_back(CreateShaderGraphNode("Color", 1, Vector2(48.0f, 48.0f)));
        document.outputColor = ShaderGraphLink{ .nodeId = 1, .slot = "color" };
        document.outputAlpha = ShaderGraphLink{ .nodeId = 1, .slot = "color" };
        return document;
    }

    ShaderGraphNode CreateShaderGraphNode(const std::string &_type, int _id, const Vector2 &_position)
    {
        ShaderGraphNode node;
        node.id = _id;
        node.type = _type;
        node.position = _position;

        if (_type == "Float")
        {
            node.floatValue = 1.0f;
        }
        else if (_type == "Vector2")
        {
            node.vec2Value = Vector2(1.0f);
        }
        else if (_type == "Vector3")
        {
            node.vec3Value = Vector3(1.0f);
        }
        else if (_type == "Vector4")
        {
            node.vec4Value = Vector4(1.0f);
        }
        else if (_type == "Color")
        {
            node.colorValue = Color(1.0f);
        }
        else if (_type == "Panner")
        {
            node.speedValue = Vector2(0.25f, 0.0f);
        }
        else if (_type == "Property")
        {
            node.propertyType = ShaderGraphPropertyType::FLOAT;
        }
        else if (_type == "StickyNote")
        {
            node.title = "Note";
            node.text = "Add a short explanation for this part of the graph.";
        }
        else if (_type == "Preview")
        {
            node.title = "Preview";
        }
        else if (_type == "Position")
        {
            node.title = "Position";
        }
        else if (_type == "Normal")
        {
            node.title = "Normal";
        }
        else if (_type == "AmbientLight")
        {
            node.title = "Ambient Light";
        }

        return node;
    }

    ShaderGraphProperty CreateShaderGraphProperty(ShaderGraphPropertyType _type, int _id, const std::string &_name)
    {
        ShaderGraphProperty property;
        property.id = _id;
        property.type = _type;
        property.name = _name.empty() ? std::string(GetShaderGraphPropertyTypeDisplayName(_type)) : _name;

        if (_type == ShaderGraphPropertyType::COLOR)
            property.colorValue = Color(1.0f);
        else if (_type == ShaderGraphPropertyType::TEXTURE)
            property.texturePath.clear();

        return property;
    }

    bool LoadShaderGraphDocument(const std::string &_path, ShaderGraphDocument &_document)
    {
        if (!std::filesystem::exists(_path))
            return false;

        YAML::Node root = YAML::LoadFile(_path);
        if (!root || !root.IsMap())
            return false;

        ShaderGraphDocument document = MakeDefaultShaderGraphDocument();
        document.nodes.clear();
        document.nextNodeId = std::max(root["nextNodeId"].as<int>(1), 1);
        document.nextPropertyId = std::max(root["nextPropertyId"].as<int>(1), 1);
        document.outputColor = DecodeLink(root["outputColor"]);
        document.outputAlpha = DecodeLink(root["outputAlpha"]);
        document.vertexPosition = DecodeLink(root["vertexPosition"]);
        document.vertexNormal = DecodeLink(root["vertexNormal"]);

        YAML::Node propertiesNode = root["properties"];
        if (propertiesNode && propertiesNode.IsSequence())
        {
            for (const YAML::Node &propertyEntry : propertiesNode)
            {
                if (!propertyEntry || !propertyEntry.IsMap())
                    continue;

                ShaderGraphProperty property;
                property.id = propertyEntry["id"].as<int>(document.nextPropertyId++);
                property.name = propertyEntry["name"].as<std::string>("Property");
                property.type = StringToPropertyType(propertyEntry["type"].as<std::string>("float"));
                property.floatValue = propertyEntry["floatValue"].as<float>(1.0f);
                property.vec2Value = propertyEntry["vec2Value"].as<Vector2>(Vector2(1.0f));
                property.vec3Value = propertyEntry["vec3Value"].as<Vector3>(Vector3(1.0f));
                property.vec4Value = propertyEntry["vec4Value"].as<Vector4>(Vector4(1.0f));
                property.colorValue = propertyEntry["colorValue"].as<Color>(Color(1.0f));
                property.texturePath = propertyEntry["texturePath"].as<std::string>("");
                document.properties.push_back(property);
                document.nextPropertyId = std::max(document.nextPropertyId, property.id + 1);
            }
        }

        YAML::Node nodesNode = root["nodes"];
        if (nodesNode && nodesNode.IsSequence())
        {
            for (const YAML::Node &nodeEntry : nodesNode)
            {
                if (!nodeEntry || !nodeEntry.IsMap())
                    continue;

                ShaderGraphNode node;
                node.id = nodeEntry["id"].as<int>(document.nextNodeId++);
                node.type = nodeEntry["type"].as<std::string>("Color");
                node.position = nodeEntry["position"].as<Vector2>(Vector2(0.0f));
                node.floatValue = nodeEntry["floatValue"].as<float>(1.0f);
                node.vec2Value = nodeEntry["vec2Value"].as<Vector2>(Vector2(1.0f));
                node.vec3Value = nodeEntry["vec3Value"].as<Vector3>(Vector3(1.0f));
                node.vec4Value = nodeEntry["vec4Value"].as<Vector4>(Vector4(1.0f));
                node.colorValue = nodeEntry["colorValue"].as<Color>(Color(1.0f));
                node.speedValue = nodeEntry["speedValue"].as<Vector2>(Vector2(0.25f, 0.0f));
                node.texturePath = nodeEntry["texturePath"].as<std::string>("");
                node.propertyId = nodeEntry["propertyId"].as<int>(-1);
                node.propertyType = StringToPropertyType(nodeEntry["propertyType"].as<std::string>("float"));
                node.title = nodeEntry["title"].as<std::string>("");
                node.text = nodeEntry["text"].as<std::string>("");
                node.inputA = DecodeLink(nodeEntry["inputA"]);
                node.inputB = DecodeLink(nodeEntry["inputB"]);
                node.inputT = DecodeLink(nodeEntry["inputT"]);
                node.inputUV = DecodeLink(nodeEntry["inputUV"]);
                document.nodes.push_back(node);
                document.nextNodeId = std::max(document.nextNodeId, node.id + 1);
            }
        }

        if (document.nodes.empty())
            document = MakeDefaultShaderGraphDocument();

        _document = document;
        return true;
    }

    bool SaveShaderGraphDocument(const std::string &_path, const ShaderGraphDocument &_document)
    {
        YAML::Node root(YAML::NodeType::Map);
        root["nextNodeId"] = std::max(_document.nextNodeId, 1);
        root["nextPropertyId"] = std::max(_document.nextPropertyId, 1);
        root["outputColor"] = EncodeLink(_document.outputColor);
        root["outputAlpha"] = EncodeLink(_document.outputAlpha);
        root["vertexPosition"] = EncodeLink(_document.vertexPosition);
        root["vertexNormal"] = EncodeLink(_document.vertexNormal);

        YAML::Node propertiesNode(YAML::NodeType::Sequence);
        for (const ShaderGraphProperty &property : _document.properties)
        {
            YAML::Node propertyEntry(YAML::NodeType::Map);
            propertyEntry["id"] = property.id;
            propertyEntry["name"] = property.name;
            propertyEntry["type"] = PropertyTypeToString(property.type);
            propertyEntry["floatValue"] = property.floatValue;
            propertyEntry["vec2Value"] = property.vec2Value;
            propertyEntry["vec3Value"] = property.vec3Value;
            propertyEntry["vec4Value"] = property.vec4Value;
            propertyEntry["colorValue"] = property.colorValue;
            propertyEntry["texturePath"] = property.texturePath;
            propertiesNode.push_back(propertyEntry);
        }
        root["properties"] = propertiesNode;

        YAML::Node nodesNode(YAML::NodeType::Sequence);
        for (const ShaderGraphNode &node : _document.nodes)
        {
            YAML::Node nodeEntry(YAML::NodeType::Map);
            nodeEntry["id"] = node.id;
            nodeEntry["type"] = node.type;
            nodeEntry["position"] = node.position;
            nodeEntry["floatValue"] = node.floatValue;
            nodeEntry["vec2Value"] = node.vec2Value;
            nodeEntry["vec3Value"] = node.vec3Value;
            nodeEntry["vec4Value"] = node.vec4Value;
            nodeEntry["colorValue"] = node.colorValue;
            nodeEntry["speedValue"] = node.speedValue;
            nodeEntry["texturePath"] = node.texturePath;
            nodeEntry["propertyId"] = node.propertyId;
            nodeEntry["propertyType"] = PropertyTypeToString(node.propertyType);
            nodeEntry["title"] = node.title;
            nodeEntry["text"] = node.text;
            nodeEntry["inputA"] = EncodeLink(node.inputA);
            nodeEntry["inputB"] = EncodeLink(node.inputB);
            nodeEntry["inputT"] = EncodeLink(node.inputT);
            nodeEntry["inputUV"] = EncodeLink(node.inputUV);
            nodesNode.push_back(nodeEntry);
        }

        root["nodes"] = nodesNode;

        std::error_code ec;
        std::filesystem::path path(_path);
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream output(_path);
        if (!output.is_open())
            return false;

        output << root;
        return true;
    }

    bool GenerateShaderGraphAssets(const std::string &_graphPath, const ShaderGraphDocument &_document, std::string *_errorMessage)
    {
        std::vector<TextureUniformInfo> vertexTextureUniforms = {};
        std::vector<PropertyUniformInfo> vertexPropertyUniforms = {};
        std::vector<TextureUniformInfo> fragmentTextureUniforms = {};
        std::vector<PropertyUniformInfo> fragmentPropertyUniforms = {};
        std::vector<TextureUniformInfo> textureUniforms = {};
        std::vector<PropertyUniformInfo> propertyUniforms = {};
        const std::string vertexPath = GetShaderGraphGeneratedVertexPath(_graphPath);
        const std::string fragmentPath = GetShaderGraphGeneratedFragmentPath(_graphPath);
        const std::string materialPath = GetShaderGraphGeneratedMaterialPath(_graphPath);

        const std::string vertexSource = BuildVertexShaderSource(_document, vertexTextureUniforms, vertexPropertyUniforms);
        const std::string fragmentSource = BuildFragmentShaderSource(_document, fragmentTextureUniforms, fragmentPropertyUniforms);
        AppendTextureUniforms(textureUniforms, vertexTextureUniforms);
        AppendTextureUniforms(textureUniforms, fragmentTextureUniforms);
        AppendPropertyUniforms(propertyUniforms, vertexPropertyUniforms);
        AppendPropertyUniforms(propertyUniforms, fragmentPropertyUniforms);

        if (!WriteTextFile(vertexPath, vertexSource))
        {
            if (_errorMessage != nullptr)
                *_errorMessage = "Failed to write generated vertex shader.";
            return false;
        }

        if (!WriteTextFile(fragmentPath, fragmentSource))
        {
            if (_errorMessage != nullptr)
                *_errorMessage = "Failed to write generated fragment shader.";
            return false;
        }

        YAML::Node materialRoot(YAML::NodeType::Map);
        if (std::filesystem::exists(materialPath))
        {
            YAML::Node loaded = YAML::LoadFile(materialPath);
            if (loaded && loaded.IsMap())
                materialRoot = loaded;
        }

        YAML::Node shaderNode(YAML::NodeType::Map);
        shaderNode["path"] = vertexPath;
        materialRoot["shader"] = shaderNode;

        if (!materialRoot["color"])
            materialRoot["color"] = Color(1.0f);
        if (!materialRoot["backFaceCulling"])
            materialRoot["backFaceCulling"] = true;

        YAML::Node uniformsNode = materialRoot["uniforms"];
        if (!uniformsNode || !uniformsNode.IsMap())
            uniformsNode = YAML::Node(YAML::NodeType::Map);

        RemoveGeneratedTextureUniforms(uniformsNode);

        std::unordered_set<std::string> activePropertyUniformNames = {};
        for (const PropertyUniformInfo &uniform : propertyUniforms)
            activePropertyUniformNames.insert(uniform.uniformName);
        RemoveStaleGeneratedPropertyUniforms(uniformsNode, activePropertyUniformNames);

        for (const TextureUniformInfo &uniform : textureUniforms)
        {
            YAML::Node uniformNode(YAML::NodeType::Map);
            uniformNode["type"] = "texture";

            YAML::Node valueNode(YAML::NodeType::Map);
            valueNode["path"] = uniform.texturePath;
            uniformNode["value"] = valueNode;
            uniformsNode[uniform.uniformName] = uniformNode;
        }

        for (const PropertyUniformInfo &uniform : propertyUniforms)
        {
            const YAML::Node existingUniform = uniformsNode[uniform.uniformName];
            uniformsNode[uniform.uniformName] = MakePropertyUniformNode(uniform.property, existingUniform);
        }

        if (uniformsNode.size() > 0u)
            materialRoot["uniforms"] = uniformsNode;
        else
            materialRoot.remove("uniforms");

        std::ofstream materialFile(materialPath);
        if (!materialFile.is_open())
        {
            if (_errorMessage != nullptr)
                *_errorMessage = "Failed to write generated companion material.";
            return false;
        }

        materialFile << materialRoot;

        if (_errorMessage != nullptr)
            _errorMessage->clear();
        return true;
    }

    std::string GetShaderGraphGeneratedVertexPath(const std::string &_graphPath)
    {
        return GetGeneratedBasePath(_graphPath) + ".vs";
    }

    std::string GetShaderGraphGeneratedFragmentPath(const std::string &_graphPath)
    {
        return GetGeneratedBasePath(_graphPath) + ".fs";
    }

    std::string GetShaderGraphGeneratedMaterialPath(const std::string &_graphPath)
    {
        return GetGeneratedBasePath(_graphPath) + ".material";
    }

    const char *GetShaderGraphNodeDisplayName(const std::string &_type)
    {
        if (_type == "Float") return "Float";
        if (_type == "Vector2") return "Vector2";
        if (_type == "Vector3") return "Vector3";
        if (_type == "Vector4") return "Vector4";
        if (_type == "Color") return "Color";
        if (_type == "UV") return "UV";
        if (_type == "Time") return "Time";
        if (_type == "Position") return "Position";
        if (_type == "Normal") return "Normal";
        if (_type == "AmbientLight") return "Ambient Light";
        if (_type == "Texture2D") return "Texture2D";
        if (_type == "Add") return "Add";
        if (_type == "Multiply") return "Multiply";
        if (_type == "Sine") return "Sine";
        if (_type == "Lerp") return "Lerp";
        if (_type == "Panner") return "Panner";
        if (_type == "Property") return "Property";
        if (_type == "StickyNote") return "Sticky Note";
        if (_type == "Preview") return "Preview";
        return "Node";
    }

    const char *GetShaderGraphPropertyTypeDisplayName(ShaderGraphPropertyType _type)
    {
        switch (_type)
        {
            case ShaderGraphPropertyType::FLOAT: return "Float";
            case ShaderGraphPropertyType::VEC2: return "Vector2";
            case ShaderGraphPropertyType::VEC3: return "Vector3";
            case ShaderGraphPropertyType::VEC4: return "Vector4";
            case ShaderGraphPropertyType::COLOR: return "Color";
            case ShaderGraphPropertyType::TEXTURE: return "Texture2D";
            default: return "Float";
        }
    }

    const char *GetShaderGraphPropertyTypeMaterialName(ShaderGraphPropertyType _type)
    {
        return PropertyTypeToString(_type);
    }

    ShaderGraphValueType GetShaderGraphValueType(ShaderGraphPropertyType _type)
    {
        switch (_type)
        {
            case ShaderGraphPropertyType::FLOAT: return ShaderGraphValueType::FLOAT;
            case ShaderGraphPropertyType::VEC2: return ShaderGraphValueType::VEC2;
            case ShaderGraphPropertyType::VEC3: return ShaderGraphValueType::VEC3;
            case ShaderGraphPropertyType::VEC4:
            case ShaderGraphPropertyType::COLOR:
            case ShaderGraphPropertyType::TEXTURE:
                return ShaderGraphValueType::VEC4;
            default: return ShaderGraphValueType::FLOAT;
        }
    }

    std::string GetShaderGraphPropertyUniformName(const ShaderGraphProperty &_property)
    {
        return "sgp_" + SanitizeUniformIdentifier(_property.name) + "_" + std::to_string(std::max(_property.id, 0));
    }

    std::vector<ShaderGraphPinInfo> GetShaderGraphNodeInputPins(const ShaderGraphNode &_node)
    {
        if (_node.type == "Property" && _node.propertyType == ShaderGraphPropertyType::TEXTURE)
            return { ShaderGraphPinInfo{ .name = "uv", .type = ShaderGraphValueType::VEC2 } };
        if (_node.type == "Texture2D")
            return { ShaderGraphPinInfo{ .name = "uv", .type = ShaderGraphValueType::VEC2 } };
        if (_node.type == "Add" || _node.type == "Multiply")
            return { ShaderGraphPinInfo{ .name = "a", .type = ShaderGraphValueType::UNKNOWN }, ShaderGraphPinInfo{ .name = "b", .type = ShaderGraphValueType::UNKNOWN } };
        if (_node.type == "Sine")
            return { ShaderGraphPinInfo{ .name = "input", .type = ShaderGraphValueType::UNKNOWN } };
        if (_node.type == "Lerp")
            return {
                ShaderGraphPinInfo{ .name = "a", .type = ShaderGraphValueType::UNKNOWN },
                ShaderGraphPinInfo{ .name = "b", .type = ShaderGraphValueType::UNKNOWN },
                ShaderGraphPinInfo{ .name = "t", .type = ShaderGraphValueType::FLOAT }
            };
        if (_node.type == "Panner")
            return { ShaderGraphPinInfo{ .name = "uv", .type = ShaderGraphValueType::VEC2 }, ShaderGraphPinInfo{ .name = "speed", .type = ShaderGraphValueType::VEC2 } };
        if (_node.type == "Preview")
            return { ShaderGraphPinInfo{ .name = "input", .type = ShaderGraphValueType::UNKNOWN } };

        return {};
    }

    std::vector<ShaderGraphPinInfo> GetShaderGraphNodeOutputPins(const ShaderGraphNode &_node)
    {
        if (_node.type == "Property")
        {
            if (_node.propertyType == ShaderGraphPropertyType::TEXTURE)
                return { ShaderGraphPinInfo{ .name = "color", .type = ShaderGraphValueType::VEC4 } };
            if (_node.propertyType == ShaderGraphPropertyType::COLOR)
                return { ShaderGraphPinInfo{ .name = "color", .type = ShaderGraphValueType::VEC4 } };
            return { ShaderGraphPinInfo{ .name = "value", .type = GetShaderGraphValueType(_node.propertyType) } };
        }
        if (_node.type == "Float")
            return { ShaderGraphPinInfo{ .name = "value", .type = ShaderGraphValueType::FLOAT } };
        if (_node.type == "Vector2")
            return { ShaderGraphPinInfo{ .name = "value", .type = ShaderGraphValueType::VEC2 } };
        if (_node.type == "Vector3")
            return { ShaderGraphPinInfo{ .name = "value", .type = ShaderGraphValueType::VEC3 } };
        if (_node.type == "Vector4")
            return { ShaderGraphPinInfo{ .name = "value", .type = ShaderGraphValueType::VEC4 } };
        if (_node.type == "Color")
            return { ShaderGraphPinInfo{ .name = "color", .type = ShaderGraphValueType::VEC4 } };
        if (_node.type == "UV")
            return { ShaderGraphPinInfo{ .name = "uv", .type = ShaderGraphValueType::VEC2 } };
        if (_node.type == "Time")
            return { ShaderGraphPinInfo{ .name = "time", .type = ShaderGraphValueType::FLOAT } };
        if (_node.type == "Position")
            return { ShaderGraphPinInfo{ .name = "position", .type = ShaderGraphValueType::VEC3 } };
        if (_node.type == "Normal")
            return { ShaderGraphPinInfo{ .name = "normal", .type = ShaderGraphValueType::VEC3 } };
        if (_node.type == "AmbientLight")
            return { ShaderGraphPinInfo{ .name = "ambient", .type = ShaderGraphValueType::VEC3 } };
        if (_node.type == "Texture2D")
            return { ShaderGraphPinInfo{ .name = "color", .type = ShaderGraphValueType::VEC4 } };
        if (_node.type == "Panner")
            return { ShaderGraphPinInfo{ .name = "uv", .type = ShaderGraphValueType::VEC2 } };
        if (_node.type == "StickyNote" || _node.type == "Preview")
            return {};

        return { ShaderGraphPinInfo{ .name = "value", .type = ShaderGraphValueType::UNKNOWN } };
    }

    ShaderGraphLink GetShaderGraphNodeInputLink(const ShaderGraphNode &_node, const std::string &_pinName)
    {
        if (_pinName == "a")
            return _node.inputA;
        if (_pinName == "b")
            return _node.inputB;
        if (_pinName == "t")
            return _node.inputT;
        if (_pinName == "uv")
            return _node.inputUV;
        if (_pinName == "speed")
            return _node.inputB;
        if (_pinName == "input")
            return _node.inputA;
        return {};
    }

    bool SetShaderGraphNodeInputLink(ShaderGraphNode &_node, const std::string &_pinName, const ShaderGraphLink &_link)
    {
        if (_pinName == "a" || _pinName == "input")
        {
            _node.inputA = _link;
            return true;
        }
        if (_pinName == "b")
        {
            _node.inputB = _link;
            return true;
        }
        if (_pinName == "speed")
        {
            _node.inputB = _link;
            return true;
        }
        if (_pinName == "t")
        {
            _node.inputT = _link;
            return true;
        }
        if (_pinName == "uv")
        {
            _node.inputUV = _link;
            return true;
        }
        return false;
    }

    bool ClearShaderGraphNodeInputLink(ShaderGraphNode &_node, const std::string &_pinName)
    {
        return SetShaderGraphNodeInputLink(_node, _pinName, {});
    }

    ShaderGraphNode *FindShaderGraphNode(ShaderGraphDocument &_document, int _nodeId)
    {
        for (ShaderGraphNode &node : _document.nodes)
        {
            if (node.id == _nodeId)
                return &node;
        }

        return nullptr;
    }

    const ShaderGraphNode *FindShaderGraphNode(const ShaderGraphDocument &_document, int _nodeId)
    {
        for (const ShaderGraphNode &node : _document.nodes)
        {
            if (node.id == _nodeId)
                return &node;
        }

        return nullptr;
    }

    ShaderGraphProperty *FindShaderGraphProperty(ShaderGraphDocument &_document, int _propertyId)
    {
        for (ShaderGraphProperty &property : _document.properties)
        {
            if (property.id == _propertyId)
                return &property;
        }

        return nullptr;
    }

    const ShaderGraphProperty *FindShaderGraphProperty(const ShaderGraphDocument &_document, int _propertyId)
    {
        for (const ShaderGraphProperty &property : _document.properties)
        {
            if (property.id == _propertyId)
                return &property;
        }

        return nullptr;
    }

    bool RemoveShaderGraphNode(ShaderGraphDocument &_document, int _nodeId)
    {
        auto eraseIt = std::remove_if(
            _document.nodes.begin(),
            _document.nodes.end(),
            [_nodeId](const ShaderGraphNode &_node)
            {
                return _node.id == _nodeId;
            });

        if (eraseIt == _document.nodes.end())
            return false;

        _document.nodes.erase(eraseIt, _document.nodes.end());

        if (_document.outputColor.nodeId == _nodeId)
            _document.outputColor = {};
        if (_document.outputAlpha.nodeId == _nodeId)
            _document.outputAlpha = {};
        if (_document.vertexPosition.nodeId == _nodeId)
            _document.vertexPosition = {};
        if (_document.vertexNormal.nodeId == _nodeId)
            _document.vertexNormal = {};

        for (ShaderGraphNode &node : _document.nodes)
        {
            if (node.inputA.nodeId == _nodeId)
                node.inputA = {};
            if (node.inputB.nodeId == _nodeId)
                node.inputB = {};
            if (node.inputT.nodeId == _nodeId)
                node.inputT = {};
            if (node.inputUV.nodeId == _nodeId)
                node.inputUV = {};
        }

        return true;
    }

    bool RemoveShaderGraphProperty(ShaderGraphDocument &_document, int _propertyId)
    {
        auto eraseIt = std::remove_if(
            _document.properties.begin(),
            _document.properties.end(),
            [_propertyId](const ShaderGraphProperty &_property)
            {
                return _property.id == _propertyId;
            });

        if (eraseIt == _document.properties.end())
            return false;

        _document.properties.erase(eraseIt, _document.properties.end());

        for (ShaderGraphNode &node : _document.nodes)
        {
            if (node.type == "Property" && node.propertyId == _propertyId)
                node.propertyId = -1;
        }

        return true;
    }
}
