//////////////////////////////////////////////////////////////////////////
// AssetManager.cpp
// A singleton object that manages all Meshes and their textures. It loads
// them all into memory and then instances them so there is ever only 1 
// copy of the textures in memory at a time. It also handles preloading
// a given list of models/materials (useful for level loading)
// (c) 2012 Overclocked Games LLC
//////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "AssetManager.h"
#include "CredibleModelData.h"
#include "DirectXTK\DDSTextureLoader.h"
#include "DirectXTK\WICTextureLoader.h"
#include "Terrain.h"
#include "Logger.h"
#include "ObjectInstance.h"
#include "BuildingInstance.h"
#include "ResourceModelInstance.h"
#include "GameItemInstance.h"
#include "AnimatedObjectInstance.h"
#include "DoodadInstance.h"
#include "TinyUtilities.h"

using namespace Engine;

AssetManager* AssetManager::_instance = nullptr;

AssetManager::AssetManager(void)
	: _d3dDevice(nullptr), _dc(nullptr), _camera(nullptr), _loadedTerrain(nullptr), _perObjectCB(nullptr)
{}

AssetManager::~AssetManager(void)
{
	if (_d3dDevice) 
	{
		_d3dDevice->Release();
		_d3dDevice = nullptr;
	}

	if (_dc)
	{
		_dc->Release();
		_dc = nullptr;
	}

	if (_loadedTerrain)
	{
		delete _loadedTerrain;
	}

	_loadedModels.clear();

	// ComPtr will release when destructor is called
	_loadedTextures.clear();
}

AssetManager* AssetManager::Instance()
{
	if (_instance == nullptr)
	{
		_instance = new AssetManager();
	}
	return _instance;
}

void AssetManager::Initialize(
	ID3D11Device* device, 
	ID3D11DeviceContext* dc, 
	ConstantBuffer<ObjectConstBuffer>* perObjectCB, 
	const wchar_t* assetRootDir, 
	Camera* camera)
{
	_d3dDevice = device;
	_d3dDevice->AddRef();

	_dc = dc;
	_dc->AddRef();

	_perObjectCB = perObjectCB;

	_assetRootDirectory.assign(assetRootDir);
	_assetRootDirectory.append(L"\\");

	_camera = camera;

	//_groundTextureArray = GetTextureArray(groundTextures);
	_waterTexture = this->GetTexture(L"Assets\\Textures\\Terrain\\water.dds");

	//
	// Load model metadata
	//
	std::wstring xmlPath(_assetRootDirectory);
	xmlPath.append(L"Engine\\Assets\\MetaData\\ModelMetaData.xml");
	PathString narrowXmlString(xmlPath.c_str());

	tinyxml2::XMLDocument modelMetaDataXml;

	int result = 0;
	if ((result = modelMetaDataXml.LoadFile(narrowXmlString.ToCStr())) != tinyxml2::XML_NO_ERROR)
	{
		MUKASHIDEBUG_CRITICALERROR(L"Failed to load Engine\\Assets\\MetaData\\ModelMetaData.xml! Error reported by tinyxml2: %d", result);
	}

	tinyxml2::XMLElement* modelIterator = modelMetaDataXml.FirstChildElement("models")->FirstChildElement("model");

	do 
	{
		InstanceType modelName = TinyUtilities::GetInstanceTypeAttribute("filename", modelIterator);		

		for (auto it = ItemSlot::ITEM_SLOT_BEGIN + 1; it != ItemSlot::ITEM_SLOT_END; it++)
		{
			ItemSlot slot = static_cast<ItemSlot>(it);
			_defaultTextures[modelName][slot] = L"";
		}

		tinyxml2::XMLElement* textureMapIterator = modelIterator->FirstChildElement("texturemappings")->FirstChildElement("texturemap");

		do
		{
			ItemSlot itemSlot = InstanceSaveTypes::StringToItemSlot(TinyUtilities::GetStringAttribute("slot", textureMapIterator));
			_defaultTextures[modelName][itemSlot] = TinyUtilities::GetStringAttribute("texture", textureMapIterator);

			textureMapIterator = textureMapIterator->NextSiblingElement();
		} while (textureMapIterator);


		modelIterator = modelIterator->NextSiblingElement();
	} while (modelIterator);

	//
	// Load Instance to model mappings
	// 
	xmlPath = _assetRootDirectory;
	xmlPath.append(L"Engine\\Assets\\MetaData\\InstanceToModelMapping.xml");
	narrowXmlString = xmlPath.c_str();

	tinyxml2::XMLDocument instanceMappingXml;

	result = 0;
	if ((result = instanceMappingXml.LoadFile(narrowXmlString.ToCStr())) != tinyxml2::XML_NO_ERROR)
	{
		MUKASHIDEBUG_CRITICALERROR(L"Failed to load Engine\\Assets\\MetaData\\InstanceToModelMapping.xml! Error reported by tinyxml2: %d", result);
	}

	tinyxml2::XMLElement* instanceIterator = instanceMappingXml.FirstChildElement("instances")->FirstChildElement("instance");

	do
	{
		InstanceType fromInstance = TinyUtilities::GetInstanceTypeAttribute("enum", instanceIterator);
		InstanceType toModelInstance = TinyUtilities::GetInstanceTypeAttribute("mappedto", instanceIterator);

		_instanceToModelInstance[fromInstance] = toModelInstance;

		instanceIterator = instanceIterator->NextSiblingElement();
	} while (instanceIterator);
}

//
// Returns a pointer to a shader texture array resource
//
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AssetManager::GetTextureArray(_In_ std::vector<std::wstring>& assetRelativePaths)
{
	HRESULT hr = S_OK;

	// Check if we've initialized
	if (_d3dDevice == nullptr)
	{
		MUKASHIDEBUG_CRITICALERROR(L"Must call AssetManager::Initialize() first!");
		throw new std::exception("Must call AssetManager::Initialize() first!");
	}

	//
	// Load the texture elements individually from file.  These textures
	// won't be used by the GPU (0 bind flags), they are just used to 
	// load the image data from file.  We use the STAGING usage so the
	// CPU can read the resource.
	//

	size_t size = assetRelativePaths.size();

	std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> srcTex(size);
	for(UINT i = 0; i < size; ++i)
	{
		PathString filePath(assetRelativePaths[i].c_str());
		std::wstring ext(filePath.GetExtension());
		if (ext == L".dds" || ext == L".DDS")
		{
			HRESULT hr = CreateDDSTextureFromFileEx(_d3dDevice, assetRelativePaths[i].c_str(), 0, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ, 0, false, (ID3D11Resource**)srcTex[i].GetAddressOf(), nullptr);			
			MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);

			if (FAILED(hr))
			{
				MUKASHIDEBUG_CRITICALERROR(L"Call to CreateDDSTextureFromMemory failed for texture at: %s", filePath.ToWCStr());
			}
		}
		else
		{
			HRESULT hr = CreateWICTextureFromFileEx(_d3dDevice, _dc, filePath.ToWCStr(), 0, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ, 0, false, (ID3D11Resource**)srcTex[i].GetAddressOf(), nullptr);			
			MUKASHIDEBUG_CRITICALERROR_ONFAILED(hr);

			if (FAILED(hr))
			{
				MUKASHIDEBUG_CRITICALERROR(L"Call to CreateWICTextureFromFile failed for texture at: %s", filePath.ToWCStr());
			}
		}
	}

	//
	// Create the texture array.  Each element in the texture 
	// array has the same format/dimensions.
	//

	D3D11_TEXTURE2D_DESC texElementDesc;
	srcTex[0]->GetDesc(&texElementDesc);

	D3D11_TEXTURE2D_DESC texArrayDesc;
	texArrayDesc.Width              = texElementDesc.Width;
	texArrayDesc.Height             = texElementDesc.Height;
	texArrayDesc.MipLevels          = texElementDesc.MipLevels;
	texArrayDesc.ArraySize          = (UINT)size;
	texArrayDesc.Format             = texElementDesc.Format;
	texArrayDesc.SampleDesc.Count   = 1;
	texArrayDesc.SampleDesc.Quality = 0;
	texArrayDesc.Usage              = D3D11_USAGE_DEFAULT;
	texArrayDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
	texArrayDesc.CPUAccessFlags     = 0;
	texArrayDesc.MiscFlags          = 0;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texArray = nullptr;
	hr = _d3dDevice->CreateTexture2D( &texArrayDesc, 0, &texArray);

	//
	// Copy individual texture elements into texture array.
	//

	// for each texture element...
	for(UINT texElement = 0; texElement < size; ++texElement)
	{
		// for each mipmap level...
		for(UINT mipLevel = 0; mipLevel < texElementDesc.MipLevels; ++mipLevel)
		{
			D3D11_MAPPED_SUBRESOURCE mappedTex2D;
			hr = _dc->Map(srcTex[texElement].Get(), mipLevel, D3D11_MAP_READ, 0, &mappedTex2D);

			_dc->UpdateSubresource(texArray.Get(), 
				D3D11CalcSubresource(mipLevel, texElement, texElementDesc.MipLevels),
				0, mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);

			_dc->Unmap(srcTex[texElement].Get(), mipLevel);
		}
	}	

	//
	// Create a resource view to the texture array.
	//

	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	viewDesc.Format = texArrayDesc.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	viewDesc.Texture2DArray.MostDetailedMip = 0;
	viewDesc.Texture2DArray.MipLevels = texArrayDesc.MipLevels;
	viewDesc.Texture2DArray.FirstArraySlice = 0;
	viewDesc.Texture2DArray.ArraySize = (UINT)size;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texArraySRV = nullptr;
	hr = _d3dDevice->CreateShaderResourceView(texArray.Get(), &viewDesc, &texArraySRV);

	return texArraySRV;
}

//
// Return a pointer to a shader texture resource
//
ID3D11ShaderResourceView* AssetManager::GetTexture(_In_ std::wstring assetRelativePath)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ret = nullptr;

	// Check if we've initialized
	if (_d3dDevice == nullptr)
	{
		MUKASHIDEBUG_CRITICALERROR(L"Must call AssetManager::Initialize() first!");
		return nullptr;
	}

	// Attempt to find this texture in the pool of assets already loaded into memory
	// Texture keys are the filenames 
	PathString tmp(assetRelativePath.c_str());
	std::wstring key(tmp.GetFileNameOnly());
	

	auto it = _loadedTextures.find(key);
	if (it != _loadedTextures.end())
	{
		ret = it->second;
	}
	else
	{
		// attempt to load it instead
		byte* memBlock = nullptr;
		std::ifstream::pos_type size;

		std::wstring filePath(_assetRootDirectory);
		filePath.append(assetRelativePath);
		PathString tempPathString(filePath.c_str());

		std::wstring ext(tempPathString.GetExtension());
		if (ext == L".dds" || ext == L".DDS")
		{
			std::ifstream file(filePath, std::ios::in | std::ios::binary | std::ios::ate);

			if (file.is_open())
			{
				size = file.tellg();
				memBlock = new byte[(unsigned int)size];
				file.seekg(0, std::ios::beg);
				file.read((char*)memBlock, size);
				file.close();
			}
			else
			{
				MUKASHIDEBUG_CRITICALERROR(L"Failed to load texture at: %s", filePath.c_str());
			}

			HRESULT hr = CreateDDSTextureFromMemory(_d3dDevice, memBlock, (size_t)size, nullptr, &ret);
			MUKASHIDEBUG_CRITICALERROR_ONTRUE(FAILED(hr));

			if (FAILED(hr))
			{
				MUKASHIDEBUG_CRITICALERROR(L"Call to CreateDDSTextureFromMemory failed for texture at: %s", filePath.c_str());
			}
		}
		else
		{
			HRESULT hr = CreateWICTextureFromFile(_d3dDevice, _dc, filePath.c_str(), nullptr, &ret);			
			MUKASHIDEBUG_CRITICALERROR_ONTRUE(FAILED(hr));

			if (FAILED(hr))
			{
				MUKASHIDEBUG_CRITICALERROR(L"Call to CreateWICTextureFromFile failed for texture at: %s", filePath.c_str());
			}
		}

		_loadedTextures[key] = ret;

		if (memBlock != nullptr)
		{
			delete[] memBlock;
			memBlock = nullptr;
		}
	}

	return ret.Get();
}


CredibleModelData* AssetManager::LoadKsmModel(_In_ InstanceType instanceType)
{
	CredibleModelData* model = nullptr;

	//
	// Convert the instance type to the correct model (not all InstanceTypes have their own model, there is a relationship
	// of potentially many InstanceTypes to any KSM model saved on disk. Some InstanceTypes map directly to a model though
	// for ease of loading, so simply identify the InstanceType that maps to the model that needs to be loaded
	InstanceType modelInstanceType = GetModelInstanceType(instanceType);

	if (_instanceToModelInstance.find(instanceType) != _instanceToModelInstance.end())
	{
		modelInstanceType = _instanceToModelInstance[instanceType];
	}

	// Check if the model has already been loaded
	for (auto& modelCategory : _loadedModels)
	{
		if (modelCategory.first == modelInstanceType)
		{
			model = modelCategory.second.get();
			break;
		}
	}

	if (nullptr == model)
	{
		// attempt to load it instead
		std::unique_ptr<byte[]> memBlock = nullptr;
		std::ifstream::pos_type size;

		// Combine the root directory with the asset name to get the path to the asset
		std::wstring filePath(_assetRootDirectory);
		filePath.append(InstanceSaveTypes::InstanceTypeToModelPath(modelInstanceType));
		std::ifstream file(filePath, std::ios::in | std::ios::binary | std::ios::ate);

		if (file.is_open())
		{
			size = file.tellg();
			memBlock.reset( new byte[(unsigned int)size] );
			file.seekg(0, std::ios::beg);
			file.read((char*)memBlock.get(), size);
			file.close();
		}
		else
		{
			MUKASHIDEBUG_CRITICALERROR(L"Failed to load model at: %s", filePath.c_str());
			return nullptr;
		}

		bool readSubsetsAsInstances = false;

		if (instanceType > InstanceType::NUETRAL_BEGIN &&
			instanceType < InstanceType::EVIL_END)
		{
			readSubsetsAsInstances = true;
		}

		_loadedModels[modelInstanceType] = std::make_unique<CredibleModelData>(memBlock.get(), _d3dDevice, readSubsetsAsInstances);
	}

	model = _loadedModels[modelInstanceType].get();

	//
	// Set default textures
	for (auto it = ItemSlot::ITEM_SLOT_BEGIN + 1; it != ItemSlot::ITEM_SLOT_END; it++)
	{
		ItemSlot slot = static_cast<ItemSlot>(it);
		model->SetTextureMapping(slot, _defaultTextures[modelInstanceType][slot]);
	}

	// TODO: Remove this and place it in the XML configurations somewhere
	if (instanceType == Doodad_Hitodama || instanceType == Doodad_Fire)
	{
		for (auto& node : model->Nodes)
		{
			for (auto& mesh : node->Meshes)
			{
				mesh->MeshMaterial.Ambient = XMFLOAT4(1.5f, 1.5f, 1.5f, 1.0f);
				mesh->MeshMaterial.Diffuse = XMFLOAT4(1.5f, 1.5f, 1.5f, 1.0f);
			}
		}
	}

	return model;
}

Terrain* AssetManager::CreateUpperTerrain()
{
	if (_loadedTerrain)
	{
		delete _loadedTerrain;
		_loadedTerrain = nullptr;
	}
	_loadedTerrain = new Terrain(_assetRootDirectory.c_str(), _d3dDevice, _dc, _perObjectCB, _waterTexture.Get());
	return _loadedTerrain;
}

ID3D11ShaderResourceView* AssetManager::GetItemUITexture( _In_ InstanceType item )
{
	return GetTexture(InstanceSaveTypes::ItemToUIDisplayPath(item));
}