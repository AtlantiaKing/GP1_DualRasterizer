#include "pch.h"
#include "Mesh.h"
#include "Utils.h"
#include "Material.h"

namespace dae
{
	Mesh::Mesh(ID3D11Device* pDevice, const std::string& filePath, Material* pMaterial, ID3D11SamplerState* pSampleState)
		: m_pMaterial{ pMaterial }
	{
		bool parseResult{ Utils::ParseOBJ(filePath, m_Vertices, m_Indices) };
		if (!parseResult)
		{
			std::cout << "Failed to load OBJ from " << filePath << "\n";
			return;
		}

		// Create Input Layout
		m_pInputLayout = pMaterial->LoadInputLayout(pDevice);

		// Create vertex buffer
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.ByteWidth = sizeof(Vertex) * static_cast<uint32_t>(m_Vertices.size());
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = m_Vertices.data();

		HRESULT result{ pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer) };
		if (FAILED(result)) return;

		// Create index buffer
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.ByteWidth = sizeof(uint32_t) * static_cast<uint32_t>(m_Indices.size());
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;
		initData.pSysMem = m_Indices.data();

		result = pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
		if (FAILED(result)) return;

		if (pSampleState) SetSamplerState(pSampleState);
	}

	Mesh::~Mesh()
	{
		if (m_pIndexBuffer) m_pIndexBuffer->Release();
		if (m_pVertexBuffer) m_pVertexBuffer->Release();

		if (m_pInputLayout) m_pInputLayout->Release();

		delete m_pMaterial;
	}

	void Mesh::RotateY(float angle)
	{
		Matrix rotationMatrix{ Matrix::CreateRotationY(angle) };
		m_WorldMatrix = rotationMatrix * m_WorldMatrix;
	}

	void Mesh::SetPosition(const Vector3& position)
	{
		m_WorldMatrix[3][0] = position.x;
		m_WorldMatrix[3][1] = position.y;
		m_WorldMatrix[3][2] = position.z;
	}

	void Mesh::HardwareRender(ID3D11DeviceContext* pDeviceContext) const
	{
		if (!m_IsVisible) return;

		// Set primitive topology
		pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set input layout
		pDeviceContext->IASetInputLayout(m_pInputLayout);

		// Set vertex buffer
		constexpr UINT stride{ sizeof(Vertex) };
		constexpr UINT offset{ 0 };
		pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

		// Set index buffer
		pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		// Draw
		D3DX11_TECHNIQUE_DESC techniqueDesc{};
		m_pMaterial->GetTechnique()->GetDesc(&techniqueDesc);
		for (UINT p{}; p < techniqueDesc.Passes; ++p)
		{
			m_pMaterial->GetTechnique()->GetPassByIndex(p)->Apply(0, pDeviceContext);
			pDeviceContext->DrawIndexed(static_cast<uint32_t>(m_Indices.size()), 0, 0);
		}
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

	bool Mesh::IsVisible() const
	{
		return m_IsVisible;
	}

	void Mesh::SetMatrices(const Matrix& viewProjectionMatrix, const Matrix& inverseViewMatrix)
	{
		m_pMaterial->SetMatrix(MatrixType::WorldViewProjection, m_WorldMatrix * viewProjectionMatrix);
		m_pMaterial->SetMatrix(MatrixType::InverseView, inverseViewMatrix);
		m_pMaterial->SetMatrix(MatrixType::World, m_WorldMatrix);
	}

	void Mesh::SetSamplerState(ID3D11SamplerState* pSampleState)
	{
		m_pMaterial->SetSampleState(pSampleState);
	}

	void Mesh::SetRasterizerState(ID3D11RasterizerState* pRasterizerState)
	{
		m_pMaterial->SetRasterizerState(pRasterizerState);
	}

	void Mesh::SetVisibility(bool isVisible)
	{
		m_IsVisible = isVisible;
	}
}