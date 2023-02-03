#pragma once

#include <vector>
#include "DataTypes.h"

namespace dae
{
	class Mesh;
	class Camera;
	class Texture;

	class SoftwareRenderer
	{
	public:
		SoftwareRenderer(SDL_Window* pWindow);
		~SoftwareRenderer() = default;

		SoftwareRenderer(const SoftwareRenderer&) = delete;
		SoftwareRenderer(SoftwareRenderer&&) noexcept = delete;
		SoftwareRenderer& operator=(const SoftwareRenderer&) = delete;
		SoftwareRenderer& operator=(SoftwareRenderer&&) noexcept = delete;

		void Render(const std::vector<Mesh*>& pMeshes, Camera* pCamera, bool useUniformBackground) const;
		void ToggleShowingDepthBuffer();
		void ToggleShowingBoundingBoxes();
		void ToggleLightingMode();
		void ToggleNormalMap();
		void SetCullMode(CullMode cullMode);

		bool SaveBufferToImage() const;

	private:
		SDL_Window* m_pWindow{};

		SoftwareRenderInfo m_Info{};

		CullMode m_CullMode{ CullMode::Back };

		void ClearBackground(bool useUniformBackground) const;
		void ResetDepthBuffer() const;
	};
}
