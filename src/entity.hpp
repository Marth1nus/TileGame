#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "common.hpp"
#include <entt/entt.hpp>
#include <box2d/box2d.h>

namespace game::entity::component
{
  struct transform
  {
    glm::vec3 position;
    glm::f32 rotation;
  };
  struct box2D_body
  {
    b2BodyId body_id;
  };
}
namespace game::entity
{

}

#endif // ENTITY_HPP