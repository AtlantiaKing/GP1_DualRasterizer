#include "pch.h"
#include "SoftwareRenderer.h"
#include "Mesh.h"
#include "Camera.h"
#include "Texture.h"
#include "Utils.h"

namespace dae
{
	dae::SoftwareRenderer::SoftwareRenderer(SDL_Window* pWindow)
		: m_pWindow{ pWindow }
	{
		//Initialize
		SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

		//Create Buffers
		m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
		m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
		m_pBackBufferPixels = static_cast<uint32_t*>(m_pBackBuffer->pixels);
		m_pDepthBufferPixels = new float[static_cast<uint32_t>(m_Width * m_Height)];
		ResetDepthBuffer();
	}

	dae::SoftwareRenderer::~SoftwareRenderer()
	{
		delete[] m_pDepthBufferPixels;
	}

	void dae::SoftwareRenderer::Render(Camera* pCamera)
	{
		// Reset the depth buffer
		ResetDepthBuffer();

		// Paint the canvas black
		ClearBackground();

		//Lock BackBuffer
		SDL_LockSurface(m_pBackBuffer);

		if (m_pMesh)
		{
			std::vector<Vertex_Out> verticesOut{};

			// Convert all the vertices in the mesh from world space to NDC space
			VertexTransformationFunction(verticesOut, pCamera);

			// Create a vector for all the vertices in raster space
			std::vector<Vector2> verticesRasterSpace{};

			// Convert all the vertices from NDC space to raster space
			for (const Vertex_Out& ndcVertex : verticesOut)
			{
				verticesRasterSpace.push_back(CalculateNDCToRaster(ndcVertex.position));
			}

#ifdef IS_CLIPPING_ENABLED
			// Source: https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm

			// Calculate the points of the screen
			std::vector<Vector2> rasterVertices
			{
				{ 0.0f, 0.0f },
				{ 0.0f, static_cast<float>(m_Height) },
				{ static_cast<float>(m_Width), static_cast<float>(m_Height) },
				{ static_cast<float>(m_Width), 0.0f }
			};

			// Check each triangle if clipping should be applied
			for (int i{}; i < m_Mesh.indices.size(); i += 3)
			{
				// Calcalate the indexes of the vertices on this triangle
				const uint32_t vertexIdx0{ m_Mesh.indices[i] };
				const uint32_t vertexIdx1{ m_Mesh.indices[i + 1] };
				const uint32_t vertexIdx2{ m_Mesh.indices[i + 2] };

				// If one of the indexes are the same, this triangle should be skipped
				if (vertexIdx0 == vertexIdx1 || vertexIdx1 == vertexIdx2 || vertexIdx0 == vertexIdx2)
					continue;

				// Retrieve if the vertices are inside the frustum
				const bool isV0InFrustum{ !m_Camera.IsOutsideFrustum(m_Mesh.vertices_out[vertexIdx0].position) };
				const bool isV1InFrustum{ !m_Camera.IsOutsideFrustum(m_Mesh.vertices_out[vertexIdx1].position) };
				const bool isV2InFrustum{ !m_Camera.IsOutsideFrustum(m_Mesh.vertices_out[vertexIdx2].position) };

				// If the triangle is completely inside or completely outside the frustum, continue to the next triangle
				if (isV0InFrustum && isV1InFrustum && isV2InFrustum) continue;

				// A list of all the vertices when clipped
				std::vector<Vertex_Out> outputVertexList{ m_Mesh.vertices_out[vertexIdx0], m_Mesh.vertices_out[vertexIdx1], m_Mesh.vertices_out[vertexIdx2] };
				std::vector<Vector2> outputList{ verticesRasterSpace[vertexIdx0], verticesRasterSpace[vertexIdx1], verticesRasterSpace[vertexIdx2] };

				// For each point of the screen
				for (int rasterIdx{}; rasterIdx < rasterVertices.size(); ++rasterIdx)
				{
					// Calculate the current edge of the screen
					const Vector2 edgeStart{ rasterVertices[(rasterIdx + 1) % rasterVertices.size()] };
					const Vector2 edgeEnd{ rasterVertices[rasterIdx] };
					const Vector2 edge{ edgeStart, edgeEnd };

					// Make a copy of the current output lists and clear the output lists
					const std::vector<Vertex_Out> inputVertexList{ outputVertexList };
					const std::vector<Vector2> inputList{ outputList };
					outputVertexList.clear();
					outputList.clear();

					// For each edge on the triangle
					for (int edgeIdx{}; edgeIdx < inputList.size(); ++edgeIdx)
					{
						const int prevIndex{ edgeIdx };
						const int curIndex{ static_cast<int>((edgeIdx + 1) % inputList.size()) };

						// Calculate the points on the edge
						const Vector2 prevPoint{ inputList[prevIndex] };
						const Vector2 curPoint{ inputList[curIndex] };

						// Calculate the intersection point of the current edge of the triangle and the current edge of the screen
						Vector2 intersectPoint{ GeometryUtils::GetIntersectPoint(prevPoint, curPoint, edgeStart, edgeEnd) };

						// If the intersection point is within a margin from the screen, clamp the intersection point to the edge of the screen
						const float margin{ 0.01f };
						if (intersectPoint.x > -margin && intersectPoint.y > -margin && intersectPoint.x < m_Width + margin && intersectPoint.y < m_Height + margin)
						{
							// Clamp the intersection point to make sure it doesn't go out of the frustum
							intersectPoint.x = std::clamp(intersectPoint.x, 0.0f, static_cast<float>(m_Width));
							intersectPoint.y = std::clamp(intersectPoint.y, 0.0f, static_cast<float>(m_Height));
						}

						// Calculate if the two points of the triangle edge are on screen or not
						const bool curPointInsideRaster{ Vector2::Cross(edge, Vector2{ edgeStart, curPoint}) >= 0 };
						const bool prevPointInsideRaster{ Vector2::Cross(edge, Vector2{ edgeStart, prevPoint}) >= 0 };

						// If only one of both points of the edge is on screen, add the intersection point to the output
						//			And interpolate between all the values of the vertex
						// Additionally if the current point is on screen, add this point to the output as well
						// Else if none of the points is on screen, try adding a corner to the output
						if (curPointInsideRaster)
						{
							if (!prevPointInsideRaster)
							{
								// Only one of the points is on screen, so add the intersection point to the output
								outputList.push_back(intersectPoint);

								// Calculate the interpolating distances
								const float prevDistance{ (curPoint - intersectPoint).Magnitude() };
								const float curDistance{ (intersectPoint - prevPoint).Magnitude() };
								const float totalDistance{ curDistance + prevDistance };

								// Calculate the interpolated vertex
								Vertex_Out newVertex{};
								newVertex.uv = inputVertexList[curIndex].uv * curDistance / totalDistance
									+ inputVertexList[prevIndex].uv * prevDistance / totalDistance;
								newVertex.normal = (inputVertexList[curIndex].normal * curDistance / totalDistance
									+ inputVertexList[prevIndex].normal * prevDistance / totalDistance).Normalized();
								newVertex.tangent = (inputVertexList[curIndex].tangent * curDistance / totalDistance
									+ inputVertexList[prevIndex].tangent * prevDistance / totalDistance).Normalized();
								newVertex.viewDirection = (inputVertexList[curIndex].viewDirection * curDistance / totalDistance
									+ inputVertexList[prevIndex].viewDirection * prevDistance / totalDistance).Normalized();
								newVertex.position.z = inputVertexList[curIndex].position.z * curDistance / totalDistance
									+ inputVertexList[prevIndex].position.z * prevDistance / totalDistance;
								newVertex.position.w = inputVertexList[curIndex].position.w * curDistance / totalDistance
									+ inputVertexList[prevIndex].position.w * prevDistance / totalDistance;
								outputVertexList.push_back(newVertex);
							}

							//The current point is on screen, so add this point to the output
							outputList.push_back(curPoint);
							outputVertexList.push_back(inputVertexList[curIndex]);
						}
						else if (prevPointInsideRaster)
						{
							// Only one of the points is on screen, so add the intersection point to the output
							outputList.push_back(intersectPoint);

							// Calculate the interpolating distances
							const float prevDistance{ (curPoint - intersectPoint).Magnitude() };
							const float curDistance{ (intersectPoint - prevPoint).Magnitude() };
							const float totalDistance{ curDistance + prevDistance };

							// Calculate the interpolated vertex
							Vertex_Out newVertex{};
							newVertex.uv = inputVertexList[curIndex].uv * curDistance / totalDistance
								+ inputVertexList[prevIndex].uv * prevDistance / totalDistance;
							newVertex.normal = (inputVertexList[curIndex].normal * curDistance / totalDistance
								+ inputVertexList[prevIndex].normal * prevDistance / totalDistance).Normalized();
							newVertex.tangent = (inputVertexList[curIndex].tangent * curDistance / totalDistance
								+ inputVertexList[prevIndex].tangent * prevDistance / totalDistance).Normalized();
							newVertex.viewDirection = (inputVertexList[curIndex].viewDirection * curDistance / totalDistance
								+ inputVertexList[prevIndex].viewDirection * prevDistance / totalDistance).Normalized();
							newVertex.position.z = inputVertexList[curIndex].position.z * curDistance / totalDistance
								+ inputVertexList[prevIndex].position.z * prevDistance / totalDistance;
							newVertex.position.w = inputVertexList[curIndex].position.w * curDistance / totalDistance
								+ inputVertexList[prevIndex].position.w * prevDistance / totalDistance;
							outputVertexList.push_back(newVertex);
						}
					}
				}

				if (outputList.size() < 3) continue;

				// Replace the already created raster vertices with the first 3 raster vertices in the output list
				verticesRasterSpace[i] = outputList[0];
				verticesRasterSpace[i + 1] = outputList[1];
				verticesRasterSpace[i + 2] = outputList[2];

				// Replace the already created vertices out vertices with the first 3 vertices out in the output list
				m_Mesh.vertices_out[i] = outputVertexList[0];
				m_Mesh.vertices_out[i + 1] = outputVertexList[1];
				m_Mesh.vertices_out[i + 2] = outputVertexList[2];

				// Calculate the NDC positions and add it to the vertices out
				const Vector3 i0NDC{ CalculateRasterToNDC(outputList[0], m_Mesh.vertices_out[i].position.z) };
				m_Mesh.vertices_out[i].position.x = i0NDC.x;
				m_Mesh.vertices_out[i].position.y = i0NDC.y;

				const Vector3 i1NDC{ CalculateRasterToNDC(outputList[1], m_Mesh.vertices_out[i + 1].position.z) };
				m_Mesh.vertices_out[i + 1].position.x = i1NDC.x;
				m_Mesh.vertices_out[i + 1].position.y = i1NDC.y;

				const Vector3 i2NDC{ CalculateRasterToNDC(outputList[2], m_Mesh.vertices_out[i + 1].position.z) };
				m_Mesh.vertices_out[i + 2].position.x = i2NDC.x;
				m_Mesh.vertices_out[i + 2].position.y = i2NDC.y;

				// For each extra vertex that we have created, add them to the raster vertices and vertices out
				for (int extraVertexIdx{ 3 }; extraVertexIdx < outputList.size(); ++extraVertexIdx)
				{
					// Calculate the NDC position and add it to the vertex out
					const Vector3 ndcPosition{ CalculateRasterToNDC(outputList[extraVertexIdx], 1.0f) };
					outputVertexList[extraVertexIdx].position.x = ndcPosition.x;
					outputVertexList[extraVertexIdx].position.y = ndcPosition.y;

					// Add the vertices to the lists
					verticesRasterSpace.push_back(outputList[extraVertexIdx]);
					m_Mesh.vertices_out.push_back(outputVertexList[extraVertexIdx]);
				}

				// Make a list of the indices of all the vertices that are created by clipping
				std::vector<int> indices{ i, i + 1, i + 2 };
				for (int outputIdx{ static_cast<int>(outputList.size()) - 4 }; outputIdx >= 0; --outputIdx)
				{
					indices.push_back(static_cast<int>(m_Mesh.vertices_out.size()) - outputIdx - 1);
				}

				if (outputList.size() == 3)
				{
					// If there are only 3 points, replace the already created indices with the new ones 
					m_Mesh.useIndices[i] = indices[0];
					m_Mesh.useIndices[i + 1] = indices[1];
					m_Mesh.useIndices[i + 2] = indices[2];

					// Make sure the wind order of the indices is correct
					OrderTriangleIndices(verticesRasterSpace, i, i + 1, i + 2);
				}
				else
				{
					// The current edge
					int currentV0Idx{};
					int currentV1Idx{ 1 };

					// The current vertex we are making a new edge with
					int currentCheckIdx{ 2 };

					// The smallest angle found
					float previousAngle{};

					// Is this the first triangle to be tested
					bool isFirst{ true };

					// The amount of vertices that we have added to the indices list
					int verticesAdded{};

					// While not all the vertices have been added to the indices list
					while (verticesAdded < outputList.size())
					{
						// Create the current edge
						const Vector2 currentEdge{ (outputList[currentV1Idx] - outputList[currentV0Idx]).Normalized() };

						// For every vertex (excluding 0 because we are starting every edge from that vertex)
						for (int checkVertexIdx{ 1 }; checkVertexIdx < outputList.size(); ++checkVertexIdx)
						{
							// If the test vertex is part of the current edge, continue ot the next vertex
							if (currentV0Idx == checkVertexIdx) continue;
							if (currentV1Idx == checkVertexIdx) continue;

							// Create the test edge
							const Vector2 checkEdge{ (outputList[checkVertexIdx] - outputList[currentV0Idx]).Normalized() };

							// Calculate the signed angle between the two edges
							const float angle{ atan2f(Vector2::Cross(currentEdge, checkEdge), Vector2::Dot(currentEdge, checkEdge)) };

							// If the angle between these edges is negative, continue to the next edge
							if (angle < FLT_EPSILON) continue;

							// If no edge has been checked, or the new angle is smaller then the previous angle, save the current distance and current vertex index we are testing
							if (previousAngle < FLT_EPSILON || previousAngle > angle)
							{
								previousAngle = angle;
								currentCheckIdx = checkVertexIdx;
							}
						}

						// We now have a new triangle stored in currentV0Idx, currentV1Idx and currentCheckIdx

						// If this the first triangle that is being created
						if (isFirst)
						{
							// Replace the already created indices with the new indices
							m_Mesh.useIndices[i] = indices[currentV0Idx];
							m_Mesh.useIndices[i + 1] = indices[currentV1Idx];
							m_Mesh.useIndices[i + 2] = indices[currentCheckIdx];

							// Make sure the wind order of the indices is correct
							OrderTriangleIndices(verticesRasterSpace, i, i + 1, i + 2);

							// Add these 3 vertices to the vertex count
							verticesAdded += 3;

							// Make sure all new triangles are added extra
							isFirst = false;
						}
						else
						{
							// A brand new triangle has been created

							// Add the indices of this triangle to the back of the indices list
							m_Mesh.useIndices.push_back(indices[currentV0Idx]);
							m_Mesh.useIndices.push_back(indices[currentV1Idx]);
							m_Mesh.useIndices.push_back(indices[currentCheckIdx]);

							// Make sure the wind order of the indices is correct
							int indicesSizeInt{ static_cast<int>(m_Mesh.useIndices.size()) };
							OrderTriangleIndices(verticesRasterSpace, indicesSizeInt - 3, indicesSizeInt - 2, indicesSizeInt - 1);

							// Only one new vertex has been added, so add 1 to the vertex count
							++verticesAdded;
						}

						// Set the current edge vertex to the last tested vertex
						currentV1Idx = currentCheckIdx;
						// Reset the angle to 0
						previousAngle = 0;
					}
				}
			}
#endif

			// Get all the indices of the mesh
			std::vector<uint32_t>& indices{ m_pMesh->GetIndices() };

			// Depending on the topology of the mesh, use indices differently
			switch (m_pMesh->GetPrimitiveTopology())
			{
			case PrimitiveTopology::TriangleList:
				// For each triangle
				for (int curStartVertexIdx{}; curStartVertexIdx < indices.size(); curStartVertexIdx += 3)
				{
					RenderTriangle(verticesRasterSpace, verticesOut, indices, curStartVertexIdx, false);
				}
				break;
			case PrimitiveTopology::TriangleStrip:
				// For each triangle
				for (int curStartVertexIdx{}; curStartVertexIdx < indices.size() - 2; ++curStartVertexIdx)
				{
					RenderTriangle(verticesRasterSpace, verticesOut, indices, curStartVertexIdx, curStartVertexIdx % 2);
				}
				break;
			}
		}

		//Update SDL Surface
		SDL_UnlockSurface(m_pBackBuffer);
		SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
		SDL_UpdateWindowSurface(m_pWindow);
	}

	void dae::SoftwareRenderer::ToggleRenderState(RendererState toggleState)
	{
		// If the toggleState is equal to the current state, switch back to the default state
		// Else, set the current state to the toggleState
		m_RendererState = m_RendererState == toggleState ? RendererState::Default : toggleState;
	}

	void dae::SoftwareRenderer::ToggleLightingMode()
	{
		// Shuffle through all the lighting modes
		m_LightingMode = static_cast<LightingMode>((static_cast<int>(m_LightingMode) + 1) % (static_cast<int>(LightingMode::Specular) + 1));
	}

	void dae::SoftwareRenderer::ToggleNormalMap()
	{
		// Toggle the normal map active variable
		m_IsNormalMapActive = !m_IsNormalMapActive;
	}

	void SoftwareRenderer::SetTextures(Texture* pDiffuseTexture, Texture* pNormalTexture, Texture* pSpecularTexture, Texture* pGlossinessTexture)
	{
		m_pDiffuseTexture = pDiffuseTexture;
		m_pNormalTexture = pNormalTexture;
		m_pSpecularTexture = pSpecularTexture;
		m_pGlossinessTexture = pGlossinessTexture;
	}

	void SoftwareRenderer::SetMesh(Mesh* pMesh)
	{
		// Set the current mesh that should be displayed
		m_pMesh = pMesh;
	}

	bool dae::SoftwareRenderer::SaveBufferToImage() const
	{
		return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
	}

	void dae::SoftwareRenderer::VertexTransformationFunction(std::vector<Vertex_Out>& verticesOut, Camera* pCamera)
	{
		// Retrieve the world matrix of the current mesh
		Matrix& worldMatrix{ m_pMesh->GetWorldMatrix() };
		std::vector<Vertex> vertices{ m_pMesh->GetVertices() };

		// Calculate the transformation matrix for this mesh
		Matrix worldViewProjectionMatrix{ worldMatrix * pCamera->GetViewMatrix() * pCamera->GetProjectionMatrix() };

		// Reserve the amount of vertices into the new vertex list
		verticesOut.reserve(vertices.size());

		// For each vertex in the mesh
		for (const Vertex& v : vertices)
		{
			// Create a new vertex	
			Vertex_Out vOut{ {}, v.normal, v.tangent, v.uv, v.color };

			// Tranform the vertex using the inversed view matrix
			vOut.position = worldViewProjectionMatrix.TransformPoint({ v.position, 1.0f });

			// Calculate the view direction
			vOut.viewDirection = Vector3{ vOut.position.x, vOut.position.y, vOut.position.z };
			vOut.viewDirection.Normalize();

			// Divide all properties of the position by the original z (stored in position.w)
			vOut.position.x /= vOut.position.w;
			vOut.position.y /= vOut.position.w;
			vOut.position.z /= vOut.position.w;

			// Transform the normal and the tangent of the vertex
			vOut.normal = worldMatrix.TransformVector(v.normal);
			vOut.tangent = worldMatrix.TransformVector(v.tangent);

			// Add the new vertex to the list of NDC vertices
			verticesOut.emplace_back(vOut);
		}
	}

	void dae::SoftwareRenderer::RenderTriangle(const std::vector<Vector2>& rasterVertices, const std::vector<Vertex_Out>& verticesOut, const std::vector<uint32_t>& indices, int curVertexIdx, bool swapVertices) const
	{
		// Calcalate the indexes of the vertices on this triangle
		const uint32_t vertexIdx0{ indices[static_cast<uint32_t>(curVertexIdx)] };
		const uint32_t vertexIdx1{ indices[static_cast<uint32_t>(curVertexIdx + 1 * !swapVertices + 2 * swapVertices)] };
		const uint32_t vertexIdx2{ indices[static_cast<uint32_t>(curVertexIdx + 2 * !swapVertices + 1 * swapVertices)] };

		// If a triangle has the same vertex twice
		// Or if a one of the vertices is outside the frustum
		// Continue
		if (vertexIdx0 == vertexIdx1 || vertexIdx1 == vertexIdx2 || vertexIdx0 == vertexIdx2 ||
			IsOutsideFrustum(verticesOut[vertexIdx0].position) ||
			IsOutsideFrustum(verticesOut[vertexIdx1].position) ||
			IsOutsideFrustum(verticesOut[vertexIdx2].position))
			return;

		// Get all the current vertices
		const Vector2 v0{ rasterVertices[vertexIdx0] };
		const Vector2 v1{ rasterVertices[vertexIdx1] };
		const Vector2 v2{ rasterVertices[vertexIdx2] };

		// Calculate the edges of the current triangle
		const Vector2 edge01{ v1 - v0 };
		const Vector2 edge12{ v2 - v1 };
		const Vector2 edge20{ v0 - v2 };

		// Calculate the area of the current triangle
		const float fullTriangleArea{ Vector2::Cross(edge01, edge12) };

		// If the triangle area is 0 or NaN, continue to the next triangle
		if (fullTriangleArea < FLT_EPSILON || isnan(fullTriangleArea)) return;

		// Calculate the bounding box of this triangle
		Vector2 minBoundingBox{ Vector2::Min(v0, Vector2::Min(v1, v2)) };
		Vector2 maxBoundingBox{ Vector2::Max(v0, Vector2::Max(v1, v2)) };

		// A margin that enlarges the bounding box, makes sure that some pixels do no get ignored
		const int margin{ 1 };

		// Calculate the start and end pixel bounds of this triangle
		const int startX{ std::clamp(static_cast<int>(minBoundingBox.x - margin), 0, m_Width) };
		const int startY{ std::clamp(static_cast<int>(minBoundingBox.y - margin), 0, m_Height) };
		const int endX{ std::clamp(static_cast<int>(maxBoundingBox.x + margin), 0, m_Width) };
		const int endY{ std::clamp(static_cast<int>(maxBoundingBox.y + margin), 0, m_Height) };

		// For each pixel
		for (int py{ startY }; py < endY; ++py)
		{
			for (int px{ startX }; px < endX; ++px)
			{
				// Calculate the pixel index and create a Vector2 of the current pixel
				const int pixelIdx{ px + py * m_Width };
				const Vector2 curPixel{ static_cast<float>(px), static_cast<float>(py) };

				// If only the bounding box should be rendered, do no triangle checks, just display a white color
				if (m_RendererState == RendererState::BoundingBox)
				{
					m_pBackBufferPixels[pixelIdx] = SDL_MapRGB(m_pBackBuffer->format,
						static_cast<uint8_t>(255),
						static_cast<uint8_t>(255),
						static_cast<uint8_t>(255));

					continue;
				}

				// Calculate the vector between the first vertex and the point
				const Vector2 v0ToPoint{ curPixel - v0 };
				const Vector2 v1ToPoint{ curPixel - v1 };
				const Vector2 v2ToPoint{ curPixel - v2 };

				// Calculate cross product from edge to start to point
				const float edge01PointCross{ Vector2::Cross(edge01, v0ToPoint) };
				const float edge12PointCross{ Vector2::Cross(edge12, v1ToPoint) };
				const float edge20PointCross{ Vector2::Cross(edge20, v2ToPoint) };

				// Check if pixel is inside triangle, if not continue to the next pixel
				if (!(edge01PointCross >= 0 && edge12PointCross >= 0 && edge20PointCross >= 0)) continue;

				// Calculate the barycentric weights
				const float weightV0{ edge12PointCross / fullTriangleArea };
				const float weightV1{ edge20PointCross / fullTriangleArea };
				const float weightV2{ edge01PointCross / fullTriangleArea };

				// Calculate the Z depth at this pixel
				const float interpolatedZDepth
				{
					1.0f /
						(weightV0 / verticesOut[vertexIdx0].position.z +
						weightV1 / verticesOut[vertexIdx1].position.z +
						weightV2 / verticesOut[vertexIdx2].position.z)
				};

				// If the depth is outside the frustum,
				// Or if the current depth buffer is less then the current depth,
				// continue to the next pixel
				if (m_pDepthBufferPixels[pixelIdx] < interpolatedZDepth)
					continue;

				// Save the new depth
				m_pDepthBufferPixels[pixelIdx] = interpolatedZDepth;

				// The pixel info
				Vertex_Out pixelInfo{};

				// Switch between all the render states
				switch (m_RendererState)
				{
				case RendererState::Default:
				{
					// Calculate the W depth at this pixel
					const float interpolatedWDepth
					{
						1.0f /
							(weightV0 / verticesOut[vertexIdx0].position.w +
							weightV1 / verticesOut[vertexIdx1].position.w +
							weightV2 / verticesOut[vertexIdx2].position.w)
					};

					// Calculate the UV coordinate at this pixel
					pixelInfo.uv =
					{
						(weightV0 * verticesOut[vertexIdx0].uv / verticesOut[vertexIdx0].position.w +
						weightV1 * verticesOut[vertexIdx1].uv / verticesOut[vertexIdx1].position.w +
						weightV2 * verticesOut[vertexIdx2].uv / verticesOut[vertexIdx2].position.w)
							* interpolatedWDepth
					};

					// Calculate the normal at this pixel
					pixelInfo.normal =
						Vector3{
							(weightV0 * verticesOut[vertexIdx0].normal / verticesOut[vertexIdx0].position.w +
							weightV1 * verticesOut[vertexIdx1].normal / verticesOut[vertexIdx1].position.w +
							weightV2 * verticesOut[vertexIdx2].normal / verticesOut[vertexIdx2].position.w)
								* interpolatedWDepth
					}.Normalized();

					// Calculate the tangent at this pixel
					pixelInfo.tangent =
						Vector3{
							(weightV0 * verticesOut[vertexIdx0].tangent / verticesOut[vertexIdx0].position.w +
							weightV1 * verticesOut[vertexIdx1].tangent / verticesOut[vertexIdx1].position.w +
							weightV2 * verticesOut[vertexIdx2].tangent / verticesOut[vertexIdx2].position.w)
								* interpolatedWDepth
					}.Normalized();

					// Calculate the view direction at this pixel
					pixelInfo.viewDirection =
						Vector3{
							(weightV0 * verticesOut[vertexIdx0].viewDirection / verticesOut[vertexIdx0].position.w +
							weightV1 * verticesOut[vertexIdx1].viewDirection / verticesOut[vertexIdx1].position.w +
							weightV2 * verticesOut[vertexIdx2].viewDirection / verticesOut[vertexIdx2].position.w)
								* interpolatedWDepth
					}.Normalized();

					break;
				}
				case RendererState::Depth:
				{
					// Remap the Z depth
					const float depthColor{ Remap(interpolatedZDepth, 0.997f, 1.0f) };

					// Set the color of the current pixel to showcase the depth
					pixelInfo.color = { depthColor, depthColor, depthColor };
					break;
				}
				}

				// Calculate the shading at this pixel and display it on screen
				PixelShading(px + (py * m_Width), pixelInfo);
			}
		}
	}

	void SoftwareRenderer::ClearBackground() const
	{
		// Fill the background with black (0,0,0)
		int colorValue{ static_cast<int>(0.39f * 255) };
		SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, colorValue, colorValue, colorValue));
	}

	void SoftwareRenderer::ResetDepthBuffer() const
	{
		// The nr of pixels in the buffer
		const int nrPixels{ m_Width * m_Height };

		// Set everything in the depth buffer to the value FLT_MAX
		std::fill_n(m_pDepthBufferPixels, nrPixels, FLT_MAX);
	}

	void SoftwareRenderer::PixelShading(int pixelIdx, const Vertex_Out& pixelInfo) const
	{
		// The normal that should be used in calculations
		Vector3 useNormal{ pixelInfo.normal };

		// If the normal map is active
		if (m_IsNormalMapActive)
		{
			// Calculate the binormal in this pixel
			Vector3 binormal = Vector3::Cross(pixelInfo.normal, pixelInfo.tangent);

			// Create a matrix using the tangent, normal and binormal
			Matrix tangentSpaceAxis = Matrix{ pixelInfo.tangent, binormal, pixelInfo.normal, Vector3::Zero };

			// Sample a color from the normal map and clamp it between -1 and 1
			ColorRGB currentNormalMap{ 2.0f * m_pNormalTexture->Sample(pixelInfo.uv) - ColorRGB{ 1.0f, 1.0f, 1.0f } };

			// Make a vector3 of the colorRGB object
			Vector3 normalMapSample{ currentNormalMap.r, currentNormalMap.g, currentNormalMap.b };

			// Transform the normal map value using the calculated matrix of this pixel
			useNormal = tangentSpaceAxis.TransformVector(normalMapSample);
		}

		// Create the light data
		Vector3 lightDirection{ 0.577f, -0.577f, 0.577f };
		lightDirection.Normalize();
		const float lightIntensity{ 7.0f };
		const float specularShininess{ 25.0f };

		// The final color that will be rendered
		ColorRGB finalColor{};
		ColorRGB ambientColor{ 0.025f, 0.025f, 0.025f };

		// Depending on the rendering state, do other things
		switch (m_RendererState)
		{
		case RendererState::Default:
		{
			// Calculate the observed area in this pixel
			const float observedArea{ Vector3::DotClamped(useNormal.Normalized(), -lightDirection.Normalized()) };

			// Depending on the lighting mode, different shading should be applied
			switch (m_LightingMode)
			{
			case LightingMode::Combined:
			{
				// Calculate the lambert shader
				const ColorRGB lambert{ LightingUtils::Lambert(1.0f, m_pDiffuseTexture->Sample(pixelInfo.uv)) };
				// Calculate the phong exponent
				const float specularExp{ specularShininess * m_pGlossinessTexture->Sample(pixelInfo.uv).r };
				// Calculate the phong shader
				const ColorRGB specular{ m_pSpecularTexture->Sample(pixelInfo.uv) * LightingUtils::Phong(1.0f, specularExp, -lightDirection, pixelInfo.viewDirection, useNormal) };

				// Lambert + Phong + ObservedArea
				finalColor += (lightIntensity * lambert + specular) * observedArea + ambientColor;
				break;
			}
			case LightingMode::ObservedArea:
			{
				// Only show the calculated observed area
				finalColor += ColorRGB{ observedArea, observedArea, observedArea };
				break;
			}
			case LightingMode::Diffuse:
			{
				// Calculate the lambert shader and display it on screen together with the observed area
				finalColor += lightIntensity * observedArea * LightingUtils::Lambert(1.0f, m_pDiffuseTexture->Sample(pixelInfo.uv));
				break;
			}
			case LightingMode::Specular:
			{
				// Calculate the phong exponent
				const float specularExp{ specularShininess * m_pGlossinessTexture->Sample(pixelInfo.uv).r };
				// Calculate the phong shader
				const ColorRGB specular{ m_pSpecularTexture->Sample(pixelInfo.uv) * LightingUtils::Phong(1.0f, specularExp, -lightDirection, pixelInfo.viewDirection, useNormal) };
				// Phong + observed area
				finalColor += specular * observedArea;
				break;
			}
			}

			// Create the ambient color	and add it to the final color
			const ColorRGB ambientColor{ 0.025f, 0.025f, 0.025f };
			finalColor += ambientColor;

			break;
		}
		case RendererState::Depth:
		{
			// Only render the depth which is saved in the color attribute of the pixel info
			finalColor += pixelInfo.color;
			break;
		}
		}

		//Update Color in Buffer
		finalColor.MaxToOne();

		m_pBackBufferPixels[pixelIdx] = SDL_MapRGB(m_pBackBuffer->format,
			static_cast<uint8_t>(finalColor.r * 255),
			static_cast<uint8_t>(finalColor.g * 255),
			static_cast<uint8_t>(finalColor.b * 255));
	}

	inline Vector2 dae::SoftwareRenderer::CalculateNDCToRaster(const Vector3& ndcVertex) const
	{
		return Vector2
		{
			(ndcVertex.x + 1) / 2.0f * m_Width,
			(1.0f - ndcVertex.y) / 2.0f * m_Height
		};
	}

	inline bool SoftwareRenderer::IsOutsideFrustum(const Vector4& v) const
	{
		return v.x < -1.0f || v.x > 1.0f || v.y < -1.0f || v.y > 1.0f || v.z < 0.0f || v.z > 1.0f;
	}
}
