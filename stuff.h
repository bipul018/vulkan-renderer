#include "common-stuff.h"

#include "window-stuff.h"


//Let's use all global states only
void init_stuff(VulkanDevice* device, AllocInterface allocr, VkRenderPass render_pass, u32 frames);


void event_stuff(MSG translated_msg);
void update_stuff(int width, int height, u32 frame_inx);
//If it returns not null handle, 
VkSemaphore render_stuff(u32 frame_inx, VkCommandBuffer cmd_buf);
void clean_stuff(void);


