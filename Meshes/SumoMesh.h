#pragma once
// SphereMesh:
// This class derives from MeshObject and creates a ID3D11Buffer of
// vertices and indices to represent a canonical sphere that is
// positioned at the origin with a radius of 1.0.

#include "MeshObject.h"

ref class SumoMesh : public MeshObject
{
internal:
	SumoMesh(_In_ ID3D11Device *device);
};




