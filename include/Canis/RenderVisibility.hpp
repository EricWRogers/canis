#pragma once

namespace Canis
{
    // Runtime grouping only; descendants keep their simulation and shadow casting.
    struct RenderVisibilityGroup { bool enabled=true; };
}
