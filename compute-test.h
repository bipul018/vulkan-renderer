#pragma once
#include "device-mem-stuff.h"
#include "common-stuff.h" 
#include "compute-interface.h"
#include "vectors.h"


typedef struct TestComputePubData TestComputePubData;
struct TestComputePubData {
  bool paused;
  size_t max_used;

  //An 'immediate rewrite' command
  //Source will be copied onto an staging buffer,
  //so can be freed on return from run_test_compute, but must remain till then
  //Both of these are reset on each run,
  
  size_t rewrite_pos_offset;
  u8Slice rewrite_pos_source;
  bool do_rewrite_pos;
  
  size_t rewrite_vel_offset;
  u8Slice rewrite_vel_source;
  bool do_rewrite_vel;
  
};
DEF_SLICE(Vec2);
typedef struct TestComputeInitParam TestComputeInitParam;
struct TestComputeInitParam {
  TestComputePubData* pub_data;
  Vec2Slice init_tex_items;
  Vec2Slice init_pos_items;
  Vec2Slice init_vel_items;
};

ComputeJobInitOutput init_test_compute(void* any_init_param, ComputeJobParam* params);
VkCommandBuffer run_test_compute(void* pdata, ComputeJobParam* params);
void clean_test_compute(void* pdata, ComputeJobParam* params );


