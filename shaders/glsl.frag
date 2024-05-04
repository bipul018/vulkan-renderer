//glsl version 4.5
#version 450

//output write
layout (location = 0) out vec4 outFragColor;
layout (location = 0) in vec4 inVertPos;
layout (location = 1) in vec2 ogPos;

layout (set = 2, binding = 0) uniform sampler smplr;
layout (set = 2, binding = 1) uniform texture2D tex;

void main()
{
  //return red
  //outFragColor = vec4(1.f,0.f,0.f,1.0f);
  outFragColor = texture(sampler2D(tex, smplr) ,ogPos);

}
