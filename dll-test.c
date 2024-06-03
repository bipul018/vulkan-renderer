#include "vectors.h"

#define DLL __declspec(dllexport)

DLL Vec2 mpos_to_world(Vec2 mpos, Vec2 cam_trans, Vec2 cam_scale, Vec2 dims, float angle){
  Vec2 ans = vec2_sub(mpos, vec2_scale_fl(dims, 0.5f));
  ans.y = -ans.y;
  Vec2 inv_scale = {.x = 1.f/cam_scale.x, .y = 1.f/cam_scale.y};
  ans = vec2_sub(ans, vec2_scale_vec(cam_trans, cam_scale));
  angle = angle * M_PI / 180.f;
  float c = cosf(-angle);
  float s = sinf(-angle);
  ans.x /= cam_scale.x;
  ans.y /= cam_scale.y;
  Vec2 prev_ans = ans;
  ans.x = c * prev_ans.x - s * prev_ans.y;
  ans.y = c * prev_ans.y + s * prev_ans.x;
  return ans;
}

DLL size_t get_max_used_count(void){
  return 5000;
}

DLL float get_yank_padding(void){
  return 4.4f;
}

DLL void* get_yank_range(size_t* count, size_t* offset, void* data){
  //data = 0;
  if(data == 0){
    data = (void*)1;
    *count = 1000;
    *offset = 0;
  }
  else{
    *offset =(*count + *offset) % get_max_used_count();
    *count = 1000;
  }
  return data;
}

DLL Vec3 get_yank_vel_n_noise(void){
  return (Vec3){0.f, 0.f, 0.1f};
}
