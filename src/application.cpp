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
    m_registry = {};
    b2DestroyWorld(std::exchange(m_world_id, {}));
    m_window = {};
  }
  auto application::run() -> int
  {
    m_time_update_stamp = glfwGetTime();
    m_running = true;
    setup();
    auto const frame = [this]
    {
      if (events())
      {
        update();
        render();
        return m_running;
      }
      return m_running = false;
    };
#if defined(__EMSCRIPTEN__)
    auto static frame_static = &frame;
    frame_static = &frame;
    auto static constexpr emscripten_main_loop = []
    {
      if (not(*frame_static)())
        emscripten_cancel_main_loop();
    };
    emscripten_set_main_loop(emscripten_main_loop, 0, 1);
#else  // defined(__EMSCRIPTEN__)
    while (frame()) // expected to be inlined
      ;
#endif // defined(__EMSCRIPTEN__)
    shutdown();
    return 0;
  }

  auto application::setup() -> void
  {
  }
  auto application::events() -> bool
  {
    glfwPollEvents();
    return m_running and_eq not glfwWindowShouldClose(m_window.get());
  }
  auto application::update() -> void
  {
    auto &old_stamp = m_time_update_stamp, new_stamp = glfwGetTime();
    if (new_stamp - old_stamp < m_dt)
      return;
    old_stamp = new_stamp;
  }
  auto application::render() -> void
  {
    glfwSwapInterval(1);

    auto frame_buffer_size = glm::ivec2{};
    glfwGetFramebufferSize(m_window.get(), &frame_buffer_size.x, &frame_buffer_size.y);
    glViewport(0, 0, frame_buffer_size.x, frame_buffer_size.y);
    glClearColor(0.1f, 0.1f, 0.1f, 0.1f);
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

    glfwSwapBuffers(m_window.get());
  }
  auto application::shutdown() -> void
  {
  }
}
