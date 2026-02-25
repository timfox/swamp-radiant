#pragma once

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "Recast.h"

#include <vector>

class NavMeshBuilder
{
public:
	enum class PartitionType
	{
		Watershed,
		Monotone,
		Layers
	};

	struct Settings
	{
		float cellSize = 0.3f;
		float cellHeight = 0.2f;
		float agentHeight = 2.0f;
		float agentRadius = 0.6f;
		float agentMaxClimb = 0.9f;
		float agentMaxSlope = 45.0f;
		float edgeMaxLen = 12.0f;
		float edgeMaxError = 1.3f;
		float regionMinSize = 8.0f;
		float regionMergeSize = 20.0f;
		float detailSampleDist = 6.0f;
		float detailSampleMaxError = 1.0f;
		PartitionType partitionType = PartitionType::Watershed;
	};

	NavMeshBuilder();
	~NavMeshBuilder();

	bool build( const std::vector<float>& vertices, const std::vector<int>& triangles, const Settings& settings );
	void reset();

	const dtNavMesh* navMesh() const;
	const dtNavMeshQuery* navQuery() const;
	const rcPolyMesh* polyMesh() const;
	const rcPolyMeshDetail* polyMeshDetail() const;
	float buildTimeMs() const;

private:
	bool buildInternal( const rcConfig& config, const std::vector<float>& vertices, const std::vector<int>& triangles, const Settings& settings );
	void destroy();

	rcContext m_ctx;
	rcHeightfield* m_heightfield;
	rcCompactHeightfield* m_compactHeightfield;
	rcContourSet* m_contourSet;
	rcPolyMesh* m_polyMesh;
	rcPolyMeshDetail* m_detailMesh;
	dtNavMesh* m_navMesh;
	dtNavMeshQuery* m_navQuery;
	float m_buildTimeMs;
};
