//glsl version 4.5
#version 450

//output write
layout (location = 0) out vec4 outFragColor;
layout (location = 0) in vec4 inVertPos;
void main()
{
  //return red
  outFragColor = vec4(1.f,0.f,0.f,1.0f);
}
