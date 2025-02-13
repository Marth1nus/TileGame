#include "application.hpp"

int main(int c_argc, char const *const *c_argv)
{
  return game::application{}.run();
}