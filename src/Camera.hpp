#pragma once

#include <glm/glm.hpp>

#include "Types.hpp"

struct Camera {
  glm::vec3 position = {0.0f, 2.0f, 0.0f};
  glm::vec3 forward = {0.0f, 0.0f, -1.0f};
  glm::vec3 up = {0.0f, 1.0f, 0.0f};
  float fieldOfView = 70.0f;
  float speed = 0.15f;

  Ray getRay(float u, float v, float aspect) const {
    float scale = glm::tan(glm::radians(fieldOfView * 0.5f));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    glm::vec3 actualUp = glm::cross(right, forward);

    // Map u, v (0 to 1) to (-aspect * scale to +aspect * scale)
    glm::vec3 dir = forward + (right * (2.0f * u - 1.0f) * aspect * scale) +
                    (actualUp * (1.0f - 2.0f * v) * scale);

    return {position, glm::normalize(dir)};
  }
};