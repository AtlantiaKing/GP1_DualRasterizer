#include "pch.h"
#include "MaterialShaded.h"
#include "Texture.h"

namespace dae
{
	dae::MaterialShaded::MaterialShaded(ID3D11Device* pDevice, const std::wstring& assetFile)
		: Material{ pDevice, assetFile }
	{
		// Save the diffuse texture variable of the effect as a member variable
		m_pDiffuseMapVariable = m_pEffect->GetVariableByName("gDiffuseMap")->AsShaderResource();
		if (!m_pDiffuseMapVariable->IsValid()) std::wcout << L"m_pDiffuseMapVariable not valid\n";

		// Save the normal texture variable of the effect as a member variable
		m_pNormalMapVariable = m_pEffect->GetVariableByName("gNormalMap")->AsShaderResource();
		if (!m_pNormalMapVariable->IsValid()) std::wcout << L"m_pNormalMapVariable not valid\n";

		// Save the specular texture variable of the effect as a member variable
		m_pSpecularMapVariable = m_pEffect->GetVariableByName("gSpecularMap")->AsShaderResource();
		if (!m_pSpecularMapVariable->IsValid()) std::wcout << L"m_pSpecularMapVariable not valid\n";

		// Save the glossiness texture variable of the effect as a member variable
		m_pGlossinessMapVariable = m_pEffect->GetVariableByName("gGlossinessMap")->AsShaderResource();
		if (!m_pGlossinessMapVariable->IsValid()) std::wcout << L"m_pGlossinessMapVariable not valid\n";

		// Save the worldmatrix variable of the effect as a member variable
		m_pMatWorldVariable = m_pEffect->GetVariableByName("gWorld")->AsMatrix();
		if (!m_pMatWorldVariable->IsValid()) std::wcout << L"m_pMatWorldVariable not valid\n";

		// Save the inverseviewmatrix variable of the effect as a member variable
		m_pMatInverseViewVariable = m_pEffect->GetVariableByName("gViewInverse")->AsMatrix();
		if (!m_pMatInverseViewVariable->IsValid()) std::wcout << L"m_pMatInverseViewVariable not valid\n";
	}

	void MaterialShaded::SetMatrix(MatrixType type, const Matrix& matrix)
	{
		// Set the given matrix to the right variable depending on the matrix type
		// If none of the matrix types are specified here, call the base SetMatrix function
		switch (type)
		{
		case dae::MatrixType::World:
		{
			m_pMatWorldVariable->SetMatrix(reinterpret_cast<const float*>(&matrix));
			break;
		}
		case dae::MatrixType::InverseView:
		{
			// Set the current matrix to the worldviewprojection variable of the effect
			m_pMatInverseViewVariable->SetMatrix(reinterpret_cast<const float*>(&matrix));
			break;
		}
		default:
			Material::SetMatrix(type, matrix);
		}
	}

	void MaterialShaded::SetTexture(Texture* pTexture)
	{
		// The current texture variable in the effect
		ID3DX11EffectShaderResourceVariable* pCurMapVariable{};

		// Get the right texture variable depending on the texture type
		switch (pTexture->GetType())
		{
		case dae::Texture::TextureType::Diffuse:
			pCurMapVariable = m_pDiffuseMapVariable;
			break;
		case dae::Texture::TextureType::Normal:
			pCurMapVariable = m_pNormalMapVariable;
			break;
		case dae::Texture::TextureType::Specular:
			pCurMapVariable = m_pSpecularMapVariable;
			break;
		case dae::Texture::TextureType::Glossiness:
			pCurMapVariable = m_pGlossinessMapVariable;
			break;
		}

		// Set the shader resource view of the texture to the texture variable of the effect
		if (pCurMapVariable)
			pCurMapVariable->SetResource(pTexture->GetSRV());
	}
}