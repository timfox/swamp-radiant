#pragma once

#include "navmesh.h"

#include <vector>

class NavMeshSystem
{
public:
	NavMeshSystem();

	bool rebuild();
	void reset();

	const NavMeshBuilder::Settings& settings() const;
	NavMeshBuilder::Settings& settings();

	const dtNavMesh* navMesh() const;
	const dtNavMeshQuery* navQuery() const;
	float buildTimeMs() const;

private:
	void collectGeometry();

	NavMeshBuilder m_builder;
	NavMeshBuilder::Settings m_settings;
	std::vector<float> m_vertices;
	std::vector<int> m_triangles;
};

NavMeshSystem& GlobalNavMeshSystem();
