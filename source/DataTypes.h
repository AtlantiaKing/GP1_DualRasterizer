#pragma once
#include "Math.h"

namespace dae
{
	enum class LightingMode
	{
		Combined,
		ObservedArea,
		Diffuse,
		Specular
	};

	enum class PrimitiveTopology
	{
		TriangleList,
		TriangleStrip
	};

	enum class CullMode
	{
		Back,
		Front,
		None
	};

	struct Vertex
	{
		Vector3 position{};
		Vector3 normal{}; //W4
		Vector3 tangent{}; //W4
		Vector2 uv{};
		ColorRGB color{ 1.0f, 1.0f, 1.0f };
		Vector3 viewDirection{}; //W4
	};

	struct Vertex_Out
	{
		Vector4 position{};
		Vector3 normal{};
		Vector3 tangent{};
		Vector2 uv{};
		ColorRGB color{ colors::White };
		Vector3 viewDirection{};
	};

	struct SoftwareRenderInfo
	{
		~SoftwareRenderInfo()
		{
			delete[] pDepthBuffer;
		}

		int width{};
		int height{};
		bool isShowingBoundingBoxes{};
		bool isShowingDepthBuffer{};
		uint32_t* pBackBufferPixels{};
		float* pDepthBuffer{}; 
		SDL_Surface* pFrontBuffer{};
		SDL_Surface* pBackBuffer{};
		bool isNormalMapActive{ true };
		LightingMode lightingMode{ LightingMode::Combined };
	};
}