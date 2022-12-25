#pragma once
#include "DataTypes.h"

namespace dae
{
	class Mesh final
	{
	public:
		Mesh(const std::string& filePath);
		~Mesh() = default;

		void RotateY(float angle);

		Matrix& GetWorldMatrix();
		std::vector<Vertex>& GetVertices();
		std::vector<uint32_t>& GetIndices();
		PrimitiveTopology GetPrimitiveTopology() const;
	private:
		std::vector<Vertex> m_Vertices{};
		std::vector<uint32_t> m_Indices{};
		PrimitiveTopology m_PrimitiveTopology{ PrimitiveTopology::TriangleList };

		Matrix m_WorldMatrix{ Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ, Vector3::Zero };
	};
}
