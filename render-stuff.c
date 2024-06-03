#include "render-stuff.h"
#include <stdio.h>
#include <string.h>

/* int create_render_pass(StackAllocator* stk_allocr, size_t stk_offset, */
/*                        const VkAllocationCallbacks* alloc_callbacks, */
/*                        CreateRenderPassParam param) { */

OptRenderPass create_render_pass(VkDevice device, VkFormat img_format, VkFormat depth_stencil_format){

    VkResult result = VK_SUCCESS;
    OptRenderPass render_pass = {0};

    VkAttachmentDescription attachments[] = {
      {
	.format = img_format,
	.samples = VK_SAMPLE_COUNT_1_BIT,
	.loadOp =
	VK_ATTACHMENT_LOAD_OP_CLEAR, // Determines what to do to
	// attachment before render
	.storeOp =
	VK_ATTACHMENT_STORE_OP_STORE, // Whether to store rendered
	// things back or not
	.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      },
      {
	.format = depth_stencil_format,
	.samples = VK_SAMPLE_COUNT_1_BIT,
	.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
	.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	.finalLayout =
	VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      },
    };

    VkAttachmentReference color_attach_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_attach_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    // Determines the shader input / output refrenced in the subpass
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attach_ref,
        .pDepthStencilAttachment = &depth_attach_ref,
    };

    VkSubpassDependency subpass_dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = _countof(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    };

    result = vkCreateRenderPass(device, &create_info,
                                get_glob_vk_alloc(), &render_pass.value);

    if (result != VK_SUCCESS)
      render_pass.code = CREATE_RENDER_PASS_FAILED;

    return render_pass;
}

OptRenderPass clear_render_pass(OptRenderPass render_pass, VkDevice device){
    switch (render_pass.code) {
    case CREATE_RENDER_PASS_OK:
        vkDestroyRenderPass(device, render_pass.value,
                            get_glob_vk_alloc());
    case CREATE_RENDER_PASS_FAILED:
    case CREATE_RENDER_PASS_TOP_FAIL_CODE:
      render_pass = (OptRenderPass){0};
    }
    return render_pass;
}

/* uint32_t* read_spirv_in_stk_allocr(StackAllocator* stk_allocr, */
/*                                    size_t* p_stk_offset, */
/*                                    const char* file_name, */
/*                                    size_t* p_file_size) { */

u32Slice read_binary_u32(AllocInterface allocr,
			const char* file_name){
  /* if (!stk_allocr || !p_stk_offset) */
  /*       return nullptr; */

    FILE* file = fopen(file_name, "rb");
    u32Slice buffer = {0};
    size_t size = 0;
    /* if (p_file_size) */
    /*     *p_file_size = 0; */
    if (!file) {
      return buffer;
    }

    fseek(file, 0, SEEK_END);
    //size_t file_size = ftell(file);
    buffer = SLICE_ALLOC(u32, (_align_up(ftell(file), sizeof(u32)))/sizeof(u32),
			 allocr);
    fseek(file, 0, SEEK_SET);
    if(nullptr == buffer.data){
      fclose(file);
      return buffer;
    }
    /* uint32_t* buffer = */
    /*     stack_allocate(stk_allocr, p_stk_offset, */
    /*                    align_up_(file_size, 4), sizeof(uint32_t)); */

    memset(buffer.data, 0, u32_slice_bytes(buffer));
    //memset(buffer, 0, align_up_(file_size, 4));
    /* if (!buffer) */
    /*   return nullptr; */

    fread(buffer.data, 1, u32_slice_bytes(buffer), file);
    /* file_size = align_up_(file_size, 4); */
    /* if (p_file_size) */
    /*     *p_file_size = file_size; */

    fclose(file);

    return buffer;
}

/* int create_shader_module_from_file(StackAllocator* stk_allocr, */
/*                                    size_t stk_offset, */
/*                                    const VkAllocationCallbacks* */
/*                                    alloc_callbacks, */
/*                                    CreateShaderModuleFromFileParam */
                                   /* param) { */
OptShaderModule create_shader_module_from_file(AllocInterface allocr,
					       VkDevice device,
					       const char* shader_file_name){

    VkResult result = VK_SUCCESS;
    OptShaderModule shader = {0};

    //size_t code_size = 0;
    /* uint32_t* shader_code = read_spirv_in_stk_allocr( */
    /*     stk_allocr, &stk_offset, param.shader_file_name, &code_size); */
    u32Slice shader_code = read_binary_u32(allocr, shader_file_name);
    if (nullptr == shader_code.data){
        shader.code = CREATE_SHADER_MODULE_FROM_FILE_READ_FILE_ERROR;
	return shader;
    }

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = shader_code.data,
        .codeSize = u32_slice_bytes(shader_code),
    };

    result =
        vkCreateShaderModule(device, &create_info,
                             get_glob_vk_alloc(), &shader.value);
    SLICE_FREE(shader_code, allocr);
    if (result != VK_SUCCESS)
        shader.code = CREATE_SHADER_MODULE_FROM_FILE_ERROR;

    return shader;
}

GraphicsPipelineCreationInfos
default_graphics_pipeline_creation_infos() {
    GraphicsPipelineCreationInfos infos;

    infos
        .input_assembly_state = (
            VkPipelineInputAssemblyStateCreateInfo){
            .sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

    static const VkDynamicState default_dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    infos.dynamic_state = (VkPipelineDynamicStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = _countof(default_dynamic_states),
        .pDynamicStates = default_dynamic_states,
    };

    infos.viewport_state = (VkPipelineViewportStateCreateInfo){
        .sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    infos
        .rasterization_state = (VkPipelineRasterizationStateCreateInfo
        ){
            .sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .lineWidth = 1.f,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE
        };

    infos.multisample_state = (VkPipelineMultisampleStateCreateInfo){
        .sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Place for depth/stencil testing
    infos
        .depth_stencil_state = (VkPipelineDepthStencilStateCreateInfo)
        {
            .sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
        };

    static const VkPipelineColorBlendAttachmentState
        default_color_blend_attaches[] = {
            { .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
              VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
              VK_COLOR_COMPONENT_A_BIT,
              .blendEnable = VK_FALSE }
        };

    infos.color_blend_state = (VkPipelineColorBlendStateCreateInfo){
        .sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .pAttachments = default_color_blend_attaches,
        .attachmentCount = _countof(default_color_blend_attaches),
    };

    return infos;
}

/* int create_graphics_pipeline(StackAllocator* stk_allocr, */
/*                              size_t stk_offset, */
/*                              VkAllocationCallbacks* alloc_callbacks, */
/*                              CreateGraphicsPipelineParam param) { */


typedef struct ShaderCreateInfo ShaderCreateInfo;
struct ShaderCreateInfo {
  const char* file_name;
  VkShaderStageFlagBits stage;
};

DEF_SLICE(ShaderCreateInfo);
DEF_SLICE(VkShaderModule);

DEF_SLICE(VkPipelineShaderStageCreateInfo);
DEF_SLICE_PAIR(VkShaderModule, VkPipelineShaderStageCreateInfo);
typedef VkShaderModuleVkPipelineShaderStageCreateInfoSlicePair ShaderSlicePair;
 
OptPipeline create_graphics_pipeline(AllocInterface allocr, VkDevice device,
				     CreateGraphicsPipelineParam param){
    VkResult result = VK_SUCCESS;
    OptPipeline pipeline = {0};

    uint32_t shader_count = 0;

    VkShaderModule shader_modules_buffer[3] = { 0 };
    //VkShaderModuleSlice shader_modules = {.data = shader_modules_buffer};

    VkPipelineShaderStageCreateInfo shader_create_infos_buffer[3] = {0};
    //VkPipelineShaderStageCreateInfoSlice shader_create_infos = {.data = shader_infos_buffer};
    ShaderSlicePair used_shaders ={
      .data1 = shader_modules_buffer,
      .data2 = shader_create_infos_buffer
    };

    //Must provide vertex and fragment shader names
    if((nullptr == param.shaders.vert) ||
       (nullptr == param.shaders.frag)){
      pipeline.code = CREATE_GRAPHICS_PIPELINE_VERT_FRAG_SHADER_NAME_ERR;
      return pipeline;
    }
    
    ShaderCreateInfoSlice shader_infos =
      SLICE_FROM_ARRAY(ShaderCreateInfo,
		       ((ShaderCreateInfo[]){
			 {
			   .file_name = param.shaders.vert,
			   .stage = VK_SHADER_STAGE_VERTEX_BIT,
			 },
			 {
			   .file_name = param.shaders.frag,
			   .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			 },
			 {
			   .file_name = param.shaders.geom,
			   .stage = VK_SHADER_STAGE_GEOMETRY_BIT
			 }}));

    
    for_slice(shader_infos, i){
      if(nullptr == shader_infos.data[i].file_name)
	continue;
      OptShaderModule shader_module =
	create_shader_module_from_file(allocr, device, shader_infos.data[i].file_name);
      if(shader_module.code >= 0){
	used_shaders.data1[used_shaders.count] = shader_module.value;
	used_shaders.data2[used_shaders.count] = (VkPipelineShaderStageCreateInfo)
	  {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	    .stage = shader_infos.data[i].stage,
	    .module = shader_module.value,
	    .pName = "main",
	  };
	used_shaders.count++;
      }
      else{

	pipeline.code = CREATE_GRAPHICS_PIPELINE_SHADER_MODULES_CREATION_FAILED;
	goto cleanup;
	break;
      }
    }

    
    VkPipelineVertexInputStateCreateInfo vertex_input_state = {
      .sType =VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pVertexBindingDescriptions = param.vert_bindings.data,
      .vertexBindingDescriptionCount = param.vert_bindings.count,
      .pVertexAttributeDescriptions = param.vert_attrs.data,
      .vertexAttributeDescriptionCount = param.vert_attrs.count
    };

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = used_shaders.count,
        .pStages = shader_create_infos_buffer,
	
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState =
        &param.create_infos.input_assembly_state,
        .pViewportState = &param.create_infos.viewport_state,
        .pRasterizationState =
        &param.create_infos.rasterization_state,
        .pMultisampleState = &param.create_infos.multisample_state,
        .pDepthStencilState = &param.create_infos.depth_stencil_state,
        .pColorBlendState = &param.create_infos.color_blend_state,
        .pDynamicState = &param.create_infos.dynamic_state,

        .layout = param.pipe_layout,
        .basePipelineIndex = -1,

        .renderPass = param.compatible_render_pass,
        .subpass = param.subpass_index,

    };

    result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &create_info,
        get_glob_vk_alloc(),
        &pipeline.value);

    
    if (result != VK_SUCCESS)
      pipeline.code = CREATE_GRAPHICS_PIPELINE_FAILED;
    
 cleanup:
    
    //for (int i = 0; i < COUNT_OF(shader_modules); ++i) {
    for_slice(used_shaders, i){
      vkDestroyShaderModule(device, shader_modules_buffer[i],
			    get_glob_vk_alloc());
    }

    return pipeline;
}

/* void clear_graphics_pipeline( */
/*     const VkAllocationCallbacks* alloc_callbacks, */
/*     ClearGraphicsPipelineParam param, int err_code) { */

OptPipeline clear_graphics_pipeline(OptPipeline pipeline, VkDevice device){

    switch (pipeline.code) {
    case CREATE_GRAPHICS_PIPELINE_OK:
        vkDestroyPipeline(device, pipeline.value,
                          get_glob_vk_alloc());
    case CREATE_GRAPHICS_PIPELINE_FAILED:
    case CREATE_GRAPHICS_PIPELINE_SHADER_MODULES_CREATION_FAILED:
    case CREATE_GRAPHICS_PIPELINE_VERT_FRAG_SHADER_NAME_ERR:
    case CREATE_GRAPHICS_PIPELINE_TOP_FAIL_CODE:
      pipeline = (OptPipeline){0};


    }
    return pipeline;
}


/* int create_semaphores(StackAllocator* stk_allocr, size_t stk_offset, */
/*                       const VkAllocationCallbacks* alloc_callbacks, */
/*                       CreateSemaphoresParam param) { */

OptSemaphores create_semaphores(AllocInterface allocr,
				VkDevice device,
				u32 count){
    VkResult result = VK_SUCCESS;
    OptSemaphores semas = {0};

    semas.value = SLICE_ALLOC(VkSemaphore, count, allocr);

    if(semas.value.data == nullptr){
      semas.code = CREATE_SEMAPHORES_ALLOC_FAIL;
      return semas;
    };

    VkSemaphoreCreateInfo sema_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    for_slice(semas.value, i){
      semas.value.data[i] = VK_NULL_HANDLE;
    }
    for_slice(semas.value, i){

      result = vkCreateSemaphore(device, &sema_create_info,
				 get_glob_vk_alloc(),
				 semas.value.data + i);
      if(result != VK_SUCCESS)
	semas.code = CREATE_SEMAPHORES_FAILED;

    }

    return semas;
}

OptSemaphores clear_semaphores(AllocInterface allocr,
			       OptSemaphores semas,
			       VkDevice device){

    switch (semas.code) {
    case CREATE_SEMAPHORES_OK:

    case CREATE_SEMAPHORES_FAILED:
      //for (int i = 0; i < param.semaphores_count; ++i) {
      for_slice(semas.value, i){
	if (VK_NULL_HANDLE != semas.value.data[i])
	  vkDestroySemaphore(device,
			     semas.value.data[i],
			     get_glob_vk_alloc());
      }

      SLICE_FREE(semas.value, allocr);
      //free(param.p_semaphores[0]);

    case CREATE_SEMAPHORES_ALLOC_FAIL:
    case CREATE_SEMAPHORES_TOP_FAIL_CODE:
      semas = (OptSemaphores){0};
    }
    return semas;
}

/* int create_fences(StackAllocator* stk_allocr, size_t stk_offset, */
/*                   const VkAllocationCallbacks* alloc_callbacks, */
/*                   CreateFencesParam param) { */
OptFences create_fences(AllocInterface allocr,
			VkDevice device,
			  u32 count){
    VkResult result = VK_SUCCESS;

    OptFences fences = {0};
     
    
    //param.p_fences[0] = malloc(param.fences_count * sizeof(VkFence));
    fences.value = SLICE_ALLOC(VkFence, count, allocr);

    if(nullptr == fences.value.data){
        fences.code = CREATE_FENCES_ALLOC_FAILED;
	return fences;
    }

    // p_win->in_flight_fences = malloc(p_win->max_frames_in_flight *
    // sizeof(VkFence));

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for_slice(fences.value, i)
      fences.value.data[i] = VK_NULL_HANDLE;
      
    //for (int i = 0; i < param.fences_count; ++i) {
    for_slice(fences.value, i){
      result = vkCreateFence(device, &fence_create_info,
			     get_glob_vk_alloc(), fences.value.data + i);
      if(result != VK_SUCCESS){
	fences.code = CREATE_FENCES_FAILED;
      }
    }

    return fences;
}

/* void clear_fences(const VkAllocationCallbacks* alloc_callbacks, */
/*                   ClearFencesParam param, int err_codes) { */
OptFences clear_fences(AllocInterface allocr,
		       OptFences fences,
		       VkDevice device){
    switch (fences.code) {
    case CREATE_FENCES_OK:


    case CREATE_FENCES_FAILED:
      for_slice(fences.value, i){
	if(VK_NULL_HANDLE != fences.value.data[i])
	  vkDestroyFence(device, fences.value.data[i],
			 get_glob_vk_alloc());
        }
      SLICE_FREE(fences.value, allocr);

    case CREATE_FENCES_ALLOC_FAILED:
    case CREATE_FENCES_TOP_FAIL_CODE:
      fences = (OptFences){0};
    }
    return fences;
}

/* int create_primary_command_buffers(StackAllocator* stk_allocr, */
/*                                    size_t stk_offset, */
/*                                    VkAllocationCallbacks* */
/*                                    alloc_callbacks, */
/*                                    CreatePrimaryCommandBuffersParam */
/*                                    param) { */

OptPrimaryCommandBuffers create_primary_command_buffers(AllocInterface allocr,
							VkDevice device,
							OptCommandPool cmd_pool,
							u32 count){
    VkResult result = VK_SUCCESS;
    OptPrimaryCommandBuffers cmd_bufs = {0};

    if(cmd_pool.code < 0){
      cmd_bufs.code = CREATE_PRIMARY_COMMAND_BUFFERS_INVALID_POOL;
      return cmd_bufs;
    }
    
    cmd_bufs.value = SLICE_ALLOC(VkCommandBuffer, count, allocr);
    
    /* param.p_cmd_buffers[0] = */
    /*     malloc(param.cmd_buffer_count * sizeof(VkCommandBuffer)); */

    if(nullptr == cmd_bufs.value.data){
      cmd_bufs.code = CREATE_PRIMARY_COMMAND_BUFFERS_ALLOC_FAILED;
      return cmd_bufs;
    }
    /* if (!param.p_cmd_buffers[0]) */
    /*     return CREATE_PRIMARY_COMMAND_BUFFERS_ALLOC_FAILED; */

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool.value,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = cmd_bufs.value.count,
    };

    result = vkAllocateCommandBuffers(device, &alloc_info,
                                      cmd_bufs.value.data);

    if (result != VK_SUCCESS)
      cmd_bufs.code = CREATE_PRIMARY_COMMAND_BUFFERS_FAILED;

    return cmd_bufs;
}

/* void clear_primary_command_buffers( */
/*     VkAllocationCallbacks* alloc_callbacks, */
/*     ClearPrimaryCommandBuffersParam param, int err_codes) { */
  
OptPrimaryCommandBuffers clear_primary_command_buffers(AllocInterface allocr,
						       OptPrimaryCommandBuffers cmd_buffers){
    switch (cmd_buffers.code) {
    case CREATE_PRIMARY_COMMAND_BUFFERS_OK:

    case CREATE_PRIMARY_COMMAND_BUFFERS_FAILED:
      SLICE_FREE(cmd_buffers.value, allocr);

    case CREATE_PRIMARY_COMMAND_BUFFERS_ALLOC_FAILED:
    case CREATE_PRIMARY_COMMAND_BUFFERS_INVALID_POOL:
    case CREATE_PRIMARY_COMMAND_BUFFERS_TOP_FAIL_CODE:
      cmd_buffers = (OptPrimaryCommandBuffers){0};

    }
    return cmd_buffers;
}

enum BeginRenderingOperationsCodes begin_rendering_operations(BeginRenderingOperationsParam param) {
    VkResult result = VK_SUCCESS;

    result = vkWaitForFences(
        param.device, 1, &param.render_done_fence, VK_TRUE,
        UINT64_MAX);

    if (result != VK_SUCCESS)
        return BEGIN_RENDERING_OPERATIONS_WAIT_FOR_FENCE_FAIL;

    //Skip drawing for minimize before acuiring
    if (!param.framebuffer_render_extent.width || !param.framebuffer_render_extent.height)
        return BEGIN_RENDERING_OPERATIONS_MINIMIZED;


    result = vkAcquireNextImageKHR(
        param.device, param.swapchain, UINT64_MAX,
        param.present_done_semaphore, VK_NULL_HANDLE,
        param.p_img_inx);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
        return BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN;
    


    vkResetCommandBuffer(param.cmd_buffer, 0);


    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,

    };

    result = vkBeginCommandBuffer(param.cmd_buffer, &cmd_begin_info);

    if (result != VK_SUCCESS)
        return BEGIN_RENDERING_OPERATIONS_BEGIN_CMD_BUFFER_FAIL;

    VkClearValue clear_values[] = {
        param.clear_value,
        (VkClearValue){
            .depthStencil = { .depth = 1.f, .stencil = 0 },
        }
    };

    VkRenderPassBeginInfo rndr_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = param.render_pass,
        .framebuffer = param.framebuffers[*param.p_img_inx],
        .renderArea = { .offset = { 0, 0 },
                        .extent = param.framebuffer_render_extent },
        .clearValueCount = _countof(clear_values),
        .pClearValues = clear_values,

    };
    vkCmdBeginRenderPass(param.cmd_buffer, &rndr_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
    return BEGIN_RENDERING_OPERATIONS_OK;
}

enum EndRenderingOperationsCodes end_rendering_operations(EndRenderingOperationsParam param) {
    vkCmdEndRenderPass(param.cmd_buffer);
    if (vkEndCommandBuffer(param.cmd_buffer) != VK_SUCCESS)
        return END_RENDERING_OPERATIONS_END_CMD_BUFFER_FAIL;


    vkResetFences(param.device, 1, &param.render_done_fence);

    VkSubmitInfo render_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &param.cmd_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &param.render_done_semaphore,
        .waitSemaphoreCount = param.render_wait_semaphores.count,
        .pWaitSemaphores = param.render_wait_semaphores.data,
        .pWaitDstStageMask =
        &(VkPipelineStageFlags){
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
    };
    VkResult result = VK_SUCCESS;
    result = vkQueueSubmit(param.graphics_queue, 1,
                           &render_submit_info,
                           param.render_done_fence);
    if (result != VK_SUCCESS)
        return END_RENDERING_OPERATIONS_GRAPHICS_QUEUE_SUBMIT_FAIL;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &param.swapchain,
        .pImageIndices = &param.img_index,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &param.render_done_semaphore,
    };

    result =
        vkQueuePresentKHR(param.present_queue, &present_info);

    if (result == VK_SUBOPTIMAL_KHR ||
        result == VK_ERROR_OUT_OF_DATE_KHR)
        return END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN;
    if (result < VK_SUCCESS)
        return END_RENDERING_OPERATIONS_FAILED;
    return END_RENDERING_OPERATIONS_OK;
}
