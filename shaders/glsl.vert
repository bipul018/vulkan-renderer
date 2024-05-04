#version 450

#extension GL_EXT_debug_printf : enable

layout (location = 0) in vec2 inVertPos;
layout (location = 0) out vec4 outVertPos;
layout (location = 1) out vec2 ogPos;
layout (set = 1, binding = 0) uniform UniformData {
       vec2 translate;
} unf;
layout (set = 0, binding = 0) uniform RotUni{
  float val;
} rot;

void main()
{
  /* //const array of positions for the triangle */
  /* const vec3 positions[3] = vec3[3]( */
  /* 	vec3(1.f,1.f, 0.0f), */
  /* 	vec3(-1.f,1.f, 0.0f), */
  /* 	vec3(0.f,-1.f, 0.0f) */
  /* ); */

  /* //output the position of each vertex */
  /* gl_Position = vec4(positions[gl_VertexIndex], 1.0f); */

  mat2 rotmat = mat2(cos(rot.val), -sin(rot.val), sin(rot.val), cos(rot.val));
  
  //outVertPos = vec4(  inVertPos + unf.translate, 0.5f, 1.0f);
  outVertPos = vec4(  rotmat*inVertPos + unf.translate , 0.5f, 1.0f);
  ogPos = 2* inVertPos;
  gl_Position = outVertPos;
  //debugPrintfEXT("Hello from vertex shader");
}
