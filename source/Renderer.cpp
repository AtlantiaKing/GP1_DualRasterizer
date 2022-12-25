#include "pch.h"
#include "Renderer.h"
#include "SoftwareRenderer.h"
#include "Camera.h"
#include "Mesh.h"
#include "Texture.h"

namespace dae {

	Renderer::Renderer(SDL_Window* pWindow) 
		: m_pWindow(pWindow)
	{
		//Initialize
		SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

		// Create and initialize the camera
		m_pCamera = new Camera{};
		m_pCamera->Initialize(45.0f, { 0.0f, 0.0f, -50.0f }, static_cast<float>(m_Width) / m_Height);

		// Create all textures and add them to the container
		Texture* pDiffuseTexture{ Texture::LoadFromFile("Resources/vehicle_diffuse.png") };
		m_pTextures.push_back(pDiffuseTexture);
		Texture* pNormalTexture{ Texture::LoadFromFile("Resources/vehicle_normal.png") };
		m_pTextures.push_back(pNormalTexture);
		Texture* pSpecularTexture{ Texture::LoadFromFile("Resources/vehicle_specular.png") };
		m_pTextures.push_back(pSpecularTexture);
		Texture* pGlossinessTexture{ Texture::LoadFromFile("Resources/vehicle_gloss.png") };
		m_pTextures.push_back(pGlossinessTexture);

		// Create the software rasterizer
		m_pSoftwareRender = new SoftwareRenderer{ pWindow, pDiffuseTexture, pNormalTexture, pSpecularTexture, pGlossinessTexture };

		// Create the vehicle mesh and add it to the container
		Mesh* pVehicleMesh{ new Mesh{ "Resources/vehicle.obj" } };
		m_pMeshes.push_back(pVehicleMesh);

		// Set the software renderer to render the vehicle
		m_pSoftwareRender->SetMesh(pVehicleMesh);
	}

	Renderer::~Renderer()
	{
		delete m_pSoftwareRender;
		
		for (Mesh* pMesh : m_pMeshes)
		{
			delete pMesh;
		}

		for (Texture* pTexture : m_pTextures)
		{
			delete pTexture;
		}

		delete m_pCamera;
	}

	void Renderer::Update(const Timer* pTimer)
	{
		// Update camera movement
		m_pCamera->Update(pTimer);

		// Rotate all the meshes
		const float rotationSpeed{ 45.0f * TO_RADIANS };
		for (Mesh* pMesh : m_pMeshes)
		{
			pMesh->RotateY(rotationSpeed * pTimer->GetElapsed());
		}
	}


	void Renderer::Render() const
	{
		// Render the scene using the software rasterizer
		m_pSoftwareRender->Render(m_pCamera);
	}
}
