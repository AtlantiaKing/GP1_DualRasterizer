#include "pch.h"
#include "Renderer.h"
#include "HardwareRenderer.h"
#include "SoftwareRenderer.h"
#include "Camera.h"
#include "Mesh.h"
#include "Texture.h"
#include "MaterialShaded.h"
#include "MaterialTransparent.h"

namespace dae {

	Renderer::Renderer(SDL_Window* pWindow) 
		: m_pWindow(pWindow)
	{
		//Initialize
		SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

		// Create and initialize the camera
		m_pCamera = new Camera{};
		m_pCamera->Initialize(45.0f, { 0.0f, 0.0f, 0.0f }, static_cast<float>(m_Width) / m_Height);

		// Create the hardware rasterizer
		m_pHardwareRender = new HardwareRenderer{ pWindow };

		// Create the software rasterizer
		m_pSoftwareRender = new SoftwareRenderer{ pWindow };
		
		// Load all the textures and meshes
		LoadMeshes();
	}

	Renderer::~Renderer()
	{
		for (Mesh* pMesh : m_pMeshes)
		{
			delete pMesh;
		}

		for (Texture* pTexture : m_pTextures)
		{
			delete pTexture;
		}

		delete m_pSoftwareRender;
		delete m_pHardwareRender;

		delete m_pCamera;
	}

	void Renderer::Update(const Timer* pTimer)
	{
		// Update camera movement
		m_pCamera->Update(pTimer);

		// Calculate the viewprojection matrix
		const Matrix ViewProjMatrix{ m_pCamera->GetViewMatrix() * m_pCamera->GetProjectionMatrix() };

		// Rotate all the meshes
		const float rotationSpeed{ 45.0f * TO_RADIANS };
		for (Mesh* pMesh : m_pMeshes)
		{
			pMesh->RotateY(rotationSpeed * pTimer->GetElapsed());
			pMesh->SetMatrices(ViewProjMatrix, m_pCamera->GetInverseViewMatrix());
		}
	}


	void Renderer::Render() const
	{
		switch (m_RenderMode)
		{
		case dae::Renderer::RenderMode::Software:
			// Render the scene using the software rasterizer
			m_pSoftwareRender->Render(m_pCamera);
			break;
		case dae::Renderer::RenderMode::Hardware:
			// Render the scene using the software rasterizer
			m_pHardwareRender->Render(m_pMeshes);
			break;
		}
	}

	void Renderer::ToggleRenderMode()
	{
		// Go to the next render mode
		m_RenderMode = static_cast<RenderMode>((static_cast<int>(m_RenderMode) + 1) % (static_cast<int>(RenderMode::Hardware) + 1));
	}

	void Renderer::LoadMeshes()
	{
		// Retrieve the DirectX device from the hardware renderer
		ID3D11Device* pDirectXDevice{ m_pHardwareRender->GetDevice() };
		// Retrieve the current sample state from the hardware renderer
		ID3D11SamplerState* pSampleState{ m_pHardwareRender->GetSampleState() };

		// Create the vehicle effect
		MaterialShaded* vehicleMaterial{ new MaterialShaded{ pDirectXDevice, L"Resources/PosTex3D.fx" } };

		// Load all the textures needed for the vehicle
		Texture* pVehicleDiffuseTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/vehicle_diffuse.png", Texture::TextureType::Diffuse) };
		m_pTextures.push_back(pVehicleDiffuseTexture);
		Texture* pNormalTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/vehicle_normal.png", Texture::TextureType::Normal) };
		m_pTextures.push_back(pNormalTexture);
		Texture* pSpecularTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/vehicle_specular.png", Texture::TextureType::Specular) };
		m_pTextures.push_back(pSpecularTexture);
		Texture* pGlossinessTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/vehicle_gloss.png", Texture::TextureType::Glossiness) };
		m_pTextures.push_back(pGlossinessTexture);

		// Apply the textures to the vehicle effect
		vehicleMaterial->SetTexture(pVehicleDiffuseTexture);
		vehicleMaterial->SetTexture(pNormalTexture);
		vehicleMaterial->SetTexture(pSpecularTexture);
		vehicleMaterial->SetTexture(pGlossinessTexture);

		// Create the vehicle mesh and add it to the list of meshes
		Mesh* pVehicle{ new Mesh{ pDirectXDevice, "Resources/vehicle.obj", vehicleMaterial, pSampleState } };
		pVehicle->SetPosition({ 0.0f, 0.0f, 50.0f });
		m_pMeshes.push_back(pVehicle);



		// Create the fire effect
		MaterialTransparent* transparentMaterial{ new MaterialTransparent{ pDirectXDevice, L"Resources/Transparent3D.fx" } };

		// Load the texture needed for the fire
		Texture* pFireDiffuseTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/fireFX_diffuse.png", Texture::TextureType::Diffuse) };
		m_pTextures.push_back(pFireDiffuseTexture);

		// Apply the texture to the fire effect
		transparentMaterial->SetTexture(pFireDiffuseTexture);

		// Create the fire mesh and add it ot the list of meshes
		Mesh* pFire{ new Mesh{ pDirectXDevice, "Resources/fireFX.obj", transparentMaterial, pSampleState } };
		pFire->SetPosition({ 0.0f, 0.0f, 50.0f });
		m_pMeshes.push_back(pFire);


		
		// Set the software renderer to render the vehicle
		m_pSoftwareRender->SetMesh(pVehicle);
		m_pSoftwareRender->SetTextures(pVehicleDiffuseTexture, pNormalTexture, pSpecularTexture, pGlossinessTexture);
	}
}
