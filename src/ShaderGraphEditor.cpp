#include <Canis/Editor.hpp>

#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/ShaderGraph.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <GraphEditor.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Canis
{
    namespace
    {
        constexpr int kShaderGraphFragmentOutputSelectionId = -2;
        constexpr int kShaderGraphVertexOutputSelectionId = -3;
        constexpr GraphEditor::NodeIndex kInvalidGraphNodeIndex = static_cast<GraphEditor::NodeIndex>(-1);
        constexpr GraphEditor::SlotIndex kInvalidGraphSlotIndex = static_cast<GraphEditor::SlotIndex>(-1);

        struct ShaderGraphPendingLinkCreate
        {
            bool valid = false;
            bool startedFromInput = false;
            int nodeId = -1;
            std::string slot = {};
            Vector2 graphPosition = Vector2(0.0f);
        };

        struct ShaderGraphEditorViewState
        {
            GraphEditor::Options options = {};
            GraphEditor::ViewState viewState = {};
            ImVec2 contextMousePos = ImVec2(0.0f, 0.0f);
            ShaderGraphPendingLinkCreate pendingLinkCreate = {};
        };

        std::unordered_map<std::string, ShaderGraphEditorViewState> g_shaderGraphEditorViewStates = {};

        enum ShaderGraphTemplateIndex : size_t
        {
            ShaderGraphTemplate_Float = 0,
            ShaderGraphTemplate_Vector2,
            ShaderGraphTemplate_Vector3,
            ShaderGraphTemplate_Vector4,
            ShaderGraphTemplate_Color,
            ShaderGraphTemplate_UV,
            ShaderGraphTemplate_Time,
            ShaderGraphTemplate_Position,
            ShaderGraphTemplate_Normal,
            ShaderGraphTemplate_PropertyFloat,
            ShaderGraphTemplate_PropertyVector2,
            ShaderGraphTemplate_PropertyVector3,
            ShaderGraphTemplate_PropertyVector4,
            ShaderGraphTemplate_PropertyColor,
            ShaderGraphTemplate_PropertyTexture2D,
            ShaderGraphTemplate_Texture2D,
            ShaderGraphTemplate_Add,
            ShaderGraphTemplate_Multiply,
            ShaderGraphTemplate_Sine,
            ShaderGraphTemplate_Lerp,
            ShaderGraphTemplate_Panner,
            ShaderGraphTemplate_StickyNote,
            ShaderGraphTemplate_Preview,
            ShaderGraphTemplate_VertexOutput,
            ShaderGraphTemplate_Output,
            ShaderGraphTemplate_Count
        };

        const char *kTextureUvInputNames[] = { "UV" };
        const char *kTextureOutputNames[] = { "Color" };
        const char *kMathInputNames[] = { "A", "B" };
        const char *kMathOutputNames[] = { "Value" };
        const char *kSingleInputName[] = { "In" };
        const char *kSingleOutputName[] = { "Value" };
        const char *kLerpInputNames[] = { "A", "B", "T" };
        const char *kPannerInputNames[] = { "UV", "Speed" };
        const char *kPreviewInputNames[] = { "In" };
        const char *kUvOutputNames[] = { "UV" };
        const char *kTimeOutputNames[] = { "Time" };
        const char *kPositionOutputNames[] = { "Position" };
        const char *kNormalOutputNames[] = { "Normal" };
        const char *kVertexOutputInputNames[] = { "Position", "Normal" };
        const char *kOutputInputNames[] = { "Color", "Alpha" };

        ImU32 kTextureInputColors[] = { IM_COL32(96, 156, 255, 255) };
        ImU32 kTextureOutputColors[] = { IM_COL32(255, 214, 64, 255) };
        ImU32 kMathInputColors[] = { IM_COL32(180, 180, 180, 255), IM_COL32(180, 180, 180, 255) };
        ImU32 kMathOutputColors[] = { IM_COL32(255, 214, 64, 255) };
        ImU32 kSingleInputColors[] = { IM_COL32(180, 180, 180, 255) };
        ImU32 kPannerInputColors[] = { IM_COL32(96, 156, 255, 255), IM_COL32(96, 156, 255, 255) };
        ImU32 kSingleOutputFloatColors[] = { IM_COL32(205, 160, 255, 255) };
        ImU32 kSingleOutputVec2Colors[] = { IM_COL32(96, 156, 255, 255) };
        ImU32 kSingleOutputVec3Colors[] = { IM_COL32(90, 208, 160, 255) };
        ImU32 kSingleOutputVec4Colors[] = { IM_COL32(255, 182, 82, 255) };
        ImU32 kVertexOutputInputColors[] = { IM_COL32(90, 208, 160, 255), IM_COL32(90, 208, 160, 255) };
        ImU32 kOutputInputColors[] = { IM_COL32(255, 182, 82, 255), IM_COL32(205, 160, 255, 255) };

        GraphEditor::Template MakeTemplate(
            ImU32 _headerColor,
            ImU32 _backgroundColor,
            ImU32 _hoverColor,
            ImU8 _inputCount,
            const char **_inputNames,
            ImU32 *_inputColors,
            ImU8 _outputCount,
            const char **_outputNames,
            ImU32 *_outputColors)
        {
            GraphEditor::Template graphTemplate{};
            graphTemplate.mHeaderColor = _headerColor;
            graphTemplate.mBackgroundColor = _backgroundColor;
            graphTemplate.mBackgroundColorOver = _hoverColor;
            graphTemplate.mInputCount = _inputCount;
            graphTemplate.mInputNames = _inputNames;
            graphTemplate.mInputColors = _inputColors;
            graphTemplate.mOutputCount = _outputCount;
            graphTemplate.mOutputNames = _outputNames;
            graphTemplate.mOutputColors = _outputColors;
            return graphTemplate;
        }

        std::string FormatShaderGraphFloat(float _value, int _precision = 2)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(_precision) << _value;
            std::string value = stream.str();

            const std::size_t decimalPoint = value.find('.');
            if (decimalPoint != std::string::npos)
            {
                while (!value.empty() && value.back() == '0')
                    value.pop_back();
                if (!value.empty() && value.back() == '.')
                    value.pop_back();
            }

            if (value == "-0")
                value = "0";

            return value;
        }

        std::string FormatShaderGraphVector2(const Vector2 &_value)
        {
            return FormatShaderGraphFloat(_value.x) + ", " + FormatShaderGraphFloat(_value.y);
        }

        std::string FormatShaderGraphVector3(const Vector3 &_value)
        {
            return FormatShaderGraphFloat(_value.x) + ", " + FormatShaderGraphFloat(_value.y) + ", " + FormatShaderGraphFloat(_value.z);
        }

        std::string FormatShaderGraphVector4(const Vector4 &_value)
        {
            return FormatShaderGraphFloat(_value.x) + ", " + FormatShaderGraphFloat(_value.y) + ", " + FormatShaderGraphFloat(_value.z) + ", " + FormatShaderGraphFloat(_value.w);
        }

        float ScaleShaderGraphUi(float _value, float _zoom, float _minimum)
        {
            return std::max(_minimum, _value * _zoom);
        }

        float GetShaderGraphNodeBodyFontSize(float _zoom)
        {
            return ScaleShaderGraphUi(ImGui::GetFontSize() * 0.92f, _zoom, 5.0f);
        }

        float DrawShaderGraphNodeText(ImDrawList *_drawList, const ImRect &_rect, ImVec2 &_cursor, const std::string &_text, ImU32 _color, float _zoom)
        {
            const float fontSize = GetShaderGraphNodeBodyFontSize(_zoom);
            const float wrapWidth = std::max(1.0f, _rect.GetWidth() - ScaleShaderGraphUi(8.0f, _zoom, 4.0f));
            const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, wrapWidth, wrapWidth, _text.c_str());
            const ImVec4 clipRect(_rect.Min.x, _rect.Min.y, _rect.Max.x, _rect.Max.y);
            _drawList->AddText(ImGui::GetFont(), fontSize, _cursor, _color, _text.c_str(), nullptr, wrapWidth, &clipRect);
            _cursor.y += textSize.y + ScaleShaderGraphUi(6.0f, _zoom, 3.0f);
            return textSize.y;
        }

        enum class ShaderGraphPreviewKind
        {
            NONE = 0,
            FLOAT,
            VEC2,
            VEC3,
            VEC4,
            COLOR,
            TEXTURE,
            UV,
            TIME,
            POSITION,
            NORMAL
        };

        struct ShaderGraphPreviewValue
        {
            ShaderGraphPreviewKind kind = ShaderGraphPreviewKind::NONE;
            ShaderGraphValueType numericType = ShaderGraphValueType::UNKNOWN;
            Vector4 numericValue = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            std::string texturePath = {};
            ImVec4 tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        int GetShaderGraphValueTypeRank(ShaderGraphValueType _type)
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

        ShaderGraphValueType GetHigherShaderGraphValueType(ShaderGraphValueType _a, ShaderGraphValueType _b)
        {
            return (GetShaderGraphValueTypeRank(_a) >= GetShaderGraphValueTypeRank(_b)) ? _a : _b;
        }

        ShaderGraphPreviewKind GetShaderGraphPreviewKind(ShaderGraphValueType _type, bool _preferColor = false)
        {
            switch (_type)
            {
                case ShaderGraphValueType::FLOAT: return ShaderGraphPreviewKind::FLOAT;
                case ShaderGraphValueType::VEC2: return ShaderGraphPreviewKind::VEC2;
                case ShaderGraphValueType::VEC3: return ShaderGraphPreviewKind::VEC3;
                case ShaderGraphValueType::VEC4: return _preferColor ? ShaderGraphPreviewKind::COLOR : ShaderGraphPreviewKind::VEC4;
                default: return ShaderGraphPreviewKind::NONE;
            }
        }

        bool HasShaderGraphNumericPreview(const ShaderGraphPreviewValue &_preview)
        {
            return _preview.kind == ShaderGraphPreviewKind::FLOAT ||
                _preview.kind == ShaderGraphPreviewKind::VEC2 ||
                _preview.kind == ShaderGraphPreviewKind::VEC3 ||
                _preview.kind == ShaderGraphPreviewKind::VEC4 ||
                _preview.kind == ShaderGraphPreviewKind::COLOR;
        }

        ShaderGraphPreviewValue MakeShaderGraphNumericPreview(ShaderGraphValueType _type, const Vector4 &_value, bool _preferColor = false)
        {
            ShaderGraphPreviewValue preview;
            preview.kind = GetShaderGraphPreviewKind(_type, _preferColor);
            preview.numericType = _type;
            preview.numericValue = _value;
            return preview;
        }

        Vector4 ConvertShaderGraphPreviewNumeric(const Vector4 &_value, ShaderGraphValueType _sourceType, ShaderGraphValueType _targetType)
        {
            if (_targetType == ShaderGraphValueType::UNKNOWN || _sourceType == _targetType)
                return _value;

            switch (_targetType)
            {
                case ShaderGraphValueType::FLOAT:
                    return Vector4(_value.x, 0.0f, 0.0f, 1.0f);
                case ShaderGraphValueType::VEC2:
                    switch (_sourceType)
                    {
                        case ShaderGraphValueType::FLOAT: return Vector4(_value.x, _value.x, 0.0f, 1.0f);
                        case ShaderGraphValueType::VEC3:
                        case ShaderGraphValueType::VEC4: return Vector4(_value.x, _value.y, 0.0f, 1.0f);
                        default: return _value;
                    }
                case ShaderGraphValueType::VEC3:
                    switch (_sourceType)
                    {
                        case ShaderGraphValueType::FLOAT: return Vector4(_value.x, _value.x, _value.x, 1.0f);
                        case ShaderGraphValueType::VEC2: return Vector4(_value.x, _value.y, 0.0f, 1.0f);
                        case ShaderGraphValueType::VEC4: return Vector4(_value.x, _value.y, _value.z, 1.0f);
                        default: return _value;
                    }
                case ShaderGraphValueType::VEC4:
                    switch (_sourceType)
                    {
                        case ShaderGraphValueType::FLOAT: return Vector4(_value.x, _value.x, _value.x, _value.x);
                        case ShaderGraphValueType::VEC2: return Vector4(_value.x, _value.y, 0.0f, 1.0f);
                        case ShaderGraphValueType::VEC3: return Vector4(_value.x, _value.y, _value.z, 1.0f);
                        default: return _value;
                    }
                default:
                    return _value;
            }
        }

        ImVec4 ClampPreviewColor(const Vector4 &_value)
        {
            return ImVec4(
                std::clamp(_value.x, 0.0f, 1.0f),
                std::clamp(_value.y, 0.0f, 1.0f),
                std::clamp(_value.z, 0.0f, 1.0f),
                std::clamp(_value.w, 0.0f, 1.0f));
        }

        ImVec4 GetShaderGraphPreviewTint(const ShaderGraphPreviewValue &_preview)
        {
            if (_preview.kind == ShaderGraphPreviewKind::TEXTURE)
                return _preview.tint;

            if (HasShaderGraphNumericPreview(_preview))
                return ClampPreviewColor(ConvertShaderGraphPreviewNumeric(_preview.numericValue, _preview.numericType, ShaderGraphValueType::VEC4));

            if (_preview.kind == ShaderGraphPreviewKind::FLOAT)
            {
                const float value = std::clamp(_preview.numericValue.x, 0.0f, 1.0f);
                return ImVec4(value, value, value, 1.0f);
            }

            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        std::string GetShaderGraphPreviewSummary(const ShaderGraphPreviewValue &_preview)
        {
            switch (_preview.kind)
            {
                case ShaderGraphPreviewKind::FLOAT:
                    return "Float  " + FormatShaderGraphFloat(_preview.numericValue.x, 3);
                case ShaderGraphPreviewKind::VEC2:
                    return "Vector2  " + FormatShaderGraphVector2(Vector2(_preview.numericValue.x, _preview.numericValue.y));
                case ShaderGraphPreviewKind::VEC3:
                    return "Vector3  " + FormatShaderGraphVector3(Vector3(_preview.numericValue.x, _preview.numericValue.y, _preview.numericValue.z));
                case ShaderGraphPreviewKind::VEC4:
                    return "Vector4  " + FormatShaderGraphVector4(_preview.numericValue);
                case ShaderGraphPreviewKind::COLOR:
                    return "Color  " + FormatShaderGraphVector4(_preview.numericValue);
                case ShaderGraphPreviewKind::TEXTURE:
                    return "Texture  " + (_preview.texturePath.empty() ? std::string("[ none ]") : std::filesystem::path(_preview.texturePath).stem().string());
                case ShaderGraphPreviewKind::UV:
                    return "Mesh UVs";
                case ShaderGraphPreviewKind::TIME:
                    return "TIME";
                case ShaderGraphPreviewKind::POSITION:
                    return "World Position";
                case ShaderGraphPreviewKind::NORMAL:
                    return "World Normal";
                default:
                    return "Connect any output to inspect it here.";
            }
        }

        ShaderGraphPreviewValue ResolveShaderGraphPreviewValue(const ShaderGraphDocument &_document, const ShaderGraphLink &_link, int _depth = 0);

        ShaderGraphPreviewValue ResolveShaderGraphPreviewValue(const ShaderGraphDocument &_document, const ShaderGraphNode &_node, int _depth)
        {
            if (_depth > 24)
                return {};

            if (_node.type == "Float")
                return MakeShaderGraphNumericPreview(ShaderGraphValueType::FLOAT, Vector4(_node.floatValue, 0.0f, 0.0f, 1.0f));

            if (_node.type == "Vector2")
                return MakeShaderGraphNumericPreview(ShaderGraphValueType::VEC2, Vector4(_node.vec2Value.x, _node.vec2Value.y, 0.0f, 1.0f));

            if (_node.type == "Vector3")
                return MakeShaderGraphNumericPreview(ShaderGraphValueType::VEC3, Vector4(_node.vec3Value.x, _node.vec3Value.y, _node.vec3Value.z, 1.0f));

            if (_node.type == "Vector4")
                return MakeShaderGraphNumericPreview(ShaderGraphValueType::VEC4, _node.vec4Value);

            if (_node.type == "Color")
                return MakeShaderGraphNumericPreview(ShaderGraphValueType::VEC4, _node.colorValue, true);

            if (_node.type == "UV")
            {
                ShaderGraphPreviewValue preview;
                preview.kind = ShaderGraphPreviewKind::UV;
                return preview;
            }

            if (_node.type == "Time")
            {
                ShaderGraphPreviewValue preview;
                preview.kind = ShaderGraphPreviewKind::TIME;
                return preview;
            }

            if (_node.type == "Position")
            {
                ShaderGraphPreviewValue preview;
                preview.kind = ShaderGraphPreviewKind::POSITION;
                return preview;
            }

            if (_node.type == "Normal")
            {
                ShaderGraphPreviewValue preview;
                preview.kind = ShaderGraphPreviewKind::NORMAL;
                return preview;
            }

            if (_node.type == "Texture2D")
            {
                ShaderGraphPreviewValue preview;
                preview.kind = ShaderGraphPreviewKind::TEXTURE;
                preview.texturePath = _node.texturePath;
                preview.tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                return preview;
            }

            if (_node.type == "Property")
            {
                const ShaderGraphProperty *property = FindShaderGraphProperty(_document, _node.propertyId);
                if (property == nullptr)
                    return {};

                switch (property->type)
                {
                    case ShaderGraphPropertyType::FLOAT:
                        return MakeShaderGraphNumericPreview(ShaderGraphValueType::FLOAT, Vector4(property->floatValue, 0.0f, 0.0f, 1.0f));
                    case ShaderGraphPropertyType::VEC2:
                        return MakeShaderGraphNumericPreview(ShaderGraphValueType::VEC2, Vector4(property->vec2Value.x, property->vec2Value.y, 0.0f, 1.0f));
                    case ShaderGraphPropertyType::VEC3:
                        return MakeShaderGraphNumericPreview(ShaderGraphValueType::VEC3, Vector4(property->vec3Value.x, property->vec3Value.y, property->vec3Value.z, 1.0f));
                    case ShaderGraphPropertyType::VEC4:
                        return MakeShaderGraphNumericPreview(ShaderGraphValueType::VEC4, property->vec4Value);
                    case ShaderGraphPropertyType::COLOR:
                        return MakeShaderGraphNumericPreview(ShaderGraphValueType::VEC4, property->colorValue, true);
                    case ShaderGraphPropertyType::TEXTURE:
                    {
                        ShaderGraphPreviewValue preview;
                        preview.kind = ShaderGraphPreviewKind::TEXTURE;
                        preview.texturePath = property->texturePath;
                        preview.tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        return preview;
                    }
                }
            }

            if (_node.type == "Add" || _node.type == "Multiply")
            {
                const ShaderGraphPreviewValue a = ResolveShaderGraphPreviewValue(_document, _node.inputA, _depth + 1);
                const ShaderGraphPreviewValue b = ResolveShaderGraphPreviewValue(_document, _node.inputB, _depth + 1);

                if (_node.type == "Multiply")
                {
                    if (a.kind == ShaderGraphPreviewKind::TEXTURE && HasShaderGraphNumericPreview(b))
                    {
                        ShaderGraphPreviewValue preview = a;
                        preview.tint = GetShaderGraphPreviewTint(b);
                        return preview;
                    }

                    if (b.kind == ShaderGraphPreviewKind::TEXTURE && HasShaderGraphNumericPreview(a))
                    {
                        ShaderGraphPreviewValue preview = b;
                        preview.tint = GetShaderGraphPreviewTint(a);
                        return preview;
                    }
                }

                if (HasShaderGraphNumericPreview(a) && HasShaderGraphNumericPreview(b))
                {
                    const ShaderGraphValueType resultType = GetHigherShaderGraphValueType(a.numericType, b.numericType);
                    const Vector4 aValue = ConvertShaderGraphPreviewNumeric(a.numericValue, a.numericType, resultType);
                    const Vector4 bValue = ConvertShaderGraphPreviewNumeric(b.numericValue, b.numericType, resultType);
                    Vector4 resultValue = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

                    if (_node.type == "Add")
                    {
                        resultValue = Vector4(aValue.x + bValue.x, aValue.y + bValue.y, aValue.z + bValue.z, aValue.w + bValue.w);
                    }
                    else
                    {
                        resultValue = Vector4(aValue.x * bValue.x, aValue.y * bValue.y, aValue.z * bValue.z, aValue.w * bValue.w);
                    }

                    const bool preferColor = a.kind == ShaderGraphPreviewKind::COLOR || b.kind == ShaderGraphPreviewKind::COLOR;
                    return MakeShaderGraphNumericPreview(resultType, resultValue, preferColor);
                }

                return (a.kind != ShaderGraphPreviewKind::NONE) ? a : b;
            }

            if (_node.type == "Lerp")
            {
                const ShaderGraphPreviewValue a = ResolveShaderGraphPreviewValue(_document, _node.inputA, _depth + 1);
                const ShaderGraphPreviewValue b = ResolveShaderGraphPreviewValue(_document, _node.inputB, _depth + 1);
                const ShaderGraphPreviewValue t = ResolveShaderGraphPreviewValue(_document, _node.inputT, _depth + 1);

                if (HasShaderGraphNumericPreview(a) && HasShaderGraphNumericPreview(b) && HasShaderGraphNumericPreview(t))
                {
                    const ShaderGraphValueType resultType = GetHigherShaderGraphValueType(a.numericType, b.numericType);
                    const Vector4 aValue = ConvertShaderGraphPreviewNumeric(a.numericValue, a.numericType, resultType);
                    const Vector4 bValue = ConvertShaderGraphPreviewNumeric(b.numericValue, b.numericType, resultType);
                    const float mixValue = ConvertShaderGraphPreviewNumeric(t.numericValue, t.numericType, ShaderGraphValueType::FLOAT).x;
                    const Vector4 resultValue = Vector4(
                        aValue.x + (bValue.x - aValue.x) * mixValue,
                        aValue.y + (bValue.y - aValue.y) * mixValue,
                        aValue.z + (bValue.z - aValue.z) * mixValue,
                        aValue.w + (bValue.w - aValue.w) * mixValue);
                    const bool preferColor = a.kind == ShaderGraphPreviewKind::COLOR || b.kind == ShaderGraphPreviewKind::COLOR;
                    return MakeShaderGraphNumericPreview(resultType, resultValue, preferColor);
                }

                return (a.kind != ShaderGraphPreviewKind::NONE) ? a : b;
            }

            if (_node.type == "Sine")
            {
                const ShaderGraphPreviewValue input = ResolveShaderGraphPreviewValue(_document, _node.inputA, _depth + 1);
                if (HasShaderGraphNumericPreview(input))
                {
                    const Vector4 value = ConvertShaderGraphPreviewNumeric(input.numericValue, input.numericType, input.numericType);
                    const Vector4 resultValue = Vector4(std::sin(value.x), std::sin(value.y), std::sin(value.z), std::sin(value.w));
                    return MakeShaderGraphNumericPreview(input.numericType, resultValue, input.kind == ShaderGraphPreviewKind::COLOR);
                }
            }

            return {};
        }

        ShaderGraphPreviewValue ResolveShaderGraphPreviewValue(const ShaderGraphDocument &_document, const ShaderGraphLink &_link, int _depth)
        {
            if (!_link.IsValid() || _depth > 24)
                return {};

            const ShaderGraphNode *node = FindShaderGraphNode(_document, _link.nodeId);
            if (node == nullptr)
                return {};

            return ResolveShaderGraphPreviewValue(_document, *node, _depth + 1);
        }

        void DrawShaderGraphCheckerboard(ImDrawList *_drawList, const ImRect &_rect, float _cellSize, ImU32 _lightColor, ImU32 _darkColor, float _rounding)
        {
            _drawList->AddRectFilled(_rect.Min, _rect.Max, _darkColor, _rounding);

            const int cellCountX = std::max(1, static_cast<int>(std::ceil(_rect.GetWidth() / _cellSize)));
            const int cellCountY = std::max(1, static_cast<int>(std::ceil(_rect.GetHeight() / _cellSize)));
            for (int y = 0; y < cellCountY; ++y)
            {
                for (int x = 0; x < cellCountX; ++x)
                {
                    if (((x + y) & 1) != 0)
                        continue;

                    const ImVec2 min(_rect.Min.x + x * _cellSize, _rect.Min.y + y * _cellSize);
                    const ImVec2 max(std::min(min.x + _cellSize, _rect.Max.x), std::min(min.y + _cellSize, _rect.Max.y));
                    _drawList->AddRectFilled(min, max, _lightColor);
                }
            }
        }

        void DrawShaderGraphPreviewSphere(ImDrawList *_drawList, const ImRect &_rect, const ImVec4 &_tint)
        {
            const float radius = std::min(_rect.GetWidth(), _rect.GetHeight()) * 0.34f;
            const ImVec2 center((_rect.Min.x + _rect.Max.x) * 0.5f, _rect.Min.y + _rect.GetHeight() * 0.56f);

            for (int i = 0; i < 18; ++i)
            {
                const float t = static_cast<float>(i) / 17.0f;
                const float r = radius * (1.0f - t * 0.92f);
                const float light = 0.92f - t * 0.46f;
                const ImVec2 offset(-radius * 0.25f * t, -radius * 0.28f * t);
                const ImVec4 color(
                    std::clamp(_tint.x * light + 0.14f, 0.0f, 1.0f),
                    std::clamp(_tint.y * light + 0.14f, 0.0f, 1.0f),
                    std::clamp(_tint.z * light + 0.14f, 0.0f, 1.0f),
                    1.0f);
                _drawList->AddCircleFilled(
                    ImVec2(center.x + offset.x, center.y + offset.y),
                    r,
                    ImGui::ColorConvertFloat4ToU32(color),
                    48);
            }

            _drawList->AddCircle(center, radius, IM_COL32(16, 16, 16, 220), 64, 2.0f);
            _drawList->AddCircleFilled(ImVec2(center.x - radius * 0.35f, center.y - radius * 0.42f), radius * 0.16f, IM_COL32(255, 255, 255, 110), 24);
        }

        void DrawShaderGraphPreviewNode(
            ImDrawList *_drawList,
            const ImRect &_rect,
            const ShaderGraphPreviewValue &_preview,
            const std::string &_sourceLabel,
            float _zoom)
        {
            const float rounding = ScaleShaderGraphUi(8.0f, _zoom, 4.0f);
            const ImRect contentRect(
                ImVec2(_rect.Min.x + ScaleShaderGraphUi(10.0f, _zoom, 4.0f), _rect.Min.y + ScaleShaderGraphUi(8.0f, _zoom, 4.0f)),
                ImVec2(_rect.Max.x - ScaleShaderGraphUi(10.0f, _zoom, 4.0f), _rect.Max.y - ScaleShaderGraphUi(8.0f, _zoom, 4.0f)));
            const float footerHeight = ScaleShaderGraphUi(48.0f, _zoom, 26.0f);
            const ImRect previewRect(
                contentRect.Min,
                ImVec2(contentRect.Max.x, std::max(contentRect.Min.y + ScaleShaderGraphUi(32.0f, _zoom, 18.0f), contentRect.Max.y - footerHeight)));
            const ImRect footerRect(
                ImVec2(contentRect.Min.x, previewRect.Max.y + ScaleShaderGraphUi(8.0f, _zoom, 4.0f)),
                contentRect.Max);

            _drawList->AddRectFilled(previewRect.Min, previewRect.Max, IM_COL32(28, 33, 40, 255), rounding);
            _drawList->AddRect(previewRect.Min, previewRect.Max, IM_COL32(84, 96, 112, 255), rounding, 0, 1.0f);

            if (_preview.kind == ShaderGraphPreviewKind::TEXTURE && !_preview.texturePath.empty())
            {
                DrawShaderGraphCheckerboard(_drawList, previewRect, ScaleShaderGraphUi(12.0f, _zoom, 6.0f), IM_COL32(86, 92, 102, 255), IM_COL32(58, 64, 74, 255), rounding);
                if (TextureAsset *texture = AssetManager::GetTexture(_preview.texturePath))
                {
                    _drawList->AddImage(
                        (ImTextureID)(intptr_t)texture->GetGLTexture().id,
                        previewRect.Min,
                        previewRect.Max,
                        ImVec2(0.0f, 1.0f),
                        ImVec2(1.0f, 0.0f),
                        ImGui::ColorConvertFloat4ToU32(_preview.tint));
                }
            }
            else if (_preview.kind == ShaderGraphPreviewKind::COLOR ||
                     _preview.kind == ShaderGraphPreviewKind::VEC4 ||
                     _preview.kind == ShaderGraphPreviewKind::VEC3 ||
                     _preview.kind == ShaderGraphPreviewKind::VEC2 ||
                     _preview.kind == ShaderGraphPreviewKind::FLOAT)
            {
                DrawShaderGraphCheckerboard(_drawList, previewRect, ScaleShaderGraphUi(12.0f, _zoom, 6.0f), IM_COL32(84, 88, 96, 255), IM_COL32(54, 58, 66, 255), rounding);
                DrawShaderGraphPreviewSphere(_drawList, previewRect, GetShaderGraphPreviewTint(_preview));
            }
            else if (_preview.kind == ShaderGraphPreviewKind::UV)
            {
                DrawShaderGraphCheckerboard(_drawList, previewRect, ScaleShaderGraphUi(18.0f, _zoom, 8.0f), IM_COL32(72, 92, 118, 255), IM_COL32(40, 52, 68, 255), rounding);
                for (int i = 1; i < 5; ++i)
                {
                    const float x = ImLerp(previewRect.Min.x, previewRect.Max.x, i / 5.0f);
                    const float y = ImLerp(previewRect.Min.y, previewRect.Max.y, i / 5.0f);
                    _drawList->AddLine(ImVec2(x, previewRect.Min.y), ImVec2(x, previewRect.Max.y), IM_COL32(98, 146, 216, 180), 1.0f);
                    _drawList->AddLine(ImVec2(previewRect.Min.x, y), ImVec2(previewRect.Max.x, y), IM_COL32(98, 146, 216, 180), 1.0f);
                }
            }
            else if (_preview.kind == ShaderGraphPreviewKind::TIME)
            {
                _drawList->AddRectFilled(previewRect.Min, previewRect.Max, IM_COL32(52, 42, 72, 255), rounding);
                const ImVec2 center((previewRect.Min.x + previewRect.Max.x) * 0.5f, (previewRect.Min.y + previewRect.Max.y) * 0.5f);
                const float radius = std::min(previewRect.GetWidth(), previewRect.GetHeight()) * 0.3f;
                _drawList->AddCircle(center, radius, IM_COL32(210, 182, 255, 220), 48, 2.0f);
                _drawList->AddLine(center, ImVec2(center.x + radius * 0.6f, center.y - radius * 0.42f), IM_COL32(210, 182, 255, 220), 2.0f);
                _drawList->AddLine(center, ImVec2(center.x, center.y - radius * 0.72f), IM_COL32(210, 182, 255, 220), 2.0f);
            }
            else if (_preview.kind == ShaderGraphPreviewKind::POSITION)
            {
                _drawList->AddRectFilledMultiColor(previewRect.Min, previewRect.Max,
                    IM_COL32(46, 96, 84, 255), IM_COL32(82, 132, 98, 255), IM_COL32(28, 62, 54, 255), IM_COL32(38, 74, 68, 255));
                const ImVec2 center((previewRect.Min.x + previewRect.Max.x) * 0.5f, (previewRect.Min.y + previewRect.Max.y) * 0.5f);
                const float axis = std::min(previewRect.GetWidth(), previewRect.GetHeight()) * 0.28f;
                _drawList->AddLine(center, ImVec2(center.x + axis, center.y), IM_COL32(228, 86, 86, 220), 2.0f);
                _drawList->AddLine(center, ImVec2(center.x, center.y - axis), IM_COL32(90, 220, 110, 220), 2.0f);
                _drawList->AddLine(center, ImVec2(center.x - axis * 0.6f, center.y + axis * 0.5f), IM_COL32(96, 156, 255, 220), 2.0f);
            }
            else if (_preview.kind == ShaderGraphPreviewKind::NORMAL)
            {
                DrawShaderGraphCheckerboard(_drawList, previewRect, ScaleShaderGraphUi(14.0f, _zoom, 7.0f), IM_COL32(80, 102, 128, 255), IM_COL32(54, 68, 86, 255), rounding);
                DrawShaderGraphPreviewSphere(_drawList, previewRect, ImVec4(0.48f, 0.76f, 1.0f, 1.0f));
            }
            else
            {
                _drawList->AddRectFilled(previewRect.Min, previewRect.Max, IM_COL32(36, 40, 48, 255), rounding);
                ImVec2 cursor(previewRect.Min.x + ScaleShaderGraphUi(12.0f, _zoom, 5.0f), previewRect.Min.y + ScaleShaderGraphUi(12.0f, _zoom, 5.0f));
                DrawShaderGraphNodeText(_drawList, previewRect, cursor, "Connect a node output to preview it here.", IM_COL32(210, 216, 226, 255), _zoom);
            }

            _drawList->AddRectFilled(footerRect.Min, footerRect.Max, IM_COL32(20, 24, 30, 220), rounding);
            _drawList->AddRect(footerRect.Min, footerRect.Max, IM_COL32(74, 82, 94, 255), rounding, 0, 1.0f);

            ImVec2 footerCursor(footerRect.Min.x + ScaleShaderGraphUi(10.0f, _zoom, 4.0f), footerRect.Min.y + ScaleShaderGraphUi(7.0f, _zoom, 3.0f));
            DrawShaderGraphNodeText(_drawList, footerRect, footerCursor, GetShaderGraphPreviewSummary(_preview), IM_COL32(230, 236, 244, 255), _zoom);
            DrawShaderGraphNodeText(_drawList, footerRect, footerCursor, "Source: " + _sourceLabel, IM_COL32(164, 174, 188, 255), _zoom);
        }

        GraphEditor::Template GetShaderGraphTemplate(size_t _index)
        {
            switch (_index)
            {
                case ShaderGraphTemplate_Float:
                    return MakeTemplate(IM_COL32(108, 82, 140, 255), IM_COL32(62, 46, 90, 255), IM_COL32(76, 58, 105, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputFloatColors);
                case ShaderGraphTemplate_Vector2:
                    return MakeTemplate(IM_COL32(62, 110, 176, 255), IM_COL32(40, 72, 114, 255), IM_COL32(48, 84, 130, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputVec2Colors);
                case ShaderGraphTemplate_Vector3:
                    return MakeTemplate(IM_COL32(52, 122, 102, 255), IM_COL32(34, 82, 68, 255), IM_COL32(40, 96, 78, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputVec3Colors);
                case ShaderGraphTemplate_Vector4:
                    return MakeTemplate(IM_COL32(164, 116, 48, 255), IM_COL32(104, 72, 30, 255), IM_COL32(122, 84, 36, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputVec4Colors);
                case ShaderGraphTemplate_Color:
                    return MakeTemplate(IM_COL32(188, 142, 42, 255), IM_COL32(112, 84, 24, 255), IM_COL32(128, 96, 30, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputVec4Colors);
                case ShaderGraphTemplate_UV:
                    return MakeTemplate(IM_COL32(54, 98, 176, 255), IM_COL32(32, 60, 110, 255), IM_COL32(38, 72, 126, 255), 0, nullptr, nullptr, 1, kUvOutputNames, kSingleOutputVec2Colors);
                case ShaderGraphTemplate_Time:
                    return MakeTemplate(IM_COL32(118, 88, 164, 255), IM_COL32(70, 52, 98, 255), IM_COL32(84, 62, 116, 255), 0, nullptr, nullptr, 1, kTimeOutputNames, kSingleOutputFloatColors);
                case ShaderGraphTemplate_Position:
                    return MakeTemplate(IM_COL32(56, 150, 114, 255), IM_COL32(32, 84, 66, 255), IM_COL32(38, 96, 76, 255), 0, nullptr, nullptr, 1, kPositionOutputNames, kSingleOutputVec3Colors);
                case ShaderGraphTemplate_Normal:
                    return MakeTemplate(IM_COL32(72, 160, 138, 255), IM_COL32(36, 88, 78, 255), IM_COL32(42, 102, 90, 255), 0, nullptr, nullptr, 1, kNormalOutputNames, kSingleOutputVec3Colors);
                case ShaderGraphTemplate_PropertyFloat:
                    return MakeTemplate(IM_COL32(150, 112, 206, 255), IM_COL32(56, 48, 72, 255), IM_COL32(68, 58, 88, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputFloatColors);
                case ShaderGraphTemplate_PropertyVector2:
                    return MakeTemplate(IM_COL32(80, 144, 210, 255), IM_COL32(42, 58, 74, 255), IM_COL32(50, 70, 90, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputVec2Colors);
                case ShaderGraphTemplate_PropertyVector3:
                    return MakeTemplate(IM_COL32(72, 180, 132, 255), IM_COL32(42, 66, 58, 255), IM_COL32(50, 80, 68, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputVec3Colors);
                case ShaderGraphTemplate_PropertyVector4:
                    return MakeTemplate(IM_COL32(210, 152, 70, 255), IM_COL32(72, 58, 42, 255), IM_COL32(88, 70, 50, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputVec4Colors);
                case ShaderGraphTemplate_PropertyColor:
                    return MakeTemplate(IM_COL32(224, 174, 76, 255), IM_COL32(76, 62, 38, 255), IM_COL32(94, 76, 46, 255), 0, nullptr, nullptr, 1, kSingleOutputName, kSingleOutputVec4Colors);
                case ShaderGraphTemplate_PropertyTexture2D:
                    return MakeTemplate(IM_COL32(220, 116, 76, 255), IM_COL32(76, 48, 40, 255), IM_COL32(92, 58, 48, 255), 1, kTextureUvInputNames, kTextureInputColors, 1, kTextureOutputNames, kTextureOutputColors);
                case ShaderGraphTemplate_Texture2D:
                    return MakeTemplate(IM_COL32(182, 148, 46, 255), IM_COL32(106, 86, 26, 255), IM_COL32(124, 100, 30, 255), 1, kTextureUvInputNames, kTextureInputColors, 1, kTextureOutputNames, kTextureOutputColors);
                case ShaderGraphTemplate_Add:
                    return MakeTemplate(IM_COL32(142, 78, 62, 255), IM_COL32(84, 44, 34, 255), IM_COL32(100, 52, 40, 255), 2, kMathInputNames, kMathInputColors, 1, kMathOutputNames, kMathOutputColors);
                case ShaderGraphTemplate_Multiply:
                    return MakeTemplate(IM_COL32(112, 72, 148, 255), IM_COL32(66, 40, 88, 255), IM_COL32(82, 50, 106, 255), 2, kMathInputNames, kMathInputColors, 1, kMathOutputNames, kMathOutputColors);
                case ShaderGraphTemplate_Sine:
                    return MakeTemplate(IM_COL32(64, 126, 118, 255), IM_COL32(38, 74, 70, 255), IM_COL32(46, 88, 84, 255), 1, kSingleInputName, kSingleInputColors, 1, kSingleOutputName, kMathOutputColors);
                case ShaderGraphTemplate_Lerp:
                    return MakeTemplate(IM_COL32(148, 94, 56, 255), IM_COL32(86, 54, 32, 255), IM_COL32(100, 64, 38, 255), 3, kLerpInputNames, kMathInputColors, 1, kMathOutputNames, kMathOutputColors);
                case ShaderGraphTemplate_Panner:
                    return MakeTemplate(IM_COL32(58, 116, 164, 255), IM_COL32(36, 68, 96, 255), IM_COL32(42, 82, 110, 255), 2, kPannerInputNames, kPannerInputColors, 1, kUvOutputNames, kSingleOutputVec2Colors);
                case ShaderGraphTemplate_StickyNote:
                    return MakeTemplate(IM_COL32(238, 190, 78, 255), IM_COL32(112, 86, 34, 255), IM_COL32(132, 102, 42, 255), 0, nullptr, nullptr, 0, nullptr, nullptr);
                case ShaderGraphTemplate_Preview:
                    return MakeTemplate(IM_COL32(102, 118, 136, 255), IM_COL32(42, 48, 56, 255), IM_COL32(52, 60, 70, 255), 1, kPreviewInputNames, kSingleInputColors, 0, nullptr, nullptr);
                case ShaderGraphTemplate_VertexOutput:
                    return MakeTemplate(IM_COL32(56, 128, 116, 255), IM_COL32(30, 72, 66, 255), IM_COL32(38, 88, 80, 255), 2, kVertexOutputInputNames, kVertexOutputInputColors, 0, nullptr, nullptr);
                case ShaderGraphTemplate_Output:
                    return MakeTemplate(IM_COL32(54, 132, 74, 255), IM_COL32(30, 74, 40, 255), IM_COL32(40, 90, 52, 255), 2, kOutputInputNames, kOutputInputColors, 0, nullptr, nullptr);
                default:
                    return MakeTemplate(IM_COL32(96, 96, 96, 255), IM_COL32(60, 60, 60, 255), IM_COL32(72, 72, 72, 255), 0, nullptr, nullptr, 0, nullptr, nullptr);
            }
        }

        size_t GetShaderGraphTemplateIndex(const ShaderGraphNode &_node)
        {
            if (_node.type == "Property")
            {
                switch (_node.propertyType)
                {
                    case ShaderGraphPropertyType::FLOAT: return ShaderGraphTemplate_PropertyFloat;
                    case ShaderGraphPropertyType::VEC2: return ShaderGraphTemplate_PropertyVector2;
                    case ShaderGraphPropertyType::VEC3: return ShaderGraphTemplate_PropertyVector3;
                    case ShaderGraphPropertyType::VEC4: return ShaderGraphTemplate_PropertyVector4;
                    case ShaderGraphPropertyType::COLOR: return ShaderGraphTemplate_PropertyColor;
                    case ShaderGraphPropertyType::TEXTURE: return ShaderGraphTemplate_PropertyTexture2D;
                }
            }
            if (_node.type == "Float") return ShaderGraphTemplate_Float;
            if (_node.type == "Vector2") return ShaderGraphTemplate_Vector2;
            if (_node.type == "Vector3") return ShaderGraphTemplate_Vector3;
            if (_node.type == "Vector4") return ShaderGraphTemplate_Vector4;
            if (_node.type == "Color") return ShaderGraphTemplate_Color;
            if (_node.type == "UV") return ShaderGraphTemplate_UV;
            if (_node.type == "Time") return ShaderGraphTemplate_Time;
            if (_node.type == "Position") return ShaderGraphTemplate_Position;
            if (_node.type == "Normal") return ShaderGraphTemplate_Normal;
            if (_node.type == "Texture2D") return ShaderGraphTemplate_Texture2D;
            if (_node.type == "Add") return ShaderGraphTemplate_Add;
            if (_node.type == "Multiply") return ShaderGraphTemplate_Multiply;
            if (_node.type == "Sine") return ShaderGraphTemplate_Sine;
            if (_node.type == "Lerp") return ShaderGraphTemplate_Lerp;
            if (_node.type == "Panner") return ShaderGraphTemplate_Panner;
            if (_node.type == "StickyNote") return ShaderGraphTemplate_StickyNote;
            if (_node.type == "Preview") return ShaderGraphTemplate_Preview;
            return ShaderGraphTemplate_Color;
        }

        ImVec2 GetShaderGraphNodeSize(const ShaderGraphNode &_node)
        {
            const float baseWidth = std::max(240.0f, ImGui::GetFontSize() * 12.0f);
            const float baseHeight = std::max(132.0f, ImGui::GetFontSize() * 6.2f);

            if (_node.type == "Texture2D")
                return ImVec2(baseWidth + 38.0f, baseHeight + 34.0f);
            if (_node.type == "Property" && _node.propertyType == ShaderGraphPropertyType::TEXTURE)
                return ImVec2(baseWidth + 38.0f, baseHeight + 34.0f);
            if (_node.type == "Lerp")
                return ImVec2(baseWidth + 18.0f, baseHeight + 26.0f);
            if (_node.type == "Panner")
                return ImVec2(baseWidth + 28.0f, baseHeight + 18.0f);
            if (_node.type == "Color")
                return ImVec2(baseWidth + 12.0f, baseHeight + 10.0f);
            if (_node.type == "StickyNote")
                return ImVec2(std::max(280.0f, ImGui::GetFontSize() * 14.0f), std::max(176.0f, ImGui::GetFontSize() * 8.0f));
            if (_node.type == "Preview")
                return ImVec2(std::max(260.0f, ImGui::GetFontSize() * 12.5f), std::max(220.0f, ImGui::GetFontSize() * 10.5f));

            return ImVec2(baseWidth, baseHeight);
        }

        std::string DescribeShaderGraphLink(const ShaderGraphDocument &_document, const ShaderGraphLink &_link)
        {
            if (!_link.IsValid())
                return "[ none ]";

            const ShaderGraphNode *node = FindShaderGraphNode(_document, _link.nodeId);
            if (node == nullptr)
                return "[ missing ]";

            return std::string(GetShaderGraphNodeDisplayName(node->type)) + " #" + std::to_string(node->id) + "." + _link.slot;
        }

        std::string GetTexturePathLabel(const std::string &_path)
        {
            if (_path.empty())
                return "[ none ]";

            return std::filesystem::path(_path).stem().string();
        }

        const char *GetShaderGraphNodeTitleTooltip(const ShaderGraphNode &_node)
        {
            if (_node.type == "UV")
                return "Mesh UV coordinates for sampling textures and scrolling effects.";
            if (_node.type == "Time")
                return "A running time value in seconds. Useful for animation and procedural motion.";
            if (_node.type == "Position")
                return "Current world-space position. In the vertex stage it is the editable vertex position, and in the fragment stage it is the interpolated world position.";
            if (_node.type == "Normal")
                return "Current world-space normal. In the vertex stage it is the editable vertex normal, and in the fragment stage it is the interpolated surface normal.";
            if (_node.type == "Texture2D")
                return "Samples a texture using the UV input. If no UV is connected, it uses the mesh UVs.";
            if (_node.type == "Property")
                return "References a blackboard property so the material can override this value per asset or per instance.";
            if (_node.type == "Panner")
                return "Adds time-based motion to UVs so textures can scroll without changing the graph.";
            if (_node.type == "StickyNote")
                return "A documentation note for organizing and explaining the graph. It does not affect the shader.";
            if (_node.type == "Preview")
                return "Shows a quick visual approximation of the connected value so you can inspect the graph while editing.";
            if (_node.type == "VertexOutput")
                return "Final vertex-stage outputs. Connect world-space position and normal here to deform the mesh before rasterization.";
            if (_node.type == "Output")
                return "Final fragment outputs. Connect color and alpha here to generate the shader.";
            if (_node.type == "Add")
                return "Adds A and B together.";
            if (_node.type == "Multiply")
                return "Multiplies A and B together.";
            if (_node.type == "Sine")
                return "Applies sine to the input value.";
            if (_node.type == "Lerp")
                return "Blends between A and B using T.";
            return nullptr;
        }

        std::string GetGeneratedShaderBasePath(const std::string &_graphPath)
        {
            std::filesystem::path vertexPath(GetShaderGraphGeneratedVertexPath(_graphPath));
            vertexPath.replace_extension();
            return vertexPath.generic_string();
        }

        struct ShaderGraphResolvedLink
        {
            GraphEditor::Link graphLink = {};
            bool targetsOutput = false;
            GraphEditor::NodeIndex targetNodeIndex = kInvalidGraphNodeIndex;
            std::string targetPinName = {};
        };

        class ShaderGraphGraphDelegate final : public GraphEditor::Delegate
        {
        public:
            ShaderGraphGraphDelegate(
                ShaderGraphDocument &_document,
                int &_selectedNodeId,
                bool &_documentChanged,
                bool &_semanticChanged)
                : m_document(_document)
                , m_selectedNodeId(_selectedNodeId)
                , m_documentChanged(_documentChanged)
                , m_semanticChanged(_semanticChanged)
            {
            }

            bool AllowedLink(GraphEditor::NodeIndex _from, GraphEditor::NodeIndex _to) override
            {
                return _from != _to && !IsSpecialOutputNodeIndex(_from);
            }

            void SelectNode(GraphEditor::NodeIndex _nodeIndex, bool _selected) override
            {
                if (!_selected)
                {
                    if (IsFragmentOutputNodeIndex(_nodeIndex) && m_selectedNodeId == kShaderGraphFragmentOutputSelectionId)
                        m_selectedNodeId = -1;
                    else if (IsVertexOutputNodeIndex(_nodeIndex) && m_selectedNodeId == kShaderGraphVertexOutputSelectionId)
                        m_selectedNodeId = -1;

                    ShaderGraphNode *node = GetNodeByIndex(_nodeIndex);
                    if (node != nullptr && m_selectedNodeId == node->id)
                        m_selectedNodeId = -1;
                    return;
                }

                if (IsFragmentOutputNodeIndex(_nodeIndex))
                    m_selectedNodeId = kShaderGraphFragmentOutputSelectionId;
                else if (IsVertexOutputNodeIndex(_nodeIndex))
                    m_selectedNodeId = kShaderGraphVertexOutputSelectionId;
                else if (ShaderGraphNode *node = GetNodeByIndex(_nodeIndex))
                    m_selectedNodeId = node->id;
            }

            void MoveSelectedNodes(const ImVec2 _delta) override
            {
                if (m_selectedNodeId < 0)
                    return;

                if (ShaderGraphNode *node = FindShaderGraphNode(m_document, m_selectedNodeId))
                {
                    node->position.x += _delta.x;
                    node->position.y += _delta.y;
                    m_documentChanged = true;
                }
            }

            void AddLink(GraphEditor::NodeIndex _sourceNodeIndex, GraphEditor::SlotIndex _sourceOutputSlotIndex, GraphEditor::NodeIndex _targetNodeIndex, GraphEditor::SlotIndex _targetInputSlotIndex) override
            {
                // ImGuizmo's GraphEditor stores the source/output-side node in the
                // fields named "input" and the target/input-side node in the fields
                // named "output". We match that convention here so link drawing and
                // hit-testing stay consistent with the widget internals.
                if (IsSpecialOutputNodeIndex(_sourceNodeIndex))
                    return;

                const ShaderGraphNode *sourceNode = GetNodeByIndex(_sourceNodeIndex);
                if (sourceNode == nullptr)
                    return;

                const std::vector<ShaderGraphPinInfo> sourceOutputs = GetShaderGraphNodeOutputPins(*sourceNode);
                if (_sourceOutputSlotIndex >= sourceOutputs.size())
                    return;

                ShaderGraphLink link;
                link.nodeId = sourceNode->id;
                link.slot = sourceOutputs[_sourceOutputSlotIndex].name;

                if (IsFragmentOutputNodeIndex(_targetNodeIndex))
                {
                    if (_targetInputSlotIndex == 0u)
                        m_document.outputColor = link;
                    else if (_targetInputSlotIndex == 1u)
                        m_document.outputAlpha = link;
                    else
                        return;
                }
                else if (IsVertexOutputNodeIndex(_targetNodeIndex))
                {
                    if (_targetInputSlotIndex == 0u)
                        m_document.vertexPosition = link;
                    else if (_targetInputSlotIndex == 1u)
                        m_document.vertexNormal = link;
                    else
                        return;
                }
                else
                {
                    ShaderGraphNode *targetNode = GetNodeByIndex(_targetNodeIndex);
                    if (targetNode == nullptr)
                        return;

                    const std::vector<ShaderGraphPinInfo> inputPins = GetShaderGraphNodeInputPins(*targetNode);
                    if (_targetInputSlotIndex >= inputPins.size())
                        return;

                    if (!SetShaderGraphNodeInputLink(*targetNode, inputPins[_targetInputSlotIndex].name, link))
                        return;
                }

                m_documentChanged = true;
                m_semanticChanged = true;
            }

            void DelLink(GraphEditor::LinkIndex _linkIndex) override
            {
                const std::vector<ShaderGraphResolvedLink> links = BuildLinks();
                if (_linkIndex >= links.size())
                    return;

                const ShaderGraphResolvedLink &resolved = links[_linkIndex];
                if (resolved.targetsOutput)
                {
                    if (resolved.targetNodeIndex == GetFragmentOutputNodeIndex())
                    {
                        if (resolved.graphLink.mOutputSlotIndex == 0u)
                            m_document.outputColor = {};
                        else if (resolved.graphLink.mOutputSlotIndex == 1u)
                            m_document.outputAlpha = {};
                    }
                    else if (resolved.targetNodeIndex == GetVertexOutputNodeIndex())
                    {
                        if (resolved.graphLink.mOutputSlotIndex == 0u)
                            m_document.vertexPosition = {};
                        else if (resolved.graphLink.mOutputSlotIndex == 1u)
                            m_document.vertexNormal = {};
                    }
                }
                else
                {
                    ShaderGraphNode *targetNode = GetNodeByIndex(resolved.targetNodeIndex);
                    if (targetNode != nullptr)
                        (void)ClearShaderGraphNodeInputLink(*targetNode, resolved.targetPinName);
                }

                m_documentChanged = true;
                m_semanticChanged = true;
            }

            void DropLink(GraphEditor::NodeIndex _nodeIndex, GraphEditor::SlotIndex _slotIndex, bool _startedFromInput, const ImVec2 &_graphPos) override
            {
                if (IsSpecialOutputNodeIndex(_nodeIndex))
                    return;

                const ShaderGraphNode *node = GetNodeByIndex(_nodeIndex);
                if (node == nullptr)
                    return;

                const std::vector<ShaderGraphPinInfo> pins = _startedFromInput ? GetShaderGraphNodeInputPins(*node) : GetShaderGraphNodeOutputPins(*node);
                if (_slotIndex >= pins.size())
                    return;

                m_pendingLinkCreate.valid = true;
                m_pendingLinkCreate.startedFromInput = _startedFromInput;
                m_pendingLinkCreate.nodeId = node->id;
                m_pendingLinkCreate.slot = pins[_slotIndex].name;
                m_pendingLinkCreate.graphPosition = Vector2(_graphPos.x, _graphPos.y);

                m_contextNodeIndex = kInvalidGraphNodeIndex;
                m_contextInputSlotIndex = kInvalidGraphSlotIndex;
                m_contextOutputSlotIndex = kInvalidGraphSlotIndex;
                m_contextMenuRequested = true;
                m_contextMenuScreenPos = ImGui::GetMousePos();
            }

            void CustomDraw(ImDrawList *_drawList, ImRect _rectangle, GraphEditor::NodeIndex _nodeIndex, float _zoom) override
            {
                ImVec2 cursor(_rectangle.Min.x + ScaleShaderGraphUi(8.0f, _zoom, 4.0f), _rectangle.Min.y + ScaleShaderGraphUi(6.0f, _zoom, 3.0f));

                if (IsVertexOutputNodeIndex(_nodeIndex))
                {
                    DrawShaderGraphNodeText(_drawList, _rectangle, cursor, "Final vertex stage", IM_COL32(228, 245, 240, 255), _zoom);
                    DrawShaderGraphNodeText(_drawList, _rectangle, cursor, "World Position and Normal", IM_COL32(186, 220, 210, 255), _zoom);
                    return;
                }

                if (IsFragmentOutputNodeIndex(_nodeIndex))
                {
                    DrawShaderGraphNodeText(_drawList, _rectangle, cursor, "Final fragment color", IM_COL32(230, 245, 232, 255), _zoom);
                    DrawShaderGraphNodeText(_drawList, _rectangle, cursor, "Color and Alpha inputs", IM_COL32(190, 220, 196, 255), _zoom);
                    return;
                }

                const ShaderGraphNode *node = GetNodeByIndex(_nodeIndex);
                if (node == nullptr)
                    return;

                auto drawLine = [&](const std::string &_text, ImU32 _color) -> void
                {
                    (void)DrawShaderGraphNodeText(_drawList, _rectangle, cursor, _text, _color, _zoom);
                };

                if (node->type == "Property")
                {
                    const ShaderGraphProperty *property = FindShaderGraphProperty(m_document, node->propertyId);
                    const std::string name = property != nullptr ? property->name : "[ missing property ]";
                    const ShaderGraphPropertyType type = property != nullptr ? property->type : node->propertyType;

                    drawLine(name, IM_COL32(250, 240, 206, 255));
                    drawLine(std::string("Uniform: ") + GetShaderGraphPropertyTypeDisplayName(type), IM_COL32(210, 216, 226, 255));

                    if (property != nullptr && type == ShaderGraphPropertyType::COLOR)
                    {
                        const ImVec2 swatchMin = cursor;
                        const ImVec2 swatchMax(cursor.x + 54.0f, cursor.y + 20.0f);
                        const ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(property->colorValue.r, property->colorValue.g, property->colorValue.b, property->colorValue.a));
                        _drawList->AddRectFilled(swatchMin, swatchMax, color, 4.0f);
                        _drawList->AddRect(swatchMin, swatchMax, IM_COL32(20, 20, 20, 255), 4.0f);
                        cursor.y += 26.0f;
                    }
                    else if (property != nullptr && type == ShaderGraphPropertyType::TEXTURE)
                    {
                        drawLine("Texture: " + GetTexturePathLabel(property->texturePath), IM_COL32(255, 224, 190, 255));
                    }
                    else if (property != nullptr && type == ShaderGraphPropertyType::FLOAT)
                    {
                        drawLine("Default: " + FormatShaderGraphFloat(property->floatValue), IM_COL32(226, 218, 255, 255));
                    }
                    else if (property != nullptr && type == ShaderGraphPropertyType::VEC2)
                    {
                        drawLine("Default: " + FormatShaderGraphVector2(property->vec2Value), IM_COL32(210, 228, 255, 255));
                    }
                    else if (property != nullptr && type == ShaderGraphPropertyType::VEC3)
                    {
                        drawLine("Default: " + FormatShaderGraphVector3(property->vec3Value), IM_COL32(208, 242, 226, 255));
                    }
                    else if (property != nullptr && type == ShaderGraphPropertyType::VEC4)
                    {
                        drawLine("Default: " + FormatShaderGraphVector4(property->vec4Value), IM_COL32(255, 232, 196, 255));
                    }
                }
                else if (node->type == "Float")
                    drawLine("Value: " + FormatShaderGraphFloat(node->floatValue), IM_COL32(230, 220, 255, 255));
                else if (node->type == "Vector2")
                    drawLine("XY: " + FormatShaderGraphVector2(node->vec2Value), IM_COL32(210, 228, 255, 255));
                else if (node->type == "Vector3")
                    drawLine("XYZ: " + FormatShaderGraphVector3(node->vec3Value), IM_COL32(208, 242, 226, 255));
                else if (node->type == "Vector4")
                    drawLine("XYZW: " + FormatShaderGraphVector4(node->vec4Value), IM_COL32(255, 232, 196, 255));
                else if (node->type == "Color")
                {
                    const ImVec2 swatchMin = cursor;
                    const ImVec2 swatchMax(cursor.x + 48.0f, cursor.y + 18.0f);
                    const ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(node->colorValue.r, node->colorValue.g, node->colorValue.b, node->colorValue.a));
                    _drawList->AddRectFilled(swatchMin, swatchMax, color, 4.0f);
                    _drawList->AddRect(swatchMin, swatchMax, IM_COL32(20, 20, 20, 255), 4.0f);
                    cursor.y += 24.0f;
                    drawLine("RGBA: " + FormatShaderGraphVector4(node->colorValue), IM_COL32(255, 232, 196, 255));
                }
                else if (node->type == "Texture2D")
                {
                    drawLine("Texture: " + GetTexturePathLabel(node->texturePath), IM_COL32(255, 239, 188, 255));
                }
                else if (node->type == "Panner")
                {
                    drawLine("Speed: " + FormatShaderGraphVector2(node->speedValue), IM_COL32(206, 228, 255, 255));
                }
                else if (node->type == "Position")
                {
                    drawLine("World position", IM_COL32(206, 242, 224, 255));
                }
                else if (node->type == "Normal")
                {
                    drawLine("World normal", IM_COL32(206, 242, 224, 255));
                }
                else if (node->type == "StickyNote")
                {
                    drawLine(node->title.empty() ? "Note" : node->title, IM_COL32(255, 242, 196, 255));
                    drawLine(node->text.empty() ? "Explain a graph section here." : node->text, IM_COL32(38, 30, 12, 255));
                }
                else if (node->type == "Preview")
                {
                    const ShaderGraphLink previewLink = GetShaderGraphNodeInputLink(*node, "input");
                    const ShaderGraphPreviewValue previewValue = ResolveShaderGraphPreviewValue(m_document, previewLink);
                    DrawShaderGraphPreviewNode(_drawList, _rectangle, previewValue, DescribeShaderGraphLink(m_document, previewLink), _zoom);
                }
                else if (node->type == "UV")
                {
                    drawLine("Mesh UVs", IM_COL32(210, 228, 255, 255));
                }
                else if (node->type == "Time")
                {
                    drawLine("TIME uniform", IM_COL32(228, 214, 255, 255));
                }
                else if (node->type == "Add")
                {
                    drawLine("A + B", IM_COL32(240, 220, 210, 255));
                }
                else if (node->type == "Multiply")
                {
                    drawLine("A * B", IM_COL32(232, 214, 255, 255));
                }
                else if (node->type == "Sine")
                {
                    drawLine("sin(In)", IM_COL32(212, 236, 232, 255));
                }
                else if (node->type == "Lerp")
                {
                    drawLine("mix(A, B, T)", IM_COL32(250, 226, 206, 255));
                }
            }

            const char *GetNodeTitleTooltip(GraphEditor::NodeIndex _nodeIndex) override
            {
                if (IsVertexOutputNodeIndex(_nodeIndex))
                    return "Final vertex outputs. Connect world position and normal here.";
                if (IsFragmentOutputNodeIndex(_nodeIndex))
                    return "Final fragment outputs. Connect color and alpha here to generate the shader.";

                const ShaderGraphNode *node = GetNodeByIndex(_nodeIndex);
                if (node == nullptr)
                    return nullptr;

                return GetShaderGraphNodeTitleTooltip(*node);
            }

            void RightClick(GraphEditor::NodeIndex _nodeIndex, GraphEditor::SlotIndex _slotIndexInput, GraphEditor::SlotIndex _slotIndexOutput) override
            {
                m_contextNodeIndex = _nodeIndex;
                m_contextInputSlotIndex = _slotIndexInput;
                m_contextOutputSlotIndex = _slotIndexOutput;
                m_contextMenuRequested = true;
                m_contextMenuScreenPos = ImGui::GetMousePos();
            }

            const size_t GetTemplateCount() override
            {
                return ShaderGraphTemplate_Count;
            }

            const GraphEditor::Template GetTemplate(GraphEditor::TemplateIndex _index) override
            {
                return GetShaderGraphTemplate(_index);
            }

            const size_t GetNodeCount() override
            {
                return m_document.nodes.size() + 2u;
            }

            const GraphEditor::Node GetNode(GraphEditor::NodeIndex _index) override
            {
                if (IsVertexOutputNodeIndex(_index))
                {
                    const ImRect outputRect = GetVertexOutputNodeRect();
                    return GraphEditor::Node{ "Vertex Output", ShaderGraphTemplate_VertexOutput, outputRect, m_selectedNodeId == kShaderGraphVertexOutputSelectionId };
                }

                if (IsFragmentOutputNodeIndex(_index))
                {
                    const ImRect outputRect = GetFragmentOutputNodeRect();
                    return GraphEditor::Node{ "Output", ShaderGraphTemplate_Output, outputRect, m_selectedNodeId == kShaderGraphFragmentOutputSelectionId };
                }

                const ShaderGraphNode *node = GetNodeByIndex(_index);
                if (node == nullptr)
                    return GraphEditor::Node{ "Invalid", ShaderGraphTemplate_Color, ImRect(ImVec2(0.0f, 0.0f), ImVec2(220.0f, 132.0f)), false };

                const ImVec2 size = GetShaderGraphNodeSize(*node);
                return GraphEditor::Node
                {
                    GetShaderGraphNodeDisplayName(node->type),
                    GetShaderGraphTemplateIndex(*node),
                    ImRect(ImVec2(node->position.x, node->position.y), ImVec2(node->position.x + size.x, node->position.y + size.y)),
                    m_selectedNodeId == node->id
                };
            }

            const size_t GetLinkCount() override
            {
                return BuildLinks().size();
            }

            const GraphEditor::Link GetLink(GraphEditor::LinkIndex _index) override
            {
                return BuildLinks()[_index].graphLink;
            }

            GraphEditor::NodeIndex GetContextNodeIndex() const
            {
                return m_contextNodeIndex;
            }

            bool ConsumeContextMenuRequest(ImVec2 &_screenPos)
            {
                if (!m_contextMenuRequested)
                    return false;

                _screenPos = m_contextMenuScreenPos;
                m_contextMenuRequested = false;
                return true;
            }

            bool ConsumePendingLinkCreate(ShaderGraphPendingLinkCreate &_pendingLinkCreate)
            {
                if (!m_pendingLinkCreate.valid)
                    return false;

                _pendingLinkCreate = m_pendingLinkCreate;
                m_pendingLinkCreate = {};
                return true;
            }

        private:
            GraphEditor::NodeIndex GetVertexOutputNodeIndex() const
            {
                return m_document.nodes.size();
            }

            GraphEditor::NodeIndex GetFragmentOutputNodeIndex() const
            {
                return m_document.nodes.size() + 1u;
            }

            bool IsVertexOutputNodeIndex(GraphEditor::NodeIndex _index) const
            {
                return _index == GetVertexOutputNodeIndex();
            }

            bool IsFragmentOutputNodeIndex(GraphEditor::NodeIndex _index) const
            {
                return _index == GetFragmentOutputNodeIndex();
            }

            bool IsSpecialOutputNodeIndex(GraphEditor::NodeIndex _index) const
            {
                return IsVertexOutputNodeIndex(_index) || IsFragmentOutputNodeIndex(_index);
            }

            ShaderGraphNode *GetNodeByIndex(GraphEditor::NodeIndex _index)
            {
                if (_index >= m_document.nodes.size())
                    return nullptr;
                return &m_document.nodes[_index];
            }

            const ShaderGraphNode *GetNodeByIndex(GraphEditor::NodeIndex _index) const
            {
                if (_index >= m_document.nodes.size())
                    return nullptr;
                return &m_document.nodes[_index];
            }

            GraphEditor::NodeIndex FindNodeIndexById(int _nodeId) const
            {
                for (GraphEditor::NodeIndex index = 0; index < m_document.nodes.size(); ++index)
                {
                    if (m_document.nodes[index].id == _nodeId)
                        return index;
                }

                return kInvalidGraphNodeIndex;
            }

            ImRect GetVertexOutputNodeRect() const
            {
                float maxX = 420.0f;
                float minY = 80.0f;
                for (const ShaderGraphNode &node : m_document.nodes)
                {
                    const ImVec2 size = GetShaderGraphNodeSize(node);
                    maxX = std::max(maxX, node.position.x + size.x);
                    minY = std::min(minY, node.position.y);
                }

                const ImVec2 min(maxX + 180.0f, std::max(48.0f, minY));
                const float outputWidth = std::max(300.0f, ImGui::GetFontSize() * 13.0f);
                const float outputHeight = std::max(128.0f, ImGui::GetFontSize() * 5.8f);
                const ImVec2 max(min.x + outputWidth, min.y + outputHeight);
                return ImRect(min, max);
            }

            ImRect GetFragmentOutputNodeRect() const
            {
                const ImRect vertexRect = GetVertexOutputNodeRect();
                const float outputWidth = std::max(280.0f, ImGui::GetFontSize() * 12.5f);
                const float outputHeight = std::max(128.0f, ImGui::GetFontSize() * 5.8f);
                const float gap = std::max(44.0f, ImGui::GetFontSize() * 2.2f);
                const ImVec2 min(vertexRect.Min.x, vertexRect.Max.y + gap);
                const ImVec2 max(min.x + outputWidth, min.y + outputHeight);
                return ImRect(min, max);
            }

            std::vector<ShaderGraphResolvedLink> BuildLinks() const
            {
                std::vector<ShaderGraphResolvedLink> links = {};

                for (GraphEditor::NodeIndex targetIndex = 0; targetIndex < m_document.nodes.size(); ++targetIndex)
                {
                    const ShaderGraphNode &targetNode = m_document.nodes[targetIndex];
                    const std::vector<ShaderGraphPinInfo> inputPins = GetShaderGraphNodeInputPins(targetNode);
                    for (size_t inputIndex = 0; inputIndex < inputPins.size(); ++inputIndex)
                    {
                        const ShaderGraphLink link = GetShaderGraphNodeInputLink(targetNode, inputPins[inputIndex].name);
                        if (!link.IsValid())
                            continue;

                        const GraphEditor::NodeIndex sourceIndex = FindNodeIndexById(link.nodeId);
                        if (sourceIndex == kInvalidGraphNodeIndex)
                            continue;

                        const ShaderGraphNode &sourceNode = m_document.nodes[sourceIndex];
                        const std::vector<ShaderGraphPinInfo> outputPins = GetShaderGraphNodeOutputPins(sourceNode);
                        const auto outputIt = std::find_if(
                            outputPins.begin(),
                            outputPins.end(),
                            [&](const ShaderGraphPinInfo &_pin)
                            {
                                return link.slot == _pin.name;
                            });

                        if (outputIt == outputPins.end())
                            continue;

                        ShaderGraphResolvedLink resolved{};
                        resolved.graphLink =
                        {
                            sourceIndex,
                            static_cast<GraphEditor::SlotIndex>(std::distance(outputPins.begin(), outputIt)),
                            targetIndex,
                            static_cast<GraphEditor::SlotIndex>(inputIndex)
                        };
                        resolved.targetsOutput = false;
                        resolved.targetNodeIndex = targetIndex;
                        resolved.targetPinName = inputPins[inputIndex].name;
                        links.push_back(resolved);
                    }
                }

                const GraphEditor::NodeIndex vertexOutputNodeIndex = GetVertexOutputNodeIndex();
                const GraphEditor::NodeIndex fragmentOutputNodeIndex = GetFragmentOutputNodeIndex();
                if (m_document.vertexPosition.IsValid())
                {
                    const GraphEditor::NodeIndex sourceIndex = FindNodeIndexById(m_document.vertexPosition.nodeId);
                    if (sourceIndex != kInvalidGraphNodeIndex)
                    {
                        const std::vector<ShaderGraphPinInfo> outputPins = GetShaderGraphNodeOutputPins(m_document.nodes[sourceIndex]);
                        const auto outputIt = std::find_if(
                            outputPins.begin(),
                            outputPins.end(),
                            [&](const ShaderGraphPinInfo &_pin)
                            {
                                return m_document.vertexPosition.slot == _pin.name;
                            });
                        if (outputIt != outputPins.end())
                        {
                            ShaderGraphResolvedLink resolved{};
                            resolved.graphLink =
                            {
                                sourceIndex,
                                static_cast<GraphEditor::SlotIndex>(std::distance(outputPins.begin(), outputIt)),
                                vertexOutputNodeIndex,
                                0u
                            };
                            resolved.targetsOutput = true;
                            resolved.targetNodeIndex = vertexOutputNodeIndex;
                            resolved.targetPinName = "Position";
                            links.push_back(resolved);
                        }
                    }
                }

                if (m_document.vertexNormal.IsValid())
                {
                    const GraphEditor::NodeIndex sourceIndex = FindNodeIndexById(m_document.vertexNormal.nodeId);
                    if (sourceIndex != kInvalidGraphNodeIndex)
                    {
                        const std::vector<ShaderGraphPinInfo> outputPins = GetShaderGraphNodeOutputPins(m_document.nodes[sourceIndex]);
                        const auto outputIt = std::find_if(
                            outputPins.begin(),
                            outputPins.end(),
                            [&](const ShaderGraphPinInfo &_pin)
                            {
                                return m_document.vertexNormal.slot == _pin.name;
                            });
                        if (outputIt != outputPins.end())
                        {
                            ShaderGraphResolvedLink resolved{};
                            resolved.graphLink =
                            {
                                sourceIndex,
                                static_cast<GraphEditor::SlotIndex>(std::distance(outputPins.begin(), outputIt)),
                                vertexOutputNodeIndex,
                                1u
                            };
                            resolved.targetsOutput = true;
                            resolved.targetNodeIndex = vertexOutputNodeIndex;
                            resolved.targetPinName = "Normal";
                            links.push_back(resolved);
                        }
                    }
                }

                if (m_document.outputColor.IsValid())
                {
                    const GraphEditor::NodeIndex sourceIndex = FindNodeIndexById(m_document.outputColor.nodeId);
                    if (sourceIndex != kInvalidGraphNodeIndex)
                    {
                        const std::vector<ShaderGraphPinInfo> outputPins = GetShaderGraphNodeOutputPins(m_document.nodes[sourceIndex]);
                        const auto outputIt = std::find_if(
                            outputPins.begin(),
                            outputPins.end(),
                            [&](const ShaderGraphPinInfo &_pin)
                            {
                                return m_document.outputColor.slot == _pin.name;
                            });
                        if (outputIt != outputPins.end())
                        {
                            ShaderGraphResolvedLink resolved{};
                            resolved.graphLink =
                            {
                                sourceIndex,
                                static_cast<GraphEditor::SlotIndex>(std::distance(outputPins.begin(), outputIt)),
                                fragmentOutputNodeIndex,
                                0u
                            };
                            resolved.targetsOutput = true;
                            resolved.targetNodeIndex = fragmentOutputNodeIndex;
                            resolved.targetPinName = "Color";
                            links.push_back(resolved);
                        }
                    }
                }

                if (m_document.outputAlpha.IsValid())
                {
                    const GraphEditor::NodeIndex sourceIndex = FindNodeIndexById(m_document.outputAlpha.nodeId);
                    if (sourceIndex != kInvalidGraphNodeIndex)
                    {
                        const std::vector<ShaderGraphPinInfo> outputPins = GetShaderGraphNodeOutputPins(m_document.nodes[sourceIndex]);
                        const auto outputIt = std::find_if(
                            outputPins.begin(),
                            outputPins.end(),
                            [&](const ShaderGraphPinInfo &_pin)
                            {
                                return m_document.outputAlpha.slot == _pin.name;
                            });
                        if (outputIt != outputPins.end())
                        {
                            ShaderGraphResolvedLink resolved{};
                            resolved.graphLink =
                            {
                                sourceIndex,
                                static_cast<GraphEditor::SlotIndex>(std::distance(outputPins.begin(), outputIt)),
                                fragmentOutputNodeIndex,
                                1u
                            };
                            resolved.targetsOutput = true;
                            resolved.targetNodeIndex = fragmentOutputNodeIndex;
                            resolved.targetPinName = "Alpha";
                            links.push_back(resolved);
                        }
                    }
                }

                return links;
            }

            ShaderGraphDocument &m_document;
            int &m_selectedNodeId;
            bool &m_documentChanged;
            bool &m_semanticChanged;
            GraphEditor::NodeIndex m_contextNodeIndex = kInvalidGraphNodeIndex;
            GraphEditor::SlotIndex m_contextInputSlotIndex = kInvalidGraphSlotIndex;
            GraphEditor::SlotIndex m_contextOutputSlotIndex = kInvalidGraphSlotIndex;
            bool m_contextMenuRequested = false;
            ImVec2 m_contextMenuScreenPos = ImVec2(0.0f, 0.0f);
            ShaderGraphPendingLinkCreate m_pendingLinkCreate = {};
        };

        int ScoreShaderGraphPinMatch(ShaderGraphValueType _sourceType, ShaderGraphValueType _targetType)
        {
            if (_sourceType == _targetType)
                return 4;
            if (_sourceType == ShaderGraphValueType::UNKNOWN || _targetType == ShaderGraphValueType::UNKNOWN)
                return 3;
            return 2;
        }

        std::size_t FindBestShaderGraphPinIndex(const std::vector<ShaderGraphPinInfo> &_pins, ShaderGraphValueType _preferredType)
        {
            if (_pins.empty())
                return static_cast<std::size_t>(-1);

            int bestScore = -1;
            std::size_t bestIndex = 0;
            for (std::size_t i = 0; i < _pins.size(); ++i)
            {
                const int score = ScoreShaderGraphPinMatch(_preferredType, _pins[i].type);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestIndex = i;
                }
            }

            return bestIndex;
        }

        ShaderGraphValueType FindShaderGraphPinTypeByName(const std::vector<ShaderGraphPinInfo> &_pins, const std::string &_name)
        {
            const auto pinIt = std::find_if(
                _pins.begin(),
                _pins.end(),
                [&](const ShaderGraphPinInfo &_pin)
                {
                    return _pin.name == _name;
                });

            if (pinIt == _pins.end())
                return ShaderGraphValueType::UNKNOWN;

            return pinIt->type;
        }

        bool ApplyPendingLinkCreate(ShaderGraphDocument &_document, const ShaderGraphPendingLinkCreate &_pendingLinkCreate, ShaderGraphNode &_node)
        {
            if (!_pendingLinkCreate.valid)
                return false;

            if (_pendingLinkCreate.startedFromInput)
            {
                ShaderGraphNode *targetNode = FindShaderGraphNode(_document, _pendingLinkCreate.nodeId);
                if (targetNode == nullptr)
                    return false;

                const std::vector<ShaderGraphPinInfo> targetInputPins = GetShaderGraphNodeInputPins(*targetNode);
                const ShaderGraphValueType targetType = FindShaderGraphPinTypeByName(targetInputPins, _pendingLinkCreate.slot);
                const std::vector<ShaderGraphPinInfo> outputPins = GetShaderGraphNodeOutputPins(_node);
                const std::size_t bestIndex = FindBestShaderGraphPinIndex(outputPins, targetType);
                if (bestIndex == static_cast<std::size_t>(-1))
                    return false;

                return SetShaderGraphNodeInputLink(*targetNode, _pendingLinkCreate.slot, ShaderGraphLink{ .nodeId = _node.id, .slot = outputPins[bestIndex].name });
            }

            const ShaderGraphNode *sourceNode = FindShaderGraphNode(_document, _pendingLinkCreate.nodeId);
            if (sourceNode == nullptr)
                return false;

            const std::vector<ShaderGraphPinInfo> sourceOutputPins = GetShaderGraphNodeOutputPins(*sourceNode);
            const ShaderGraphValueType sourceType = FindShaderGraphPinTypeByName(sourceOutputPins, _pendingLinkCreate.slot);
            const std::vector<ShaderGraphPinInfo> inputPins = GetShaderGraphNodeInputPins(_node);
            const std::size_t bestIndex = FindBestShaderGraphPinIndex(inputPins, sourceType);
            if (bestIndex == static_cast<std::size_t>(-1))
                return false;

            return SetShaderGraphNodeInputLink(_node, inputPins[bestIndex].name, ShaderGraphLink{ .nodeId = sourceNode->id, .slot = _pendingLinkCreate.slot });
        }

        ShaderGraphNode CreateShaderGraphPropertyNode(const ShaderGraphProperty &_property, int _nodeId, const Vector2 &_position)
        {
            ShaderGraphNode node = CreateShaderGraphNode("Property", _nodeId, _position);
            node.propertyId = _property.id;
            node.propertyType = _property.type;
            return node;
        }

        bool AcceptShaderGraphTextureDrop(std::string &_texturePath)
        {
            if (!ImGui::BeginDragDropTarget())
                return false;

            bool changed = false;
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                std::string path = std::string(dropped.path);
                if (path.empty() || !FileExists(path.c_str()))
                    path = AssetManager::GetPath(dropped.uuid);

                if (MetaFileAsset *droppedMeta = AssetManager::GetMetaFile(path))
                {
                    if (droppedMeta->type == MetaFileAsset::FileType::TEXTURE)
                    {
                        _texturePath = path;
                        changed = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
            return changed;
        }

        bool DrawShaderGraphPropertyTypeCombo(const char *_label, ShaderGraphPropertyType &_type)
        {
            bool changed = false;
            if (ImGui::BeginCombo(_label, GetShaderGraphPropertyTypeDisplayName(_type)))
            {
                constexpr ShaderGraphPropertyType kTypes[] =
                {
                    ShaderGraphPropertyType::FLOAT,
                    ShaderGraphPropertyType::VEC2,
                    ShaderGraphPropertyType::VEC3,
                    ShaderGraphPropertyType::VEC4,
                    ShaderGraphPropertyType::COLOR,
                    ShaderGraphPropertyType::TEXTURE
                };

                for (ShaderGraphPropertyType type : kTypes)
                {
                    const bool selected = _type == type;
                    if (ImGui::Selectable(GetShaderGraphPropertyTypeDisplayName(type), selected))
                    {
                        _type = type;
                        changed = true;
                    }

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            return changed;
        }

        bool DrawShaderGraphPropertyValueEditor(ShaderGraphProperty &_property)
        {
            bool changed = false;
            if (_property.type == ShaderGraphPropertyType::FLOAT)
            {
                float value = _property.floatValue;
                if (ImGui::DragFloat("Default", &value, 0.01f))
                {
                    _property.floatValue = value;
                    changed = true;
                }
            }
            else if (_property.type == ShaderGraphPropertyType::VEC2)
            {
                Vector2 value = _property.vec2Value;
                if (ImGui::DragFloat2("Default", &value[0], 0.01f))
                {
                    _property.vec2Value = value;
                    changed = true;
                }
            }
            else if (_property.type == ShaderGraphPropertyType::VEC3)
            {
                Vector3 value = _property.vec3Value;
                if (ImGui::DragFloat3("Default", &value[0], 0.01f))
                {
                    _property.vec3Value = value;
                    changed = true;
                }
            }
            else if (_property.type == ShaderGraphPropertyType::VEC4)
            {
                Vector4 value = _property.vec4Value;
                if (ImGui::DragFloat4("Default", &value[0], 0.01f))
                {
                    _property.vec4Value = value;
                    changed = true;
                }
            }
            else if (_property.type == ShaderGraphPropertyType::COLOR)
            {
                Color value = _property.colorValue;
                if (ImGui::ColorEdit4("Default", &value[0], ImGuiColorEditFlags_Float))
                {
                    _property.colorValue = value;
                    changed = true;
                }
            }
            else if (_property.type == ShaderGraphPropertyType::TEXTURE)
            {
                const std::string textureLabel = GetTexturePathLabel(_property.texturePath);
                (void)ImGui::Button(textureLabel.c_str(), ImVec2(180.0f, 0.0f));
                if (AcceptShaderGraphTextureDrop(_property.texturePath))
                    changed = true;

                ImGui::SameLine();
                if (ImGui::SmallButton("Clear"))
                {
                    _property.texturePath.clear();
                    changed = true;
                }
            }

            return changed;
        }

        Vector2 GetShaderGraphViewCenterPosition(const ImVec2 &_canvasSize, const GraphEditor::ViewState &_viewState)
        {
            const float graphX = (_canvasSize.x * 0.5f) / _viewState.mFactor - _viewState.mPosition.x;
            const float graphY = (_canvasSize.y * 0.5f) / _viewState.mFactor - _viewState.mPosition.y;
            return Vector2(graphX, graphY);
        }

        void DrawShaderGraphBlackboard(
            ShaderGraphDocument &_document,
            const ImVec2 &_canvasSize,
            const GraphEditor::ViewState &_viewState,
            int &_selectedNodeId,
            bool &_documentChanged,
            bool &_semanticChanged)
        {
            ImGui::TextUnformatted("Blackboard");
            ImGui::TextDisabled("Custom uniforms");
            ImGui::Separator();

            auto addPropertyButton = [&](ShaderGraphPropertyType _type, const char *_label) -> void
            {
                if (ImGui::SmallButton(_label))
                {
                    ShaderGraphProperty property = CreateShaderGraphProperty(
                        _type,
                        _document.nextPropertyId++,
                        std::string("New ") + GetShaderGraphPropertyTypeDisplayName(_type));
                    _document.properties.push_back(property);
                    _documentChanged = true;
                    _semanticChanged = true;
                }
            };

            addPropertyButton(ShaderGraphPropertyType::FLOAT, "+ Float");
            ImGui::SameLine();
            addPropertyButton(ShaderGraphPropertyType::VEC2, "+ Vec2");
            addPropertyButton(ShaderGraphPropertyType::VEC3, "+ Vec3");
            ImGui::SameLine();
            addPropertyButton(ShaderGraphPropertyType::VEC4, "+ Vec4");
            addPropertyButton(ShaderGraphPropertyType::COLOR, "+ Color");
            ImGui::SameLine();
            addPropertyButton(ShaderGraphPropertyType::TEXTURE, "+ Texture");

            ImGui::Separator();

            if (_document.properties.empty())
            {
                ImGui::TextDisabled("Add a property to expose a material uniform.");
                return;
            }

            for (std::size_t i = 0; i < _document.properties.size(); ++i)
            {
                ShaderGraphProperty &property = _document.properties[i];
                ImGui::PushID(property.id);

                bool open = ImGui::TreeNodeEx(
                    property.name.c_str(),
                    ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
                    "%s", property.name.c_str());

                ImGui::SameLine();
                if (ImGui::SmallButton("Node"))
                {
                    Vector2 position = GetShaderGraphViewCenterPosition(_canvasSize, _viewState);
                    position.x += static_cast<float>(_document.nodes.size() % 4) * 28.0f;
                    position.y += static_cast<float>(_document.nodes.size() % 4) * 24.0f;

                    ShaderGraphNode node = CreateShaderGraphPropertyNode(property, _document.nextNodeId++, position);
                    _document.nodes.push_back(node);
                    _selectedNodeId = node.id;
                    _documentChanged = true;
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    const int propertyId = property.id;
                    if (RemoveShaderGraphProperty(_document, propertyId))
                    {
                        _documentChanged = true;
                        _semanticChanged = true;
                    }
                    if (open)
                        ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }

                if (open)
                {
                    std::string name = property.name;
                    if (ImGui::InputText("Name", &name))
                    {
                        property.name = name;
                        _documentChanged = true;
                        _semanticChanged = true;
                    }

                    ShaderGraphPropertyType type = property.type;
                    if (DrawShaderGraphPropertyTypeCombo("Type", type))
                    {
                        property.type = type;
                        for (ShaderGraphNode &node : _document.nodes)
                        {
                            if (node.type == "Property" && node.propertyId == property.id)
                                node.propertyType = property.type;
                        }
                        _documentChanged = true;
                        _semanticChanged = true;
                    }

                    if (DrawShaderGraphPropertyValueEditor(property))
                    {
                        _documentChanged = true;
                        _semanticChanged = true;
                    }

                    ImGui::TextDisabled("%s", GetShaderGraphPropertyUniformName(property).c_str());
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }

        Vector2 ScreenToShaderGraphPosition(const ImVec2 &_screenPos, const ImVec2 &_canvasScreenPos, const GraphEditor::ViewState &_viewState)
        {
            const float localX = (_screenPos.x - _canvasScreenPos.x) / _viewState.mFactor;
            const float localY = (_screenPos.y - _canvasScreenPos.y) / _viewState.mFactor;
            return Vector2(localX - _viewState.mPosition.x, localY - _viewState.mPosition.y);
        }

        void RefreshGeneratedShaderGraphAssets(const std::string &_graphPath)
        {
            const std::string vertexPath = GetShaderGraphGeneratedVertexPath(_graphPath);
            const std::string fragmentPath = GetShaderGraphGeneratedFragmentPath(_graphPath);
            const std::string materialPath = GetShaderGraphGeneratedMaterialPath(_graphPath);

            (void)AssetManager::GetMetaFile(vertexPath);
            (void)AssetManager::GetMetaFile(fragmentPath);
            (void)AssetManager::GetMetaFile(materialPath);

            AssetManager::Free<ShaderAsset>(GetGeneratedShaderBasePath(_graphPath));
            AssetManager::ReloadLoadedShaders();
        }

        bool IsShaderGraphAssetPath(const std::string &_path, MetaFileAsset **_outMeta = nullptr)
        {
            MetaFileAsset *meta = AssetManager::GetMetaFile(_path);
            if (_outMeta != nullptr)
                *_outMeta = meta;

            return meta != nullptr && meta->type == MetaFileAsset::FileType::SHADERGRAPH;
        }

        void PrepareShaderGraphDocument(
            const std::string &_shaderGraphPath,
            std::string &_statePath,
            int &_selectedNodeId,
            ShaderGraphDocument &_document)
        {
            if (_statePath != _shaderGraphPath)
            {
                _statePath = _shaderGraphPath;
                _selectedNodeId = -1;
            }

            if (!LoadShaderGraphDocument(_shaderGraphPath, _document))
            {
                _document = MakeDefaultShaderGraphDocument();
                (void)SaveShaderGraphDocument(_shaderGraphPath, _document);
            }

            if (_selectedNodeId >= 0 && FindShaderGraphNode(_document, _selectedNodeId) == nullptr)
                _selectedNodeId = -1;
        }

        void CommitShaderGraphDocument(
            const std::string &_shaderGraphPath,
            const ShaderGraphDocument &_document,
            bool _documentChanged,
            bool _semanticChanged,
            bool _manualGenerate)
        {
            if (_documentChanged && !SaveShaderGraphDocument(_shaderGraphPath, _document))
                Debug::Warning("Failed to save shader graph document: %s", _shaderGraphPath.c_str());

            if (_manualGenerate || _semanticChanged)
            {
                std::string errorMessage = {};
                if (!GenerateShaderGraphAssets(_shaderGraphPath, _document, &errorMessage))
                {
                    if (errorMessage.empty())
                        errorMessage = "Failed to generate shader graph assets.";
                    Debug::Warning("%s", errorMessage.c_str());
                }
                else
                {
                    RefreshGeneratedShaderGraphAssets(_shaderGraphPath);
                }
            }
        }

        void DrawShaderGraphNodeProperties(
            ShaderGraphDocument &_document,
            int &_selectedNodeId,
            bool &_documentChanged,
            bool &_semanticChanged)
        {
            if (_selectedNodeId == kShaderGraphVertexOutputSelectionId)
            {
                ImGui::TextUnformatted("Vertex Output");

                ImGui::Text("Position: %s", DescribeShaderGraphLink(_document, _document.vertexPosition).c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear Position"))
                {
                    _document.vertexPosition = {};
                    _documentChanged = true;
                    _semanticChanged = true;
                }

                ImGui::Text("Normal: %s", DescribeShaderGraphLink(_document, _document.vertexNormal).c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear Normal"))
                {
                    _document.vertexNormal = {};
                    _documentChanged = true;
                    _semanticChanged = true;
                }
            }
            else if (_selectedNodeId == kShaderGraphFragmentOutputSelectionId)
            {
                ImGui::TextUnformatted("Output");

                ImGui::Text("Color: %s", DescribeShaderGraphLink(_document, _document.outputColor).c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear Color"))
                {
                    _document.outputColor = {};
                    _documentChanged = true;
                    _semanticChanged = true;
                }

                ImGui::Text("Alpha: %s", DescribeShaderGraphLink(_document, _document.outputAlpha).c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear Alpha"))
                {
                    _document.outputAlpha = {};
                    _documentChanged = true;
                    _semanticChanged = true;
                }
            }
            else if (ShaderGraphNode *selectedNode = FindShaderGraphNode(_document, _selectedNodeId))
            {
                ImGui::Text("%s #%d", GetShaderGraphNodeDisplayName(selectedNode->type), selectedNode->id);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete Selected"))
                {
                    const int nodeId = selectedNode->id;
                    if (RemoveShaderGraphNode(_document, nodeId))
                    {
                        _selectedNodeId = -1;
                        _documentChanged = true;
                        _semanticChanged = true;
                        selectedNode = nullptr;
                    }
                }

                if (selectedNode == nullptr)
                    return;

                if (selectedNode->type == "Float")
                {
                    float value = selectedNode->floatValue;
                    if (ImGui::DragFloat("value", &value, 0.01f))
                    {
                        selectedNode->floatValue = value;
                        _documentChanged = true;
                        _semanticChanged = true;
                    }
                }
                else if (selectedNode->type == "Vector2")
                {
                    Vector2 value = selectedNode->vec2Value;
                    if (ImGui::DragFloat2("value", &value[0], 0.01f))
                    {
                        selectedNode->vec2Value = value;
                        _documentChanged = true;
                        _semanticChanged = true;
                    }
                }
                else if (selectedNode->type == "Vector3")
                {
                    Vector3 value = selectedNode->vec3Value;
                    if (ImGui::DragFloat3("value", &value[0], 0.01f))
                    {
                        selectedNode->vec3Value = value;
                        _documentChanged = true;
                        _semanticChanged = true;
                    }
                }
                else if (selectedNode->type == "Vector4")
                {
                    Vector4 value = selectedNode->vec4Value;
                    if (ImGui::DragFloat4("value", &value[0], 0.01f))
                    {
                        selectedNode->vec4Value = value;
                        _documentChanged = true;
                        _semanticChanged = true;
                    }
                }
                else if (selectedNode->type == "Color")
                {
                    Color value = selectedNode->colorValue;
                    if (ImGui::ColorEdit4("color", &value[0], ImGuiColorEditFlags_Float))
                    {
                        selectedNode->colorValue = value;
                        _documentChanged = true;
                        _semanticChanged = true;
                    }
                }
                else if (selectedNode->type == "Texture2D")
                {
                    const std::string textureLabel = GetTexturePathLabel(selectedNode->texturePath);
                    (void)ImGui::Button(textureLabel.c_str(), ImVec2(220.0f, 0.0f));

                    if (AcceptShaderGraphTextureDrop(selectedNode->texturePath))
                    {
                        _documentChanged = true;
                        _semanticChanged = true;
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear Texture"))
                    {
                        selectedNode->texturePath.clear();
                        _documentChanged = true;
                        _semanticChanged = true;
                    }
                }
                else if (selectedNode->type == "Property")
                {
                    const char *preview = "[ missing property ]";
                    if (const ShaderGraphProperty *property = FindShaderGraphProperty(_document, selectedNode->propertyId))
                        preview = property->name.c_str();

                    if (ImGui::BeginCombo("Property", preview))
                    {
                        for (const ShaderGraphProperty &property : _document.properties)
                        {
                            const bool selected = selectedNode->propertyId == property.id;
                            if (ImGui::Selectable(property.name.c_str(), selected))
                            {
                                selectedNode->propertyId = property.id;
                                selectedNode->propertyType = property.type;
                                _documentChanged = true;
                                _semanticChanged = true;
                            }

                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (ShaderGraphProperty *property = FindShaderGraphProperty(_document, selectedNode->propertyId))
                    {
                        ShaderGraphPropertyType type = property->type;
                        if (DrawShaderGraphPropertyTypeCombo("Type", type))
                        {
                            property->type = type;
                            selectedNode->propertyType = type;
                            for (ShaderGraphNode &node : _document.nodes)
                            {
                                if (node.type == "Property" && node.propertyId == property->id)
                                    node.propertyType = type;
                            }
                            _documentChanged = true;
                            _semanticChanged = true;
                        }

                        std::string name = property->name;
                        if (ImGui::InputText("Name", &name))
                        {
                            property->name = name;
                            _documentChanged = true;
                            _semanticChanged = true;
                        }

                        if (DrawShaderGraphPropertyValueEditor(*property))
                        {
                            _documentChanged = true;
                            _semanticChanged = true;
                        }

                        ImGui::TextDisabled("Uniform: %s", GetShaderGraphPropertyUniformName(*property).c_str());
                    }
                }
                else if (selectedNode->type == "Panner")
                {
                    Vector2 value = selectedNode->speedValue;
                    if (ImGui::DragFloat2("speed", &value[0], 0.01f))
                    {
                        selectedNode->speedValue = value;
                        _documentChanged = true;
                        _semanticChanged = true;
                    }
                }
                else if (selectedNode->type == "Position")
                {
                    ImGui::TextDisabled("Built-in world position input.");
                }
                else if (selectedNode->type == "Normal")
                {
                    ImGui::TextDisabled("Built-in world normal input.");
                }
                else if (selectedNode->type == "StickyNote")
                {
                    std::string title = selectedNode->title;
                    if (ImGui::InputText("Title", &title))
                    {
                        selectedNode->title = title;
                        _documentChanged = true;
                    }

                    std::string text = selectedNode->text;
                    if (ImGui::InputTextMultiline("Text", &text, ImVec2(-1.0f, 120.0f)))
                    {
                        selectedNode->text = text;
                        _documentChanged = true;
                    }
                }
                else if (selectedNode->type == "Preview")
                {
                    ImGui::TextDisabled("Connect any output to preview the shape of that value.");
                }

                const std::vector<ShaderGraphPinInfo> inputPins = GetShaderGraphNodeInputPins(*selectedNode);
                if (!inputPins.empty())
                {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Inputs");
                    for (const ShaderGraphPinInfo &pin : inputPins)
                    {
                        const ShaderGraphLink link = GetShaderGraphNodeInputLink(*selectedNode, pin.name);
                        ImGui::Text("%s: %s", pin.name, DescribeShaderGraphLink(_document, link).c_str());
                        ImGui::SameLine();
                        if (ImGui::SmallButton((std::string("Clear##") + pin.name).c_str()))
                        {
                            if (ClearShaderGraphNodeInputLink(*selectedNode, pin.name))
                            {
                                _documentChanged = true;
                                _semanticChanged = true;
                            }
                        }
                    }
                }
            }
            else
            {
                ImGui::TextUnformatted("Select a node in the ShaderGraph window to edit it here.");
            }
        }
    }

    void Editor::DrawShaderGraphWindow()
    {
        if (m_mainDockspaceID != 0u)
            ImGui::SetNextWindowDockID(m_mainDockspaceID, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);

        const bool shaderGraphVisible = ImGui::Begin("ShaderGraph");
        if (!shaderGraphVisible)
        {
            ImGui::End();
            return;
        }

        MetaFileAsset *meta = nullptr;
        if (!IsShaderGraphAssetPath(m_selectedAssetPath, &meta))
        {
            ImGui::TextUnformatted("Select a .shadergraph asset in Assets to edit it here.");
            ImGui::TextDisabled("The Inspector will show node properties for the active shader graph.");
            ImGui::End();
            return;
        }

        ShaderGraphDocument document = {};
        PrepareShaderGraphDocument(m_selectedAssetPath, m_shaderGraphStatePath, m_shaderGraphSelectedNodeId, document);

        ShaderGraphEditorViewState &viewState = g_shaderGraphEditorViewStates[m_selectedAssetPath];
        viewState.options.mDrawIONameOnHover = false;
        viewState.options.mDrawIONameInsideNode = true;
        viewState.options.mNodeSlotRadius = 7.0f;
        viewState.options.mLineThickness = 4.0f;
        bool documentChanged = false;
        bool semanticChanged = false;
        bool manualGenerate = false;
        GraphEditor::FitOnScreen fitRequest = GraphEditor::Fit_None;

        ImGui::Text("Asset: %s", meta->name.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Generate Shader"))
            manualGenerate = true;
        ImGui::SameLine();
        if (ImGui::Button("Fit Graph"))
            fitRequest = GraphEditor::Fit_AllNodes;

        ImGui::TextDisabled("%s", meta->path.c_str());
        ImGui::TextDisabled("Right click to add nodes. Drop a dragged link on empty space to create the next node.");
        ImGui::Separator();

        const ImGuiStyle &style = ImGui::GetStyle();
        const float blackboardWidth = std::max(260.0f, ImGui::GetFontSize() * 13.0f);
        ImVec2 availableSize = ImGui::GetContentRegionAvail();
        ImVec2 graphAreaSize(
            std::max(320.0f, availableSize.x - blackboardWidth - style.ItemSpacing.x),
            availableSize.y);

        ImGui::BeginChild("##shader_graph_blackboard", ImVec2(blackboardWidth, 0.0f), true);
        DrawShaderGraphBlackboard(document, graphAreaSize, viewState.viewState, m_shaderGraphSelectedNodeId, documentChanged, semanticChanged);
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginGroup();

        const ImVec2 canvasScreenPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const ImRect canvasRect(canvasScreenPos, ImVec2(canvasScreenPos.x + canvasSize.x, canvasScreenPos.y + canvasSize.y));
        ShaderGraphGraphDelegate delegate(document, m_shaderGraphSelectedNodeId, documentChanged, semanticChanged);
        GraphEditor::Show(delegate, viewState.options, viewState.viewState, true, &fitRequest);

        bool openContextMenu = false;
        ImVec2 popupScreenPos = viewState.contextMousePos;

        ShaderGraphPendingLinkCreate pendingLinkCreate = {};
        if (delegate.ConsumePendingLinkCreate(pendingLinkCreate))
        {
            viewState.pendingLinkCreate = pendingLinkCreate;
            popupScreenPos = ImGui::GetMousePos();
            openContextMenu = true;
        }

        ImVec2 delegatePopupScreenPos = ImVec2(0.0f, 0.0f);
        if (delegate.ConsumeContextMenuRequest(delegatePopupScreenPos))
        {
            popupScreenPos = delegatePopupScreenPos;
            openContextMenu = true;
        }

        if (!openContextMenu && canvasRect.Contains(ImGui::GetMousePos()) && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            viewState.pendingLinkCreate = {};
            popupScreenPos = ImGui::GetMousePos();
            openContextMenu = true;
        }

        if (openContextMenu)
        {
            viewState.contextMousePos = popupScreenPos;
            ImGui::SetNextWindowPos(viewState.contextMousePos, ImGuiCond_Appearing);
            ImGui::OpenPopup("ShaderGraphContextMenu");
        }

        if (ImGui::BeginPopup("ShaderGraphContextMenu"))
        {
            const GraphEditor::NodeIndex contextNodeIndex = delegate.GetContextNodeIndex();

            if (contextNodeIndex != kInvalidGraphNodeIndex && contextNodeIndex < document.nodes.size())
            {
                if (ImGui::MenuItem("Delete Node"))
                {
                    const int nodeId = document.nodes[contextNodeIndex].id;
                    if (RemoveShaderGraphNode(document, nodeId))
                    {
                        if (m_shaderGraphSelectedNodeId == nodeId)
                            m_shaderGraphSelectedNodeId = -1;
                        documentChanged = true;
                        semanticChanged = true;
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Separator();
            }

            auto getNodeCreatePosition = [&]() -> Vector2
            {
                return viewState.pendingLinkCreate.valid
                    ? viewState.pendingLinkCreate.graphPosition
                    : ScreenToShaderGraphPosition(viewState.contextMousePos, canvasScreenPos, viewState.viewState);
            };

            auto addNode = [&](ShaderGraphNode node) -> void
            {
                if (ApplyPendingLinkCreate(document, viewState.pendingLinkCreate, node))
                    semanticChanged = true;

                document.nodes.push_back(node);
                m_shaderGraphSelectedNodeId = node.id;
                documentChanged = true;
                semanticChanged = true;
                viewState.pendingLinkCreate = {};
                ImGui::CloseCurrentPopup();
            };

            auto addNodeMenuItem = [&](const char *_label, const char *_type) -> void
            {
                if (ImGui::MenuItem(_label))
                    addNode(CreateShaderGraphNode(_type, document.nextNodeId++, getNodeCreatePosition()));
            };

            auto addPropertyNodeMenuItem = [&](const ShaderGraphProperty &_property) -> void
            {
                ImGui::PushID(_property.id);
                if (ImGui::MenuItem(_property.name.c_str(), GetShaderGraphPropertyTypeDisplayName(_property.type)))
                    addNode(CreateShaderGraphPropertyNode(_property, document.nextNodeId++, getNodeCreatePosition()));
                ImGui::PopID();
            };

            if (ImGui::BeginMenu("Vertex"))
            {
                addNodeMenuItem("Position (World)", "Position");
                addNodeMenuItem("Normal (World)", "Normal");
                addNodeMenuItem("UV", "UV");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Input"))
            {
                addNodeMenuItem("Time", "Time");
                addNodeMenuItem("Texture2D", "Texture2D");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Values"))
            {
                addNodeMenuItem("Float", "Float");
                addNodeMenuItem("Vector2", "Vector2");
                addNodeMenuItem("Vector3", "Vector3");
                addNodeMenuItem("Vector4", "Vector4");
                addNodeMenuItem("Color", "Color");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Properties"))
            {
                if (document.properties.empty())
                    ImGui::TextDisabled("Add uniforms in the Blackboard.");
                else
                {
                    for (const ShaderGraphProperty &property : document.properties)
                        addPropertyNodeMenuItem(property);
                }

                ImGui::Separator();
                if (ImGui::MenuItem("New Float Property"))
                {
                    ShaderGraphProperty property = CreateShaderGraphProperty(
                        ShaderGraphPropertyType::FLOAT,
                        document.nextPropertyId++,
                        "New Float");
                    document.properties.push_back(property);
                    addNode(CreateShaderGraphPropertyNode(document.properties.back(), document.nextNodeId++, getNodeCreatePosition()));
                }
                if (ImGui::MenuItem("New Color Property"))
                {
                    ShaderGraphProperty property = CreateShaderGraphProperty(
                        ShaderGraphPropertyType::COLOR,
                        document.nextPropertyId++,
                        "New Color");
                    document.properties.push_back(property);
                    addNode(CreateShaderGraphPropertyNode(document.properties.back(), document.nextNodeId++, getNodeCreatePosition()));
                }
                if (ImGui::MenuItem("New Texture Property"))
                {
                    ShaderGraphProperty property = CreateShaderGraphProperty(
                        ShaderGraphPropertyType::TEXTURE,
                        document.nextPropertyId++,
                        "New Texture");
                    document.properties.push_back(property);
                    addNode(CreateShaderGraphPropertyNode(document.properties.back(), document.nextNodeId++, getNodeCreatePosition()));
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Math"))
            {
                addNodeMenuItem("Add", "Add");
                addNodeMenuItem("Multiply", "Multiply");
                addNodeMenuItem("Sine", "Sine");
                addNodeMenuItem("Lerp", "Lerp");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("UV"))
            {
                addNodeMenuItem("Panner", "Panner");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Visual"))
            {
                addNodeMenuItem("Sticky Note", "StickyNote");
                addNodeMenuItem("Preview", "Preview");
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
        else if (!ImGui::IsPopupOpen("ShaderGraphContextMenu"))
        {
            viewState.pendingLinkCreate = {};
        }

        ImGui::EndGroup();

        CommitShaderGraphDocument(m_selectedAssetPath, document, documentChanged, semanticChanged, manualGenerate);
        ImGui::End();
    }

    bool Editor::DrawShaderGraphAssetInspector(const std::string &_shaderGraphPath)
    {
        MetaFileAsset *meta = nullptr;
        if (!IsShaderGraphAssetPath(_shaderGraphPath, &meta))
            return false;

        ShaderGraphDocument document = {};
        PrepareShaderGraphDocument(_shaderGraphPath, m_shaderGraphStatePath, m_shaderGraphSelectedNodeId, document);

        bool documentChanged = false;
        bool semanticChanged = false;
        bool manualGenerate = false;

        ImGui::Text("Asset: %s", meta->name.c_str());
        ImGui::Text("Path: %s", meta->path.c_str());

        if (ImGui::Button("Generate Shader"))
            manualGenerate = true;
        ImGui::SameLine();
        ImGui::TextDisabled("Canvas: ShaderGraph window");

        ImGui::Text("Generated VS: %s", GetShaderGraphGeneratedVertexPath(_shaderGraphPath).c_str());
        ImGui::Text("Generated FS: %s", GetShaderGraphGeneratedFragmentPath(_shaderGraphPath).c_str());
        ImGui::Text("Generated Material: %s", GetShaderGraphGeneratedMaterialPath(_shaderGraphPath).c_str());
        ImGui::Separator();
        ImGui::TextUnformatted("Node Properties");

        DrawShaderGraphNodeProperties(document, m_shaderGraphSelectedNodeId, documentChanged, semanticChanged);
        CommitShaderGraphDocument(_shaderGraphPath, document, documentChanged, semanticChanged, manualGenerate);
        return true;
    }
}
