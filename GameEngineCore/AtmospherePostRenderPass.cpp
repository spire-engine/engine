#include "PostRenderPass.h"
#include "Mesh.h"
#include "Engine.h"
#include "CoreLib/LibIO.h"
#include "Atmosphere.h"
#include "CoreLib/Graphics/TextureFile.h"
#include <assert.h>

using namespace CoreLib;
using namespace CoreLib::IO;

namespace GameEngine
{
	using namespace VectorMath;

	class AtmospherePostRenderPass : public PostRenderPass
	{
	protected:
		RefPtr<RenderTarget> colorBuffer, depthBuffer, colorOutBuffer;
		RefPtr<Buffer> parameterBuffer;
		
		RefPtr<Texture2D> transmittanceTex, irradianceTex;
		RefPtr<Texture3D> inscatterTex;

		RefPtr<DescriptorSet> atmosphereDesc;
		bool isValid = true;
	public:
		virtual void Create(Renderer * renderer) override
		{
			PostRenderPass::Create(renderer);
			parameterBuffer = hwRenderer->CreateBuffer(BufferUsage::UniformBuffer, sizeof(AtmosphereParameters));
			String irradianceDataFile = Engine::Instance()->FindFile("Atmosphere/irradiance.raw", ResourceType::Material);
			String inscatterDataFile = Engine::Instance()->FindFile("Atmosphere/inscatter.raw", ResourceType::Material);
			String transmittanceDataFile = Engine::Instance()->FindFile("Atmosphere/transmittance.raw", ResourceType::Material);
			if (irradianceDataFile.Length() == 0 || inscatterDataFile.Length() == 0 || transmittanceDataFile.Length() == 0)
			{
				Print("missing atmosphere precompute data.\n");
				isValid = false;
				return;
			}
			else
			{
				// load precomputed textures
				{
					BinaryReader reader(new FileStream(irradianceDataFile));
					List<float> irradianceData;
					irradianceData.SetSize(16 * 64 * 3);
					reader.Read(irradianceData.Buffer(), irradianceData.Count());
					irradianceTex = hwRenderer->CreateTexture2D("AtmospherePostRenderPass::irradianceTex", TextureUsage::Sampled, 64, 16, 1, StorageFormat::RGBA_F16);
					List<char> irradianceData4 = Graphics::TranslateThreeChannelTextureFormat((char*)irradianceData.Buffer(), 16 * 64, sizeof(float));
					irradianceTex->SetData(64, 16, 1, DataType::Float4, irradianceData4.Buffer());
				}
				{
					BinaryReader reader(new FileStream(inscatterDataFile));
					List<float> inscatterData;
					const int res = 64;
					const int nr = res / 2;
					const int nv = res * 2;
					const int nb = res / 2;
					const int na = 8;
					inscatterData.SetSize(nr * nv * nb * na * 4);
					reader.Read(inscatterData.Buffer(), inscatterData.Count());
					inscatterTex = hwRenderer->CreateTexture3D("AtmospherePostRenderPass::inscatterTex", TextureUsage::Sampled, na*nb, nv, nr, 1, StorageFormat::RGBA_F16);
					inscatterTex->SetData(0, 0, 0, 0, na*nb, nv, nr, DataType::Float4, inscatterData.Buffer());
				}
				{
					BinaryReader reader(new FileStream(transmittanceDataFile));
					List<float> transmittanceData;
					transmittanceData.SetSize(256 * 64 * 3);
					reader.Read(transmittanceData.Buffer(), transmittanceData.Count());
					transmittanceTex = hwRenderer->CreateTexture2D("AtmospherePostRenderPass::transmittanceTex", TextureUsage::Sampled, 256, 64, 1, StorageFormat::RGBA_F16);
					List<char> irradianceData4 = Graphics::TranslateThreeChannelTextureFormat((char*)transmittanceData.Buffer(), 256 * 64, sizeof(float));
					transmittanceTex->SetData(0, 256, 64, 1, DataType::Float4, irradianceData4.Buffer());
				}
			}

			// initialize parameter buffer with default params
			AtmosphereParameters defaultParams;
			defaultParams.SunDir = Vec3::Create(1.0f, 1.0f, 0.5f).Normalize();
			parameterBuffer->SetData(&defaultParams, sizeof(defaultParams));
		}
		virtual void AcquireRenderTargets() override
		{
			colorBuffer = viewRes->LoadSharedRenderTarget(sources[0].Name, sources[0].Format);
			depthBuffer = viewRes->LoadSharedRenderTarget(sources[1].Name, sources[1].Format);
			colorOutBuffer = viewRes->LoadSharedRenderTarget(sources[2].Name, sources[2].Format);
		}
		virtual void SetupPipelineBindingLayout(PipelineBuilder * pipelineBuilder, List<AttachmentLayout> & renderTargets) override
		{
			renderTargets.Add(AttachmentLayout(TextureUsage::ColorAttachment, StorageFormat::RGBA_F16));
			pipelineBuilder->SetDebugName("atmosphere");

			atmosphereDesc = hwRenderer->CreateDescriptorSet(descLayouts[0].Ptr());
		}
		virtual void UpdateDescriptorSetBinding(SharedModuleInstances sharedModules, DescriptorSetBindings & binding) override
		{
			binding.Bind(0, atmosphereDesc.Ptr());
			binding.Bind(1, sharedModules.View->GetCurrentDescriptorSet());
		}
		virtual void UpdateRenderAttachments(RenderAttachments & attachments) override
		{
			if (!colorBuffer->Texture)
				return;

			atmosphereDesc->BeginUpdate();
			atmosphereDesc->Update(0, parameterBuffer.Ptr());
			atmosphereDesc->Update(1, colorBuffer->Texture.Ptr(), TextureAspect::Color);
			atmosphereDesc->Update(2, depthBuffer->Texture.Ptr(), TextureAspect::Depth);
			atmosphereDesc->Update(3, transmittanceTex.Ptr(), TextureAspect::Color);
			atmosphereDesc->Update(4, irradianceTex.Ptr(), TextureAspect::Color);
			atmosphereDesc->Update(5, inscatterTex.Ptr(), TextureAspect::Color);
			atmosphereDesc->Update(6, sharedRes->linearSampler.Ptr());
			atmosphereDesc->Update(7, sharedRes->nearestSampler.Ptr());
			atmosphereDesc->EndUpdate();

			attachments.SetAttachment(0, colorOutBuffer->Texture.Ptr());
		}
		virtual String GetShaderFileName() override
		{
			return "Atmosphere.slang";
		}
		virtual const char * GetName() override
		{
			return "Atmosphere";
		}
		virtual void SetParameters(void * data, int count) override
		{
			assert(count == sizeof(AtmosphereParameters));
			parameterBuffer->SetData(data, count);
		}
	public:
		AtmospherePostRenderPass(ViewResource * viewRes)
			: PostRenderPass(viewRes)
		{}
	};

	PostRenderPass * CreateAtmospherePostRenderPass(ViewResource * viewRes)
	{
		return new AtmospherePostRenderPass(viewRes);
	}
}