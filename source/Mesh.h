#pragma once
#include "DataTypes.h"

namespace dae
{
	class Material;
	class Texture;
	class Camera;

	class Mesh final
	{
	public:
		Mesh(ID3D11Device* pDevice, const std::string& filePath, Material* pMaterial, ID3D11SamplerState* pSampleState = nullptr);
		~Mesh();

		Mesh(const Mesh& other) = delete;
		Mesh& operator=(const Mesh& other) = delete;
		Mesh(Mesh&& other) = delete;
		Mesh& operator=(Mesh&& other) = delete;

		// Shared
		void RotateY(float angle);
		void SetPosition(const Vector3& position);
		const Matrix& GetWorldMatrix() const;
		void SetCullMode(CullMode cullMode);
		void SetTexture(Texture* pTexture);

		// Software Rasterizer
		void SoftwareRender(Camera* pCamera, const SoftwareRenderInfo& renderInfo);

		// DirectX Rasterizer
		void HardwareRender(ID3D11DeviceContext* pDeviceContext) const;
		void UpdateMatrices(const Matrix& viewProjectionMatrix, const Matrix& inverseViewMatrix) const;
		void SetSamplerState(ID3D11SamplerState* pSampleState) const;
		void SetRasterizerState(ID3D11RasterizerState* pRasterizerState) const;
		void SetVisibility(bool isVisible);
		bool IsVisible() const;
	private:
		void ClipTriangle(std::vector<Vertex_Out>& verticesOut, std::vector<Vector2>& verticesRasterSpace, const std::vector<Vector2>& rasterVertices, const SoftwareRenderInfo& renderInfo, size_t i);
		void RenderTriangle(const std::vector<Vector2>& rasterVertices, const std::vector<Vertex_Out>& verticesOut, size_t curVertexIdx, bool swapVertices, const SoftwareRenderInfo& renderInfo) const;
		void PixelShading(int pixelIdx, const Vertex_Out& pixelInfo, const SoftwareRenderInfo& renderInfo) const;
		Vector3 CalculateNormalFromMap(const Vertex_Out& pixelInfo) const;

		// Shared
		Matrix m_WorldMatrix{ Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ, Vector3::Zero };
		CullMode m_CullMode{};

		// Software Rasterizer
		std::vector<Vertex> m_Vertices{};
		std::vector<uint32_t> m_UseIndices{};
		std::vector<uint32_t> m_Indices{};
		PrimitiveTopology m_PrimitiveTopology{ PrimitiveTopology::TriangleList };
		bool m_IsTransparent{};

		Texture* m_pDiffuseMap{};
		Texture* m_pNormalMap{};
		Texture* m_pGlossinessMap{};
		Texture* m_pSpecularMap{};

		// DirectX Rasterizer
		bool m_IsVisible{ true };
		Material* m_pMaterial{};
		ID3D11InputLayout* m_pInputLayout{};
		ID3D11Buffer* m_pVertexBuffer{};
		ID3D11Buffer* m_pIndexBuffer{};
	};
}
