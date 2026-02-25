#pragma once

#include "renderable.h"

class Renderer;

extern bool g_navmeshOverlayEnabled;

bool NavMeshOverlay_isEnabled();
void NavMeshOverlay_toggle();
void NavMeshOverlay_setEnabled( bool enabled );
void NavMeshOverlay_render( Renderer& renderer );

void NavMesh_rebuild();
