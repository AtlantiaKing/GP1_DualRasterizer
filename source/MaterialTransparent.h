#pragma once
#include "Material.h"

namespace dae
{
	class Texture;

	class MaterialTransparent final : public Material
	{
	public:
		MaterialTransparent(ID3D11Device* pDevice, const std::wstring& assetFile);
		virtual ~MaterialTransparent() = default;

		MaterialTransparent(const MaterialTransparent&) = delete;
		MaterialTransparent(MaterialTransparent&&) = delete;
		MaterialTransparent& operator=(const MaterialTransparent&) = delete;
		MaterialTransparent& operator=(MaterialTransparent&&) = delete;

		virtual void SetTexture(Texture* pTexture) override;
	private:
		ID3DX11EffectShaderResourceVariable* m_pDiffuseMapVariable{};
	};
}

