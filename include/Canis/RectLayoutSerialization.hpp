#pragma once
namespace YAML { class Node; }
namespace Canis
{
    // Upgrade one legacy entity in memory. Scene saves emit only the new schema.
    bool MigrateRectLayoutEntity(YAML::Node& entity);
}
