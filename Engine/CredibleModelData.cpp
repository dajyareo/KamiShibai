//////////////////////////////////////////////////////////////////////////
// KsmModel.cpp
// Implementation file for the KamiShibai Model data structure. 
// (c) 2012 Overclocked Games LLC
//////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "AssetManager.h"
#include "ObjectInstance.h"
#include "ModelComponents.h"
#include "KsmMesh.h"
#include "GeometryGenerator.h"
#include "ObjectInstance.h"
#include "../KsmCreatorLib/LightHelper.h"
#include "CredibleModelData.h"
#include "Logger.h"
#include "BillboardAnimation.h"
#include "AnimationEnum.h"
#include "MathHelper.h"
#include "InstancedRenderer.h"
#include <time.h>
#include "DataReader.h"

using namespace Engine;
using namespace std;

CredibleModelData::CredibleModelData(
	_In_ byte* data,
	_In_ ID3D11Device* device, 
	_In_opt_ bool readSubsetAsInstanceType/* = false*/)
	:_device(device), _animationTimer(0)
{
	initializeFromKsm(data, readSubsetAsInstanceType);
}

CredibleModelData::CredibleModelData(	
	_In_ ID3D11Device* device,
	_In_ GeometryGenerator::MeshData& meshData,
	_In_ ID3D11ShaderResourceView* diffuseTexture)
	: _device(device), _animationTimer(0)
{
	initializeFromMeshData(meshData, diffuseTexture, nullptr);
}

// We do not need to clean up the textures here because the Asset Manager will handle
// that for us
CredibleModelData::~CredibleModelData(void)
{}

bool CredibleModelData::IntersectedBy(
	_In_ const XMVECTOR& rayOrigin,
	_In_ const XMVECTOR& rayDirection,
	_In_ const XMFLOAT4X4& worldTransform,
	_In_ const XMMATRIX& inverseView)
{
	for (const auto& node : Nodes)
	{
		for (const auto& mesh : node->Meshes)
		{
			bool result = mesh->IntersectedBy(
				rayOrigin,
				rayDirection,
				worldTransform,
				node->GlobalTransform,
				inverseView);

			if (result)
			{
				return true;
			}
		}
	}

	return false;
}

bool CredibleMesh::IntersectedBy(
	_In_ const XMVECTOR& rayOrigin,
	_In_ const XMVECTOR& rayDirection,
	_In_ const XMFLOAT4X4& worldTransform,
	_In_ XMFLOAT4X4 nodeTransform,
	_In_ const XMMATRIX& inverseView)
{
	MathHelper::Transpose(nodeTransform);
	XMMATRIX nodeTransformMatrix = XMLoadFloat4x4(&nodeTransform);

	// Transform the ray to local space
	XMMATRIX world = XMLoadFloat4x4(&worldTransform);
	XMVECTOR worldDeterminant = XMMatrixDeterminant(world);
	XMMATRIX inverseWorld = XMMatrixInverse(&worldDeterminant, world);

	// View space to the object's local space.
	XMMATRIX toLocal = XMMatrixMultiply(inverseView, inverseWorld);

	XMVECTOR newRayDirection = rayDirection;
	XMVECTOR newRayOrigin = rayOrigin;

	newRayOrigin = XMVector3TransformCoord(newRayDirection, toLocal);
	newRayDirection = XMVector3TransformNormal(newRayDirection, toLocal);

	// Make the ray a unit length for the intersection test
	newRayDirection = XMVector3Normalize(newRayDirection);

	//////////////////////////////////////////////////////////////////////////
	float tmin = 0.0f;

	if (MeshBoundingBox.Intersects(newRayOrigin, newRayDirection, tmin))
	{
		tmin = MathHelper::Infinity;
		for (UINT n = 0; n < Indices.size() / 3; ++n)
		{
			// Indices for this triangle.
			UINT i0 = Indices[n * 3 + 0];
			UINT i1 = Indices[n * 3 + 1];
			UINT i2 = Indices[n * 3 + 2];

			// Vertices for this triangle.D
			XMVECTOR v0 = XMVectorSet(PositionalVertices[i0].x, PositionalVertices[i0].y, PositionalVertices[i0].z, 0.0f);
			XMVECTOR v1 = XMVectorSet(PositionalVertices[i1].x, PositionalVertices[i1].y, PositionalVertices[i1].z, 0.0f);
			XMVECTOR v2 = XMVectorSet(PositionalVertices[i2].x, PositionalVertices[i2].y, PositionalVertices[i2].z, 0.0f);

			v0 = XMVector3TransformCoord(v0, nodeTransformMatrix);
			v1 = XMVector3TransformCoord(v1, nodeTransformMatrix);
			v2 = XMVector3TransformCoord(v2, nodeTransformMatrix);

			// Stop once we've found a triangle that's been hit
			if (TriangleTests::Intersects(newRayOrigin, newRayDirection, v0, v1, v2, tmin))
			{
				return true;
			}
		}
	}

	return false;
}

std::vector<XMFLOAT3> CredibleMesh::GetAllIntersectionsBy(
	_In_ const XMVECTOR& rayOrigin,
	_In_ const XMVECTOR& rayDirection,
	_In_opt_ const XMMATRIX* translation /*= nullptr*/)
{
	float tmin = MathHelper::Infinity;
	std::vector<XMFLOAT3> intersections;

	for (UINT n = 0; n < Indices.size() / 3; ++n)
	{
		// Indices for this triangle.
		UINT i0 = Indices[n * 3 + 0];
		UINT i1 = Indices[n * 3 + 1];
		UINT i2 = Indices[n * 3 + 2];

		// Vertices for this triangle.D
		XMVECTOR v0 = XMVectorSet(PositionalVertices[i0].x, PositionalVertices[i0].y, PositionalVertices[i0].z, 0.0f);
		XMVECTOR v1 = XMVectorSet(PositionalVertices[i1].x, PositionalVertices[i1].y, PositionalVertices[i1].z, 0.0f);
		XMVECTOR v2 = XMVectorSet(PositionalVertices[i2].x, PositionalVertices[i2].y, PositionalVertices[i2].z, 0.0f);

		if (translation)
		{
			v0 = XMVector3TransformCoord(v0, *translation);
			v1 = XMVector3TransformCoord(v1, *translation);
			v2 = XMVector3TransformCoord(v2, *translation);
		}

		// Stop once we've found a triangle that's been hit
		if (TriangleTests::Intersects(rayOrigin, rayDirection, v0, v1, v2, tmin))
		{
			XMFLOAT3 intersection;
			XMStoreFloat3(&intersection, v0);
			intersections.push_back(intersection);
		}
	}

	return intersections;
}

std::vector<XMFLOAT3> CredibleModelData::GetAllIntersectionsBy(
	_In_ const XMVECTOR& rayOrigin,
	_In_ const XMVECTOR& rayDirection,
	_In_opt_ const XMMATRIX* translation /*= nullptr*/)
{
	vector<XMFLOAT3> intersections;

	for (const auto& node : Nodes)
	{
		for (const auto& mesh : node->Meshes)
		{
			vector<XMFLOAT3> i = mesh->GetAllIntersectionsBy(
				rayOrigin,
				rayDirection, 
				translation);

			intersections.insert(
				intersections.end(),
				i.begin(), i.end());
		}
	}

	return intersections;
}

void CredibleModelData::DrawInstancedNode(
	_In_ ID3D11DeviceContext* dc,
	_In_ ID3D11Buffer* instanceDataBuffer,
	_In_ UINT count,
	_In_ ConstantBuffer<ObjectConstBuffer>* cb,
	_In_ CredibleMesh* mesh)
{
	//TODO: look into using materials loaded from models

	//cb->Data.Mat.Ambient = helper->vAmbientColor;
	//cb->Data.Mat.Diffuse = helper->vDiffuseColor;
	//cb->Data.Mat.Reflect = helper->vEmissiveColor;
	//cb->Data.Mat.Specular = helper->vSpecularColor;

	cb->ApplyChanges(dc);

	if (mesh->DiffuseTexture)
	{
		dc->PSSetShaderResources(0, 1, &mesh->DiffuseTexture);		
	}

	UINT stride[2] = { sizeof(VertexHardwareInstanced), sizeof(InstancedRenderer::InstancedData) };
	UINT offset[2] = { 0, 0 };
	ID3D11Buffer* vbs[2] = { mesh->VB.Get(), instanceDataBuffer };

	dc->IASetVertexBuffers(0, 2, vbs, stride, offset);
	dc->IASetIndexBuffer(mesh->IB.Get(), mesh->IndexBufferFormat, 0);

	dc->DrawIndexedInstanced(
		mesh->IndexCount,
		count,
		0,
		0,
		0);
}

void CredibleModelData::initializeFromMeshData(
	_In_ GeometryGenerator::MeshData& meshData,
	_In_opt_ ID3D11ShaderResourceView* diffuseTexture /*= nullptr*/,
	_In_opt_ ID3D11ShaderResourceView* normalTexture /*= nullptr*/)
{
	RootNode = new CredibleNode(L"RootNode");
	CredibleNode* child = new CredibleNode(L"MeshData");

	child->Meshes.push_back(make_unique<CredibleMesh>(meshData, diffuseTexture, normalTexture, _device));
	RootNode->Children.push_back(child);
	child->Parent = RootNode;

	unique_ptr<CredibleNode> rootUp = nullptr;
	unique_ptr<CredibleNode> childUp = nullptr;
	rootUp.reset(RootNode);
	childUp.reset(child);
	Nodes.push_back(move(rootUp));
	Nodes.push_back(move(childUp));
	
	std::vector<DirectX::VertexPositionNormalTexture> staticVertices;
	staticVertices.reserve(meshData.Vertices.size());
}

void CredibleModelData::CalculateGlobalTransform(CredibleNode* node)
{
	// concatenate all parent transforms to get the global transform for this node
	node->GlobalTransform = node->LocalTransform;
	CredibleNode* parentNode = node->Parent;
	while (parentNode)
	{
		XMMATRIX lt = XMLoadFloat4x4(&parentNode->LocalTransform);
		XMMATRIX gt = XMLoadFloat4x4(&node->GlobalTransform);
		XMMATRIX result = lt * gt;
		XMStoreFloat4x4(&node->GlobalTransform, result);

		parentNode = parentNode->Parent;
	}
}

void CredibleModelData::Draw(
	_In_ ID3D11DeviceContext* dc,
	_In_ ConstantBuffer<ObjectConstBuffer>* cb,
	_In_ ConstantBuffer<AnimatedConstBuffer>* aCb,
	_In_ XMFLOAT4X4 world, 
	_In_ const std::vector<XMFLOAT4X4>& localBoneTransforms,
	_In_ const vector<wstring>& disabledNodes,
	_In_ map<ItemSlot, ID3D11ShaderResourceView*>& textureOverrides)
{
	if (_hasAnimations)
	{
		UpdateTransforms(RootNode, localBoneTransforms);
	}

	DrawNode(dc, cb, aCb, RootNode, world, disabledNodes, textureOverrides);
}

void Engine::CredibleModelData::generateBtConvexShapeTraverse(
	_In_ CredibleNode* node,
	_In_ btTriangleIndexVertexArray* shape,
	_In_ XMFLOAT4X4 scale)
{
	XMFLOAT4X4 nodeGlobalTransform = node->GlobalTransform;
	MathHelper::Transpose(nodeGlobalTransform);

	DirectX::XMMATRIX nodeGlobalTransformWorld = XMLoadFloat4x4(&nodeGlobalTransform);
	XMMATRIX scaleTranslation = XMLoadFloat4x4(&scale);
	nodeGlobalTransformWorld = XMMatrixMultiply(nodeGlobalTransformWorld, scaleTranslation);

	for (const auto& mesh : node->Meshes)
	{
		for (const auto& vertex : mesh->PositionalVertices)
		{
			XMVECTOR vertexVector = XMLoadFloat3(&vertex);
			vertexVector = XMVector3TransformCoord(vertexVector, nodeGlobalTransformWorld);

			XMFLOAT3 tmp;
			XMStoreFloat3(&tmp, vertexVector);
			mesh->translatedVertices.push_back(tmp);
		}

		btIndexedMesh im;
		im.m_vertexBase = (unsigned char*)&mesh->translatedVertices[0];
		im.m_vertexStride = sizeof(XMFLOAT3);
		im.m_numVertices = (int)mesh->translatedVertices.size();
		im.m_triangleIndexBase = (const unsigned char*)&mesh->Indices[0];
		im.m_triangleIndexStride = sizeof(UINT) * 3;
		im.m_numTriangles = (int)mesh->Indices.size() / 3;
		im.m_indexType = PHY_INTEGER;
		im.m_vertexType = PHY_FLOAT;

		shape->addIndexedMesh(im, PHY_INTEGER);

		//btConvexTriangleMeshShape* t = new btConvexTriangleMeshShape();
		//btStridingMeshInterface mi;

		//for (const auto& index : mesh->Indices)
		//{
		//	XMFLOAT3 vertexFloat = mesh->PositionalVertices[index];
		//	XMVECTOR vertexVector = XMLoadFloat3(&vertexFloat);
		//	vertexVector = XMVector3TransformCoord(vertexVector, nodeGlobalTransformWorld);

		//	XMFLOAT3 tmp;
		//	XMStoreFloat3(&tmp, vertexVector);
		//	shape->addPoint(btVector3(tmp.x, tmp.y, tmp.z));
		//}
		//for (const auto& vertex : mesh->PositionalVertices)
		//{
		//	XMVECTOR vertexVector = XMLoadFloat3(&vertex);
		//	vertexVector = XMVector3TransformCoord(vertexVector, nodeGlobalTransformWorld);

		//	XMFLOAT3 tmp;
		//	XMStoreFloat3(&tmp, vertexVector);
		//	shape->addPoint(btVector3(tmp.x, tmp.y, tmp.z));
		//}
	}

	for (unsigned int i = 0; i < node->Children.size(); ++i)
	{
		generateBtConvexShapeTraverse(node->Children[i], shape, scale);
	}
}

void CredibleModelData::DrawNode(
	_In_ ID3D11DeviceContext* dc, 
	_In_ ConstantBuffer<ObjectConstBuffer>* cb, 
	_In_ ConstantBuffer<AnimatedConstBuffer>* aCb,
	CredibleNode* node, 
	_In_ XMFLOAT4X4 worldTransform,
	_In_ const vector<wstring>& disabledNodes,
	_In_ map<ItemSlot, ID3D11ShaderResourceView*>& textureOverrides)
{
	XMFLOAT4X4 nodeGlobalTransform = node->GlobalTransform;
	MathHelper::Transpose(nodeGlobalTransform);

	DirectX::XMMATRIX nodeGlobalTransformWorld = XMLoadFloat4x4(&nodeGlobalTransform);

	XMMATRIX w = XMLoadFloat4x4(&worldTransform);
	nodeGlobalTransformWorld *= w;

	XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(nodeGlobalTransformWorld);

	//
	// Fill constant buffer with object's drawing data
	//
	DirectX::XMStoreFloat4x4(&cb->Data.World, DirectX::XMMatrixTranspose(nodeGlobalTransformWorld));
	DirectX::XMStoreFloat4x4(&cb->Data.WorldInvTranspose, DirectX::XMMatrixTranspose(worldInvTranspose));

	if (find(disabledNodes.begin(), disabledNodes.end(), node->Name) == disabledNodes.end())
	{
		for (unsigned int i = 0; i < node->Meshes.size(); ++i)
		{
			const CredibleMesh* mesh = node->Meshes[i].get();

			// Upload bone matrices
			if (mesh->HasBones())
			{
				calculateGlobalBoneMatrices(node, mesh);
				MUKASHIDEBUG_CRITICALERROR_ONFALSE(_globalBoneTransforms.size() == mesh->NumBones);

				for (unsigned int a = 0; a < mesh->NumBones; a++)
				{
					XMFLOAT4X4& mat = _globalBoneTransforms[a];
					MathHelper::Transpose(mat);

					DirectX::XMFLOAT4X4 boneMatrix;
					boneMatrix = mat;

					XMMATRIX boneTransformMatrix = DirectX::XMLoadFloat4x4(&boneMatrix);
					DirectX::XMStoreFloat4x4(
						&aCb->Data.gBoneTransforms[a++],
						DirectX::XMMatrixTranspose(boneTransformMatrix));
				}

				aCb->ApplyChanges(dc);
			}

			//TODO: look into using materials loaded from models
			cb->Data.Mat = mesh->MeshMaterial;

			//cb->Data.Mat.Ambient = helper->vAmbientColor;
			//cb->Data.Mat.Diffuse = helper->vDiffuseColor;
			//cb->Data.Mat.Reflect = helper->vEmissiveColor;
			//cb->Data.Mat.Specular = helper->vSpecularColor;

			cb->ApplyChanges(dc);

			if (mesh->DiffuseTexture)
			{
				if (textureOverrides[_textureMapping[mesh->DiffuseTexture]])
				{
					dc->PSSetShaderResources(0, 1, &textureOverrides[_textureMapping[mesh->DiffuseTexture]]);
				}
				else
				{
					dc->PSSetShaderResources(0, 1, &mesh->DiffuseTexture);
				}
			}

			UINT vOffset = 0;
			dc->IASetVertexBuffers(0, 1, mesh->VB.GetAddressOf(), &_vertexStride, &vOffset);
			dc->IASetIndexBuffer(mesh->IB.Get(), mesh->IndexBufferFormat, 0);

			dc->DrawIndexed(
				mesh->IndexCount,
				0,
				0);
		}
	}

	// render all child nodes
	for (unsigned int i = 0; i < node->Children.size(); ++i)
	{
		DrawNode(dc, cb, aCb, node->Children[i], worldTransform, disabledNodes, textureOverrides);
	}
}

void CredibleModelData::calculateGlobalBoneMatrices(
	_In_ const CredibleNode* node,
	_In_ const CredibleMesh* mesh)
{
	// resize array and initialize it with identity matrices
	XMFLOAT4X4 identity;
	XMStoreFloat4x4(&identity, XMMatrixIdentity());
	_globalBoneTransforms.resize(mesh->NumBones, identity);

	// calculate the mesh's inverse global transform
	XMFLOAT4X4 globalInverseMeshTransform = node->GlobalTransform;
	MathHelper::Inverse(globalInverseMeshTransform);

	// Bone matrices transform from mesh coordinates in bind pose to mesh coordinates in skinned pose
	// Therefore the formula is offsetMatrix * currentGlobalTransform * inverseCurrentMeshTransform
	for (size_t a = 0; a < mesh->NumBones; ++a)
	{
		const CredibleBone* bone = &mesh->Bones[a];
		const XMFLOAT4X4& currentGlobalTransform = _boneNodesByName[bone->Name]->GlobalTransform;

		XMMATRIX git = XMLoadFloat4x4(&globalInverseMeshTransform);
		XMMATRIX cgt = XMLoadFloat4x4(&currentGlobalTransform);
		XMMATRIX om = XMLoadFloat4x4(&bone->Offset);

		XMMATRIX result = git * cgt * om;
		XMStoreFloat4x4(&_globalBoneTransforms[a], result);
	}
}

void CredibleModelData::UpdateTransforms(CredibleNode* node, const std::vector<XMFLOAT4X4>& transforms)
{
	// update node local transform
	if (node->ChannelIndex != -1)
	{
		node->LocalTransform = transforms[node->ChannelIndex];
	}

	// update global transform as well
	CalculateGlobalTransform(node);

	// continue for all children
	for (std::vector<CredibleNode*>::iterator it = node->Children.begin(); it != node->Children.end(); ++it)
	{
		UpdateTransforms(*it, transforms);
	}
}

CredibleNode* CredibleModelData::readNodeTree(
	_In_ byte* data, 
	_In_ CredibleNode* parent, 
	_Inout_ int& readerPosition,
	_In_ ID3D11Device* device)
{
	CredibleNode* ksmNode =  new CredibleNode(data, parent, readerPosition, device);

	// Store in unique ptr to manage memory for this object
	unique_ptr<CredibleNode> up;
	up.reset(ksmNode);
	Nodes.push_back(std::move(up));	

	UINT childCount = DataReader::Read<INT>(data, readerPosition);
	for (UINT c = 0; c < childCount; ++c)
	{
		CredibleNode* child = readNodeTree(data, ksmNode, readerPosition, device);
		ksmNode->Children.push_back(child);
	}

	// Keep a pointer to all the meshes in the model for quick access
	for (auto& mesh : ksmNode->Meshes)
	{
		_allMeshes.push_back(mesh.get());
	}

	return ksmNode;
}

void CredibleModelData::initializeFromKsm(
	_In_ byte* data, 
	_In_opt_ bool readSubsetAsInstanceType /*= false*/)
{
	UNREFERENCED_PARAMETER(readSubsetAsInstanceType);
	int readerPos = 0;

	// Read Global Data
	_numMeshes = DataReader::Read<UINT>(data, readerPos);
	_numMaterials = DataReader::Read<UINT>(data, readerPos);
	_numTextures = DataReader::Read<UINT>(data, readerPos);
	_numAnimations = DataReader::Read<UINT>(data, readerPos);

	_hasAnimations = (_numAnimations > 0);

	// Read in Node Tree
	RootNode = readNodeTree(data, nullptr, readerPos, _device);

	if (_hasAnimations)
	{
		_vertexStride = sizeof(VertexPositionNormalTextureBoneWeight);
	}
	else
	{
		_vertexStride = sizeof(VertexPositionNormalTexture);
	}


	for (unsigned int i = 0; i < _numMeshes; ++i)
	{
		CredibleMesh* mesh = _allMeshes[i];

		//
		// Get all the bones that affect meshes (not all necessarily will). Storing them by name in a collection
		// allows for quick access. These will be used when calculating the global transform (from bind pose to 
		// skinned pose) since we'll need quick access to the node to get it's global transform
		//
		for (unsigned int n = 0; n < mesh->NumBones; ++n)
		{
			const CredibleBone* bone = &mesh->Bones[n];

			auto found = std::find_if(Nodes.begin(), Nodes.end(), [&bone](const unique_ptr<CredibleNode>& node)->bool
			{
				return node->Name == bone->Name;
			});
			MUKASHIDEBUG_CRITICALERROR_ONFALSE(found != Nodes.end());

			_boneNodesByName[bone->Name] = (*found).get();
		}
	}


	// Read in Animations
	for (UINT a = 0; a < _numAnimations; ++a)
	{
		AnimationClip animationClip;

		// Read in the animation name		
		animationClip.Name = DataReader::ReadString<wchar_t, UINT>(data, readerPos);
		animationClip.Duration = DataReader::Read<float>(data, readerPos);
		animationClip.TicksPerSecond = DataReader::Read<float>(data, readerPos);

		UINT boneAnimationCount = DataReader::Read<UINT>(data, readerPos);

		for (UINT b = 0; b < boneAnimationCount; ++b)
		{
			BoneAnimation boneAnimation;

			UINT keyFrameCount = DataReader::Read<UINT>(data, readerPos);

			for (UINT k = 0; k < keyFrameCount; ++k)
			{
				Keyframe keyframe;
				keyframe.Translation	= DataReader::Read<XMFLOAT3>(data, readerPos);
				keyframe.Scale			= DataReader::Read<XMFLOAT3>(data, readerPos);
				keyframe.RotationQuat	= DataReader::Read<XMFLOAT4>(data, readerPos);
				keyframe.TimePos		= DataReader::Read<float>(data, readerPos);

				boneAnimation.Keyframes.push_back(keyframe);
			}

			animationClip.BoneAnimations.push_back(boneAnimation);
		}

		animationClip.TotalFrames = DataReader::Read<UINT>(data, readerPos);

		ModelAnimationData.AddAnimationClip(animationClip);
	}

	//
	// Read in bounding box data
	_boundingBox.Center = DataReader::Read<XMFLOAT3>(data, readerPos);
	_boundingBox.Extents = DataReader::Read<XMFLOAT3>(data, readerPos);
}

void CredibleModelData::SetTextureMapping(
	_In_ ItemSlot itemSlot,
	_In_ wstring textureName)
{
	for (auto& mesh : _allMeshes)
	{
		if (mesh->DefaultDiffuseTextureName.compare(textureName) == 0)
		{
			_textureMapping[mesh->DiffuseTexture] = itemSlot;
		}
	}
}

CredibleNode::CredibleNode(
	_In_ byte* data,
	_In_ CredibleNode* parent,
	_Inout_ int& readerPosition,
	_In_ ID3D11Device* device)
	: CredibleNode()
{
	Name = DataReader::ReadString<wchar_t, UINT>(data, readerPosition);
	LocalTransform = DataReader::Read<XMFLOAT4X4>(data, readerPosition);

	// Calculate the global transform
	Parent = parent;
	CredibleModelData::CalculateGlobalTransform(this);

	ChannelIndex = DataReader::Read<size_t>(data, readerPosition);

	UINT meshCount = DataReader::Read<INT>(data, readerPosition);
	for (UINT m = 0; m < meshCount; ++m)
	{
		Meshes.push_back(make_unique<CredibleMesh>(data, readerPosition, device));
	}
}

CredibleMesh::CredibleMesh(
	_In_ byte* data,
	_Inout_ int& readerPosition,
	_In_ ID3D11Device* device)
{
	NumBones = DataReader::Read<UINT>(data, readerPosition);
	FaceCount = DataReader::Read<UINT>(data, readerPosition);
	VertexCount = DataReader::Read<UINT>(data, readerPosition);
	IndexCount = DataReader::Read<UINT>(data, readerPosition);

	MeshMaterial.Diffuse = DataReader::Read<XMFLOAT4>(data, readerPosition);
	MeshMaterial.Ambient = DataReader::Read<XMFLOAT4>(data, readerPosition);
	MeshMaterial.Reflect = DataReader::Read<XMFLOAT4>(data, readerPosition);
	MeshMaterial.Specular = DataReader::Read<XMFLOAT4>(data, readerPosition);

	MaterialOpacity = DataReader::Read<float>(data, readerPosition);
	MaterialShininess = DataReader::Read<float>(data, readerPosition);
	SpecularStrength = DataReader::Read<float>(data, readerPosition);

	bool indicesAre16Bit = DataReader::Read<bool>(data, readerPosition);
	//indicesAre16Bit = indicesAre16Bit;

	D3D11_BUFFER_DESC bufferDesc;
	D3D11_SUBRESOURCE_DATA resourceData;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;	
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	UINT* iPtr = DataReader::ReadArray<UINT>(data, IndexCount, readerPosition);
	HRESULT hr;
	if (indicesAre16Bit)
	{
		IndexBufferFormat = DXGI_FORMAT_R16_UINT;
		vector<USHORT> indices;

		for (UINT i = 0; i < IndexCount; ++i)
		{
			indices.push_back(static_cast<USHORT>(iPtr[i]));
			Indices.push_back(iPtr[i]);
		}

		bufferDesc.ByteWidth = sizeof(USHORT) * (UINT)indices.size();
		resourceData.pSysMem = &indices[0];

		hr = device->CreateBuffer(&bufferDesc, &resourceData, &IB);
		MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);
	}
	else
	{
		IndexBufferFormat = DXGI_FORMAT_R32_UINT;
		vector<UINT> indices;

		for (UINT i = 0; i < IndexCount; ++i)
		{
			indices.push_back(iPtr[i]);
			Indices.push_back(iPtr[i]);
		}

		bufferDesc.ByteWidth = sizeof(UINT) * (UINT)indices.size();
		resourceData.pSysMem = &indices[0];

		hr = device->CreateBuffer(&bufferDesc, &resourceData, &IB);
		MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);
	}

	bool containsAnimations = DataReader::Read<bool>(data, readerPosition);

	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	UINT vertexStride = 0;
	

	if (containsAnimations)
	{
		vector<VertexPositionNormalTextureBoneWeight> vertices;

		VertexPositionNormalTextureBoneWeight* vPtr = DataReader::ReadArray<VertexPositionNormalTextureBoneWeight>(data, VertexCount, readerPosition);
		for (UINT i = 0; i < VertexCount; ++i)
		{
			vertices.push_back(vPtr[i]);
			PositionalVertices.push_back(vPtr[i].position);
		}

		vertexStride = sizeof(VertexPositionNormalTextureBoneWeight);
		resourceData.pSysMem = &vertices[0];

		bufferDesc.ByteWidth = vertexStride * VertexCount;

		hr = device->CreateBuffer(&bufferDesc, &resourceData, &VB);
		MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);
	}
	else
	{
		vector<VertexPositionNormalTexture> vertices;

		VertexPositionNormalTexture* vPtr = DataReader::ReadArray<VertexPositionNormalTexture>(data, VertexCount, readerPosition);
		for (UINT i = 0; i < VertexCount; ++i)
		{
			vertices.push_back(vPtr[i]);
			PositionalVertices.push_back(vPtr[i].position);
		}

		vertexStride = sizeof(VertexPositionNormalTexture);
		resourceData.pSysMem = &vertices[0];

		bufferDesc.ByteWidth = vertexStride * VertexCount;

		hr = device->CreateBuffer(&bufferDesc, &resourceData, &VB);
		MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);
	}

	//
	// Load Textures
	wstring diffusePath = DataReader::ReadString<wchar_t, UINT>(data, readerPosition);
	if (!diffusePath.empty())
	{
		DiffuseTexture = AssetManager::Instance()->GetTexture(diffusePath);

		// Store the name of the default texture
		PathString defaultTextureName(diffusePath.c_str());
		DefaultDiffuseTextureName = defaultTextureName.GetFullFileName();
	}

	wstring normalPath = DataReader::ReadString<wchar_t, UINT>(data, readerPosition);
	if (!normalPath.empty())
	{
		NormalTexture = AssetManager::Instance()->GetTexture(normalPath);
	}

	//
	// Load Bones
	for (UINT b = 0; b < NumBones; ++b)
	{
		CredibleBone bone;
		
		bone.Name = DataReader::ReadString<wchar_t, UINT>(data, readerPosition);
		bone.Offset = DataReader::Read<XMFLOAT4X4>(data, readerPosition);

		Bones.push_back(bone);
	}

	//
	// Load Mesh BoundingBox
	MeshBoundingBox.Center = DataReader::Read<XMFLOAT3>(data, readerPosition);
	MeshBoundingBox.Extents = DataReader::Read<XMFLOAT3>(data, readerPosition);
}

CredibleMesh::CredibleMesh(
	_In_ GeometryGenerator::MeshData& meshData,
	_In_ ID3D11ShaderResourceView* diffuseTexture,
	_In_ ID3D11ShaderResourceView* normalTexture,
	_In_ ID3D11Device* device)
{
	NumBones = 0;
	FaceCount = meshData.Indices.size() / 3;
	VertexCount = meshData.Vertices.size();
	IndexCount = meshData.Indices.size();

	MeshMaterial.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 0.8f);
	MeshMaterial.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 0.8f);
	MeshMaterial.Reflect = XMFLOAT4(0, 0, 0, 0);
	MeshMaterial.Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f);

	MaterialOpacity = 1.0f;
	MaterialShininess = 1.0f;
	SpecularStrength = 16.0f;

	bool indicesAre16Bit = IndexCount < USHORT_MAX;
	//indicesAre16Bit = indicesAre16Bit;

	D3D11_BUFFER_DESC bufferDesc;
	D3D11_SUBRESOURCE_DATA resourceData;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	HRESULT hr;
	if (indicesAre16Bit)
	{
		IndexBufferFormat = DXGI_FORMAT_R16_UINT;
		vector<USHORT> indices;

		for (UINT i = 0; i < IndexCount; ++i)
		{
			indices.push_back(static_cast<USHORT>(meshData.Indices[i]));
			Indices.push_back(meshData.Indices[i]);
		}

		bufferDesc.ByteWidth = sizeof(USHORT) * (UINT)indices.size();
		resourceData.pSysMem = &indices[0];

		hr = device->CreateBuffer(&bufferDesc, &resourceData, &IB);
		MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);
	}
	else
	{
		IndexBufferFormat = DXGI_FORMAT_R32_UINT;
		vector<UINT> indices;

		for (UINT i = 0; i < IndexCount; ++i)
		{
			indices.push_back(meshData.Indices[i]);
			Indices.push_back(meshData.Indices[i]);
		}

		bufferDesc.ByteWidth = sizeof(UINT) * (UINT)indices.size();
		resourceData.pSysMem = &indices[0];

		hr = device->CreateBuffer(&bufferDesc, &resourceData, &IB);
		MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);
	}

	for (const auto& vertex : meshData.Vertices)
	{
		PositionalVertices.push_back(vertex.position);
	}

	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	UINT vertexStride = 0;
	vertexStride = sizeof(VertexPositionNormalTexture);
	resourceData.pSysMem = &meshData.Vertices[0];

	bufferDesc.ByteWidth = vertexStride * VertexCount;

	hr = device->CreateBuffer(&bufferDesc, &resourceData, &VB);
	MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);
	
	//
	// Load Textures
	DiffuseTexture = diffuseTexture;
	NormalTexture = normalTexture;
}
