#include "pch.h"
#include "Mesh.h"
#include "Utils.h"
#include "Material.h"
#include "Texture.h"
#include "MaterialTransparent.h"
#include <ppl.h> // Parallel Stuff
#include <future>

#define IS_CLIPPING_ENABLED
#define PARALLEL

namespace dae
{
	Mesh::Mesh(ID3D11Device* pDevice, const std::string& filePath, Material* pMaterial, ID3D11SamplerState* pSampleState)
		: m_IsTransparent{ typeid(*pMaterial) == typeid(MaterialTransparent) }
		, m_pMaterial{ pMaterial }
	{
		const bool parseResult{ Utils::ParseOBJ(filePath, m_Vertices, m_Indices) };
		if (!parseResult)
		{
			std::cout << "Failed to load OBJ from " << filePath << "\n";
			return;
		}

		// Set the cullmode to none when using a transparent material
		if (m_IsTransparent) m_CullMode = CullMode::None;

		// Create Input Layout
		m_pInputLayout = pMaterial->LoadInputLayout(pDevice);

		// Create vertex buffer
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.ByteWidth = sizeof(Vertex) * static_cast<uint32_t>(m_Vertices.size());
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = m_Vertices.data();

		HRESULT result{ pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer) };
		if (FAILED(result)) return;

		// Create index buffer
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.ByteWidth = sizeof(uint32_t) * static_cast<uint32_t>(m_Indices.size());
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;
		initData.pSysMem = m_Indices.data();

		result = pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
		if (FAILED(result)) return;

		if (pSampleState) SetSamplerState(pSampleState);
	}

	Mesh::~Mesh()
	{
		if (m_pIndexBuffer) m_pIndexBuffer->Release();
		if (m_pVertexBuffer) m_pVertexBuffer->Release();

		if (m_pInputLayout) m_pInputLayout->Release();

		delete m_pMaterial;
	}

	void Mesh::RotateY(float angle)
	{
		const Matrix rotationMatrix{ Matrix::CreateRotationY(angle) };
		m_WorldMatrix = rotationMatrix * m_WorldMatrix;
	}

	void Mesh::SetPosition(const Vector3& position)
	{
		m_WorldMatrix[3][0] = position.x;
		m_WorldMatrix[3][1] = position.y;
		m_WorldMatrix[3][2] = position.z;
	}

	void Mesh::HardwareRender(ID3D11DeviceContext* pDeviceContext) const
	{
		if (!m_IsVisible) return;

		// Set primitive topology
		pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set input layout
		pDeviceContext->IASetInputLayout(m_pInputLayout);

		// Set vertex buffer
		constexpr UINT stride{ sizeof(Vertex) };
		constexpr UINT offset{ 0 };
		pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

		// Set index buffer
		pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		// Draw
		D3DX11_TECHNIQUE_DESC techniqueDesc{};
		m_pMaterial->GetTechnique()->GetDesc(&techniqueDesc);
		for (UINT p{}; p < techniqueDesc.Passes; ++p)
		{
			m_pMaterial->GetTechnique()->GetPassByIndex(p)->Apply(0, pDeviceContext);
			pDeviceContext->DrawIndexed(static_cast<uint32_t>(m_Indices.size()), 0, 0);
		}
	}

	const Matrix& Mesh::GetWorldMatrix() const
	{
		return m_WorldMatrix;
	}

	void Mesh::SoftwareRender(Camera* pCamera, const SoftwareRenderInfo& renderInfo)
	{
		std::vector<Vertex_Out> verticesOut{};

		// Convert all the vertices in the mesh from world space to NDC space
		GeometryUtils::VertexTransformationFunction(m_WorldMatrix, m_Vertices, verticesOut, pCamera);

		// Create a vector for all the vertices in raster space
		std::vector<Vector2> verticesRasterSpace{};

		// Convert all the vertices from NDC space to raster space
		verticesRasterSpace.reserve(verticesOut.size());
		for (const Vertex_Out& ndcVertex : verticesOut)
		{
			verticesRasterSpace.emplace_back(

				(ndcVertex.position.x + 1) / 2.0f * renderInfo.width ,
					(1.0f - ndcVertex.position.y) / 2.0f * renderInfo.height
			);
		}

#ifdef IS_CLIPPING_ENABLED
		m_UseIndices.clear();
		m_UseIndices.reserve(m_Indices.size());

		// Calculate the points of the screen
		const std::vector<Vector2> rasterVertices
		{
			{ 0.0f, 0.0f },
			{ 0.0f, static_cast<float>(renderInfo.height) },
			{ static_cast<float>(renderInfo.width), static_cast<float>(renderInfo.height) },
			{ static_cast<float>(renderInfo.width), 0.0f }
		};

		// Check each triangle if clipping should be applied
		for (uint32_t i{}; i < m_Indices.size(); i += 3)
		{
			ClipTriangle(verticesOut, verticesRasterSpace, rasterVertices, renderInfo, i);
		}
#endif

#ifdef IS_CLIPPING_ENABLED
		const uint32_t nrIndices{ static_cast<uint32_t>(m_UseIndices.size()) };
#else
		const uint32_t nrIndices{ static_cast<uint32_t>(m_Indices.size()) };
#endif

		// Depending on the topology of the mesh, use indices differently
		switch (m_PrimitiveTopology)
		{
		case PrimitiveTopology::TriangleList:
		{
#ifndef PARALLEL
			// For each triangle
			for (uint32_t curStartVertexIdx{}; curStartVertexIdx < nrIndices; curStartVertexIdx += 3)
			{
				RenderTriangle(verticesRasterSpace, verticesOut, curStartVertexIdx, false, renderInfo);
			}
#else
			// For each triangle (multithreaded, except for transparent meshes)
			if (m_IsTransparent)
			{
				for (uint32_t curStartVertexIdx{}; curStartVertexIdx < nrIndices; curStartVertexIdx += 3)
				{
					RenderTriangle(verticesRasterSpace, verticesOut, curStartVertexIdx, false, renderInfo);
				}
			}
			else
			{
				// Parallel For Logic
				concurrency::parallel_for(0u, nrIndices / 3,
					[&](size_t i)
					{
						RenderTriangle(verticesRasterSpace, verticesOut, i * 3, false, renderInfo);
					});
			}
#endif
			break;
		}
		case PrimitiveTopology::TriangleStrip:
		{
			// For each triangle
			for (uint32_t curStartVertexIdx{}; curStartVertexIdx < nrIndices - 2; ++curStartVertexIdx)
			{
				RenderTriangle(verticesRasterSpace, verticesOut, curStartVertexIdx, curStartVertexIdx % 2, renderInfo);
			}
			break;
		}
		}
	}

	// Source: https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm
	void Mesh::ClipTriangle(std::vector<Vertex_Out>& verticesOut, std::vector<Vector2>& verticesRasterSpace, const std::vector<Vector2>& rasterVertices, const SoftwareRenderInfo& renderInfo, size_t i)
	{
		// Calcalate the indexes of the vertices on this triangle
		const uint32_t vertexIdx0{ m_Indices[i] };
		const uint32_t vertexIdx1{ m_Indices[i + 1] };
		const uint32_t vertexIdx2{ m_Indices[i + 2] };

		// If one of the indexes are the same, this triangle should be skipped
		if (vertexIdx0 == vertexIdx1 || vertexIdx1 == vertexIdx2 || vertexIdx0 == vertexIdx2)
			return;

		const Vertex_Out& v0{ verticesOut[vertexIdx0] };
		const Vertex_Out& v1{ verticesOut[vertexIdx1] };
		const Vertex_Out& v2{ verticesOut[vertexIdx2] };

		// Retrieve if the vertices are inside the frustum
		const bool isV0InFrustum{ !GeometryUtils::IsOutsideFrustum(v0.position) };
		const bool isV1InFrustum{ !GeometryUtils::IsOutsideFrustum(v1.position) };
		const bool isV2InFrustum{ !GeometryUtils::IsOutsideFrustum(v2.position) };

		// If the triangle is completely inside or completely outside the frustum, continue to the next triangle
		if (isV0InFrustum && isV1InFrustum && isV2InFrustum || !isV0InFrustum && !isV1InFrustum && !isV2InFrustum)
		{
			if (isV0InFrustum)
			{
				m_UseIndices.push_back(vertexIdx0);
				m_UseIndices.push_back(vertexIdx1);
				m_UseIndices.push_back(vertexIdx2);
			}
			return;
		}

		// A list of all the vertices when clipped
		std::vector<Vertex_Out> outputVertexList{ v0, v1, v2 };
		std::vector<Vector2> outputList{ verticesRasterSpace[vertexIdx0], verticesRasterSpace[vertexIdx1], verticesRasterSpace[vertexIdx2] };

		// For each point of the screen
		for (size_t rasterIdx{}; rasterIdx < rasterVertices.size(); ++rasterIdx)
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
			for (size_t edgeIdx{}; edgeIdx < inputList.size(); ++edgeIdx)
			{
				const size_t prevIndex{ edgeIdx };
				const size_t curIndex{ ((edgeIdx + 1) % inputList.size()) };

				// Calculate the points on the edge
				const Vector2 prevPoint{ inputList[prevIndex] };
				const Vector2 curPoint{ inputList[curIndex] };

				// Calculate the intersection point of the current edge of the triangle and the current edge of the screen
				Vector2 intersectPoint{ GeometryUtils::GetIntersectPoint(prevPoint, curPoint, edgeStart, edgeEnd) };

				// If the intersection point is within a margin from the screen, clamp the intersection point to the edge of the screen
				constexpr float margin{ 0.01f };
				if (intersectPoint.x > -margin && intersectPoint.y > -margin && intersectPoint.x < renderInfo.width + margin && intersectPoint.y < renderInfo.height + margin)
				{
					// Clamp the intersection point to make sure it doesn't go out of the frustum
					intersectPoint.x = std::clamp(intersectPoint.x, 0.0f, static_cast<float>(renderInfo.width));
					intersectPoint.y = std::clamp(intersectPoint.y, 0.0f, static_cast<float>(renderInfo.height));
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

		if (outputList.size() < 3) return;

		// Replace the already created raster vertices with the first 3 raster vertices in the output list
		verticesRasterSpace[i] = outputList[0];
		verticesRasterSpace[i + 1] = outputList[1];
		verticesRasterSpace[i + 2] = outputList[2];

		// Replace the already created vertices out vertices with the first 3 vertices out in the output list
		verticesOut[i] = outputVertexList[0];
		verticesOut[i + 1] = outputVertexList[1];
		verticesOut[i + 2] = outputVertexList[2];

		// Calculate the NDC positions and add it to the vertices out
		const Vector3 i0NDC{ GeometryUtils::CalculateRasterToNDC(outputList[0], verticesOut[i].position.z, renderInfo) };
		verticesOut[i].position.x = i0NDC.x;
		verticesOut[i].position.y = i0NDC.y;

		const Vector3 i1NDC{ GeometryUtils::CalculateRasterToNDC(outputList[1], verticesOut[i + 1].position.z, renderInfo) };
		verticesOut[i + 1].position.x = i1NDC.x;
		verticesOut[i + 1].position.y = i1NDC.y;

		const Vector3 i2NDC{ GeometryUtils::CalculateRasterToNDC(outputList[2], verticesOut[i + 1].position.z, renderInfo) };
		verticesOut[i + 2].position.x = i2NDC.x;
		verticesOut[i + 2].position.y = i2NDC.y;

		// For each extra vertex that we have created, add them to the raster vertices and vertices out
		for (size_t extraVertexIdx{ 3 }; extraVertexIdx < outputList.size(); ++extraVertexIdx)
		{
			// Calculate the NDC position and add it to the vertex out
			const Vector3 ndcPosition{ GeometryUtils::CalculateRasterToNDC(outputList[extraVertexIdx], 1.0f, renderInfo) };
			outputVertexList[extraVertexIdx].position.x = ndcPosition.x;
			outputVertexList[extraVertexIdx].position.y = ndcPosition.y;

			// Add the vertices to the lists
			verticesRasterSpace.push_back(outputList[extraVertexIdx]);
			verticesOut.push_back(outputVertexList[extraVertexIdx]);
		}

		// Make a list of the indices of all the vertices that are created by clipping
		std::vector<size_t> indices{ i, i + 1, i + 2 };
		for (int outputIdx{ static_cast<int>(outputList.size()) - 4 }; outputIdx >= 0; --outputIdx)
		{
			indices.push_back(verticesOut.size() - outputIdx - 1);
		}

		if (outputList.size() == 3)
		{
			// If there are only 3 points, replace the already created indices with the new ones 
			m_UseIndices.push_back(static_cast<uint32_t>(indices[0]));
			m_UseIndices.push_back(static_cast<uint32_t>(indices[1]));
			m_UseIndices.push_back(static_cast<uint32_t>(indices[2]));

			// Make sure the wind order of the indices is correct
			int indicesSizeInt{ static_cast<int>(m_UseIndices.size()) };
			GeometryUtils::OrderTriangleIndices(m_UseIndices, verticesRasterSpace, indicesSizeInt - 3, indicesSizeInt - 2, indicesSizeInt - 1);
		}
		else
		{
			// The current edge
			size_t currentV0Idx{};
			size_t currentV1Idx{ 1 };

			// The current vertex we are making a new edge with
			size_t currentCheckIdx{ 2 };

			// The smallest angle found
			float previousAngle{};
			
			// The amount of vertices that we have added to the indices list
			size_t verticesAdded{};

			// While not all the vertices have been added to the indices list
			while (verticesAdded < outputList.size())
			{
				// Create the current edge
				const Vector2 currentEdge{ (outputList[currentV1Idx] - outputList[currentV0Idx]).Normalized() };

				// For every vertex (excluding 0 because we are starting every edge from that vertex)
				for (size_t checkVertexIdx{ 1 }; checkVertexIdx < outputList.size(); ++checkVertexIdx)
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

				// A brand new triangle has been created

				// Add the indices of this triangle to the back of the indices list
				m_UseIndices.push_back(static_cast<uint32_t>(indices[currentV0Idx]));
				m_UseIndices.push_back(static_cast<uint32_t>(indices[currentV1Idx]));
				m_UseIndices.push_back(static_cast<uint32_t>(indices[currentCheckIdx]));

				// Make sure the wind order of the indices is correct
				int indicesSizeInt{ static_cast<int>(m_UseIndices.size()) };
				GeometryUtils::OrderTriangleIndices(m_UseIndices, verticesRasterSpace, indicesSizeInt - 3, indicesSizeInt - 2, indicesSizeInt - 1);

				// Only one new vertex has been added, so add 1 to the vertex count
				++verticesAdded;

				// Set the current edge vertex to the last tested vertex
				currentV1Idx = currentCheckIdx;
				// Reset the angle to 0
				previousAngle = 0;
			}
		}
	}

	void dae::Mesh::RenderTriangle(const std::vector<Vector2>& rasterVertices, const std::vector<Vertex_Out>& verticesOut, size_t curVertexIdx, bool swapVertices, const SoftwareRenderInfo& renderInfo) const
	{
#ifdef IS_CLIPPING_ENABLED
		// Calcalate the indexes of the vertices on this triangle
		const size_t vertexIdx0{ m_UseIndices[curVertexIdx] };
		const size_t vertexIdx1{ m_UseIndices[curVertexIdx + 1 * !swapVertices + 2 * swapVertices] };
		const size_t vertexIdx2{ m_UseIndices[curVertexIdx + 2 * !swapVertices + 1 * swapVertices] };
#else
		// Calcalate the indexes of the vertices on this triangle
		const size_t vertexIdx0{ m_Indices[curVertexIdx] };
		const size_t vertexIdx1{ m_Indices[curVertexIdx + 1 * !swapVertices + 2 * swapVertices] };
		const size_t vertexIdx2{ m_Indices[curVertexIdx + 2 * !swapVertices + 1 * swapVertices] };
#endif

		// If a triangle has the same vertex twice
		// Or if a one of the vertices is outside the frustum
		// Continue
		if (vertexIdx0 == vertexIdx1 || vertexIdx1 == vertexIdx2 || vertexIdx0 == vertexIdx2 ||
			GeometryUtils::IsOutsideFrustum(verticesOut[vertexIdx0].position) ||
			GeometryUtils::IsOutsideFrustum(verticesOut[vertexIdx1].position) ||
			GeometryUtils::IsOutsideFrustum(verticesOut[vertexIdx2].position))
			return;

		// Get all the current vertices
		const Vector2& v0{ rasterVertices[vertexIdx0] };
		const Vector2& v1{ rasterVertices[vertexIdx1] };
		const Vector2& v2{ rasterVertices[vertexIdx2] };

		// Calculate the edges of the current triangle
		const Vector2 edge01{ v1 - v0 };
		const Vector2 edge12{ v2 - v1 };
		const Vector2 edge20{ v0 - v2 };

		// Calculate the area of the current triangle
		const float fullTriangleArea{ Vector2::Cross(edge01, edge12) };

		// If the triangle area is 0 or NaN, continue to the next triangle
		if (abs(fullTriangleArea) < FLT_EPSILON || isnan(fullTriangleArea)) return;

		// Calculate the bounding box of this triangle
		const Vector2 minBoundingBox{ Vector2::Min(v0, Vector2::Min(v1, v2)) };
		const Vector2 maxBoundingBox{ Vector2::Max(v0, Vector2::Max(v1, v2)) };

		// A margin that enlarges the bounding box, makes sure that some pixels do no get ignored
		constexpr int margin{ 1 };

		// Calculate the start and end pixel bounds of this triangle
		const int startX{ std::clamp(static_cast<int>(minBoundingBox.x - margin), 0, renderInfo.width) };
		const int startY{ std::clamp(static_cast<int>(minBoundingBox.y - margin), 0, renderInfo.height) };
		const int endX{ std::clamp(static_cast<int>(maxBoundingBox.x + margin), 0, renderInfo.width) };
		const int endY{ std::clamp(static_cast<int>(maxBoundingBox.y + margin), 0, renderInfo.height) };

		// For each pixel
		for (int py{ startY }; py < endY; ++py)
		{
			for (int px{ startX }; px < endX; ++px)
			{
				// Calculate the pixel index and create a Vector2 of the current pixel
				const int pixelIdx{ px + py * renderInfo.width };

				// If only the bounding box should be rendered, do no triangle checks, just display a white color
				if (renderInfo.isShowingBoundingBoxes)
				{
					renderInfo.pBackBufferPixels[pixelIdx] = SDL_MapRGB(renderInfo.pBackBuffer->format,
						static_cast<uint8_t>(255),
						static_cast<uint8_t>(255),
						static_cast<uint8_t>(255));

					continue;
				}

				// Calculate the current pixel position
				const Vector2 curPixel{ static_cast<float>(px), static_cast<float>(py) };

				// Calculate the vector between the first vertex and the point
				const Vector2 v0ToPoint{ curPixel - v0 };
				const Vector2 v1ToPoint{ curPixel - v1 };
				const Vector2 v2ToPoint{ curPixel - v2 };

				// Calculate cross product from edge to start to point
				const float edge01PointCross{ Vector2::Cross(edge01, v0ToPoint) };
				const float edge12PointCross{ Vector2::Cross(edge12, v1ToPoint) };
				const float edge20PointCross{ Vector2::Cross(edge20, v2ToPoint) };

				// Calculate which side of the triangle has been hit
				const bool isFrontFaceHit{ edge01PointCross > 0 && edge12PointCross > 0 && edge20PointCross > 0 };
				const bool isBackFaceHit{ edge01PointCross < 0 && edge12PointCross < 0 && edge20PointCross < 0 };

				// Continue to the next pixel if this pixel does not meet the criteria of the cullmode
				if ((m_CullMode == CullMode::Back && !isFrontFaceHit) ||
					(m_CullMode == CullMode::Front && !isBackFaceHit) ||
					(m_CullMode == CullMode::None && !isBackFaceHit && !isFrontFaceHit)) continue;

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
				if (renderInfo.pDepthBuffer[pixelIdx] < interpolatedZDepth)
					continue;

				// Save the new depth
				if (!m_IsTransparent) renderInfo.pDepthBuffer[pixelIdx] = interpolatedZDepth;

				// The pixel info
				Vertex_Out pixelInfo{};

				// Switch between all the render states
				if (renderInfo.isShowingDepthBuffer)
				{
					if (m_IsTransparent) return;

					// Remap the Z depth
					const float depthColor{ Remap(interpolatedZDepth, 0.997f, 1.0f) };

					// Set the color of the current pixel to showcase the depth
					pixelInfo.color = { depthColor, depthColor, depthColor };
				}
				else
				{
					const Vertex_Out& v0Out{ verticesOut[vertexIdx0] };
					const Vertex_Out& v1Out{ verticesOut[vertexIdx1] };
					const Vertex_Out& v2Out{ verticesOut[vertexIdx2] };

					// Calculate the W depth at this pixel
					const float interpolatedWDepth
					{
						1.0f /
							(weightV0 / v0Out.position.w +
							weightV1 / v1Out.position.w +
							weightV2 / v2Out.position.w)
					};

					// Calculate the UV coordinate at this pixel
					pixelInfo.uv =
					{
						(weightV0 * v0Out.uv / v0Out.position.w +
						weightV1 * v1Out.uv / v1Out.position.w +
						weightV2 * v2Out.uv / v2Out.position.w)
							* interpolatedWDepth
					};

					// Calculate the normal at this pixel
					pixelInfo.normal =
						Vector3{
							(weightV0 * v0Out.normal / v0Out.position.w +
							weightV1 * v1Out.normal / v1Out.position.w +
							weightV2 * v2Out.normal / v2Out.position.w)
								* interpolatedWDepth
					}.Normalized();

					// Calculate the tangent at this pixel
					pixelInfo.tangent =
						Vector3{
							(weightV0 * v0Out.tangent / v0Out.position.w +
							weightV1 * v1Out.tangent / v1Out.position.w +
							weightV2 * v2Out.tangent / v2Out.position.w)
								* interpolatedWDepth
					}.Normalized();

					// Calculate the view direction at this pixel
					pixelInfo.viewDirection =
						Vector3{
							(weightV0 * v0Out.viewDirection / v0Out.position.w +
							weightV1 * v1Out.viewDirection / v1Out.position.w +
							weightV2 * v2Out.viewDirection / v2Out.position.w)
								* interpolatedWDepth
					}.Normalized();
				}

				// Calculate the shading at this pixel and display it on screen
				PixelShading(px + (py * renderInfo.width), pixelInfo, renderInfo);
			}
		}
	}

	void Mesh::PixelShading(int pixelIdx, const Vertex_Out& pixelInfo, const SoftwareRenderInfo& renderInfo) const
	{
		// The final color that will be rendered
		ColorRGB finalColor{};

		// Depending on the rendering state, do other things
		if (renderInfo.isShowingDepthBuffer)
		{
			// Only render the depth which is saved in the color attribute of the pixel info
			finalColor += pixelInfo.color;
		}
		else if (m_IsTransparent)
		{
			// Get the color of the texture
			const ColorRGB diffuseColor{ m_pDiffuseMap->SampleRGB(pixelInfo.uv) };

			// If the alpha is 0, continue to the next pixel 
			if (diffuseColor.a < FLT_EPSILON) return;
			
			// Get the background color
			Uint8 r{}, g{}, b{};
			SDL_GetRGB(renderInfo.pBackBufferPixels[pixelIdx], renderInfo.pBackBuffer->format, &r, &g, &b);
			constexpr float maxColorValue{ 255.0f };
			const ColorRGB prevColor{ r / maxColorValue, g / maxColorValue, b / maxColorValue };

			// Blend the background color with the new color
			finalColor += prevColor * (1.0f - diffuseColor.a) + diffuseColor * diffuseColor.a;
		}
		else
		{
			// Create the ambient color	and add it to the final color
			const Vector3 lightDirection{ 0.577f, -0.577f, 0.577f };
			constexpr float lightIntensity{ 7.0f };
			constexpr float specularShininess{ 25.0f };

			// The normal that should be used in calculations
			const Vector3 useNormal{ renderInfo.isNormalMapActive ? CalculateNormalFromMap(pixelInfo).Normalized() : pixelInfo.normal };

			// Calculate the observed area in this pixel
			const float observedArea{ Vector3::DotClamped(useNormal, -lightDirection) };

			// Depending on the lighting mode, different shading should be applied
			switch (renderInfo.lightingMode)
			{
			case LightingMode::Combined:
			{
				// The ambient color
				constexpr ColorRGB ambientColor{ 0.025f, 0.025f, 0.025f };
				// Calculate the lambert shader
				const ColorRGB lambert{ LightingUtils::Lambert(m_pDiffuseMap->SampleRGB(pixelInfo.uv)) };
				// Calculate the phong exponent
				const float specularExp{ specularShininess * m_pGlossinessMap->SampleRGB(pixelInfo.uv).r };
				// Calculate the phong shader
				const ColorRGB specular{ m_pSpecularMap->SampleRGB(pixelInfo.uv) * LightingUtils::Phong(specularExp, -lightDirection, pixelInfo.viewDirection, useNormal) };

				// Lambert + Phong + ObservedArea
				finalColor += (lightIntensity * lambert) * observedArea + specular + ambientColor;
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
				finalColor += lightIntensity * LightingUtils::Lambert(m_pDiffuseMap->SampleRGB(pixelInfo.uv)) * observedArea;
				break;
			}
			case LightingMode::Specular:
			{
				// Calculate the phong exponent
				const float specularExp{ specularShininess * m_pGlossinessMap->SampleRGB(pixelInfo.uv).r };

				// Calculate the phong shader
				const ColorRGB specular{ m_pSpecularMap->SampleRGB(pixelInfo.uv) * LightingUtils::Phong(specularExp, -lightDirection, pixelInfo.viewDirection, useNormal) };
				// Phong
				finalColor += specular;
				break;
			}
			}
		}

		//Update Color in Buffer
		finalColor.MaxToOne();

		renderInfo.pBackBufferPixels[pixelIdx] = SDL_MapRGB(renderInfo.pBackBuffer->format,
			static_cast<uint8_t>(finalColor.r * 255),
			static_cast<uint8_t>(finalColor.g * 255),
			static_cast<uint8_t>(finalColor.b * 255));
	}

	Vector3 Mesh::CalculateNormalFromMap(const Vertex_Out& pixelInfo) const
	{
		// Calculate the binormal in this pixel
		const Vector3 binormal{ Vector3::Cross(pixelInfo.normal, pixelInfo.tangent) };

		// Create a matrix using the tangent, normal and binormal
		const Matrix tangentSpaceAxis{ pixelInfo.tangent, binormal, pixelInfo.normal, Vector3::Zero };

		// Sample a color from the normal map and clamp it between -1 and 1
		const ColorRGB currentNormalMap{ 2.0f * m_pNormalMap->SampleRGB(pixelInfo.uv) - ColorRGB{ 1.0f, 1.0f, 1.0f } };

		// Make a vector3 of the colorRGB object
		const Vector3 normalMapSample{ currentNormalMap.r, currentNormalMap.g, currentNormalMap.b };

		// Transform the normal map value using the calculated matrix of this pixel
		return tangentSpaceAxis.TransformVector(normalMapSample);
	}

	void Mesh::SetCullMode(CullMode cullMode)
	{
		m_CullMode = cullMode;
	}

	void Mesh::SetTexture(Texture* pTexture)
	{
		// Get the right texture variable depending on the texture type
		switch (pTexture->GetType())
		{
		case dae::Texture::TextureType::Diffuse:
			m_pDiffuseMap = pTexture;
			break;
		case dae::Texture::TextureType::Normal:
			m_pNormalMap = pTexture;
			break;
		case dae::Texture::TextureType::Specular:
			m_pSpecularMap = pTexture;
			break;
		case dae::Texture::TextureType::Glossiness:
			m_pGlossinessMap = pTexture;
			break;
		}

		m_pMaterial->SetTexture(pTexture);
	}

	bool Mesh::IsVisible() const
	{
		return m_IsVisible;
	}

	void Mesh::UpdateMatrices(const Matrix& viewProjectionMatrix, const Matrix& inverseViewMatrix) const
	{
		m_pMaterial->SetMatrix(MatrixType::WorldViewProjection, m_WorldMatrix * viewProjectionMatrix);
		m_pMaterial->SetMatrix(MatrixType::InverseView, inverseViewMatrix);
		m_pMaterial->SetMatrix(MatrixType::World, m_WorldMatrix);
	}

	void Mesh::SetSamplerState(ID3D11SamplerState* pSampleState) const 
	{
		m_pMaterial->SetSampleState(pSampleState);
	}

	void Mesh::SetRasterizerState(ID3D11RasterizerState* pRasterizerState) const
	{
		m_pMaterial->SetRasterizerState(pRasterizerState);
	}

	void Mesh::SetVisibility(bool isVisible)
	{
		m_IsVisible = isVisible;
	}
}