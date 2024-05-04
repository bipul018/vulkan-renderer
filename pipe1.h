#pragma once
//This is for pipeline category 1
#include "common-stuff.h"
#include "vectors.h"

//Vertex input structs or typedefs, one per binding

typedef Vec2 Pipe1Input0;

//Descriptor structs or typedefs, one per binding
typedef Vec2 Pipe1Desc1_0;
typedef float Pipe1Des0_0;
typedef struct{
  VkSampler sampler;
  VkImageView texture2d;
} Pipe1Desc2_0;

//Create descriptor layouts, one per set, : maybe store as a global static handle



//Create pipeline layout, one per set : also store this as a global static handle

//Create pipeline, takes in shader names, maybe some other options, but is not globally stored


