#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "common.hpp"
#include "render.hpp"
#include <entt/entt.hpp>
#include <box2d/box2d.h>

namespace game::entity
{

}
namespace game::entity::component
{
  struct box2D_body
  {
    b2BodyId body_id;
  };
  struct render_string
  {
    render::string string;
  };
}
namespace game
{
  namespace component = entity::component;
}

#endif // ENTITY_HPP