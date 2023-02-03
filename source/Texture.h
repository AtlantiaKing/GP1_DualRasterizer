#pragma once
#include <SDL_surface.h>
#include <string>
#include "ColorRGB.h"

namespace dae
{
	class Texture final
	{
	public:
		enum class TextureType
		{
			Diffuse,
			Normal,
			Specular,
			Glossiness
		};

		~Texture();

		Texture(const Texture& other) = delete;
		Texture& operator=(const Texture& other) = delete;
		Texture(Texture&& other) = delete;
		Texture& operator=(Texture&& other) = delete;
		
		// Shared
		static Texture* LoadFromFile(ID3D11Device* pDevice, const std::string& path, TextureType type);

		// Software Rasterizer
		ColorRGB SampleRGB(const Vector2& uv) const;

		// Hardware Rasterizer
		ID3D11Texture2D* GetResource() const;
		ID3D11ShaderResourceView* GetSRV() const;
		TextureType GetType() const;
	private:
		Texture(ID3D11Device* pDevice, SDL_Surface* pSurface, TextureType type);
		
		// Software Rasterizer
		SDL_Surface* m_pSurface{ nullptr };
		uint32_t* m_pSurfacePixels{ nullptr };

		// Hardware Rasterizer
		TextureType m_Type{};
		ID3D11Texture2D* m_pResource{};
		ID3D11ShaderResourceView* m_pSRV{};
	};
}

