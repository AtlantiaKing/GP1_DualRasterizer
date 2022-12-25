#include "pch.h"
#include "Mesh.h"
#include "Utils.h"

namespace dae
{
	Mesh::Mesh(const std::string& filePath)
	{
		Utils::ParseOBJ(filePath, m_Vertices, m_Indices);
	}

	void Mesh::RotateY(float angle)
	{
		Matrix rotationMatrix{ Matrix::CreateRotationY(angle) };
		m_WorldMatrix = rotationMatrix * m_WorldMatrix;
	}

	Matrix& Mesh::GetWorldMatrix()
	{
		return m_WorldMatrix;
	}

	std::vector<Vertex>& Mesh::GetVertices()
	{
		return m_Vertices;
	}

	std::vector<uint32_t>& Mesh::GetIndices()
	{
		return m_Indices;
	}

	PrimitiveTopology Mesh::GetPrimitiveTopology() const
	{
		return m_PrimitiveTopology;
	}
}