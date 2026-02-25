#include "navmesh.h"

#include "DetourNavMeshBuilder.h"
#include "DetourCommon.h"

#include <algorithm>
#include <cmath>
#include <cstring>

NavMeshBuilder::NavMeshBuilder() :
	m_heightfield( 0 ),
	m_compactHeightfield( 0 ),
	m_contourSet( 0 ),
	m_polyMesh( 0 ),
	m_detailMesh( 0 ),
	m_navMesh( 0 ),
	m_navQuery( 0 ),
	m_buildTimeMs( 0.0f )
{
}

NavMeshBuilder::~NavMeshBuilder(){
	reset();
}

bool NavMeshBuilder::build( const std::vector<float>& vertices, const std::vector<int>& triangles, const Settings& settings ){
	reset();

	if ( vertices.empty() || triangles.empty() ) {
		return false;
	}

	const int vertCount = std::max( 0, int( vertices.size() / 3 ) );
	const int triCount = std::max( 0, int( triangles.size() / 3 ) );
	if ( vertCount < 3 || triCount < 1 ) {
		return false;
	}

	float boundsMin[3];
	float boundsMax[3];
	rcCalcBounds( vertices.data(), vertCount, boundsMin, boundsMax );

	rcConfig config;
	std::memset( &config, 0, sizeof( config ) );
	config.cs = settings.cellSize;
	config.ch = settings.cellHeight;
	config.walkableSlopeAngle = settings.agentMaxSlope;
	config.walkableHeight = std::max( 3, int( std::ceil( settings.agentHeight / config.ch ) ) );
	config.walkableClimb = std::max( 0, int( std::floor( settings.agentMaxClimb / config.ch ) ) );
	config.walkableRadius = std::max( 0, int( std::ceil( settings.agentRadius / config.cs ) ) );
	config.maxEdgeLen = std::max( 0, int( settings.edgeMaxLen / config.cs ) );
	config.maxSimplificationError = settings.edgeMaxError;
	config.minRegionArea = std::max( 0, int( rcSqr( settings.regionMinSize ) ) );
	config.mergeRegionArea = std::max( 0, int( rcSqr( settings.regionMergeSize ) ) );
	config.maxVertsPerPoly = 6;
	config.detailSampleDist = settings.detailSampleDist;
	config.detailSampleMaxError = settings.detailSampleMaxError;
	rcVcopy( config.bmin, boundsMin );
	rcVcopy( config.bmax, boundsMax );
	rcCalcGridSize( config.bmin, config.bmax, config.cs, &config.width, &config.height );

	if ( !buildInternal( config, vertices, triangles, settings ) ) {
		reset();
		return false;
	}

	m_buildTimeMs = float( m_ctx.getAccumulatedTime( RC_TIMER_TOTAL ) );
	return true;
}

void NavMeshBuilder::reset(){
	destroy();
	m_ctx.resetTimers();
	m_ctx.resetLog();
	m_buildTimeMs = 0.0f;
}

const dtNavMesh* NavMeshBuilder::navMesh() const {
	return m_navMesh;
}

const dtNavMeshQuery* NavMeshBuilder::navQuery() const {
	return m_navQuery;
}

const rcPolyMesh* NavMeshBuilder::polyMesh() const {
	return m_polyMesh;
}

const rcPolyMeshDetail* NavMeshBuilder::polyMeshDetail() const {
	return m_detailMesh;
}

float NavMeshBuilder::buildTimeMs() const {
	return m_buildTimeMs;
}

bool NavMeshBuilder::buildInternal( const rcConfig& config, const std::vector<float>& vertices, const std::vector<int>& triangles, const Settings& settings ){
	const int vertCount = int( vertices.size() / 3 );
	const int triCount = int( triangles.size() / 3 );

	rcScopedTimer timer( &m_ctx, RC_TIMER_TOTAL );

	m_heightfield = rcAllocHeightfield();
	if ( m_heightfield == 0 ) {
		return false;
	}
	if ( !rcCreateHeightfield( &m_ctx, *m_heightfield, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch ) ) {
		return false;
	}

	std::vector<unsigned char> triAreas( triCount, RC_WALKABLE_AREA );
	if ( !rcMarkWalkableTriangles( &m_ctx, config.walkableSlopeAngle, vertices.data(), vertCount, triangles.data(), triCount, triAreas.data() ) ) {
		return false;
	}
	if ( !rcRasterizeTriangles( &m_ctx, vertices.data(), vertCount, triangles.data(), triAreas.data(), triCount, *m_heightfield, config.walkableClimb ) ) {
		return false;
	}

	rcFilterLowHangingWalkableObstacles( &m_ctx, config.walkableClimb, *m_heightfield );
	rcFilterLedgeSpans( &m_ctx, config.walkableHeight, config.walkableClimb, *m_heightfield );
	rcFilterWalkableLowHeightSpans( &m_ctx, config.walkableHeight, *m_heightfield );

	m_compactHeightfield = rcAllocCompactHeightfield();
	if ( m_compactHeightfield == 0 ) {
		return false;
	}
	if ( !rcBuildCompactHeightfield( &m_ctx, config.walkableHeight, config.walkableClimb, *m_heightfield, *m_compactHeightfield ) ) {
		return false;
	}

	if ( !rcErodeWalkableArea( &m_ctx, config.walkableRadius, *m_compactHeightfield ) ) {
		return false;
	}

	bool regionsBuilt = false;
	switch ( settings.partitionType )
	{
	case PartitionType::Watershed:
		regionsBuilt = rcBuildRegions( &m_ctx, *m_compactHeightfield, 0, config.minRegionArea, config.mergeRegionArea );
		break;
	case PartitionType::Monotone:
		regionsBuilt = rcBuildRegionsMonotone( &m_ctx, *m_compactHeightfield, 0, config.minRegionArea, config.mergeRegionArea );
		break;
	case PartitionType::Layers:
		regionsBuilt = rcBuildLayerRegions( &m_ctx, *m_compactHeightfield, 0, config.minRegionArea );
		break;
	}
	if ( !regionsBuilt ) {
		return false;
	}

	m_contourSet = rcAllocContourSet();
	if ( m_contourSet == 0 ) {
		return false;
	}
	if ( !rcBuildContours( &m_ctx, *m_compactHeightfield, config.maxSimplificationError, config.maxEdgeLen, *m_contourSet ) ) {
		return false;
	}

	m_polyMesh = rcAllocPolyMesh();
	if ( m_polyMesh == 0 ) {
		return false;
	}
	if ( !rcBuildPolyMesh( &m_ctx, *m_contourSet, config.maxVertsPerPoly, *m_polyMesh ) ) {
		return false;
	}

	m_detailMesh = rcAllocPolyMeshDetail();
	if ( m_detailMesh == 0 ) {
		return false;
	}
	if ( !rcBuildPolyMeshDetail( &m_ctx, *m_polyMesh, *m_compactHeightfield, config.detailSampleDist, config.detailSampleMaxError, *m_detailMesh ) ) {
		return false;
	}

	for ( int i = 0; i < m_polyMesh->npolys; ++i )
	{
		m_polyMesh->flags[i] = ( m_polyMesh->areas[i] == RC_WALKABLE_AREA ) ? 0x01 : 0;
	}

	dtNavMeshCreateParams params;
	std::memset( &params, 0, sizeof( params ) );
	params.verts = m_polyMesh->verts;
	params.vertCount = m_polyMesh->nverts;
	params.polys = m_polyMesh->polys;
	params.polyAreas = m_polyMesh->areas;
	params.polyFlags = m_polyMesh->flags;
	params.polyCount = m_polyMesh->npolys;
	params.nvp = m_polyMesh->nvp;
	params.detailMeshes = m_detailMesh->meshes;
	params.detailVerts = m_detailMesh->verts;
	params.detailVertsCount = m_detailMesh->nverts;
	params.detailTris = m_detailMesh->tris;
	params.detailTriCount = m_detailMesh->ntris;
	params.walkableHeight = settings.agentHeight;
	params.walkableRadius = settings.agentRadius;
	params.walkableClimb = settings.agentMaxClimb;
	rcVcopy( params.bmin, m_polyMesh->bmin );
	rcVcopy( params.bmax, m_polyMesh->bmax );
	params.cs = config.cs;
	params.ch = config.ch;
	params.buildBvTree = true;

	unsigned char* navData = 0;
	int navDataSize = 0;
	if ( !dtCreateNavMeshData( &params, &navData, &navDataSize ) ) {
		return false;
	}

	m_navMesh = dtAllocNavMesh();
	if ( m_navMesh == 0 ) {
		dtFree( navData );
		return false;
	}

	dtStatus status = m_navMesh->init( navData, navDataSize, DT_TILE_FREE_DATA );
	if ( dtStatusFailed( status ) ) {
		dtFree( navData );
		m_navMesh = 0;
		return false;
	}

	m_navQuery = dtAllocNavMeshQuery();
	if ( m_navQuery == 0 ) {
		return false;
	}
	status = m_navQuery->init( m_navMesh, 2048 );
	if ( dtStatusFailed( status ) ) {
		return false;
	}

	return true;
}

void NavMeshBuilder::destroy(){
	if ( m_navQuery != 0 ) {
		dtFreeNavMeshQuery( m_navQuery );
		m_navQuery = 0;
	}
	if ( m_navMesh != 0 ) {
		dtFreeNavMesh( m_navMesh );
		m_navMesh = 0;
	}
	if ( m_detailMesh != 0 ) {
		rcFreePolyMeshDetail( m_detailMesh );
		m_detailMesh = 0;
	}
	if ( m_polyMesh != 0 ) {
		rcFreePolyMesh( m_polyMesh );
		m_polyMesh = 0;
	}
	if ( m_contourSet != 0 ) {
		rcFreeContourSet( m_contourSet );
		m_contourSet = 0;
	}
	if ( m_compactHeightfield != 0 ) {
		rcFreeCompactHeightfield( m_compactHeightfield );
		m_compactHeightfield = 0;
	}
	if ( m_heightfield != 0 ) {
		rcFreeHeightField( m_heightfield );
		m_heightfield = 0;
	}
}
