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

		// Show keybinds
		std::cout << "\033[33m"; // TEXT COLOR
		std::cout << "[Key Bindings - SHARED]\n";
		std::cout << "\t[F1]  Toggle Rasterizer Mode (HARDWARE / SOFTWARE)\n";
		std::cout << "\t[F2]  Toggle Vehicle Rotation (ON / OFF)\n";
		std::cout << "\t[F3]  Toggle FireFX (ON / OFF)\n";
		std::cout << "\t[F9]  Cycle CullMode (BACK / FRONT / NONE)\n";
		std::cout << "\t[F10] Toggle Uniform ClearColor (ON / OFF)\n";
		std::cout << "\t[F11] Toggle Print FPS (ON / OFF)\n";
		std::cout << "\n";
		std::cout << "\033[32m"; // TEXT COLOR
		std::cout << "[Key Bindings - HARDWARE]\n";
		std::cout << "\t[F4]  Cycle Sampler State (POINT / LINEAR / ANISOTROPIC)\n";
		std::cout << "\n";
		std::cout << "\033[35m"; // TEXT COLOR
		std::cout << "[Key Bindings - SOFTWARE]\n";
		std::cout << "\t[F5]  Cycle Shading Mode (COMBINED / OBSERVED_AREA / DIFFUSE / SPECULAR)\n";
		std::cout << "\t[F6]  Toggle NormalMap (ON / OFF)\n";
		std::cout << "\t[F7]  Toggle DepthBuffer Visualization (ON / OFF)\n";
		std::cout << "\t[F8]  Toggle BoundingBox Visualization (ON / OFF)\n";
		std::cout << "\n";
		std::cout << "\033[31m";
		std::cout << "Extra's: FireFX, clipping and multithreading have been added extra to the software rasterizer\n";
		std::cout << "\n\n";
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

	void Renderer::Update(const Timer* pTimer) const
	{
		// Update camera movement
		m_pCamera->Update(pTimer);

		// Calculate the viewprojection matrix
		const Matrix ViewProjMatrix{ m_pCamera->GetViewMatrix() * m_pCamera->GetProjectionMatrix() };

		// Rotate all the meshes
		constexpr float rotationSpeed{ 45.0f * TO_RADIANS };
		for (Mesh* pMesh : m_pMeshes)
		{
			if(m_IsMeshRotating) pMesh->RotateY(rotationSpeed * pTimer->GetElapsed());
			pMesh->UpdateMatrices(ViewProjMatrix, m_pCamera->GetInverseViewMatrix());
		}
	}

	void Renderer::Render() const
	{
		switch (m_RenderMode)
		{
		case dae::Renderer::RenderMode::Software:
		{
			// Render the scene using the software rasterizer
			m_pSoftwareRender->Render(m_pMeshes, m_pCamera, m_IsBackgroundUniform);
			break;
		}
		case dae::Renderer::RenderMode::Hardware:
		{
			// Render the scene using the hardware rasterizer
			m_pHardwareRender->Render(m_pMeshes, m_IsBackgroundUniform);
			break;
		}
		}
	}

	void Renderer::ToggleRenderMode()
	{
		// Go to the next render mode
		m_RenderMode = static_cast<RenderMode>((static_cast<int>(m_RenderMode) + 1) % (static_cast<int>(RenderMode::Hardware) + 1));

		std::cout << "\033[33m"; // TEXT COLOR
		std::cout << "**(SHARED) Rasterizer Mode = ";
		switch (m_RenderMode)
		{
		case dae::Renderer::RenderMode::Software:
			std::cout << "SOFTWARE\n";
			break;
		case dae::Renderer::RenderMode::Hardware:
			std::cout << "HARDWARE\n";
			break;
		}
	}

	void Renderer::ToggleMeshRotation()
	{
		m_IsMeshRotating = !m_IsMeshRotating;

		std::cout << "\033[33m"; // TEXT COLOR
		std::cout << "**(SHARED) Vehicle Rotation ";
		if (m_IsMeshRotating)
		{
			std::cout << "ON\n";
		}
		else
		{
			std::cout << "OFF\n";
		}
	}

	void Renderer::ToggleFireMesh() const
	{
		Mesh* pFireMesh{ m_pMeshes[1] };
		pFireMesh->SetVisibility(!pFireMesh->IsVisible());

		std::cout << "\033[33m"; // TEXT COLOR
		std::cout << "**(SHARED)FireFX ";
		if (pFireMesh->IsVisible())
		{
			std::cout << "ON\n";
		}
		else
		{
			std::cout << "OFF\n";
		}
	}

	void Renderer::ToggleSamplerState() const
	{
		if (m_RenderMode != RenderMode::Hardware) return;

		m_pHardwareRender->ToggleRenderSampleState(m_pMeshes);
	}

	void Renderer::ToggleShadingMode() const
	{
		if (m_RenderMode != RenderMode::Software) return;

		m_pSoftwareRender->ToggleLightingMode();
	}

	void Renderer::ToggleNormalMap() const
	{
		if (m_RenderMode != RenderMode::Software) return;

		m_pSoftwareRender->ToggleNormalMap();
	}

	void Renderer::ToggleShowingDepthBuffer() const
	{
		if (m_RenderMode != RenderMode::Software) return;

		m_pSoftwareRender->ToggleShowingDepthBuffer();
	}

	void Renderer::ToggleShowingBoundingBoxes() const
	{
		if (m_RenderMode != RenderMode::Software) return;

		m_pSoftwareRender->ToggleShowingBoundingBoxes();
	}

	void Renderer::ToggleUniformBackground()
	{
		m_IsBackgroundUniform = !m_IsBackgroundUniform;

		std::cout << "\033[33m"; // TEXT COLOR
		std::cout << "**(SHARED) Uniform ClearColor ";
		if (m_IsBackgroundUniform)
		{
			std::cout << "ON\n";
		}
		else
		{
			std::cout << "OFF\n";
		}
	}

	void Renderer::ToggleCullMode()
	{
		// Go to the next cull mode
		m_CullMode = static_cast<CullMode>((static_cast<int>(m_CullMode) + 1) % (static_cast<int>(CullMode::None) + 1));

		std::cout << "\033[33m"; // TEXT COLOR
		std::cout << "**(SHARED) CullMode = ";
		switch (m_CullMode)
		{
		case dae::CullMode::Back:
			std::cout << "BACK\n";
			break;
		case dae::CullMode::Front:
			std::cout << "FRONT\n";
			break;
		case dae::CullMode::None:
			std::cout << "NONE\n";
			break;
		}

		m_pMeshes[0]->SetCullMode(m_CullMode);
		m_pHardwareRender->SetRasterizerState(m_CullMode, m_pMeshes);
	}

	void Renderer::LoadMeshes()
	{
		// Retrieve the DirectX device from the hardware renderer
		ID3D11Device* pDirectXDevice{ m_pHardwareRender->GetDevice() };
		// Retrieve the current sample state from the hardware renderer
		ID3D11SamplerState* pSampleState{ m_pHardwareRender->GetSampleState() };

		// Create the vehicle effect
		MaterialShaded* vehicleMaterial{ new MaterialShaded{ pDirectXDevice, L"Resources/Vehicle.fx" } };

		// Load all the textures needed for the vehicle
		Texture* pVehicleDiffuseTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/vehicle_diffuse.png", Texture::TextureType::Diffuse) };
		m_pTextures.push_back(pVehicleDiffuseTexture);
		Texture* pNormalTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/vehicle_normal.png", Texture::TextureType::Normal) };
		m_pTextures.push_back(pNormalTexture);
		Texture* pSpecularTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/vehicle_specular.png", Texture::TextureType::Specular) };
		m_pTextures.push_back(pSpecularTexture);
		Texture* pGlossinessTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/vehicle_gloss.png", Texture::TextureType::Glossiness) };
		m_pTextures.push_back(pGlossinessTexture);

		// Create the vehicle mesh and add it to the list of meshes
		Mesh* pVehicle{ new Mesh{ pDirectXDevice, "Resources/vehicle.obj", vehicleMaterial, pSampleState } };
		pVehicle->SetPosition({ 0.0f, 0.0f, 50.0f });
		pVehicle->SetTexture(pVehicleDiffuseTexture);
		pVehicle->SetTexture(pNormalTexture);
		pVehicle->SetTexture(pSpecularTexture);
		pVehicle->SetTexture(pGlossinessTexture);
		m_pMeshes.push_back(pVehicle);



		// Create the fire effect
		MaterialTransparent* transparentMaterial{ new MaterialTransparent{ pDirectXDevice, L"Resources/Fire.fx" } };

		// Load the texture needed for the fire
		Texture* pFireDiffuseTexture{ Texture::LoadFromFile(pDirectXDevice, "Resources/fireFX_diffuse.png", Texture::TextureType::Diffuse) };
		m_pTextures.push_back(pFireDiffuseTexture);

		// Create the fire mesh and add it ot the list of meshes
		Mesh* pFire{ new Mesh{ pDirectXDevice, "Resources/fireFX.obj", transparentMaterial, pSampleState } };
		pFire->SetPosition({ 0.0f, 0.0f, 50.0f });
		pFire->SetTexture(pFireDiffuseTexture);
		m_pMeshes.push_back(pFire);
	}
}
