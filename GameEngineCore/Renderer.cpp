#include "Renderer.h"
#include "HardwareRenderer.h"
#include "Level.h"
#include "Engine.h"
#include "SkeletalMeshActor.h"
#include "CameraActor.h"
#include "EnvMapActor.h"
#include "CoreLib/Graphics/TextureFile.h"
#include "CoreLib/Imaging/Bitmap.h"
#include "CoreLib/Imaging/TextureData.h"
#include "CoreLib/WinForm/Debug.h"
#include "LightProbeRenderer.h"
#include "TextureCompressor.h"
#include "DeviceLightmapSet.h"
#include <fstream>
#include "DeviceMemory.h"
#include "WorldRenderPass.h"
#include "PostRenderPass.h"
#include "RenderProcedure.h"
#include "ComputeTaskManager.h"

using namespace CoreLib;
using namespace VectorMath;

namespace GameEngine
{
	int Align(int ptr, int alignment)
	{
		int m = ptr % alignment;
		if (m)
		{
			int padding = alignment - m;
			return ptr + padding;
		}
		return ptr;
	}

	class RendererImpl : public Renderer
	{
		class RendererServiceImpl : public RendererService
		{
		private:
			RendererImpl * renderer;
			RefPtr<Drawable> CreateDrawableShared(Mesh * mesh, Material * material, bool cacheMesh)
			{
				auto sceneResources = renderer->sceneRes.Ptr();
				RefPtr<Drawable> rs = new Drawable(sceneResources);
                if (cacheMesh)
                    rs->mesh = sceneResources->LoadDrawableMesh(mesh);
                else
                    rs->mesh = sceneResources->CreateDrawableMesh(mesh);
				rs->material = material;
				return rs;
			}

		public:
			RendererServiceImpl(RendererImpl * pRenderer)
				: renderer(pRenderer)
			{
            }
			void CreateTransformModuleInstance(ModuleInstance & rs, const char * name, int uniformBufferSize)
			{
				auto sceneResources = renderer->sceneRes.Ptr();
				renderer->sharedRes.CreateModuleInstance(rs, Engine::GetShaderCompiler()->LoadSystemTypeSymbol(name), &sceneResources->transformMemory, uniformBufferSize);
			}

			virtual CoreLib::RefPtr<Drawable> CreateStaticDrawable(Mesh * mesh, int elementId, Material * material, bool cacheMesh) override
			{
                if (!material)
                    material = Engine::Instance()->GetLevel()->LoadErrorMaterial();
				if (!material->MaterialModule)
					renderer->sceneRes->RegisterMaterial(material);
				RefPtr<Drawable> rs = CreateDrawableShared(mesh, material, cacheMesh);
				rs->type = DrawableType::Static;
                rs->primType = mesh->GetPrimitiveType();
				rs->elementRange = mesh->ElementRanges[elementId];
				CreateTransformModuleInstance(*rs->transformModule, "StaticMeshTransform", (int)(sizeof(Vec4) * 5));
                uint32_t lightmapId = 0xFFFFFFFF;
                for (int i = 0; i < DynamicBufferLengthMultiplier; i++)
                {
                    rs->transformModule->SetUniformData(&lightmapId, sizeof(lightmapId));
                }
				return rs;
			}
			virtual CoreLib::RefPtr<Drawable> CreateSkeletalDrawable(Mesh * mesh, int elementId, Skeleton * skeleton, Material * material, bool cacheMesh) override
			{
				if (!material->MaterialModule)
					renderer->sceneRes->RegisterMaterial(material);
				RefPtr<Drawable> rs = CreateDrawableShared(mesh, material, cacheMesh);
				rs->type = DrawableType::Skeletal;
                rs->primType = mesh->GetPrimitiveType();
				rs->elementRange = mesh->ElementRanges[elementId];
				rs->skeleton = skeleton;
				CreateTransformModuleInstance(*rs->transformModule, "SkeletalAnimationTransform", 4096);
                for (int i = 0; i < DynamicBufferLengthMultiplier; i++)
                {
                    auto descSet = rs->transformModule->GetDescriptorSet(i);
                    descSet->BeginUpdate();
                    descSet->Update(1, rs->mesh->GetBlendShapeBuffer(), rs->mesh->blendShapeBufferOffset,
                        rs->mesh->blendShapeVertexCount == 0 ? -1 : rs->mesh->blendShapeVertexCount * sizeof(BlendShapeVertex));
					descSet->EndUpdate();
                }
				return rs;
			}
		};
	private:
		RendererSharedResource sharedRes;
		RefPtr<SceneResource> sceneRes;
		RefPtr<ViewResource> mainView;
		RefPtr<RendererServiceImpl> renderService;
		IRenderProcedure* currentRenderProcedure = nullptr;
        IRenderProcedure* lightProbeRenderProcedure = nullptr;
		EnumerableDictionary<uint32_t, int> worldRenderPassIds;
		List<RefPtr<WorldRenderPass>> worldRenderPasses;
		List<RefPtr<PostRenderPass>> postRenderPasses;
        List<String> renderProcedureNames;
        Dictionary<String, RefPtr<IRenderProcedure>> renderProcedures;
		HardwareRenderer * hardwareRenderer = nullptr;
		Level* level = nullptr;
		int uniformBufferAlignment = 256;
		int storageBufferAlignment = 32;
		int defaultEnvMapId = -1;
	private:
        void RegisterRenderProcedure(IRenderProcedure* proc, ViewResource* viewRes)
        {
            auto name = proc->GetName();
            renderProcedures.Add(name, proc);
            renderProcedureNames.Add(name);
            if (currentRenderProcedure == nullptr)
                currentRenderProcedure = proc;
            proc->Init(this, viewRes);
        }
		void RunRenderProcedure()
		{
			if (!level) return;
			RenderProcedureParameters params;
			params.renderStats = &sharedRes.renderStats;
			params.level = level;
			params.renderer = this;
			params.isEditorMode = Engine::Instance()->GetEngineMode() == EngineMode::Editor;
			auto curCam = level->CurrentCamera.Ptr();
			if (curCam)
				params.view = curCam->GetView();
			else
				params.view = View();
			params.rendererService = renderService.Ptr();

            if (currentRenderProcedure)
			    currentRenderProcedure->Run(params);
		}
	public:
        CoreLib::RefPtr<ComputeTaskManager> computeTaskManager;

		RendererImpl(RenderAPI api)
			: sharedRes(api)
		{
			switch (api)
			{
			case RenderAPI::Vulkan:
				hardwareRenderer = CreateVulkanHardwareRenderer(Engine::Instance()->GpuId, 
                    Path::Combine(Engine::Instance()->GetDirectory(false, ResourceType::ShaderCache), "pipeline_cache.tmp"));
				break;
			case RenderAPI::D3D12:
				hardwareRenderer = CreateD3DHardwareRenderer(Engine::Instance()->GpuId, Engine::Instance()->UseSoftwareRenderer(),
					Path::Combine(Engine::Instance()->GetDirectory(false, ResourceType::ShaderCache), "pipeline_cache_d3d.tmp"));
				break;
			case RenderAPI::Dummy:
				hardwareRenderer = CreateDummyHardwareRenderer();
				break;
			}
			Engine::Instance()->SetTargetShadingLanguage(hardwareRenderer->GetShadingLanguage());
			hardwareRenderer->Init(DynamicBufferLengthMultiplier);
            
            computeTaskManager = new ComputeTaskManager(hardwareRenderer, Engine::GetShaderCompiler());

			sharedRes.Init(hardwareRenderer);

			mainView = new ViewResource(hardwareRenderer);
			mainView->Resize(1024, 1024);
			
			RegisterRenderProcedure(CreateStandardRenderProcedure(true, true), mainView.Ptr());
            RegisterRenderProcedure(CreateLightmapDebugViewRenderProcedure(), mainView.Ptr());

            cubemapRenderView = new ViewResource(hardwareRenderer);
            cubemapRenderView->Resize(EnvMapSize, EnvMapSize);
            lightProbeRenderProcedure = CreateLightProbeRenderProcedure();
            RegisterRenderProcedure(lightProbeRenderProcedure, cubemapRenderView.Ptr());

			// Fetch uniform buffer alignment requirements
			uniformBufferAlignment = hardwareRenderer->UniformBufferAlignment();
			storageBufferAlignment = hardwareRenderer->StorageBufferAlignment();
			
			sceneRes = new SceneResource(&sharedRes);
			renderService = new RendererServiceImpl(this);
			hardwareRenderer->Wait();
		}
		~RendererImpl()
		{
			Wait();
			for (auto & postPass : postRenderPasses)
				postPass = nullptr;

            renderProcedures = decltype(renderProcedures)();
			mainView = nullptr;
			sceneRes = nullptr;
			sharedRes.Destroy();
            computeTaskManager = nullptr;
		}
        virtual ArrayView<String> GetDebugViews() override
        {
            return renderProcedureNames.GetArrayView();
        }
        virtual void SetDebugView(String viewName) override
        {
            RefPtr<IRenderProcedure> proc;
            renderProcedures.TryGetValue(viewName, proc);
            currentRenderProcedure = proc.Ptr();
        }
		virtual void Wait() override
		{
			hardwareRenderer->Wait();
		}

        virtual ComputeTaskManager* GetComputeTaskManager() override
        {
            return computeTaskManager.Ptr();
        }

		virtual int RegisterWorldRenderPass(uint32_t shaderId) override
		{
			int passId;
			if (worldRenderPassIds.TryGetValue(shaderId, passId))
				return passId;
			int newId = worldRenderPassIds.Count();
			worldRenderPassIds[shaderId] = newId;
			return newId;
		}

		virtual HardwareRenderer * GetHardwareRenderer() override
		{
			return hardwareRenderer;
		}
        virtual RendererService* GetRendererService() override
        {
            return renderService.Ptr();
        }
        virtual void UpdateLightmap(LightmapSet& lightmapSet) override
        {
            if (level)
            {
                Wait();
                sceneRes->deviceLightmapSet = new DeviceLightmapSet();
                sceneRes->deviceLightmapSet->Init(hardwareRenderer, lightmapSet);
                for (auto proc : renderProcedures)
                    proc.Value->UpdateSceneResourceBinding(sceneRes.Ptr());
            }
        }
		RefPtr<ViewResource> cubemapRenderView;
		virtual void UpdateLightProbes() override
		{
			if (!level) return;
			LightProbeRenderer lpRenderer(this, renderService.Ptr(), lightProbeRenderProcedure, cubemapRenderView.Ptr());
			int lightProbeCount = 0;
			for (auto & actor : level->Actors)
			{
				if (actor.Value->GetEngineType() == EngineActorType::EnvMap)
				{
					auto envMapActor = dynamic_cast<EnvMapActor*>(actor.Value.Ptr());
					if (envMapActor->GetEnvMapId() != -1)
						lpRenderer.RenderLightProbe(sharedRes.envMapArray.Ptr(), envMapActor->GetEnvMapId(), level, envMapActor->GetPosition());
					lightProbeCount++;
				}
			}
			if (lightProbeCount == 0)
			{
				if (defaultEnvMapId == -1)
					defaultEnvMapId = sharedRes.AllocEnvMap();
				lpRenderer.RenderLightProbe(sharedRes.envMapArray.Ptr(), defaultEnvMapId, level, Vec3::Create(0.0f, 1000.0f, 0.0f));
			}
		}
        void TryLoadLightmap()
        {
            auto lightmapFile = level->LightmapFileName;
            if (lightmapFile.Length())
                lightmapFile = Engine::Instance()->FindFile(lightmapFile, ResourceType::Level);
            if (lightmapFile.Length() == 0)
                lightmapFile = Engine::Instance()->FindFile(Path::ReplaceExt(level->FileName, "lightmap"), ResourceType::Level);
            sceneRes->deviceLightmapSet = nullptr;
            if (lightmapFile.Length())
            {
                LightmapSet lightmapSet;
                lightmapSet.LoadFromFile(level, lightmapFile);
                if (lightmapSet.ActorLightmapIds.Count() != lightmapSet.Lightmaps.Count())
                {
                    return;
                }
                for (auto & lm : lightmapSet.Lightmaps)
                {
                    if (lm.Width != lm.Height || (1 << Math::Log2Ceil(lm.Width)) != lm.Width)
                        return;
                }
                sceneRes->deviceLightmapSet = new DeviceLightmapSet();
                sceneRes->deviceLightmapSet->Init(hardwareRenderer, lightmapSet);
            }
        }
		virtual void InitializeLevel(Level* pLevel) override
		{
			if (!pLevel) return;
			level = pLevel;
            TryLoadLightmap();

			defaultEnvMapId = -1;
            for (auto proc : renderProcedures)
            {
                proc.Value->UpdateSharedResourceBinding();
                proc.Value->UpdateSceneResourceBinding(sceneRes.Ptr());
            }
			UpdateLightProbes();
			RunRenderProcedure();
			RenderFrame();
			Wait();
			sharedRes.renderStats.Clear();
		}
		virtual RenderStat & GetStats() override
		{
			return sharedRes.renderStats;
		}

		struct DescriptorSetUpdate
		{
			DescriptorSet * descSet;
			int index;
			Buffer * buffer;
			int offset;
			int length;
		};

		virtual void RenderFrame() override
		{
			if (!level) return;
            static int frameId = 0;
            frameId++;
            sharedRes.renderStats.Divisor++;
			sharedRes.renderStats.NumMaterials = 0;
			sharedRes.renderStats.NumShaders = 0;
            
            RunRenderProcedure();
		}
		virtual RendererSharedResource * GetSharedResource() override
		{
			return &sharedRes;
		}
		virtual SceneResource * GetSceneResource() override
		{
			return sceneRes.Ptr();
		}
		virtual void Resize(int w, int h) override
		{
            Wait();
			mainView->Resize(w, h);
			Wait();
		}
		Texture2D * GetRenderedImage() override
		{
			if (currentRenderProcedure)
				return currentRenderProcedure->GetOutput()->Texture.Ptr();
			return nullptr;
		}
		virtual void DestroyContext() override
		{
			sharedRes.ResetEnvMapAllocation();
			sceneRes->Clear();
		}
	};

	Renderer* CreateRenderer(RenderAPI api)
	{
		return new RendererImpl(api);
	}
}