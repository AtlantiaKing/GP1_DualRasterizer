#pragma once
#include "DataTypes.h"

namespace dae
{
	class Material;

	class Mesh final
	{
	public:
		Mesh(ID3D11Device* pDevice, const std::string& filePath, Material* pMaterial, ID3D11SamplerState* pSampleState = nullptr);
		~Mesh();

		// Shared
		void RotateY(float angle);
		void SetPosition(const Vector3& position);
		Matrix& GetWorldMatrix();

		// Software Rasterizer
		std::vector<Vertex>& GetVertices();
		std::vector<uint32_t>& GetIndices();
		PrimitiveTopology GetPrimitiveTopology() const;

		// DirectX Rasterizer
		void SetMatrices(const Matrix& viewProjectionMatrix, const Matrix& inverseViewMatrix);
		void SetSamplerState(ID3D11SamplerState* pSampleState);
		void SetRasterizerState(ID3D11RasterizerState* pRasterizerState);
		void SetVisibility(bool isVisible);
		void HardwareRender(ID3D11DeviceContext* pDeviceContext) const;
		bool IsVisible() const;
	private:
		// Shared
		Matrix m_WorldMatrix{ Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ, Vector3::Zero };

		// Software Rasterizer
		std::vector<Vertex> m_Vertices{};
		std::vector<uint32_t> m_Indices{};
		PrimitiveTopology m_PrimitiveTopology{ PrimitiveTopology::TriangleList };

		// DirectX Rasterizer
		bool m_IsVisible{ true };

		Material* m_pMaterial{};

		ID3D11InputLayout* m_pInputLayout{};

		ID3D11Buffer* m_pVertexBuffer{};
		ID3D11Buffer* m_pIndexBuffer{};
	};
}
