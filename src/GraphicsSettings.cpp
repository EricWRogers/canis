#include <Canis/GraphicsSettings.hpp>
namespace Canis
{
GraphicsSettings& GetGraphicsSettings()
{
    static GraphicsSettings settings;
    return settings;
}
}
