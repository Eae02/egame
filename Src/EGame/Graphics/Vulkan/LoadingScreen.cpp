#include "Common.hpp"
#include "Buffer.hpp"
#include "../LoadingScreen.hpp"

namespace eg::graphics_api::vk
{
	void MaybeAcquireSwapchainImage();
	
	void SubmitAndPresent(VkCommandBuffer cb, VkSemaphore signalSemaphore, VkFence signalFence);
	
	BufferHandle CreateBuffer(const BufferCreateInfo& createInfo);
	
	void DrawLoadingScreen()
	{
		VkCommandBufferAllocateInfo cmdAllocateInfo = 
		{
			/* sType              */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			/* pNext              */ nullptr,
			/* commandPool        */ ctx.mainCommandPool,
			/* level              */ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			/* commandBufferCount */ 1
		};
		VkCommandBuffer cb;
		CheckRes(vkAllocateCommandBuffers(ctx.device, &cmdAllocateInfo, &cb));
		
		VkCommandBufferBeginInfo cbBeginInfo =
		{
			/* sType */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			/* pNext */ nullptr,
			/* flags */ VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		vkBeginCommandBuffer(cb, &cbBeginInfo);
		
		MaybeAcquireSwapchainImage();
		
		int loadingImageWidth, loadingImageHeight;
		std::unique_ptr<uint8_t, FreeDel> imageData = GetLoadingImageData(loadingImageWidth, loadingImageHeight);
		
		BufferCreateInfo bufferCreateInfo;
		bufferCreateInfo.flags = BufferFlags::CopySrc | BufferFlags::MapWrite | BufferFlags::HostAllocate;
		bufferCreateInfo.size = loadingImageWidth * loadingImageHeight * 4;
		bufferCreateInfo.initialData = imageData.get();
		bufferCreateInfo.label = nullptr;
		Buffer* buffer = reinterpret_cast<Buffer*>(CreateBuffer(bufferCreateInfo));
		
		VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.image = ctx.swapchainImages[ctx.currentImage];
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);
		
		VkClearColorValue clearValue;
		clearValue.float32[0] = loadingBackgroundColor.r;
		clearValue.float32[1] = loadingBackgroundColor.g;
		clearValue.float32[2] = loadingBackgroundColor.b;
		clearValue.float32[3] = loadingBackgroundColor.a;
		VkImageSubresourceRange clearRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdClearColorImage(cb, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &clearRange);
		
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);
		
		VkBufferImageCopy copyRegion = {};
		copyRegion.imageExtent.width = loadingImageWidth;
		copyRegion.imageExtent.height = loadingImageHeight;
		copyRegion.imageExtent.depth = 1;
		copyRegion.imageOffset.x = (ctx.surfaceExtent.width - loadingImageWidth) / 2;
		copyRegion.imageOffset.y = (ctx.surfaceExtent.height - loadingImageHeight) / 2;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.layerCount = 1;
		vkCmdCopyBufferToImage(cb, buffer->buffer, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = 0;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);
		
		CheckRes(vkEndCommandBuffer(cb));
		
		VkSemaphore signalSemaphore;
		VkSemaphoreCreateInfo signalSemaphoreCI = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore(ctx.device, &signalSemaphoreCI, nullptr, &signalSemaphore);
		
		SubmitAndPresent(cb, signalSemaphore, VK_NULL_HANDLE);
		
		vkDeviceWaitIdle(ctx.device);
		vkResetCommandPool(ctx.device, ctx.mainCommandPool, 0);
		vkDestroySemaphore(ctx.device, signalSemaphore, nullptr);
	}
}
