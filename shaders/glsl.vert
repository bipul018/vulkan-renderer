#version 460
#include "glsl.h"
#extension GL_EXT_debug_printf : enable

layout (location = 0) in vec2 inVertPos;
layout (location = 1) in vec2 inTexPos;
layout (location = 0) out vec4 outVertPos;
layout (location = 1) out vec2 outTexPos;

layout ( set = TRANSFORM_UNI_SET, binding = TRANSFORM_UNI_SET) uniform TransformData{
  mat2 matrix;
  vec2 translate;
} transform;

void main()
{
  gl_PointSize = 1;
		 
  outVertPos = vec4(  transform.matrix*inVertPos + transform.translate , 0.5f, 1.0f);

  outTexPos = inTexPos;
  gl_Position = outVertPos;
}
