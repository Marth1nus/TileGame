#include "application.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else  // defined(__EMSCRIPTEN__)
#endif // defined(__EMSCRIPTEN__)

#if defined(USE_GLAD)
#include <glad/glad.h>
#else // defined(USE_GLAD)
#include <GLES3/gl3.h>
#endif // defined(USE_GLAD)

#include <GLFW/glfw3.h>
#include <lua.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace game
{
  application::application()
  {
    { // m_window
      struct glfw
      {
        glfw() { utils::assertf(glfwInit(), "%s %s", "glfw", "init fail"); }
        ~glfw() { glfwTerminate(); }
      } static const glfw{};
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
      auto window_title = "Game";
      auto window_width = 720, window_height = window_width;
      m_window = {glfwCreateWindow(window_width, window_height, window_title, 0, 0),
                  glfwDestroyWindow};
      auto window = m_window.get();
      utils::assertf(window, "%s %s", "window", "init fail");
      glfwMakeContextCurrent(window);
    }
    { // m_renderer + m_tile_meshes
#if defined(USE_GLAD)
      utils::assertf(gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress), "%s %s", "glad", "init fail");
#else  // defined(USE_GLAD)
#endif // defined(USE_GLAD)
    }
    { // m_world_id
      auto world_def = b2DefaultWorldDef();
      world_def.gravity = {0, 10};
      m_world_id = b2CreateWorld(&world_def);
    }
    { // m_L
      m_L = {luaL_newstate(), lua_close};
      auto const L = m_L.get();
      utils::assertf(L, "%s %s", "lua", "init fail");
      luaL_openlibs(L);
    }
    { // imgui
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO &io = ImGui::GetIO();
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
      io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
      io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
      ImGui::StyleColorsDark();
      ImGui_ImplGlfw_InitForOpenGL(m_window.get(), true);
#ifdef __EMSCRIPTEN__
      ImGui_ImplGlfw_InstallEmscriptenCallbacks(m_window.get(), "#canvas");
      io.IniFilename = nullptr;
#endif
      ImGui_ImplOpenGL3_Init("#version 300 es");
    }
  }
  application::~application()
  {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_L = {};
    for (auto body_id : std::exchange(m_bodies, {}))
      b2DestroyBody(body_id);
    b2DestroyWorld(std::exchange(m_world_id, {}));
    m_tile_meshes.clear();
    m_renderer = {};
    m_window = {};
  }
  auto application::run() -> int
  {
    m_time_update_stamp = glfwGetTime();
    m_running = true;
    setup();
#if defined(__EMSCRIPTEN__)
    auto static loop = [this]
    {
      if (m_running and events())
      {
        update();
        render();
      }
      else
        emscripten_cancel_main_loop();
    };
    emscripten_set_main_loop([]
                             { loop(); }, 0, 1);
#else  // defined(__EMSCRIPTEN__)
    while (m_running and events())
    {
      update();
      render();
    }
#endif // defined(__EMSCRIPTEN__)
    shutdown();
    return 0;
  }

  auto application::setup() -> void
  {
    auto const vw = 16;
    { // Renderer test
      auto width = 0, height = 0, channels = 4;
      auto pixels = std::shared_ptr<uint8_t>{
          stbi_load("res/rpg-asset-pack/3x/RPG tileset (full) v1.7 - 300%.png",
                    &width, &height, &channels, channels),
          stbi_image_free};
      utils::assertf(pixels, "%s", "failed to load tile atlas image");
      auto tile_pixels_size = glm::uvec2{48, 48};
      auto atlas_tiles_size = glm::uvec3{glm::uvec2{width, height} / tile_pixels_size, 1};
      auto atlas_pixels_size = atlas_tiles_size * glm::uvec3{tile_pixels_size, 1};
      auto pixels_span = std::span{pixels.get(), sizeof(uint8_t[4]) * atlas_pixels_size.x * atlas_pixels_size.y * atlas_pixels_size.z};
      m_renderer = {
          utils::read_all("res/shader/tile.vert.glsl"),
          utils::read_all("res/shader/tile.frag.glsl"),
          tile_pixels_size,
          atlas_tiles_size,
          // pixels_span,
      };
      m_renderer.upload_tile_textures(1, pixels_span, width);
      m_renderer.uniform_prep();
      m_renderer.uniform("projection", glm::ortho<float>(-vw, vw, vw, -vw));
      m_tile_meshes.clear();
      m_tile_meshes
          .emplace_back(glm::uvec2{5, 5}) // make a 4x4 tile chunk
          .upload(
              // tile ids
              std::array<uint32_t, 5 * 5 * 3>{
                  //
                  01, 02, 00, 02, 03,
                  33, 00, 02, 00, 35,
                  00, 33, 34, 35, 00,
                  33, 00, 66, 00, 35,
                  65, 66, 00, 66, 67, //
                  //
                  00 + 00, 00 + 00, 96 + 02, 00 + 00, 00 + 00,
                  00 + 00, 96 + 01, 00 + 00, 96 + 03, 00 + 00,
                  96 + 33, 00 + 00, 96 + 34, 00 + 00, 96 + 35,
                  00 + 00, 96 + 65, 00 + 00, 96 + 67, 00 + 00,
                  00 + 00, 00 + 00, 96 + 66, 00 + 00, 00 + 00, //
                  //
                  00, 00, 00, 00, 00,
                  00, 00, 00, 00, 00,
                  00, 00, 00, 00, 00,
                  00, 00, 00, 00, 00,
                  00, 00, 00, 00, 00, //
              },
              // chunk positions
              std::array<glm::vec3, 3>{});
    }
    { // B2World test
      auto const b = (float)vw;
      for (auto [x, y, w, h] : {
               std::array{0.0f, +b + 0, b * b, 0.5f},
               std::array{-b - 1, 0.0f, 0.5f, b * b},
               std::array{+b + 0, 0.0f, 0.5f, b * b},
           })
      {
        b2BodyDef ground_def = b2DefaultBodyDef();
        ground_def.position = {x, y};
        auto ground_id = b2CreateBody(m_world_id, &ground_def);
        auto ground_box = b2MakeBox(w, h);
        auto ground_shape_def = b2DefaultShapeDef();
        b2CreatePolygonShape(ground_id, &ground_shape_def, &ground_box);
        m_bodies.push_back(ground_id);
      }
      for (auto i = 0u; i < vw * vw; i++)
      {
        auto body_def = b2DefaultBodyDef();
        body_def.type = b2_dynamicBody;
        body_def.position = b2Vec2{float(i & 1) * 0.5f, -float(i)};
        auto body_id = b2CreateBody(m_world_id, &body_def);
        auto dynamic_box = b2MakeBox(0.5f, 0.5f);
        auto shape_def = b2DefaultShapeDef();
        shape_def.density = 1;
        shape_def.friction = 0.3f;
        b2CreatePolygonShape(body_id, &shape_def, &dynamic_box);
        m_bodies.push_back(body_id);
      }
      m_tile_meshes
          .emplace_back(glm::uvec2{1, 1})
          .upload(std::vector<uint32_t>((size_t)m_bodies.size(), 9u),
                  std::vector<glm::vec3>((size_t)m_bodies.size(), glm::vec3{}));
    }
  }
  auto application::events() -> bool
  {
    glfwPollEvents();
    return not glfwWindowShouldClose(m_window.get());
  }
  auto application::update() -> void
  {
    auto &old_stamp = m_time_update_stamp, new_stamp = glfwGetTime();
    if (new_stamp - old_stamp < m_dt)
      return;
    old_stamp = new_stamp;
    auto static i = 0;
    std::printf("| Update:%4d | Time: %9.6lfs |\n", i++, glfwGetTime());
    b2World_Step(m_world_id, m_dt, 8);
    { // display b2world boxes
      auto static bodies_positions = std::vector<glm::vec3>{};
      bodies_positions.resize(m_bodies.size());
      for (auto i = 0u; i < m_bodies.size(); i++)
      {
        auto body_id = m_bodies.at(i);
        auto pos = b2Body_GetPosition(body_id);
        auto rot = b2Body_GetRotation(body_id);
        bodies_positions.at(i) = {pos.x, pos.y, 1};
      }
      m_tile_meshes.at(1).update(bodies_positions);
    }
    if (m_tile_meshes.size() > 1) // Renderer House-follow-Mouse
    {
      auto mx = 0.0, my = mx;
      glfwGetCursorPos(m_window.get(), &mx, &my);
      auto ww = 0, wh = ww;
      glfwGetWindowSize(m_window.get(), &ww, &wh);
      auto p = glm::clamp(glm::vec2{mx / ww, my / wh} * 32.f - 16.f - 2.5f, {-16, -16}, {11, 11});
      auto static p0 = p, p1 = p, p2 = p;
      p0 += (p - p0) * 0.08f * (float)m_dt * (float)60;
      p1 += (p - p1) * 0.04f * (float)m_dt * (float)60;
      p2 += (p - p2) * 0.06f * (float)m_dt * (float)60;
      m_tile_meshes.at(0).update(std::array{
          glm::vec3{p0, 2},
          glm::vec3{p1, 1},
          glm::vec3{p2, 3}, //
      });
      auto house_pos =
          glm::uvec2{10, 14} +
          glm::uvec2{5, 5} *
              std::array{
                  glm::uvec2{0, 0},
                  glm::uvec2{1, 0},
                  glm::uvec2{0, 1},
                  glm::uvec2{1, 1},
              }
                  .at(int(glfwGetTime()) % 4);
      auto house = std::array<uint32_t, 5 * 5>{};
      for (auto i = 0; i < 5; i++)
        for (auto j = 0; j < 5; j++)
          house.at(5 * i + j) = 1 + 32 * (house_pos.x + i) + (house_pos.y + j);
      m_tile_meshes.at(0).update(house, 2);
    }
  }
  auto application::render() -> void
  {
    glfwSwapInterval(1);

    auto frame_buffer_size = glm::ivec2{};
    glfwGetFramebufferSize(m_window.get(), &frame_buffer_size.x, &frame_buffer_size.y);
    glViewport(0, 0, frame_buffer_size.x, frame_buffer_size.y);
    glClearColor(0.1, 0.1, 0.1, 0.1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    { // ImGui
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      auto &io = ImGui::GetIO();

      if (auto static ShowDemoWindow_state = true; ShowDemoWindow_state)
        ImGui::ShowDemoWindow(&ShowDemoWindow_state); // Show demo window! :)

      if (auto static statistics = true; statistics)
      {
        ImGui::Begin("Statistics", &statistics);
        auto constexpr avg_frame_time_length = 20.0f;
        auto static avg_frame_time = io.DeltaTime;
        avg_frame_time = (avg_frame_time * (avg_frame_time_length - 1.0f) + io.DeltaTime) / avg_frame_time_length;
        ImGui::Text("Average Frame Time: %06.3fms", avg_frame_time * 1000.0f);
        if (auto dt = (float)m_dt;
            ImGui::SliderFloat("Update Delta Time", &dt, 0.001f, 0.1f, "%6.3f"))
          m_dt = dt;
        ImGui::End();
      }

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
      {
        auto const current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(current_context);
      }
    }

    m_renderer.uniform_prep();

    for (auto const &tile_mesh : m_tile_meshes)
      m_renderer.render(tile_mesh);

    glfwSwapBuffers(m_window.get());
  }
  auto application::shutdown() -> void
  {
  }
}
