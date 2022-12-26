#pragma once

struct SDL_Window;
struct SDL_Surface;

namespace dae
{
	class HardwareRenderer;
	class SoftwareRenderer;
	class Camera;
	class Mesh;
	class Texture;

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(const Timer* pTimer);
		void Render() const;
		void ToggleRenderMode();

	private:
		enum class RenderMode
		{
			Software,
			Hardware
		};

		SDL_Window* m_pWindow{};

		int m_Width{};
		int m_Height{};

		Camera* m_pCamera{};
		std::vector<Mesh*> m_pMeshes{};
		std::vector<Texture*> m_pTextures{};
		RenderMode m_RenderMode{ RenderMode::Hardware };

		HardwareRenderer* m_pHardwareRender{};
		SoftwareRenderer* m_pSoftwareRender{};

		void LoadMeshes();
	};
}
