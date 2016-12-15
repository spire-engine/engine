#include "Engine.h"
#include "WorldRenderPass.h"
#include "PostRenderPass.h"
#include "Renderer.h"
#include "RenderPassRegistry.h"
#include "DirectionalLightActor.h"
#include "AtmosphereActor.h"
#include "CoreLib/Graphics/ViewFrustum.h"

namespace GameEngine
{
	class StandardViewUniforms
	{
	public:
		VectorMath::Matrix4 ViewTransform, ViewProjectionTransform, InvViewTransform, InvViewProjTransform;
		VectorMath::Vec3 CameraPos;
		float Time;
	};

	class LightUniforms
	{
	public:
		VectorMath::Vec3 lightDir; float pad0 = 0.0f;
		VectorMath::Vec3 lightColor; float pad1 = 0.0f;
		int numCascades = 0;
		int shadowMapId = -1;
		int pad2 = 0, pad3 = 0;
		VectorMath::Matrix4 lightMatrix[MaxShadowCascades];
		float zPlanes[MaxShadowCascades];
		LightUniforms()
		{
			lightDir.SetZero();
			lightColor.SetZero();
			for (int i = 0; i < MaxShadowCascades; i++)
				zPlanes[i] = 0.0f;
		}
	};

	class StandardRenderProcedure : public IRenderProcedure
	{
	private:
		bool deferred = false;
		RendererSharedResource * sharedRes = nullptr;

		WorldRenderPass * shadowRenderPass = nullptr;
		WorldRenderPass * forwardRenderPass = nullptr;
		WorldRenderPass * gBufferRenderPass = nullptr;
		PostRenderPass * atmospherePass = nullptr;
		PostRenderPass * deferredLightingPass = nullptr;

		RenderOutput * forwardBaseOutput = nullptr;
		RenderOutput * gBufferOutput = nullptr;
		RenderOutput * deferredLightingOutput = nullptr;
		StandardViewUniforms viewUniform;

		RenderPassInstance forwardBaseInstance;
		RenderPassInstance gBufferInstance;

		DrawableSink sink;

		List<DirectionalLightActor*> directionalLights;
		Array<StandardViewUniforms, 128> shadowMapViewUniforms;
		List<LightUniforms> lightingData;

		bool useAtmosphere = false;

	public:
		~StandardRenderProcedure()
		{
			if (forwardBaseOutput)
				sharedRes->DestroyRenderOutput(forwardBaseOutput);
			if (gBufferOutput)
				sharedRes->DestroyRenderOutput(gBufferOutput);
			if (deferredLightingOutput)
				sharedRes->DestroyRenderOutput(deferredLightingOutput);
		}
		virtual RenderTarget* GetOutput() override
		{
			if (useAtmosphere)
				return sharedRes->LoadSharedRenderTarget("litAtmosphereColor", StorageFormat::RGBA_8).Ptr();
			else
				return sharedRes->LoadSharedRenderTarget("litColor", StorageFormat::RGBA_8).Ptr();
		}
		virtual void Init(Renderer * renderer) override
		{
			sharedRes = renderer->GetSharedResource();
			deferred = Engine::Instance()->GetGraphicsSettings().UseDeferredRenderer;
			shadowRenderPass = CreateShadowRenderPass();
			renderer->RegisterWorldRenderPass(shadowRenderPass);
			if (deferred)
			{
				gBufferRenderPass = CreateGBufferRenderPass();
				renderer->RegisterWorldRenderPass(gBufferRenderPass);
				deferredLightingPass = CreateDeferredLightingPostRenderPass();
				renderer->RegisterPostRenderPass(deferredLightingPass);
				gBufferOutput = sharedRes->CreateRenderOutput(
					gBufferRenderPass->GetRenderTargetLayout(),
					sharedRes->LoadSharedRenderTarget("baseColorBuffer", StorageFormat::RGBA_8),
					sharedRes->LoadSharedRenderTarget("pbrBuffer", StorageFormat::RGBA_8),
					sharedRes->LoadSharedRenderTarget("normalBuffer", StorageFormat::RGB10_A2),
					sharedRes->LoadSharedRenderTarget("depthBuffer", StorageFormat::Depth24Stencil8)
				);
				gBufferInstance = gBufferRenderPass->CreateInstance(gBufferOutput, &viewUniform, sizeof(viewUniform));
			}
			else
			{
				forwardRenderPass = CreateForwardBaseRenderPass();
				renderer->RegisterWorldRenderPass(forwardRenderPass);
				forwardBaseOutput = sharedRes->CreateRenderOutput(
					forwardRenderPass->GetRenderTargetLayout(),
					sharedRes->LoadSharedRenderTarget("litColor", StorageFormat::RGBA_8),
					sharedRes->LoadSharedRenderTarget("depthBuffer", StorageFormat::Depth24Stencil8)
				);
				forwardBaseInstance = forwardRenderPass->CreateInstance(forwardBaseOutput, &viewUniform, sizeof(viewUniform));
			}

			atmospherePass = CreateAtmospherePostRenderPass();
			renderer->RegisterPostRenderPass(atmospherePass);
		}
		virtual void Run(CoreLib::List<RenderPassInstance>& renderPasses, CoreLib::List<PostRenderPass*> & postPasses, const RenderProcedureParameters & params) override
		{
			int w = 0, h = 0;
			if (forwardRenderPass)
			{
				forwardRenderPass->ResetInstancePool();
				forwardBaseOutput->GetSize(w, h);
			}
			else if (gBufferRenderPass)
			{
				gBufferRenderPass->ResetInstancePool();
				gBufferOutput->GetSize(w, h);
			}
			shadowRenderPass->ResetInstancePool();
			
			auto shadowMapRes = params.renderer->GetSharedResource()->shadowMapResources;
			shadowMapRes.Reset();
			lightingData.Clear();
			GetDrawablesParameter getDrawableParam;
			CameraActor * camera = nullptr;
			if (params.cameras.Count())
			{
				camera = params.cameras[0];
				viewUniform.CameraPos = camera->GetPosition();
				viewUniform.ViewTransform = camera->GetLocalTransform();
				getDrawableParam.CameraDir = camera->GetDirection();
				Matrix4 projMatrix;
				Matrix4::CreatePerspectiveMatrixFromViewAngle(projMatrix,
					camera->FOV, w / (float)h,
					camera->ZNear, camera->ZFar, ClipSpaceType::ZeroToOne);
				Matrix4::Multiply(viewUniform.ViewProjectionTransform, projMatrix, viewUniform.ViewTransform);
			}
			else
			{
				viewUniform.CameraPos = Vec3::Create(0.0f); 
				getDrawableParam.CameraDir = Vec3::Create(0.0f, 0.0f, -1.0f);
				Matrix4::CreateIdentityMatrix(viewUniform.ViewTransform);
				Matrix4::CreatePerspectiveMatrixFromViewAngle(viewUniform.ViewProjectionTransform,
					75.0f, w / (float)h, 40.0f, 40000.0f, ClipSpaceType::ZeroToOne);
			}
			viewUniform.ViewTransform.Inverse(viewUniform.InvViewTransform);
			viewUniform.ViewProjectionTransform.Inverse(viewUniform.InvViewProjTransform);
			viewUniform.Time = Engine::Instance()->GetTime();

			getDrawableParam.CameraPos = viewUniform.CameraPos;
			getDrawableParam.rendererService = params.rendererService;
			getDrawableParam.sink = &sink;
			
			sink.Clear();
			
			directionalLights.Clear();
			
			CoreLib::Graphics::BBox levelBounds;
			levelBounds.Init();

			for (auto & actor : params.level->Actors)
			{
				levelBounds.Union(actor.Value->Bounds);
				actor.Value->GetDrawables(getDrawableParam);
				auto actorType = actor.Value->GetEngineType();
				if (actorType == EngineActorType::Light)
				{
					auto light = dynamic_cast<LightActor*>(actor.Value.Ptr());
					if (light->lightType == LightType::Directional)
					{
						directionalLights.Add((DirectionalLightActor*)(light));
					}
				}
				else if (actorType == EngineActorType::Atmosphere)
				{
					useAtmosphere = true;
					auto atmosphere = dynamic_cast<AtmosphereActor*>(actor.Value.Ptr());
					atmosphere->Parameters.SunDir = atmosphere->Parameters.SunDir.Normalize();
					atmospherePass->SetParameters(&atmosphere->Parameters, sizeof(atmosphere->Parameters));
				}
			}
			if (camera)
			{
				int shadowMapSize = Engine::Instance()->GetGraphicsSettings().ShadowMapResolution;
				float zmin = camera->ZNear;
				float aspect = w / (float)h;
				shadowMapViewUniforms.Clear();
				for (auto dirLight : directionalLights)
				{
					LightUniforms lightData;
					lightData.lightColor = dirLight->Color;
					lightData.lightDir = dirLight->Direction;
					lightData.numCascades = dirLight->EnableCascadedShadows ? dirLight->NumShadowCascades : 0;
					float zmax = dirLight->ShadowDistance;
					if (dirLight->EnableCascadedShadows && dirLight->NumShadowCascades > 0 && dirLight->NumShadowCascades <= MaxShadowCascades)
					{
						int shadowMapStartId = shadowMapRes.AllocShadowMaps(dirLight->NumShadowCascades);
						lightData.shadowMapId = shadowMapStartId;
						if (shadowMapStartId != -1)
						{
							auto dirLightLocalTrans = dirLight->GetLocalTransform();
							Vec3 dirLightPos = Vec3::Create(dirLightLocalTrans.values[12], dirLightLocalTrans.values[13], dirLightLocalTrans.values[14]);
							for (int i = 0; i < dirLight->NumShadowCascades; i++)
							{
								StandardViewUniforms shadowMapView;
								Vec3 viewZ = dirLight->Direction;
								Vec3 viewX, viewY;
								GetOrthoVec(viewX, viewZ);
								viewY = Vec3::Cross(viewZ, viewX);
								shadowMapView.CameraPos = viewUniform.CameraPos;
								shadowMapView.Time = viewUniform.Time;
								Matrix4::CreateIdentityMatrix(shadowMapView.ViewTransform);
								shadowMapView.ViewTransform.m[0][0] = viewX.x; shadowMapView.ViewTransform.m[1][0] = viewX.y; shadowMapView.ViewTransform.m[2][0] = viewX.z;
								shadowMapView.ViewTransform.m[0][1] = viewY.x; shadowMapView.ViewTransform.m[1][1] = viewY.y; shadowMapView.ViewTransform.m[2][1] = viewY.z;
								shadowMapView.ViewTransform.m[0][2] = viewZ.x; shadowMapView.ViewTransform.m[1][2] = viewZ.y; shadowMapView.ViewTransform.m[2][2] = viewZ.z;
								float iOverN = (i+1) / (float)dirLight->NumShadowCascades;
								float zi = dirLight->TransitionFactor * zmin * pow(zmax / zmin, iOverN) + (1.0f - dirLight->TransitionFactor)*(zmin + (iOverN)*(zmax - zmin));
								lightData.zPlanes[i] = zi;
								auto frustum = camera->GetFrustum(aspect);
								auto verts = frustum.GetVertices(zmin, zi);
								float d1 = (verts[0] - verts[2]).Length2() * 0.25f;
								float d2 = (verts[4] - verts[6]).Length2() * 0.25f;
								float f = zi - zmin;
								float ti = Math::Min((d1 + d2 + f*f) / (2.0f*f), f);
								float t = zmin + ti;
								auto center = camera->GetPosition() + camera->GetDirection() * t;
								float radius = (verts[6] - center).Length();
								auto transformedCenter = shadowMapView.ViewTransform.TransformNormal(center);
								auto transformedCorner = transformedCenter - Vec3::Create(radius);
								float viewSize = radius * 2.0f;
								float texelSize = radius * 2.0f / shadowMapSize;

								transformedCorner.x = Math::FastFloor(transformedCorner.x / texelSize) * texelSize;
								transformedCorner.y = Math::FastFloor(transformedCorner.y / texelSize) * texelSize;
								transformedCorner.z = Math::FastFloor(transformedCorner.z / texelSize) * texelSize;
								Matrix4 projMatrix;
								Matrix4::CreateOrthoMatrix(projMatrix, transformedCorner.x, transformedCorner.x + viewSize,
									transformedCorner.y + viewSize, transformedCorner.y, -Vec3::Dot(dirLight->Direction, dirLightPos), 2000.0f, ClipSpaceType::ZeroToOne);

								Matrix4::Multiply(shadowMapView.ViewProjectionTransform, projMatrix, shadowMapView.ViewTransform);

								shadowMapView.ViewProjectionTransform.Inverse(shadowMapView.InvViewProjTransform);
								shadowMapView.ViewTransform.Inverse(shadowMapView.InvViewTransform);
								shadowMapViewUniforms.Add(shadowMapView);

								Matrix4 viewportMatrix;
								Matrix4::CreateIdentityMatrix(viewportMatrix);
								viewportMatrix.m[0][0] = 0.5f; viewportMatrix.m[3][0] = 0.5f;
								viewportMatrix.m[1][1] = 0.5f; viewportMatrix.m[3][1] = 0.5f;
								viewportMatrix.m[2][2] = 1.0f; viewportMatrix.m[3][2] = 0.0f;
								Matrix4::Multiply(lightData.lightMatrix[i], viewportMatrix, shadowMapView.ViewProjectionTransform);
								auto pass = shadowRenderPass->CreateInstance(shadowMapRes.shadowMapRenderOutputs[i + shadowMapStartId].Ptr(),
									&shadowMapViewUniforms[shadowMapViewUniforms.Count() - 1],
									sizeof(StandardViewUniforms));
								pass.RecordCommandBuffer(From(sink.GetDrawables()));
								renderPasses.Add(pass);
							}
						}
					}
					lightingData.Add(lightData);
				}
			}

			sharedRes->lightUniformBuffer->SetData(0, lightingData.Buffer(), Math::Min((int)sizeof(LightUniforms)*lightingData.Count(), MaxLightBufferSize));
			
			if (deferred)
			{
				gBufferInstance.RecordCommandBuffer(From(sink.GetDrawables()));
				renderPasses.Add(gBufferInstance);
				postPasses.Add(deferredLightingPass);
			}
			else
			{
				forwardBaseInstance.RecordCommandBuffer(From(sink.GetDrawables()));
				renderPasses.Add(forwardBaseInstance);
			}
			if (useAtmosphere)
			{
				postPasses.Add(atmospherePass);
			}
		}
	};

	IRenderProcedure * CreateStandardRenderProcedure()
	{
		return new StandardRenderProcedure();
	}
}