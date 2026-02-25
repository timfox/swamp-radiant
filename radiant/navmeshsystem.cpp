#include "navmeshsystem.h"

#include "brush.h"
#include "scenelib.h"

NavMeshSystem::NavMeshSystem()
	: m_settings()
{
}

bool NavMeshSystem::rebuild(){
	collectGeometry();

	if ( m_vertices.empty() || m_triangles.empty() ) {
		m_builder.reset();
		return false;
	}

	return m_builder.build( m_vertices, m_triangles, m_settings );
}

void NavMeshSystem::reset(){
	m_builder.reset();
	m_vertices.clear();
	m_triangles.clear();
}

const NavMeshBuilder::Settings& NavMeshSystem::settings() const {
	return m_settings;
}

NavMeshBuilder::Settings& NavMeshSystem::settings(){
	return m_settings;
}

const dtNavMesh* NavMeshSystem::navMesh() const {
	return m_builder.navMesh();
}

const dtNavMeshQuery* NavMeshSystem::navQuery() const {
	return m_builder.navQuery();
}

float NavMeshSystem::buildTimeMs() const {
	return m_builder.buildTimeMs();
}

void NavMeshSystem::collectGeometry(){
	m_vertices.clear();
	m_triangles.clear();

	Scene_forEachVisibleBrush( GlobalSceneGraph(), [this]( BrushInstance& instance ){
		Brush* brush = Instance_getBrush( instance );
		if ( brush == 0 ) {
			return;
		}

		Brush_forEachFace( *brush, [this]( Face& face ){
			if ( face.isDetail() || !face.contributes() ) {
				return;
			}

			const Winding& winding = face.getWinding();
			const std::size_t numPoints = winding.numpoints;
			if ( numPoints < 3 ) {
				return;
			}

			auto addVertex = [this]( const DoubleVector3& point ){
				m_vertices.push_back( float( point[0] ) );
				m_vertices.push_back( float( point[1] ) );
				m_vertices.push_back( float( point[2] ) );
			};

			for ( std::size_t i = 1; i + 1 < numPoints; ++i )
			{
				const std::size_t baseIndex = m_vertices.size() / 3;
				addVertex( winding[0].vertex );
				addVertex( winding[i].vertex );
				addVertex( winding[i + 1].vertex );

				m_triangles.push_back( int( baseIndex ) );
				m_triangles.push_back( int( baseIndex + 1 ) );
				m_triangles.push_back( int( baseIndex + 2 ) );
			}
		} );
	} );
}

NavMeshSystem& GlobalNavMeshSystem(){
	static NavMeshSystem instance;
	return instance;
}
