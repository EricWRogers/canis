#include <Canis/EditorNavigation.hpp>
#include <iostream>
#include <limits>

int main()
{
    using namespace Canis;
    // A large, offset building must slow flight near its facade, even though
    // its pivot is distant. Moving away should regain full speed.
    const Vector3 minimum(20, -5, -50), maximum(120, 50, 50);
    const auto speed = [&](float x) {
        return 8.0f * EditorCameraSelectionSpeedScale(
            EditorCameraBoundsDistance(Vector3(x, 2, 0), minimum, maximum));
    };
    if (!(speed(19.9f) < speed(19) && speed(19) < speed(16) && speed(16) < speed(10)))
        return 1;
    if (speed(10) != 8.0f || speed(150) != 8.0f || speed(30) <= 0.0f)
        return 1;

    // Near the surface and inside the selection, input must still let us leave.
    for (const float initial : {19.99f, 21.0f})
    {
        float x = initial;
        for (int frame = 0; frame < 120; ++frame)
            x -= speed(x) / 60.0f;
        if (!(x < initial - 0.35f))
            return 1;
    }

    if (EditorCameraBoundsDistance(Vector3(123, 54, 50), minimum, maximum) != 5.0f ||
        EditorCameraSelectionSpeedScale(std::numeric_limits<float>::infinity()) != 1.0f ||
        EditorCameraSelectionSpeedScale(std::numeric_limits<float>::quiet_NaN()) != 1.0f)
        return 1;

    std::cout << "Selection surface approach, retreat, interior escape and distance fallback passed.\n";
}
