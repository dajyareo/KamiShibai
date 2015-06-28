//////////////////////////////////////////////////////////////////////////
// AssetManager.h
// A singleton object that manages all Meshes and their textures. It loads
// them all into memory and then instances them so there is ever only 1 
// copy of the textures in memory at a time. It also handles pre-loading
// a given list of models/materials (useful for level loading)
// (c) 2012 Overclocked Games LLC
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "GeometryGenerator.h"
#include "../KsmCreatorLib/PathString.h"
#include "ConstantBuffer.h"
#include "AssetTypeEnum.h"
#include "InstanceSaveTypes.h"

using namespace std;

namespace Engine
{
	class ObjectInstance;
	class BuildingInstance;
	class CredibleModelData;
	class Camera;
	class Terrain;
	struct ObjectConstBuffer;

	class AssetManager
	{
	public:

		static AssetManager* Instance();

		// Tells the Model Manager to initialize
		void Initialize(ID3D11Device* device, ID3D11DeviceContext* dc, ConstantBuffer<ObjectConstBuffer>* perObjectCB, const wchar_t* assetRootDir, Camera* camera);

		// Takes a path to a txt file that contains a list of assets that should
		// be loaded into memory
		void PreloadAssets(std::wstring assetList);

		// All assets loaded into memory will be deleted (not instances)
		void DumpAssets();

		//
		// FullName:  Engine::AssetManager::LoadKsmModel
		// Loads the KSM model associated with the given instanceType
		//
		CredibleModelData* LoadKsmModel(_In_ InstanceType instanceType);

		//
		// FullName:  Engine::AssetManager::CreateUpperTerrain
		// Creates the upper (overworld) terrain
		//
		Terrain* CreateUpperTerrain();

		Terrain* GetTerrainData() const { return _loadedTerrain; }

		//
		// Return a pointer to a shader texture resource
		//
		ID3D11ShaderResourceView* GetTexture(_In_ std::wstring assetRelativePath);

		//
		// FullName:  Engine::AssetManager::GetItemUITexture
		// Given a InstanceType that is an item (will return NULL if not) it'll return the loaded
		// texture for that given item to display as a UI element (the box that shows which item is equipped)
		//
		ID3D11ShaderResourceView* GetItemUITexture(_In_ InstanceType item);

		//
		// Convert the instance type to the correct model (not all InstanceTypes have their own model, there is a relationship
		// of potentially many InstanceTypes to any KSM model saved on disk. Some InstanceTypes map directly to a model though
		// for ease of loading, so simply identify the InstanceType that maps to the model that needs to be loaded
		//
		InstanceType GetModelInstanceType(_In_ InstanceType instanceType)
		{
			if (_instanceToModelInstance.find(instanceType) != _instanceToModelInstance.end())
			{
				instanceType = _instanceToModelInstance[instanceType];
			}
			return instanceType;
		}

		//
		// Returns a pointer to a shader texture array resource
		//
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTextureArray(
			_In_ std::vector<std::wstring>& assetRelativePaths);

		ID3D11Device* GetDevice() const { return _d3dDevice; }
		 

		// Functions
	private:
		AssetManager(void);
		~AssetManager(void);

		// Performs cleanup (see note in CPP file on this)
		static void cleanUp();

		// Make class not copyable
		AssetManager(const AssetManager&);
		AssetManager& operator=(const AssetManager&);

		// Member variables
	private:
		// The single static instance of the class
		static AssetManager* _instance;

		// Used to generate terrain and other objects at runtime
		GeometryGenerator _geometryGenerator;

		// Holds the location of the Asset directory's root
		std::wstring _assetRootDirectory;

		// Pointer to the D3D device
		ID3D11Device* _d3dDevice;

		ID3D11DeviceContext* _dc;

		// The storage data structure for Textures
		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> _loadedTextures;

		// The storage data structure for Models
		std::map<InstanceType, std::unique_ptr<CredibleModelData>> _loadedModels;

		Camera* _camera;

		Terrain* _loadedTerrain;
		//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _groundTextureArray;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _waterTexture;

		// The per object constant buffer
		ConstantBuffer<ObjectConstBuffer>* _perObjectCB;

		map<InstanceType, map<ItemSlot, wstring>> _defaultTextures;

		// Maps InstanceTypes to the InstanceType that loads the model KSM data. For example, there are many InstanceTypes 
		// that all load the same model (Ochimusha.ksm). These all need to map back to the InstanceType that loads that model
		// (Ochimusha) when loading the model data for it. These InstanceTypes then customize the textures for that model in
		// the CredibleObject that wraps them to make them look unique
		map<InstanceType, InstanceType> _instanceToModelInstance;
	};
}

