#version 300 es
precision highp float;
precision highp sampler2DArray;

in vec2 fragment_pos;
in vec3 fragment_uv;

uniform sampler2DArray atlas;

out vec4 frag_color;

void main()
{
  frag_color = texture(atlas, fragment_uv);
}