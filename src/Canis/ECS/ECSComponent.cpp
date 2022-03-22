#include "ECSComponent.hpp"

std::vector<std::tuple<ECSComponentCreateFunction, ECSComponentFreeFunction, size_t> >* BaseECSComponent::componentTypes;

u_int32_t BaseECSComponent::RegisterComponentType(ECSComponentCreateFunction createfn,
			ECSComponentFreeFunction freefn, size_t size)
{
    if(componentTypes == nullptr) {
		componentTypes = new std::vector<std::tuple<ECSComponentCreateFunction, ECSComponentFreeFunction, size_t> >();
	}

    u_int32_t componentID = componentTypes->size();

    componentTypes->push_back(
        std::tuple<ECSComponentCreateFunction, ECSComponentFreeFunction, size_t>(
            createfn, freefn, size
        )
    );

    return componentID;
}