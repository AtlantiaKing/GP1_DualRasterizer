#pragma once
#include "Material.h"

namespace dae
{
	class Texture;

	class MaterialShaded final : public Material
	{
	public:
		MaterialShaded(ID3D11Device* pDevice, const std::wstring& assetFile);

		virtual void SetMatrix(MatrixType type, const Matrix& matrix) override;
		void SetTexture(Texture* pTexture);
	private:
		ID3DX11EffectShaderResourceVariable* m_pDiffuseMapVariable{};
		ID3DX11EffectShaderResourceVariable* m_pNormalMapVariable{};
		ID3DX11EffectShaderResourceVariable* m_pSpecularMapVariable{};
		ID3DX11EffectShaderResourceVariable* m_pGlossinessMapVariable{};

		ID3DX11EffectMatrixVariable* m_pMatWorldVariable{};
		ID3DX11EffectMatrixVariable* m_pMatInverseViewVariable{};
	};
}

