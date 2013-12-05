//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#include "pch.h"
#include "SumoMesh.h"
#include "../Utilities/DirectXSample.h"
#include "../Rendering/ConstantBuffers.h"

using namespace Microsoft::WRL;
using namespace DirectX;

SumoMesh::SumoMesh(_In_ ID3D11Device *device)
{
	D3D11_BUFFER_DESC bd = { 0 };
	D3D11_SUBRESOURCE_DATA initData = { 0 };

	PNTVertex sumoVertices[] =
	{
		{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.63f, 0.005f) },
		{ XMFLOAT3(-0.5f, -0.5f, 0.5f), XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.99f, 0.005f) },
		{ XMFLOAT3(-0.5f, 0.5f, -0.5f), XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(0.6345f, 0.01f) },
		{ XMFLOAT3(-0.5f, 0.5f, 0.5f), XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.99f, 0.01f) },
		{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(0.6f, 0.36f) },
		{ XMFLOAT3(0.5f, -0.5f, 0.5f), XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(0.99f, 0.36f) },
		{ XMFLOAT3(0.5f, 0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.6345f, 0.3655f) },
		{ XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.3655f) }
	};
	WORD sumoIndices[] =
	{
		0, 1, 2, // -x
		1, 3, 2,

		4, 6, 5, // +x
		5, 6, 7,

		0, 5, 1, // -y
		0, 4, 5,

		2, 7, 6, // +y
		2, 3, 7,

		0, 6, 4, // -z
		0, 2, 6,

		1, 7, 3, // +z
		1, 5, 7
	};

	m_vertexCount = 8;
	m_indexCount = ARRAYSIZE(sumoIndices);

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(PNTVertex)* m_vertexCount;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	initData.pSysMem = sumoVertices;
	DX::ThrowIfFailed(
	device->CreateBuffer(&bd, &initData, &m_vertexBuffer)
	);

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD)* m_indexCount;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	initData.pSysMem = sumoIndices;
	DX::ThrowIfFailed(
	device->CreateBuffer(&bd, &initData, &m_indexBuffer)
	);
}

