#pragma once
#include <fstream>
#include "Math.h"
#include <vector>
#include "DataTypes.h"
#include "Camera.h"

namespace dae
{
	namespace Utils
	{
		//Just parses vertices and indices
#pragma warning(push)
#pragma warning(disable : 4505) //Warning unreferenced local function
		static bool ParseOBJ(const std::string& filename, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, bool flipAxisAndWinding = true)
		{
			std::ifstream file(filename);
			if (!file)
				return false;

			std::vector<Vector3> positions{};
			std::vector<Vector3> normals{};
			std::vector<Vector2> UVs{};

			vertices.clear();
			indices.clear();

			std::string sCommand;
			// start a while iteration ending when the end of file is reached (ios::eof)
			while (!file.eof())
			{
				//read the first word of the string, use the >> operator (istream::operator>>) 
				file >> sCommand;
				//use conditional statements to process the different commands	
				if (sCommand == "#")
				{
					// Ignore Comment
				}
				else if (sCommand == "v")
				{
					//Vertex
					float x, y, z;
					file >> x >> y >> z;

					positions.emplace_back(x, y, z);
				}
				else if (sCommand == "vt")
				{
					// Vertex TexCoord
					float u, v;
					file >> u >> v;
					UVs.emplace_back(u, 1 - v);
				}
				else if (sCommand == "vn")
				{
					// Vertex Normal
					float x, y, z;
					file >> x >> y >> z;

					normals.emplace_back(x, y, z);
				}
				else if (sCommand == "f")
				{
					//if a face is read:
					//construct the 3 vertices, add them to the vertex array
					//add three indices to the index array
					//add the material index as attibute to the attribute array
					//
					// Faces or triangles
					Vertex vertex{};
					size_t iPosition, iTexCoord, iNormal;

					uint32_t tempIndices[3];
					for (size_t iFace = 0; iFace < 3; iFace++)
					{
						// OBJ format uses 1-based arrays
						file >> iPosition;
						vertex.position = positions[iPosition - 1];

						if ('/' == file.peek())//is next in buffer ==  '/' ?
						{
							file.ignore();//read and ignore one element ('/')

							if ('/' != file.peek())
							{
								// Optional texture coordinate
								file >> iTexCoord;
								vertex.uv = UVs[iTexCoord - 1];
							}

							if ('/' == file.peek())
							{
								file.ignore();

								// Optional vertex normal
								file >> iNormal;
								vertex.normal = normals[iNormal - 1];
							}
						}

						vertices.push_back(vertex);
						tempIndices[iFace] = uint32_t(vertices.size()) - 1;
						//indices.push_back(uint32_t(vertices.size()) - 1);
					}

					indices.push_back(tempIndices[0]);
					if (flipAxisAndWinding) 
					{
						indices.push_back(tempIndices[2]);
						indices.push_back(tempIndices[1]);
					}
					else
					{
						indices.push_back(tempIndices[1]);
						indices.push_back(tempIndices[2]);
					}
				}
				//read till end of line and ignore all remaining chars
				file.ignore(1000, '\n');
			}

			//Cheap Tangent Calculations
			for (uint32_t i = 0; i < indices.size(); i += 3)
			{
				uint32_t index0 = indices[i];
				uint32_t index1 = indices[size_t(i) + 1];
				uint32_t index2 = indices[size_t(i) + 2];

				const Vector3& p0 = vertices[index0].position;
				const Vector3& p1 = vertices[index1].position;
				const Vector3& p2 = vertices[index2].position;
				const Vector2& uv0 = vertices[index0].uv;
				const Vector2& uv1 = vertices[index1].uv;
				const Vector2& uv2 = vertices[index2].uv;

				const Vector3 edge0 = p1 - p0;
				const Vector3 edge1 = p2 - p0;
				const Vector2 diffX = Vector2(uv1.x - uv0.x, uv2.x - uv0.x);
				const Vector2 diffY = Vector2(uv1.y - uv0.y, uv2.y - uv0.y);
				float r = 1.f / Vector2::Cross(diffX, diffY);

				Vector3 tangent = (edge0 * diffY.y - edge1 * diffY.x) * r;
				vertices[index0].tangent += tangent;
				vertices[index1].tangent += tangent;
				vertices[index2].tangent += tangent;
			}

			//Create the Tangents (reject)
			for (auto& v : vertices)
			{
				v.tangent = Vector3::Reject(v.tangent, v.normal).Normalized();

				if(flipAxisAndWinding)
				{
					v.position.z *= -1.f;
					v.normal.z *= -1.f;
					v.tangent.z *= -1.f;
				}

			}

			return true;
		}
#pragma warning(pop)
	}

	namespace LightingUtils
	{
		inline ColorRGB Lambert(const ColorRGB& cd)
		{
			return cd / PI;
		}

		inline ColorRGB Phong(float exp, const Vector3& l, const Vector3& v, const Vector3& n)
		{
			const Vector3 reflectedLightVector{ Vector3::Reflect(l,n) };
			const float reflectedViewDot{ Vector3::DotClamped(reflectedLightVector, v) };
			const float phong{ powf(reflectedViewDot, exp) };

			return ColorRGB{ phong, phong, phong };
		}
	}

	namespace GeometryUtils
	{
		inline void VertexTransformationFunction(const Matrix& worldMatrix, const std::vector<Vertex>& vertices, std::vector<Vertex_Out>& verticesOut, Camera* pCamera)
		{
			// Calculate the transformation matrix for this mesh
			const Matrix worldViewProjectionMatrix{ worldMatrix * pCamera->GetViewMatrix() * pCamera->GetProjectionMatrix() };

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
				vOut.viewDirection = worldMatrix.TransformPoint(v.position) - pCamera->GetPosition();
				vOut.viewDirection.Normalize();

				// Divide all properties of the position by the original z (stored in position.w)
				vOut.position.x /= vOut.position.w;
				vOut.position.y /= vOut.position.w;
				vOut.position.z /= vOut.position.w;

				// Transform the normal and the tangent of the vertex
				vOut.normal = worldMatrix.TransformVector(v.normal).Normalized();
				vOut.tangent = worldMatrix.TransformVector(v.tangent).Normalized();

				// Add the new vertex to the list of NDC vertices
				verticesOut.emplace_back(vOut);
			}
		}

		inline bool IsOutsideFrustum(const Vector4& v)
		{
			return v.x < -1.0f || v.x > 1.0f || v.y < -1.0f || v.y > 1.0f || v.z < 0.0f || v.z > 1.0f;
		}

		inline void OrderTriangleIndices(std::vector<uint32_t>& useIndices, const std::vector<Vector2>& rasterVertices, int i0, int i1, int i2)
		{
			// Create of all the indices so we can swap them
			std::vector<unsigned int> indices
			{
				useIndices[i0],
				useIndices[i1],
				useIndices[i2]
			};

			// The index of the highest vertex
			size_t highestOutputIdx{};

			// The lowest X value and highest Y value found
			float lowestX{ FLT_EPSILON };
			float highestY{ 0 };

			// Find for all verices the most top left vertex
			for (size_t vertexIdx{}; vertexIdx < indices.size(); ++vertexIdx)
			{
				// If the current vertex is higher or equally high then the previously found Y value
				if (rasterVertices[indices[vertexIdx]].y >= highestY)
				{
					// If the current vertex is equally high then the previously found Y value
					if (abs(rasterVertices[indices[vertexIdx]].y - highestY) < FLT_EPSILON)
					{
						// If the current vertex is more to the left then the previously found X value
						if (rasterVertices[indices[vertexIdx]].x < lowestX)
						{
							// Save the current vertex index and its x and y value
							highestOutputIdx = vertexIdx;
							lowestX = rasterVertices[indices[vertexIdx]].x;
							highestY = rasterVertices[indices[vertexIdx]].y;
						}
					}
					else
					{
						// If the current vertex higher then the previously found Y value
						// Save the current vertex index and its x and y value
						highestOutputIdx = vertexIdx;
						lowestX = rasterVertices[indices[vertexIdx]].x;
						highestY = rasterVertices[indices[vertexIdx]].y;
					}
				}
			}
			// Swap the indice at index 0 with the indice that has the most top left vertex
			std::swap(indices[highestOutputIdx], indices[0]);

			// Create 2 edges of the triangle
			const Vector2 edge01{ rasterVertices[indices[1]] - rasterVertices[indices[0]] };
			const Vector2 edge12{ rasterVertices[indices[2]] - rasterVertices[indices[1]] };

			// If the area of this triangle is negative, swap the two other indices
			if (Vector2::Cross(edge01, edge12) < 0)
			{
				std::swap(indices[1], indices[2]);
			}

			// Replace the current indices with the swapped indices
			useIndices[i0] = indices[0];
			useIndices[i1] = indices[1];
			useIndices[i2] = indices[2];
		}

		// Source: https://www.geeksforgeeks.org/program-for-point-of-intersection-of-two-lines/
		inline Vector2 GetIntersectPoint(const Vector2& edge0v0, const Vector2& edge0v1, const Vector2& edge1v0, const Vector2& edge1v1)
		{
			const float a0{ edge0v1.y - edge0v0.y };
			const float b0{ edge0v0.x - edge0v1.x };
			const float c0{ a0 * edge0v0.x + b0 * edge0v0.y };

			const float a1{ edge1v1.y - edge1v0.y };
			const float b1{ edge1v0.x - edge1v1.x };
			const float c1{ a1 * edge1v0.x + b1 * edge1v0.y };

			const float determinant{ a0 * b1 - a1 * b0 };

			const float x{ (b1 * c0 - b0 * c1) / determinant };
			const float y{ (a0 * c1 - a1 * c0) / determinant };

			return { x, y };
		}

		inline Vector3 CalculateRasterToNDC(const Vector2& rasterVertex, float interpolatedZ, const SoftwareRenderInfo& renderInfo) 
		{
			return Vector3
			{
				(rasterVertex.x / renderInfo.width * 2.0f) - 1.0f,
				-((rasterVertex.y / renderInfo.height * 2.0f) - 1.0f),
				interpolatedZ
			};
		}
	}
}