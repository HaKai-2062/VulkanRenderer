#pragma once

#include "vertex.hpp"

namespace VE
{
	class Model
	{
	public:
		Model() = default;
		~Model();

		void LoadModel();

		const std::vector<Vertex>& GetModelVertices() { return m_Vertices; }
		const std::vector<uint32_t>& GetModelIndices() { return m_Indices; }

	private:
		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
	};
}