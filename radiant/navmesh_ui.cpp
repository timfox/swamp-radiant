#include "navmesh_ui.h"

#include "navmeshsystem.h"
#include "camwindow.h"
#include "renderer.h"
#include "igl.h"
#include "math/matrix.h"
#include "debugging/debugging.h"
#include "DetourNavMesh.h"

#include <array>
#include <vector>

namespace
{
CamWnd* getCamWnd(){
	return GlobalCamera_getCamWnd();
}

void refreshCamera(){
	if ( CamWnd* camwnd = getCamWnd() ) {
		CamWnd_Update( *camwnd );
	}
}

	class NavMeshRenderable final : public OpenGLRenderable
	{
	public:
		using Segment = std::array<float, 3>;

		NavMeshRenderable( const dtNavMesh* mesh, const std::array<float, 4>& colour, float lineWidth ) :
			m_colour( colour ),
			m_lineWidth( lineWidth )
		{
			if ( mesh == 0 ) {
				return;
			}
			for ( int tileIndex = 0; tileIndex < mesh->getMaxTiles(); ++tileIndex )
			{
				const dtMeshTile* tile = mesh->getTile( tileIndex );
				if ( tile == 0 || tile->header == 0 ) {
					continue;
				}
				const float* verts = tile->verts;
				const dtPoly* polys = tile->polys;
				for ( int polyIndex = 0; polyIndex < tile->header->polyCount; ++polyIndex )
				{
					const dtPoly* poly = &polys[polyIndex];
					if ( poly->vertCount < 3 ) {
						continue;
					}
					for ( int vert = 0; vert < poly->vertCount; ++vert )
					{
						const int idx0 = poly->verts[vert];
						const int idx1 = poly->verts[( vert + 1 ) % poly->vertCount];
						const float* v0 = &verts[idx0 * 3];
						const float* v1 = &verts[idx1 * 3];
						m_segments.push_back( Segment{ { v0[0], v0[1], v0[2] } } );
						m_segments.push_back( Segment{ { v1[0], v1[1], v1[2] } } );
					}
				}
			}
		}

		bool empty() const {
			return m_segments.empty();
		}

		void render( RenderStateFlags ) const override {
			if ( m_segments.empty() ) {
				return;
			}
			gl().glEnable( GL_BLEND );
			gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			gl().glLineWidth( m_lineWidth );
			gl().glColor4fv( m_colour.data() );
			gl().glBegin( GL_LINES );
			for ( const Segment& segment : m_segments )
			{
				gl().glVertex3fv( segment.data() );
			}
			gl().glEnd();
			gl().glLineWidth( 1.0f );
			gl().glColor4f( 1, 1, 1, 1 );
		}

	private:
		std::vector<Segment> m_segments;
		std::array<float, 4> m_colour;
		float m_lineWidth;
	};
}

bool g_navmeshOverlayEnabled = false;

bool NavMeshOverlay_isEnabled(){
	return g_navmeshOverlayEnabled;
}

void NavMeshOverlay_setEnabled( bool enabled ){
	if ( g_navmeshOverlayEnabled == enabled ) {
		return;
	}
	g_navmeshOverlayEnabled = enabled;
	if ( enabled && GlobalNavMeshSystem().navMesh() == 0 ) {
		GlobalNavMeshSystem().rebuild();
	}
	refreshCamera();
}

void NavMeshOverlay_toggle(){
	NavMeshOverlay_setEnabled( !g_navmeshOverlayEnabled );
}

void NavMeshOverlay_render( Renderer& renderer ){
	if ( !g_navmeshOverlayEnabled ) {
		return;
	}
	const dtNavMesh* mesh = GlobalNavMeshSystem().navMesh();
	if ( mesh == 0 ) {
		return;
	}
	const std::array<float, 4> colour{ 0.0f, 0.78f, 0.92f, 0.55f };
	NavMeshRenderable renderable( mesh, colour, 1.4f );
	if ( renderable.empty() ) {
		return;
	}
	renderer.PushState();
	renderer.addRenderable( renderable, g_matrix4_identity );
	renderer.PopState();
}

void NavMesh_rebuild(){
	if ( GlobalNavMeshSystem().rebuild() ) {
		globalOutputStream() << "Navmesh rebuilt in " << GlobalNavMeshSystem().buildTimeMs() << " ms\n";
	}
	else
	{
		globalWarningStream() << "Navmesh rebuild failed or produced no valid surface.\n";
	}
	refreshCamera();
}
