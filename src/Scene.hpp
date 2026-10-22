#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Types.hpp"

bool intersectBox(const Ray &worldRay, const Box &b, float &tOut,
                  glm::vec3 &nOut) {
  glm::vec3 localOrigin =
      glm::vec3(b.inverseModel * glm::vec4(worldRay.origin, 1.0f));
  glm::vec3 localDir =
      glm::vec3(b.inverseModel * glm::vec4(worldRay.dir, 0.0f));

  glm::vec3 invDir = 1.0f / localDir;
  glm::vec3 t0 = (-0.5f - localOrigin) * invDir;
  glm::vec3 t1 = (0.5f - localOrigin) * invDir;

  glm::vec3 tmin = glm::min(t0, t1);
  glm::vec3 tmax = glm::max(t0, t1);

  float tnear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
  float tfar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

  if (tnear >= tfar || tfar <= 0.001f)
    return false;

  tOut = tnear > 0.001f ? tnear : tfar;

  glm::vec3 p = localOrigin + localDir * tOut;
  glm::vec3 absP = glm::abs(p);
  glm::vec3 localNormal;
  if (absP.x > absP.y && absP.x > absP.z)
    localNormal = glm::vec3(glm::sign(p.x), 0, 0);
  else if (absP.y > absP.z)
    localNormal = glm::vec3(0, glm::sign(p.y), 0);
  else
    localNormal = glm::vec3(0, 0, glm::sign(p.z));

  nOut =
      glm::normalize(glm::vec3(b.normalMatrix * glm::vec4(localNormal, 0.0f)));

  return true;
}