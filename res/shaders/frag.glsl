#version 300 es
#define TEXTURE_SLOTS ###u
#define WEBGL         ###

precision highp float;
precision highp sampler2D;

in vec2 mpos;
in vec2 pos;
in vec2 uv;
flat in uint tex;

uniform sampler2D textures[TEXTURE_SLOTS];

out vec4 color;

vec4 get_texture_color(uint tex, vec2 uv)
{
#if WEBGL
  for (uint i = 0u; i < TEXTURE_SLOTS; i++)
    if (i == tex)
      return texture(textures[i], uv); 
  return texture(textures[0], uv); 
#else
  return texture(textures[tex], uv);
#endif
}

void main()
{
  color = get_texture_color(tex, uv);
  vec2 mp = abs(mpos - 0.5f) - 4.f;
  if (mp.x > 0.f || mp.y > 0.f)
    color = vec4(1);
}