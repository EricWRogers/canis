#include <Canis/EditorTransformConstraints.hpp>
#include <iostream>

int main()
{
    using namespace Canis;
    const Vector3 drag(2, 3, 4);
    for (int axis = 0; axis < 3; ++axis)
    {
        const Vector3 line = ConstrainEditorVector(drag, axis, false);
        const Vector3 plane = ConstrainEditorVector(drag, axis, true);
        if (line[axis] != drag[axis] || plane[axis] != 0 || line + plane != drag)
            return 1;
        const Vector3 scale = EditorScaleFactors(2, axis, true);
        for (int i = 0; i < 3; ++i)
            if (scale[i] != (i == axis ? 1.0f : 2.0f))
                return 1;
    }
    if (ConstrainEditorVector(drag, -1, false) != drag ||
        EditorScaleFactors(2, -1, false) != Vector3(2) ||
        ConstrainEditorVector(Vector3(0, 3, 0), 1, false) != Vector3(0, 3, 0) ||
        ConstrainEditorVector(drag, 2, true) != Vector3(2, 3, 0))
        return 1;
    std::cout << "Axis, plane, vertical drag, and scale constraints passed.\n";
}
