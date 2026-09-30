#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum MaterialType { Plastic, Metal, Neon };

struct Material {
    MaterialType type;
    glm::vec3 color;
    float emission;
};

struct Ray {
    glm::vec3 origin, dir;
};

struct HitRecord {
    bool hit = false;
    float t = 99999.0f;
    glm::vec3 normal;
    Material mat;
    glm::vec3 point;
};

struct Transform {
    glm::vec3 position;
    glm::vec3 size;     // Half-extents (e.g., size of 1 means from -0.5 to 0.5)
    glm::vec3 rotation; // Euler angles in degrees

    glm::mat4 getModelMatrix() const {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
        m = glm::rotate(m, glm::radians(rotation.x), {1, 0, 0});
        m = glm::rotate(m, glm::radians(rotation.y), {0, 1, 0});
        m = glm::rotate(m, glm::radians(rotation.z), {0, 0, 1});
        m = glm::scale(m, size);
        return m;
    }
};

struct Box {
    Transform transform;
    Material mat;

    // Cache these if they don't change every frame to save CPU cycles!
    glm::mat4 inverseModel;
    glm::mat3 normalMatrix;
};