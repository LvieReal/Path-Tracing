#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

#include <vector>

#include "Scene.hpp"
#include "Types.hpp"


const glm::vec3 sunDir = glm::normalize(glm::vec3(0.5f, 0.5f, 0.5f));
const float sunIntensity = 100.0f;
const glm::vec3 sunColor = glm::vec3(1.0f, 0.9f, 0.8f);

std::vector<Box> scene = {
    {// ground
     {{0.0f, 0.0f, 0.0f}, {50.0f, 1.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
     {Plastic, {0.9f, 0.9f, 0.9f}, 0.0f}},
    {// neon
     {{0.0f, 1.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 0.0f}},
     {Neon, {1.0f, 1.0f, 1.0f}, 10.0f}},
    {// box
     {{0.0f, 1.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},
     {Plastic, {0.9f, 0.9f, 0.9f}, 0.0f}},
    {// box
     {{0.0f, 1.0f, -3.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},
     {Metal, {0.9f, 0.9f, 0.9f}, 0.0f}},
    {// box
     {{3.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},
     {Plastic, {0.9f, 0.1f, 0.1f}, 0.0f}},
    {// box
     {{-3.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},
     {Plastic, {0.1f, 0.1f, 0.9f}, 0.0f}},
    {// roof
     {{0.0f, 3.0f, 0.0f}, {3.0f, 0.1f, 3.0f}, {0.0f, 0.0f, 0.0f}},
     {Plastic, {0.9f, 0.9f, 0.9f}, 0.0f}}};

HitRecord traceScene(const Ray &r) {
  HitRecord record;
  for (const auto &box : scene) {
    float t;
    glm::vec3 n;
    if (intersectBox(r, box, t, n) && t < record.t) {
      record.hit = true;
      record.t = t;
      record.normal = n;
      record.mat = box.mat;
      record.point = r.origin + r.dir * t;
    }
  }
  return record;
}

glm::vec3 skyColor(const Ray &r) {
  float t = 0.5f * (r.dir.y + 1.0f);
  glm::vec3 skyGradient =
      glm::mix(glm::vec3(0.5f, 0.7f, 1.0f), glm::vec3(0.03f, 0.05f, 0.1f), t);

  float alignment = glm::max(0.0f, glm::dot(r.dir, sunDir));

  float sunDisc = std::pow(alignment, 800.0f);

  return skyGradient + (sunColor * sunIntensity * sunDisc);
}

glm::vec3 getRayColor(Ray r, int depth, int maxDepth) {
  HitRecord hit = traceScene(r);
  if (!hit.hit)
    return skyColor(r);

  if (depth > maxDepth)
    return hit.mat.color;

  if (hit.mat.type == Neon)
    return hit.mat.color * hit.mat.emission;

  Ray nextRay;
  nextRay.origin = hit.point;

  if (hit.mat.type == Metal) {
    nextRay.dir = glm::reflect(r.dir, hit.normal);
    return hit.mat.color * getRayColor(nextRay, depth + 1, maxDepth);
  }

  glm::vec3 dir = glm::normalize(hit.normal + glm::sphericalRand(1.0f));
  nextRay.dir = glm::dot(dir, hit.normal) < 0.0f ? -dir : dir;
  return hit.mat.color * glm::max(glm::dot(nextRay.dir, hit.normal), 0.0f) *
         getRayColor(nextRay, depth + 1, maxDepth);
}