#include <Canis/App.hpp>
#include <Canis/Components.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Editor.hpp>
namespace Canis {
static ComponentConf combinedConf, vehicleConf;
void RegisterCombineMeshCollider(App &app) {
  combinedConf = {};
  REGISTER_PROPERTY(combinedConf, CombineMeshCollider, active);
  REGISTER_PROPERTY(combinedConf, CombineMeshCollider, friction);
  REGISTER_PROPERTY(combinedConf, CombineMeshCollider, weldDistance);
  DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(combinedConf, CombineMeshCollider,
                                        Transform);
  combinedConf.DrawInspector = [](Editor &editor, Entity &entity,
                                  const ScriptConf &definition) {
    auto &collider = entity.GetComponent<CombineMeshCollider>();
    DrawInspectorField(editor, "active", definition.name.c_str(),
                       collider.active);
    DrawInspectorField(editor, "friction", definition.name.c_str(),
                       collider.friction);
    DrawInspectorField(editor, "weldDistance", definition.name.c_str(),
                       collider.weldDistance);
    if (ImGui::Button("Rebuild combined collider"))
      ++collider.rebuildVersion;
    ImGui::Text("Static child models: %d / triangles: %d",
                collider.sourceModels, collider.triangleCount);
    ImGui::Text("Builds: %llu",
                static_cast<unsigned long long>(collider.rebuildCount));
  };
  app.RegisterComponent(combinedConf);
}
void RegisterWheeledVehicle(App &app) {
  vehicleConf = {};
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, wheels);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, centerOfMass);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, wheelRadius);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, wheelWidth);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, axleWidth);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, wheelbase);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, suspensionMin);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, suspensionMax);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, springFrequency);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, springDamping);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, steeringDegrees);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, rearSteerRatio);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, engineTorque);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, engineMaxRPM);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, brakeTorque);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, handbrakeTorque);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, longitudinalGrip);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, lateralGrip);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, antiRollStiffness);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, throttleResponse);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, steeringResponse);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, highSpeedSteeringRatio);
  REGISTER_PROPERTY(vehicleConf, WheeledVehicle, steeringFadeSpeedKph);
  vehicleConf.registry.drawers["wheels"] = [](Editor &editor,
                                              const std::string &,
                                              void *component,
                                              const std::string &suffix) {
    auto &vehicle = *static_cast<WheeledVehicle *>(component);
    vehicle.wheels.resize(4);
    const char *labels[] = {"Front left wheel", "Front right wheel",
                            "Rear left wheel", "Rear right wheel"};
    for (int i = 0; i < 4; ++i)
      DrawInspectorField(editor, labels[i], suffix.c_str(), vehicle.wheels[i]);
  };
  DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(vehicleConf, WheeledVehicle, Transform,
                                        Rigidbody);
  vehicleConf.Add = [](Entity &entity) {
    if (!entity.HasComponent<Transform>())
      entity.AddComponent<Transform>();
    auto *body = entity.HasComponent<Rigidbody>()
                     ? &entity.GetComponent<Rigidbody>()
                     : entity.AddComponent<Rigidbody>();
    body->motionType = RigidbodyMotionType::DYNAMIC;
    body->useGravity = true;
    entity.AddComponent<WheeledVehicle>();
  };
  vehicleConf.DrawInspector = [](Editor &editor, Entity &entity,
                                 const ScriptConf &definition) {
    auto &vehicle = entity.GetComponent<WheeledVehicle>();
    auto field = [&](const char *name, const char *help) {
      definition.registry.drawers.at(name)(editor, name, &vehicle,
                                           definition.name);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", help);
    };
    ImGui::TextDisabled("Settings update while driving.");
    if (ImGui::CollapsingHeader("Body balance",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      field("centerOfMass",
            "Optional child or descendant Transform. Its position sets the "
            "actual mass centre without moving the collision shape. Empty = "
            "automatic centre. Lower local Y for more stable cornering.");
      ImGui::TextDisabled("Move the marker's Transform to adjust balance.");
      if (vehicle.centerOfMass &&
          !vehicle.centerOfMass.HasComponent<Transform>())
        ImGui::TextWrapped("The centre marker needs a Transform.");
    }
    if (ImGui::CollapsingHeader("Power and brakes",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      field("engineTorque",
            "Peak torque in Nm. Higher values accelerate harder.");
      field("engineMaxRPM", "Engine RPM limit. The automatic gearbox and wheel "
                            "size also determine top speed.");
      field("throttleResponse", "Pedal response per second. 0 = instant; lower "
                                "positive values = gentler acceleration.");
      field("brakeTorque", "Brake torque per wheel in Nm. More torque gives "
                           "stronger braking, limited by tire grip.");
      field("handbrakeTorque", "Handbrake torque on each rear wheel in Nm.");
    }
    if (ImGui::CollapsingHeader("Steering and grip",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      field("steeringDegrees",
            "Maximum front wheel steering angle in degrees.");
      field("rearSteerRatio", "Rear steering relative to front. Negative = "
                              "counter-steering; 0 = front steering only.");
      field("steeringResponse",
            "Steering response per second. 0 = instant; lower positive values "
            "= smoother/slower turns.");
      field("highSpeedSteeringRatio", "Steering remaining at high speed: 1 = "
                                      "full steering, 0.35 = 35 percent.");
      field("steeringFadeSpeedKph",
            "Speed in km/h at which highSpeedSteeringRatio is reached; fades "
            "progressively from rest.");
      field("longitudinalGrip", "Acceleration and braking grip multiplier. 1 = "
                                "normal tires; lower = slippery.");
      field("lateralGrip", "Cornering grip multiplier. Lower values make "
                           "sideways sliding easier.");
    }
    if (ImGui::CollapsingHeader("Suspension")) {
      field("suspensionMin", "Minimum suspension length in metres.");
      field("suspensionMax",
            "Maximum suspension length in metres. Changes wheel travel.");
      field("springFrequency",
            "Suspension spring frequency in Hz. Higher = stiffer ride.");
      field("springDamping", "Damping ratio. Higher values reduce bouncing.");
      field("antiRollStiffness",
            "Anti-roll spring stiffness in N/m on both axles. 0 = disabled; "
            "higher = less body roll.");
    }
    if (ImGui::CollapsingHeader("Wheel geometry and visuals")) {
      field("wheelRadius",
            "Physical wheel radius in metres; match the tire model.");
      field("wheelWidth", "Physical wheel width in metres.");
      field("axleWidth",
            "Distance between wheel centres across the chassis in metres.");
      field("wheelbase", "Distance between front and rear axles in metres.");
      field("wheels", "Child entities animated by the native suspension.");
    }
    ImGui::Text("Wheel contacts: %d / 4", vehicle.groundedWheels);
    ImGui::Text("Speed: %.1f km/h", vehicle.speed * 3.6f);
    ImGui::Text("Physics mass centre: %.2f, %.2f, %.2f m",
                vehicle.effectiveCenterOfMass.x,
                vehicle.effectiveCenterOfMass.y,
                vehicle.effectiveCenterOfMass.z);
    ImGui::Text("Applied throttle: %.2f / steering: %.2f",
                vehicle.appliedThrottle, vehicle.appliedSteering);
    ImGui::Text("Constraint builds: %llu",
                static_cast<unsigned long long>(vehicle.rebuildCount));
  };
  app.RegisterComponent(vehicleConf);
}
} // namespace Canis
