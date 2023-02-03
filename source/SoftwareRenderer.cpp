#include "pch.h"
#include "SoftwareRenderer.h"
#include "Mesh.h"
#include "Camera.h"
#include <ppl.h> // Parallel Stuff
#include <future>

namespace dae
{
	dae::SoftwareRenderer::SoftwareRenderer(SDL_Window* pWindow)
		: m_pWindow{ pWindow }
	{
		//Initialize
		SDL_GetWindowSize(pWindow, &m_Info.width, &m_Info.height);

		//Create Buffers
		m_Info.pFrontBuffer = SDL_GetWindowSurface(pWindow);
		m_Info.pBackBuffer = SDL_CreateRGBSurface(0, m_Info.width, m_Info.height, 32, 0, 0, 0, 0);
		m_Info.pBackBufferPixels = static_cast<uint32_t*>(m_Info.pBackBuffer->pixels);
		m_Info.pDepthBuffer = new float[static_cast<uint32_t>(m_Info.width * m_Info.height)];
		ResetDepthBuffer();
	}

	void dae::SoftwareRenderer::Render(const std::vector<Mesh*>& pMeshes, Camera* pCamera, bool useUniformBackground) const
	{
		// Reset the depth buffer
		ResetDepthBuffer();

		// Paint the canvas black
		ClearBackground(useUniformBackground);

		//Lock BackBuffer
		SDL_LockSurface(m_Info.pBackBuffer);

		// For each mesh
		for (Mesh* pMesh : pMeshes)
		{
			// If the mesh is visible, render the mesh
			if (!pMesh->IsVisible()) continue;

			pMesh->SoftwareRender(pCamera, m_Info);
		}

		//Update SDL Surface
		SDL_UnlockSurface(m_Info.pBackBuffer);
		SDL_BlitSurface(m_Info.pBackBuffer, 0, m_Info.pFrontBuffer, 0);
		SDL_UpdateWindowSurface(m_pWindow);
	}

	void SoftwareRenderer::ToggleShowingDepthBuffer()
	{
		m_Info.isShowingDepthBuffer = !m_Info.isShowingDepthBuffer;

		std::cout << "\033[35m"; // TEXT COLOR
		std::cout << "**(SOFTWARE) DepthBuffer Visualization ";
		if (m_Info.isShowingDepthBuffer)
		{
			std::cout << "ON\n";
		}
		else
		{
			std::cout << "OFF\n";
		}
	}

	void SoftwareRenderer::ToggleShowingBoundingBoxes()
	{
		m_Info.isShowingBoundingBoxes = !m_Info.isShowingBoundingBoxes;

		std::cout << "\033[35m"; // TEXT COLOR
		std::cout << "**(SOFTWARE) BoundingBox Visualization ";
		if (m_Info.isShowingBoundingBoxes)
		{
			std::cout << "ON\n";
		}
		else
		{
			std::cout << "OFF\n";
		}
	}

	void dae::SoftwareRenderer::ToggleLightingMode()
	{
		// Shuffle through all the lighting modes
		m_Info.lightingMode = static_cast<LightingMode>((static_cast<int>(m_Info.lightingMode) + 1) % (static_cast<int>(LightingMode::Specular) + 1));

		std::cout << "\033[35m"; // TEXT COLOR
		std::cout << "**(SOFTWARE) Shading Mode = ";
		switch (m_Info.lightingMode)
		{
		case dae::LightingMode::Combined:
			std::cout << "COMBINED\n";
			break;
		case dae::LightingMode::ObservedArea:
			std::cout << "OBSERVED_AREA\n";
			break;
		case dae::LightingMode::Diffuse:
			std::cout << "DIFFUSE\n";
			break;
		case dae::LightingMode::Specular:
			std::cout << "SPECULAR\n";
			break;
		}
	}

	void dae::SoftwareRenderer::ToggleNormalMap()
	{
		// Toggle the normal map active variable
		m_Info.isNormalMapActive = !m_Info.isNormalMapActive;

		std::cout << "\033[35m"; // TEXT COLOR
		std::cout << "**(SOFTWARE) NormalMap ";
		if (m_Info.isNormalMapActive)
		{
			std::cout << "ON\n";
		}
		else
		{
			std::cout << "OFF\n";
		}
	}

	void SoftwareRenderer::SetCullMode(CullMode cullMode)
	{
		m_CullMode = cullMode;
	}

	bool dae::SoftwareRenderer::SaveBufferToImage() const
	{
		return SDL_SaveBMP(m_Info.pBackBuffer, "Rasterizer_ColorBuffer.bmp");
	}

	void SoftwareRenderer::ClearBackground(bool useUniformBackground) const
	{
		// Fill the background
		const Uint8 colorValue{ static_cast<Uint8>((useUniformBackground ? 0.1f : 0.39f) * 255) };
		SDL_FillRect(m_Info.pBackBuffer, nullptr, SDL_MapRGB(m_Info.pBackBuffer->format, colorValue, colorValue, colorValue));
	}

	void SoftwareRenderer::ResetDepthBuffer() const
	{
		// The nr of pixels in the buffer
		const int nrPixels{ m_Info.width * m_Info.height };

		// Set everything in the depth buffer to the value FLT_MAX
		std::fill_n(m_Info.pDepthBuffer, nrPixels, FLT_MAX);
	}
}
