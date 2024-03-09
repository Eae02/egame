#pragma once

#include "../Abstraction.hpp"

#include <vector>
#include <vulkan/vulkan_core.h>

struct SDL_Window;

namespace eg::graphics_api::vk
{
class Swapchain
{
public:
	void Create();

	bool Init(const GraphicsAPIInitArguments& initArgs);

	void Destroy();

	void SetEnableVSync(bool enableVSync);

	VkSemaphore AcquireImage();

	uint32_t NumImages() const { return static_cast<uint32_t>(m_swapchainImages.size()); }

	VkImage CurrentImage() const { return m_swapchainImages[m_currentImage]; }

	VkSurfaceFormatKHR m_surfaceFormat;
	VkExtent2D m_surfaceExtent;
	VkPresentModeKHR m_presentMode;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	uint32_t m_currentImage;

private:
	SDL_Window* m_window;

	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	uint32_t m_acquireSemaphoreIndex;
	std::vector<VkSemaphore> m_acquireSemaphores;
};
} // namespace eg::graphics_api::vk
