#pragma once

#include "../Meshes/MeshObject.h"
#include "../Rendering/Material.h"

ref class GameObject
{
internal:
	GameObject();

	void Render(
		_In_ ID3D11DeviceContext *context,
		_In_ ID3D11Buffer *primitiveConstantBuffer
		);


	void OnGround(bool ground);
	bool OnGround();


	void Mesh(_In_ MeshObject^ mesh);

	void NormalMaterial(_In_ Material^ material);
	Material^ NormalMaterial();


	void Position(DirectX::XMFLOAT3 position);
	void Position(DirectX::XMVECTOR position);
	void Velocity(DirectX::XMFLOAT3 velocity);
	void Velocity(DirectX::XMVECTOR velocity);
	DirectX::XMMATRIX ModelMatrix();
	DirectX::XMFLOAT3 Position();
	DirectX::XMVECTOR VectorPosition();
	DirectX::XMVECTOR VectorVelocity();
	DirectX::XMFLOAT3 Velocity();

protected private:
	virtual void UpdatePosition() {};
	// Object Data
	bool                m_ground;

	DirectX::XMFLOAT3   m_position;
	DirectX::XMFLOAT3   m_velocity;
	DirectX::XMFLOAT4X4 m_modelMatrix;

	Material^           m_normalMaterial;

	DirectX::XMFLOAT3   m_defaultXAxis;
	DirectX::XMFLOAT3   m_defaultYAxis;
	DirectX::XMFLOAT3   m_defaultZAxis;


	MeshObject^         m_mesh;

};



__forceinline void GameObject::OnGround(bool ground)
{
	m_ground = ground;
}

__forceinline bool GameObject::OnGround()
{
	return m_ground;
}


__forceinline void GameObject::Position(DirectX::XMFLOAT3 position)
{
	m_position = position;
	// Update any internal states that are dependent on the position.
	// UpdatePosition is a virtual function that is specific to the derived class.
	UpdatePosition();
}

__forceinline void GameObject::Position(DirectX::XMVECTOR position)
{
	XMStoreFloat3(&m_position, position);
	// Update any internal states that are dependent on the position.
	// UpdatePosition is a virtual function that is specific to the derived class.
	UpdatePosition();
}

__forceinline DirectX::XMFLOAT3 GameObject::Position()
{
	return m_position;
}

__forceinline DirectX::XMVECTOR GameObject::VectorPosition()
{
	return DirectX::XMLoadFloat3(&m_position);
}

__forceinline void GameObject::Velocity(DirectX::XMFLOAT3 velocity)
{
	m_velocity = velocity;
}

__forceinline void GameObject::Velocity(DirectX::XMVECTOR velocity)
{
	XMStoreFloat3(&m_velocity, velocity);
}

__forceinline DirectX::XMFLOAT3 GameObject::Velocity()
{
	return m_velocity;
}

__forceinline DirectX::XMVECTOR GameObject::VectorVelocity()
{
	return DirectX::XMLoadFloat3(&m_velocity);
}


__forceinline void GameObject::NormalMaterial(_In_ Material^ material)
{
	m_normalMaterial = material;
}

__forceinline Material^ GameObject::NormalMaterial()
{
	return m_normalMaterial;
}


__forceinline void GameObject::Mesh(_In_ MeshObject^ mesh)
{
	m_mesh = mesh;
}


__forceinline DirectX::XMMATRIX GameObject::ModelMatrix()
{
	return DirectX::XMLoadFloat4x4(&m_modelMatrix);
}
