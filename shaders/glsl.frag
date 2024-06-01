#version 460
#include "glsl.h"


layout (location = 0) out vec4 outFragColor;
layout (location = 0) in vec4 inVertPos;
layout (location = 1) in vec2 ogPos;

layout (set = TEXTURE_DATA_UNI_SET, binding = SAMPLER_UNI_BINDING) uniform sampler smplr;
layout (set = TEXTURE_DATA_UNI_SET, binding = TEXTURE_UNI_BINDING) uniform texture2D tex;

void main()
{
  //return red
  //outFragColor = vec4(1.f,0.f,0.f,1.0f);
  outFragColor = texture(sampler2D(tex, smplr) ,ogPos);
  outFragColor = vec4(0.4,0,0.4,1);
  
}
