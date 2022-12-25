#pragma once
#include "Math.h"

namespace dae
{
	struct Vertex
	{
		Vector3 position{};
		ColorRGB color{ 1.0f, 1.0f, 1.0f };
		Vector2 uv{};
		Vector3 normal{}; //W4
		Vector3 tangent{}; //W4
		Vector3 viewDirection{}; //W4
	};

	struct Vertex_Out
	{
		Vector4 position{};
		ColorRGB color{ colors::White };
		Vector2 uv{};
		Vector3 normal{};
		Vector3 tangent{};
		Vector3 viewDirection{};
	};

	enum class PrimitiveTopology
	{
		TriangleList,
		TriangleStrip
	};
}