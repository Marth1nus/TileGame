#version 300 es

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
  // vec2 mp = mpos;
  // float s = 0.01f;
  // if (mp.x <= s || 1.0f - s <= mp.x
  // ||  mp.y <= s || 1.0f - s <= mp.y)
  //   color = vec4(1);
}