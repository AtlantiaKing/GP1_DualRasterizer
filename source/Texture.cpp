#include "pch.h"
#include "Texture.h"
#include "Vector2.h"
#include <SDL_image.h>
#include <algorithm>

namespace dae
{
	Texture::Texture(ID3D11Device* pDevice, SDL_Surface* pSurface, TextureType type)
		: m_pSurface{ pSurface }
		, m_pSurfacePixels{ static_cast<uint32_t*>(pSurface->pixels) }
		, m_Type{ type }
	{
		// Create the texture description
		const DXGI_FORMAT format{ DXGI_FORMAT_R8G8B8A8_UNORM };
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = pSurface->w;
		desc.Height = pSurface->h;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		// Create intialize data for subresource
		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = pSurface->pixels;
		initData.SysMemPitch = static_cast<UINT>(pSurface->pitch);
		initData.SysMemSlicePitch = static_cast<UINT>(pSurface->h * pSurface->pitch);

		// Create the texture resource
		HRESULT hr = pDevice->CreateTexture2D(&desc, &initData, &m_pResource);
		if (FAILED(hr)) return;

		// Create the shader resource view description
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
		SRVDesc.Format = format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;

		// Create the shader resource view
		hr = pDevice->CreateShaderResourceView(m_pResource, &SRVDesc, &m_pSRV);
		if (FAILED(hr)) return;
	}

	Texture::~Texture()
	{
		if (m_pSurface) SDL_FreeSurface(m_pSurface);

		if (m_pResource) m_pResource->Release();
		if (m_pSRV) m_pSRV->Release();
	}

	Texture* Texture::LoadFromFile(ID3D11Device* pDevice, const std::string& path, TextureType type)
	{
		//Load SDL_Surface using IMG_LOAD
		//Create & Return a new Texture Object (using SDL_Surface)
		return new Texture{ pDevice, IMG_Load(path.c_str()), type };
	}

	ColorRGB Texture::Sample(const Vector2& uv) const
	{
		// The rgb values in [0, 255] range
		Uint8 r{};
		Uint8 g{};
		Uint8 b{};

		// Calculate the UV coordinates using clamp adressing mode
		const int x{ static_cast<int>(std::clamp(uv.x, 0.0f, 1.0f) * m_pSurface->w) };
		const int y{ static_cast<int>(std::clamp(uv.y, 0.0f, 1.0f) * m_pSurface->h) };

		// Calculate the current pixelIdx on the texture
		const Uint32 pixelIdx{ m_pSurfacePixels[x + y * m_pSurface->w] };

		// Get the r g b values from the current pixel on the texture
		SDL_GetRGB(pixelIdx, m_pSurface->format, &r, &g, &b);

		// The max value of a color attribute
		const float maxColorValue{ 255.0f };

		// Return the color in range [0, 1]
		return ColorRGB{ r / maxColorValue, g / maxColorValue, b / maxColorValue };
	}

	ID3D11Texture2D* Texture::GetResource() const
	{
		return m_pResource;
	}

	ID3D11ShaderResourceView* Texture::GetSRV() const
	{
		return m_pSRV;
	}

	Texture::TextureType Texture::GetType() const
	{
		return m_Type;
	}
}