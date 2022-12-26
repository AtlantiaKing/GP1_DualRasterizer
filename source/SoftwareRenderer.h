#pragma once

#include <cstdint>
#include <vector>
#include <memory>
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
		~SoftwareRenderer();

		SoftwareRenderer(const SoftwareRenderer&) = delete;
		SoftwareRenderer(SoftwareRenderer&&) noexcept = delete;
		SoftwareRenderer& operator=(const SoftwareRenderer&) = delete;
		SoftwareRenderer& operator=(SoftwareRenderer&&) noexcept = delete;

		void Render(Camera* pCamera, bool useUniformBackground);
		void ToggleShowingDepthBuffer();
		void ToggleShowingBoundingBoxes();
		void ToggleLightingMode();
		void ToggleNormalMap();
		void SetTextures(Texture* pDiffuseTexture, Texture* pNormalTexture, Texture* pSpecularTexture, Texture* pGlossinessTexture);
		void SetMesh(Mesh* pMesh);

		bool SaveBufferToImage() const;

	private:
		enum class LightingMode
		{
			Combined,
			ObservedArea,
			Diffuse,
			Specular
		};

		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		int m_Width{};
		int m_Height{};

		Mesh* m_pMesh{};
		Texture* m_pDiffuseTexture{};
		Texture* m_pNormalTexture{};
		Texture* m_pSpecularTexture{};
		Texture* m_pGlossinessTexture{};

		bool m_IsShowingDepthBuffer{};
		bool m_IsShowingBoundingBoxes{};
		LightingMode m_LightingMode{ LightingMode::Combined };
		bool m_IsRotatingMesh{ true };
		bool m_IsNormalMapActive{ true };

		//Function that transforms the vertices from the mesh from World space to Screen space
		void VertexTransformationFunction(std::vector<Vertex_Out>& verticesOut, Camera* pCamera);
		void RenderTriangle(const std::vector<Vector2>& rasterVertices, const std::vector<Vertex_Out>& verticesOut, const std::vector<uint32_t>& indices, int vertexIdx, bool swapVertices) const;
		void ClearBackground(bool useUniformBackground) const;
		void ResetDepthBuffer() const;
		void PixelShading(int pixelIdx, const Vertex_Out& pixelInfo) const;
		inline Vector2 CalculateNDCToRaster(const Vector3& ndcVertex) const;
		inline bool IsOutsideFrustum(const Vector4& v) const;
		//inline Vector3 CalculateRasterToNDC(const Vector2& rasterVertex, float interpolatedZ) const;
		//inline void OrderTriangleIndices(const std::vector<Vector2>& rasterVertices, int i0, int i1, int i2);
	};
}
