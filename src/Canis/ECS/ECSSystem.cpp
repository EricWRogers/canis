#include "ECSSystem.hpp"

bool BaseECSSystem::IsValid()
{
	for(glm::uint32 i = 0;  i < componentFlags.size(); i++) {
		if((componentFlags[i] & BaseECSSystem::FLAG_OPTIONAL) == 0) {
			return true;
		}
	}
	return false;
}

bool ECSSystemList::RemoveSystem(BaseECSSystem& system)
{
	for(glm::uint32 i = 0; i < systems.size(); i++) {
		if(&system == systems[i]) {
			systems.erase(systems.begin() + i);
			return true;
		}
	}
	return false;
}