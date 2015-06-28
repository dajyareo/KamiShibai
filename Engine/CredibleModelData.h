//////////////////////////////////////////////////////////////////////////
// KsmModel.h
// Header file for the KamiShibai Model data structure. 
// (c) 2012 Overclocked Games LLC
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "pch.h"
#include "ObjectManager.h"
#include "../KsmCreatorLib/LightHelper.h"
#include "KsmMesh.h"
#include "GeometryGenerator.h"
#include "AnimationEnum.h"
#include "AnimationData.h"
#include "InstanceSaveTypes.h"
#include "ConstantBuffer.h"
#include "Bullet\src\BulletCollision\CollisionShapes\btShapeHull.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace Engine
{
	class AssetManager;
	struct ObjectConstBuffer;

	//
	// Holds bone names (which map to CredibleNodes for global transform data) and Bone bind to skinned offset matrix
	//
	struct CredibleBone
	{
		wstring Name;
		XMFLOAT4X4 Offset;
	};

	//
	// Maps to a mesh in the model. A node can have many or no meshes
	//
	struct CredibleMesh
	{
		CredibleMesh() :
			VertexCount(0), FaceCount(0), IndexCount(0), DiffuseTexture(nullptr), NormalTexture(nullptr),
			Instance(INSTANCE_TYPE_BEGIN), VB(nullptr), IB(nullptr), NumBones(0), MaterialOpacity(0),
			MaterialShininess(0), SpecularStrength(0), IndexBufferFormat(DXGI_FORMAT_R32_UINT)
		{}

		CredibleMesh(
			_In_ byte* data,
			_Inout_ int& readerPosition,
			_In_ ID3D11Device* device);

		CredibleMesh(
			_In_ GeometryGenerator::MeshData& meshData,
			_In_ ID3D11ShaderResourceView* diffuseTexture,
			_In_ ID3D11ShaderResourceView* normalTexture,
			_In_ ID3D11Device* device);

		//
		// Returns bool indicating if this mesh's vertices are associated with (affected by) any bones
		//
		bool HasBones() const
		{
			return NumBones > 0;
		}

		//
		// Checks if the given ray and direction of given ray intersects with any of the mesh data in this model
		// and returns all points at which that intersection occurs
		//
		std::vector<XMFLOAT3> GetAllIntersectionsBy(
			_In_ const XMVECTOR& rayOrigin,
			_In_ const XMVECTOR& rayDirection,
			_In_opt_ const XMMATRIX* translation = nullptr);

		//
		// Checks if the given ray and direction of given ray intersects with any of the mesh data in this model
		//
		bool IntersectedBy(
			_In_ const XMVECTOR& rayOrigin,
			_In_ const XMVECTOR& rayDirection,
			_In_ const XMFLOAT4X4& worldTransform,
			_In_ XMFLOAT4X4 nodeTransform,
			_In_ const XMMATRIX& inverseView);

		// The count of vertices that make up the mesh
		UINT VertexCount;

		// The count of faces that makes up the mesh
		UINT FaceCount;

		// The count of indices that makes up the mesh
		UINT IndexCount;

		// The diffuse texture for this mesh
		ID3D11ShaderResourceView* DiffuseTexture;

		// The name of the default diffuse texture so it can be identified and replaced
		wstring DefaultDiffuseTextureName;

		// The normal texture for this mesh
		ID3D11ShaderResourceView* NormalTexture;

		// The material for this node
		Material MeshMaterial;

		// The InstanceType that this subset represents (used for items and other additions)
		InstanceType Instance;

		// Vertex buffer
		ComPtr<ID3D11Buffer> VB;

		// Index buffer
		ComPtr<ID3D11Buffer> IB;

		// opacity for the material
		float MaterialOpacity;

		// shininess for the material
		float MaterialShininess;

		// strength of the specular highlight
		float SpecularStrength;

		// The number of bones associated with this mesh
		UINT NumBones;

		// The names of the bone nodes that affect this mesh
		vector<CredibleBone> Bones;

		// The format for the index buffer
		DXGI_FORMAT IndexBufferFormat;

		// Holds only the position coords for every mesh in this model; useful for sharing only the 
		// positional data for ray intersection checking. Not used for drawing
		std::vector<XMFLOAT3> PositionalVertices;

		// Used when detecting intersections on polygons
		std::vector<UINT> Indices;

		// The bounding box surrounding the vertices in this mesh
		BoundingBox MeshBoundingBox;

		vector<XMFLOAT3> translatedVertices;
	};

	//
	// Maps to a node in the model. May not actually contain any mesh data if it's a parent node that just
	// holds transforms. Parent will always be another node unless this is the root node to the model. 
	// If the node has a bone associated with it, it will have the index of the animation it belongs to, 
	// else that will be -1
	//
	struct CredibleNode
	{
		CredibleNode() :
			Parent(nullptr), ChannelIndex((size_t)-1), Enabled(true)
		{
			XMStoreFloat4x4(&LocalTransform, XMMatrixIdentity());
			XMStoreFloat4x4(&GlobalTransform, XMMatrixIdentity());
		}

		CredibleNode(
			_In_ byte* data,
			_In_ CredibleNode* parent,
			_Inout_ int& readerPosition,
			_In_ ID3D11Device* device);

		CredibleNode(const wstring& name)
			: CredibleNode()
		{
			Name = name;
		}

		// The name of this node
		wstring Name;

		// Indicates if this mesh is enabled for drawing or not
		bool Enabled;

		// The children of this node
		std::vector<CredibleNode*> Children;

		// The parent of this node. nullptr if has no parent (the root node)
		CredibleNode* Parent;

		// Most recently calculated local transform
		XMFLOAT4X4 LocalTransform;

		// Most recently calculated global transform. This includes the local bone transform if applicable
		XMFLOAT4X4 GlobalTransform;

		// The meshes associated with this node
		vector<unique_ptr<CredibleMesh>> Meshes;

		// Index in the current animation's channel array. -1 if not animated.
		size_t ChannelIndex;
	};

	class CredibleModelData
	{

	public:

		CredibleModelData()
		{}

		CredibleModelData(_In_ ID3D11Device* device)
			: _device(device), _animationTimer(0)
		{}

		//
		// Creates a CredibleModelData object by loading it from a stream of bytes
		//
		CredibleModelData(
			_In_ byte* data,
			_In_ ID3D11Device* device,
			_In_opt_ bool readSubsetAsInstanceType = false);

		//
		// Creates a CredibleModelData object by reading the data it needs from a 
		// GeometryGenerator::MeshData object
		//
		CredibleModelData(
			_In_ ID3D11Device* device,
			_In_ GeometryGenerator::MeshData& meshData,
			_In_ ID3D11ShaderResourceView* diffuseTexture);

		~CredibleModelData();

		//
		// Prevent deep copies for performance reasons
		//
		CredibleModelData(const CredibleModelData&) = delete;
		CredibleModelData& operator=(CredibleModelData&) = delete;

		//
		// Move constructor
		//
		CredibleModelData(CredibleObject&& other);

		//
		// Move assignment operator
		//
		CredibleModelData& operator=(CredibleModelData&& rhs);

		//
		// Returns if this model contains animation data or not
		//
		bool HasAnimations() const 
		{ 
			return ModelAnimationData.ContainsData();
		}

		//
		// Handles drawing once instance of a model and all of its subsets
		//
		void Draw(
			_In_ ID3D11DeviceContext* dc,
			_In_ ConstantBuffer<ObjectConstBuffer>* cb,
			_In_ ConstantBuffer<AnimatedConstBuffer>* aCb,
			_In_ XMFLOAT4X4 world,
			_In_ const std::vector<XMFLOAT4X4>& localBoneTransforms,
			_In_ const vector<wstring>& disabledNodes,
			_In_ map<ItemSlot, ID3D11ShaderResourceView*>& textureOverrides);

		void DrawNode(
			_In_ ID3D11DeviceContext* dc,
			_In_ ConstantBuffer<ObjectConstBuffer>* cb,
			_In_ ConstantBuffer<AnimatedConstBuffer>* aCb,
			CredibleNode* node,
			_In_ XMFLOAT4X4 localBoneTransforms,
			_In_ const vector<wstring>& disabledNodes,
			_In_ map<ItemSlot, ID3D11ShaderResourceView*>& textureOverrides);

		void DrawInstancedNode(
			_In_ ID3D11DeviceContext* dc,
			_In_ ID3D11Buffer* instanceDataBuffer,
			_In_ UINT count,
			_In_ ConstantBuffer<ObjectConstBuffer>* cb,
			_In_ CredibleMesh* mesh);

		//
		// Sets the texture names that are associated with item slots. This allows us to, in the future, swap
		// out these textures for others. For example, if we wanted to change the clothing texture used for this
		// model, we'd have to know which texture used by the CredibleMesh subsets was mapped to the Body slot
		//
		void SetTextureMapping(
			_In_ ItemSlot itemSlot,
			_In_ wstring textureName);

		//
		// Updates the bone 
		//
		void UpdateTransforms(CredibleNode* pNode, const std::vector<XMFLOAT4X4>& pTransforms);

		//
		// Concatenates all parent transforms to get the global transform for the given node. Static so it 
		// can be called by the CredibleNode class as well
		//
		static void CalculateGlobalTransform(CredibleNode* node);

		//
		// Checks if the given ray and direction of given ray intersects with any of the mesh data in this model
		//
		bool IntersectedBy(
			_In_ const XMVECTOR& rayOrigin,
			_In_ const XMVECTOR& rayDirection,
			_In_ const XMFLOAT4X4& worldTransform,
			_In_ const XMMATRIX& inverseView);

		//
		// Checks if the given ray and direction of given ray intersects with any of the mesh data in this model
		// and returns all points at which that intersection occurs
		//
		std::vector<XMFLOAT3> GetAllIntersectionsBy(
			_In_ const XMVECTOR& rayOrigin,
			_In_ const XMVECTOR& rayDirection,
			_In_opt_ const XMMATRIX* translation = nullptr);

		//
		// Returns the bounding box that encapsulates this model in model space
		//
		BoundingBox GetBoundingBox() const
		{
			return _boundingBox;
		}

		btBvhTriangleMeshShape* GenerateBtConvexShape(_In_ XMFLOAT4X4 scale)
		{
			//unique_ptr<btConvexHullShape> originalConvexShape = make_unique<btConvexHullShape>();
			btBvhTriangleMeshShape* originalConvexShape = nullptr;

			btTriangleIndexVertexArray* shapeVertexArray = new btTriangleIndexVertexArray();
			generateBtConvexShapeTraverse(RootNode, shapeVertexArray, scale);

			//for (const auto& mesh : _allMeshes)
			//{
			//	for (const auto& vertex : mesh->PositionalVertices)
			//	{
			//		originalConvexShape->addPoint(btVector3(vertex.x, vertex.y, vertex.z));
			//	}
			//}

				//create a hull approximation
			//unique_ptr<btShapeHull> hull = make_unique<btShapeHull>(originalConvexShape);
			//btShapeHull* hull = new btShapeHull(originalConvexShape);
			//btScalar margin = originalConvexShape->getMargin();
			//hull->buildHull(margin);

			//btConvexHullShape* simplifiedConvexShape = new btConvexHullShape();
			//for (int i = 0; i < hull->numVertices(); i++)
			//{
			//	simplifiedConvexShape->addPoint(hull->getVertexPointer()[i], false);
			//}
			//simplifiedConvexShape->recalcLocalAabb();

			originalConvexShape = new btBvhTriangleMeshShape(shapeVertexArray, false);
			return originalConvexShape;
		}

		void generateBtConvexShapeTraverse(
			_In_ CredibleNode* node,
			_In_ btTriangleIndexVertexArray* shape,
			_In_ XMFLOAT4X4 scale);

		//
		// The animation data for this model
		//
		AnimationData ModelAnimationData;

		//
		// All nodes in this model wrapped in unique_ptr to manage memory
		//
		vector < unique_ptr<CredibleNode>> Nodes;

		//
		// Pointer to the root node of this model
		//
		CredibleNode* RootNode;		

	private:

		//
		// Initializes this model from a stream of bytes
		//
		void initializeFromKsm(
			_In_ byte* data,
			_In_opt_ bool readSubsetAsInstanceType = false);

		//
		// Called by readModelDataFromFile to read the node tree in from file
		//
		CredibleNode* readNodeTree(
			_In_ byte* data,
			_In_ CredibleNode* parent,
			_Inout_ int& readerPosition,
			_In_ ID3D11Device* device);

		//
		// Multiplies the bone offset by global bone transform to get final bone matrix
		//
		void calculateGlobalBoneMatrices(
			_In_ const CredibleNode* node,
			_In_ const CredibleMesh* mesh);

		//
		// Initializes this model from a GeometryGenerator::MeshData object
		//
		void initializeFromMeshData(
			_In_ GeometryGenerator::MeshData& meshData,
			_In_opt_ ID3D11ShaderResourceView* diffuseTexture = nullptr,
			_In_opt_ ID3D11ShaderResourceView* normalTexture = nullptr);

	private:

		// D3D device used for drawing
		ID3D11Device* _device;

		// Cached sizeof Vertex
		UINT _vertexStride;

		// Bones transformed from local space to world space
		std::vector<XMFLOAT4X4> _globalBoneTransforms;

		// The total number of meshes in this model
		UINT _numMeshes;

		// The total number of materials in this model
		UINT _numMaterials;

		// The total number of textures in this model
		UINT _numTextures;

		// The total number of animations in this model
		UINT _numAnimations;

		// Indicates if the model contains animation data or not
		bool _hasAnimations;

		// The bounding box that encapsulates this model in model space
		BoundingBox _boundingBox;

		// Holds total time for animation calculations
		double _animationTimer;

		// Pointers to all the meshes in the model for quick reference
		vector <CredibleMesh*>_allMeshes;

		// Nodes that are bone nodes for easy reference
		map<wstring, const CredibleNode*> _boneNodesByName;

		// The textures that map to each item slot
		map<ID3D11ShaderResourceView*, ItemSlot> _textureMapping;
	};
}