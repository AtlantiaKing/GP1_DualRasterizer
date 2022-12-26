#pragma once

struct SDL_Window;

namespace dae
{
	class Mesh;

	class HardwareRenderer final
	{
	public:
		HardwareRenderer(SDL_Window* pWindow);
		~HardwareRenderer();

		HardwareRenderer(const HardwareRenderer&) = delete;
		HardwareRenderer(HardwareRenderer&&) noexcept = delete;
		HardwareRenderer& operator=(const HardwareRenderer&) = delete;
		HardwareRenderer& operator=(HardwareRenderer&&) noexcept = delete;

		void ToggleRenderSampleState(std::vector<Mesh*>& pMeshes);

		ID3D11Device* GetDevice() const;
		ID3D11SamplerState* GetSampleState() const;

		void Render(const std::vector<Mesh*>& pMeshes, bool useUniformBackground) const;

	private:
		enum class SampleState
		{
			Point,
			Linear,
			Anisotropic
		};

		SDL_Window* m_pWindow{};

		int m_Width{};
		int m_Height{};

		bool m_IsInitialized{ false };

		SampleState m_SampleState{ SampleState::Point };
		bool m_IsRotatingMesh{ true };

		ID3D11SamplerState* m_pSampleState{};
		ID3D11Device* m_pDevice{};
		ID3D11DeviceContext* m_pDeviceContext{};
		IDXGISwapChain* m_pSwapChain{};
		ID3D11Texture2D* m_pDepthStencilBuffer{};
		ID3D11DepthStencilView* m_pDepthStencilView{};
		ID3D11Resource* m_pRenderTargetBuffer{};
		ID3D11RenderTargetView* m_pRenderTargetView{};

		HRESULT InitializeDirectX();
		void LoadSampleState(D3D11_FILTER filter, std::vector<Mesh*>& pMeshes);
	};
}
