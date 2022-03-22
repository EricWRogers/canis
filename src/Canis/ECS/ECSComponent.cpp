#include "ECSComponent.hpp"

std::vector<std::tuple<ECSComponentCreateFunction, ECSComponentFreeFunction, size_t> >* BaseECSComponent::componentTypes;

glm::uint32 BaseECSComponent::RegisterComponentType(ECSComponentCreateFunction createfn,
			ECSComponentFreeFunction freefn, size_t size)
{
    if(componentTypes == nullptr) {
		componentTypes = new std::vector<std::tuple<ECSComponentCreateFunction, ECSComponentFreeFunction, size_t> >();
	}

    glm::uint32 componentID = componentTypes->size();

    componentTypes->push_back(
        std::tuple<ECSComponentCreateFunction, ECSComponentFreeFunction, size_t>(
            createfn, freefn, size
        )
    );

    return componentID;
}