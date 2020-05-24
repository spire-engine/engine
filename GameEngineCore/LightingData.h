#ifndef GAME_ENGINE_LIGHTING_DATA_H
#define GAME_ENGINE_LIGHTING_DATA_H

#include "CoreLib/VectorMath.h"
#include "CoreLib/Basic.h"
#include "HardwareRenderer.h"
#include "PipelineContext.h"
#include "Level.h"
#include "RenderProcedure.h"
#include "StandardViewUniforms.h"

namespace GameEngine
{
	const unsigned short GpuLightType_Point = 0;
	const unsigned short GpuLightType_Directional = 1;
	const unsigned short GpuLightType_Spot = 2;

	struct GpuLightData
	{
		unsigned short lightType;
		unsigned short shaderMapId;
		float radius;
		float startAngle;
		float endAngle;
		VectorMath::Vec3 position;
		unsigned int direction;
		VectorMath::Vec3 color;
        float padding;
		VectorMath::Matrix4 lightMatrix;
        float padding2[4];
	};

	struct GpuLightProbeData
	{
		VectorMath::Vec3 position;
		float radius;
		VectorMath::Vec3 tintColor;
		int envMapId;
	};

	struct LightingUniform
	{
        float zPlanes[MaxShadowCascades];
		VectorMath::Matrix4 lightMatrix[MaxShadowCascades];
        VectorMath::Vec3 lightColor; float padding0;
		VectorMath::Vec3 lightDir; int sunLightEnabled = 0;
		int shadowMapId = -1;
		int numCascades = 0;
		int lightCount = 0, lightProbeCount = 0;
		VectorMath::Vec3 ambient = VectorMath::Vec3::Create(0.2f);
        float padding1;
        int lightListTilesX, lightListTilesY, lightListSizePerTile;
	};

	class LightingEnvironment
	{
	private:
		bool useEnvMap = true;
		CoreLib::RefPtr<TextureCubeArray> emptyEnvMapArray;
        CoreLib::RefPtr<Texture2DArray> emptyLightmapArray;
		void AddShadowPass(HardwareRenderer* hw, WorldRenderPass * shadowRenderPass, DrawableSink * sink, ShadowMapResource & shadowMapRes, int shadowMapId,
			StandardViewUniforms & shadowMapView, int & shadowMapViewInstancePtr);
	public:
		DeviceMemory * uniformMemory;
		ModuleInstance moduleInstance;
		CoreLib::List<GpuLightData> lights;
		CoreLib::List<GpuLightProbeData> lightProbes;
		CoreLib::List<Texture*> lightProbeTextures;
		CoreLib::List<CoreLib::RefPtr<Texture2D>> shadowMaps;
		CoreLib::RefPtr<Buffer> lightBuffer, lightProbeBuffer;
		CoreLib::List<ModuleInstance> shadowViewInstances;
		CoreLib::List<Drawable*> drawableBuffer, reorderBuffer;
        CoreLib::RefPtr<Buffer> tiledLightListBufffer;
        int tiledLightListBufferSize = 0;
        DeviceLightmapSet * deviceLightmapSet = nullptr;
		RendererSharedResource * sharedRes;
		void* lightBufferPtr, *lightProbeBufferPtr;
		int lightBufferSize, lightProbeBufferSize;
		LightingUniform uniformData;
		void GatherInfo(HardwareRenderer* hw, DrawableSink * sink, const RenderProcedureParameters & params, int w, int h, StandardViewUniforms & cameraView, WorldRenderPass * shadowPass);
		void Init(RendererSharedResource & sharedRes, DeviceMemory * uniformMemory, bool pUseEnvMap);
		void UpdateSharedResourceBinding();
        void UpdateSceneResourceBinding(SceneResource* sceneRes);
	};
}

#endif