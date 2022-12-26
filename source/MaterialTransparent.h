#pragma once
#include "Material.h"

namespace dae
{
	class Texture;

	class MaterialTransparent final : public Material
	{
	public:
		MaterialTransparent(ID3D11Device* pDevice, const std::wstring& assetFile);

		void SetTexture(Texture* pTexture);
	private:
		ID3DX11EffectShaderResourceVariable* m_pDiffuseMapVariable{};
	};
}

