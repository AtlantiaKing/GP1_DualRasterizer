#include "pch.h"
#include "MaterialTransparent.h"
#include "Texture.h"

namespace dae
{
	dae::MaterialTransparent::MaterialTransparent(ID3D11Device* pDevice, const std::wstring& assetFile)
		: Material{ pDevice, assetFile }
	{
		// Save the diffuse texture variable of the effect as a member variable
		m_pDiffuseMapVariable = m_pEffect->GetVariableByName("gDiffuseMap")->AsShaderResource();
		if (!m_pDiffuseMapVariable->IsValid()) std::wcout << L"m_pDiffuseMapVariable not valid\n";
	}

	void MaterialTransparent::SetTexture(Texture* pTexture)
	{
		m_pDiffuseMapVariable->SetResource(pTexture->GetSRV());
	}
}
