#include <Canis/Math.hpp>

namespace Canis
{
    Ray RayFromMouse(Camera &camera, Window &window, InputManager &inputManager)
    {
        glm::vec4 lRayStart_NDC(
            ((float)inputManager.mouse.x/(float)window.GetScreenWidth()  - 0.5f) * 2.0f,
            -((float)inputManager.mouse.y/(float)window.GetScreenHeight()  - 0.5f) * 2.0f,
            -1.0, // The near plane maps to Z=-1 in Normalized Device Coordinates
            1.0f
        );
        glm::vec4 lRayEnd_NDC(
            ((float)inputManager.mouse.x/(float)window.GetScreenWidth()  - 0.5f) * 2.0f,
            -((float)inputManager.mouse.y/(float)window.GetScreenHeight()  - 0.5f) * 2.0f,
            0.0,
            1.0f
        );

        glm::mat4 projection = glm::perspective(camera.FOV, (float)window.GetScreenWidth() / (float)window.GetScreenHeight(), 0.1f, 100.0f);

        glm::mat4 M = glm::inverse(projection * camera.GetViewMatrix());
        glm::vec4 lRayStart_world = M * lRayStart_NDC; lRayStart_world/=lRayStart_world.w;
        glm::vec4 lRayEnd_world   = M * lRayEnd_NDC  ; lRayEnd_world  /=lRayEnd_world.w;

        glm::vec3 lRayDir_world(lRayEnd_world - lRayStart_world);
        lRayDir_world = glm::normalize(lRayDir_world);

        return Ray { lRayStart_world, lRayDir_world };
    }

    bool HitSphere(glm::vec3 center, float radius, Ray ray)
    {
        glm::vec3 oc = ray.origin - center;
        float a = glm::dot(ray.direction, ray.direction);
        float b = 2.0f * glm::dot(oc, ray.direction);
        float c = glm::dot(oc,oc) - radius*radius;
        float discriminant = b*b - 4*a*c;
        return (discriminant>0);
    }

    glm::quat RotationBetweenVectors(glm::vec3 start, glm::vec3 dest){
        start = glm::normalize(start);
        dest = glm::normalize(dest);

        float cosTheta = glm::dot(start, dest);
        glm::vec3 rotationAxis;

        if (cosTheta < -1 + 0.001f)
        {
            rotationAxis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), start);
            float normalLength2 = glm::length2(glm::normalize(rotationAxis));

            if (normalLength2 < 0.01 ) // parallel
            {
                rotationAxis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), start);
            }

            rotationAxis = glm::normalize(rotationAxis);
            return glm::angleAxis(glm::radians(180.0f), rotationAxis);
        }

        rotationAxis = glm::cross(start, dest);

        float s = sqrt( (1+cosTheta)*2 );
        float invs = 1 / s;

        return glm::quat(
            s * 0.5f, 
            rotationAxis.x * invs,
            rotationAxis.y * invs,
            rotationAxis.z * invs
        );

    }

    void UpdateModelMatrix(TransformComponent &transform)
	{
        // TODO : this may be bad
		const glm::mat4 transformX = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::mat4 transformY = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::mat4 transformZ = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		// Y * X * Z
		const glm::mat4 roationMatrix = transformY * transformX * transformZ;

		transform.isDirty = false;

		// translation * rotation * scale (also know as TRS matrix)
		transform.modelMatrix = glm::translate(glm::mat4(1.0f), transform.position) * roationMatrix * glm::scale(glm::mat4(1.0f), transform.scale);

        /*glm::mat4 new_transform = glm::mat4(1);
        new_transform = glm::translate(new_transform, transform.position);
        new_transform = glm::rotate(new_transform, transform.rotation.x, glm::vec3(1, 0, 0));
        new_transform = glm::rotate(new_transform, transform.rotation.y, glm::vec3(0, 1, 0));
        new_transform = glm::rotate(new_transform, transform.rotation.z, glm::vec3(0, 0, 1));
        transform.modelMatrix = glm::scale(new_transform, transform.scale);*/
	}

    glm::mat4 GetModelMatrix(TransformComponent &transform)
	{
		if (transform.isDirty)
			UpdateModelMatrix(transform);

		return transform.modelMatrix;
	}

    void MoveTransformPosition(TransformComponent &transform, glm::vec3 offset)
    {
        transform.modelMatrix = glm::translate(transform.modelMatrix,offset);

        transform.position = glm::vec3(transform.modelMatrix[3]);
    }

    void SetTransformPosition(TransformComponent &transform, glm::vec3 position)
    {
        transform.position = position;

        transform.isDirty = true;

        UpdateModelMatrix(transform);
    }

    void RotateTransformRotation(TransformComponent &transform, glm::vec3 rotate)
    {
        transform.rotation += rotate;

        transform.isDirty = true;

        UpdateModelMatrix(transform);
    }

    void SetTransformRotation(TransformComponent &transform, glm::vec3 rotation)
    {
        transform.rotation = rotation;

        transform.isDirty = true;

        UpdateModelMatrix(transform);
    }
}