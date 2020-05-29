#ifndef GAME_ENGINE_RENDER_CONTEXT_H
#define GAME_ENGINE_RENDER_CONTEXT_H

#include "CoreLib/Basic.h"
#include "CoreLib/VectorMath.h"
#include "CoreLib/Graphics/TextureFile.h"
#include "HardwareRenderer.h"
#include "Skeleton.h"
#include "DeviceMemory.h"
#include "Mesh.h"
#include "FrustumCulling.h"
#include "PipelineContext.h"
#include "AsyncCommandBuffer.h"
#include "Drawable.h"
#include "ViewResource.h"
#include "View.h"
#include "ViewResource.h"
#include "Renderer.h"
#include "CoreLib/PerformanceCounter.h"
#include "DeviceLightmapSet.h"

namespace GameEngine
{
	class Mesh;
	class Skeleton;
	class Material;
	class SceneResource;
	class Renderer;
	class CameraActor;
	class RendererSharedResource;
	class Level;
	class WorldRenderPass;
	class AsyncCommandBuffer;

	const StorageFormat DepthBufferFormat = StorageFormat::Depth32;

	class RenderStat
	{
	public:
		float TotalTime;
		int Divisor = 0;
		int NumDrawCalls = 0;
		int NumPasses = 0;
		int NumShaders = 0;
		int NumMaterials = 0;
		float CpuTime = 0.0f;
		float PipelineLookupTime = 0.0f;
		CoreLib::Diagnostics::TimePoint StartTime;
		void Clear()
		{
			Divisor = 0;
			NumDrawCalls = 0;
			NumPasses = 0;
			NumShaders = 0;
			NumMaterials = 0;
			CpuTime = 0.0f;
			PipelineLookupTime = 0.0f;
		}
	};

	struct BoneTransform
	{
		VectorMath::Matrix4 TransformMatrix;
		VectorMath::Vec4 NormalMatrix[3];
	};

	class SharedModuleInstances
	{
	public:
		ModuleInstance * View;
	};

	class RenderTask : public CoreLib::RefObject
	{
	public:
		virtual void Execute(HardwareRenderer * hw, RenderStat & stats, PipelineBarriers barriers = PipelineBarriers::MemoryAndImage) = 0;
	};

	class WorldPassRenderTask : public RenderTask
	{
	public:
		int renderPassId = -1; 
		int numDrawCalls = 0; 
		int numMaterials = 0; 
		int numShaders = 0;
		SharedModuleInstances sharedModules; 
		CoreLib::List<AsyncCommandBuffer*> commandBuffers;
		CoreLib::List<CommandBuffer*> apiCommandBuffers;
		WorldRenderPass * pass = nullptr;
		RenderOutput * renderOutput = nullptr; 
		FixedFunctionPipelineStates * fixedFunctionStates = nullptr;
		Viewport viewport; 
		bool clearOutput = false;
		virtual void Execute(HardwareRenderer * hw, RenderStat & stats, PipelineBarriers barriers) override;
		void SetFixedOrderDrawContent(PipelineContext & pipelineManager, CoreLib::ArrayView<Drawable*> drawables);
		void SetDrawContent(PipelineContext & pipelineManager, CoreLib::List<Drawable*>& reorderBuffer, CoreLib::ArrayView<Drawable*> drawables);
	};

	class PostPassRenderTask : public RenderTask
	{
	public:
		PostRenderPass * postPass = nullptr;
		SharedModuleInstances sharedModules;
		virtual void Execute(HardwareRenderer * hw, RenderStat & stats, PipelineBarriers barriers) override;
	};

	struct SpireModuleStruct
	{
		int dummy;
	};

	class ShadowMapResource
	{
	private:
		int shadowMapArraySize;
		CoreLib::IntSet shadowMapArrayFreeBits;
		CoreLib::RefPtr<ViewResource> shadowView;
	public:
		CoreLib::RefPtr<Texture2DArray> shadowMapArray;
		CoreLib::RefPtr<RenderTargetLayout> shadowMapRenderTargetLayout;
		CoreLib::List<CoreLib::RefPtr<RenderOutput>> shadowMapRenderOutputs;
		int AllocShadowMaps(int count);
		void FreeShadowMaps(int id, int count);
		void Init(HardwareRenderer * hwRenderer);
		void Destroy();
		void Reset();
	};

	class RendererResource
	{
	protected:
		CoreLib::EnumerableDictionary<unsigned int, CoreLib::RefPtr<DescriptorSetLayout>> descLayouts;
	public:
		CoreLib::RefPtr<HardwareRenderer> hardwareRenderer;
		void CreateModuleInstance(ModuleInstance & mInst, ShaderTypeSymbol * shaderType, DeviceMemory * uniformMemory, int uniformBufferSize = 0);
		virtual void Destroy();
	};

	class RendererSharedResource : public RendererResource
	{
	private:
		RenderAPI api;
		int envMapAllocPtr = 0;
	public:
		RenderStat renderStats;
		CoreLib::RefPtr<TextureSampler> textureSampler, nearestSampler, linearSampler, linearClampedSampler, envMapSampler, shadowSampler;
		CoreLib::RefPtr<Texture3D> defaultColorLookupTexture;
		ShadowMapResource shadowMapResources;
	public:
		CoreLib::RefPtr<TextureCubeArray> envMapArray;
        CoreLib::RefPtr<Buffer> histogramBuffer, adaptedLuminanceBuffer;
		CoreLib::RefPtr<Buffer> fullScreenQuadVertBuffer;
		DeviceMemory indexBufferMemory, vertexBufferMemory, blendShapeMemory;
		PipelineContext pipelineManager;
	public:
		RendererSharedResource(RenderAPI pAPI)
		{
			this->api = pAPI;
		}
		int AllocEnvMap()
		{
			if (envMapAllocPtr < MaxEnvMapCount)
				return envMapAllocPtr++;
			else
				return -1;
		}
		void ResetEnvMapAllocation()
		{
			envMapAllocPtr = 0;
		}
		void Init(HardwareRenderer * phwRenderer);
		virtual void Destroy() override;
	};

	class SceneResource : public RendererResource
	{
	private:
		RendererSharedResource * rendererResource;
		CoreLib::EnumerableDictionary<CoreLib::String, CoreLib::RefPtr<DrawableMesh>> meshes;
		CoreLib::EnumerableDictionary<CoreLib::String, CoreLib::RefPtr<Texture2D>> textures;
		void CreateMaterialModuleInstance(ModuleInstance & mInst, Material* material, const char * moduleName);
	public:
		CoreLib::RefPtr<DrawableMesh> LoadDrawableMesh(Mesh * mesh);
        CoreLib::RefPtr<DrawableMesh> CreateDrawableMesh(Mesh * mesh);
        void UpdateDrawableMesh(Mesh* mesh);
		Texture2D* LoadTexture2D(const CoreLib::String & name, CoreLib::Graphics::TextureFile & data);
		Texture2D* LoadTexture(const CoreLib::String & filename);
	public:
        CoreLib::RefPtr<DeviceLightmapSet> deviceLightmapSet;
		DeviceMemory instanceUniformMemory, transformMemory;
		void RegisterMaterial(Material * material);
		
	public:
		SceneResource(RendererSharedResource * resource);
		void Clear();
	};
}
#endif