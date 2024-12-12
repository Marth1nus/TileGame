#define TEXTURE_SLOTS 16u
#define WEBGL          0
// Everything before here is ignored (defines added back at pre-compile-time)
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
  switch (int(tex)){ case 0: return texture(textures[0], uv);case 1: return texture(textures[1], uv);case 2: return texture(textures[2], uv);case 3: return texture(textures[3], uv);case 4: return texture(textures[4], uv);case 5: return texture(textures[5], uv);case 6: return texture(textures[6], uv);case 7: return texture(textures[7], uv);case 8: return texture(textures[8], uv);case 9: return texture(textures[9], uv);case 10: return texture(textures[10], uv);case 11: return texture(textures[11], uv);case 12: return texture(textures[12], uv);case 13: return texture(textures[13], uv);case 14: return texture(textures[14], uv);case 15: return texture(textures[15], uv); default: return texture(textures[0], uv); }
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