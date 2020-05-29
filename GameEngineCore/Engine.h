#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "CoreLib/Basic.h"
#include "CoreLib/PerformanceCounter.h"
#include "Level.h"
#include "CoreLib/Tokenizer.h"
#include "InputDispatcher.h"
#include "CoreLib/LibUI/LibUI.h"
#include "GraphicsSettings.h"
#include "DrawCallStatForm.h"
#include "LevelEditor.h"
#include "HardwareRenderer.h"
#include "OS.h"
#include "UISystemBase.h"
#include "VideoEncoder.h"
#include "ShaderCompiler.h"
#include "DebugGraphics.h"
#include "ComputeTaskManager.h"

namespace GameEngine
{
    struct AppLaunchParameters
    {
        bool EnableVideoCapture = false;
        bool DumpRenderStats = false;
        CoreLib::String RenderStatsDumpFileName;
        CoreLib::String Directory;
        float Length = 10.0f;
        int FramesPerSecond = 30;
        int RunForFrames = 0; // run for this many frames and then terminate
		bool HeadlessMode = false;
    };
	class EngineInitArguments
	{
	public:
		RenderAPI API;
        bool NoConsole = false;
		int Width = 400, Height = 400;
		int GpuId = 0;
		bool UseSoftwareRenderer = false;
		bool RecompileShaders = false;
		CoreLib::String GameDirectory, EngineDirectory, StartupLevelName;
        AppLaunchParameters LaunchParams;
		CoreLib::RefPtr<LevelEditor> Editor;
	};
	enum class EngineThread
	{
		GameLogic, Rendering
	};
	enum class ResourceType
	{
		Font,
		Mesh, Shader, Level, Texture, Material, Landscape, Animation, Settings, ShaderCache, ExtTools
	};
	enum class TimingMode
	{
		Natural, Fixed
	};
	enum class EngineMode
	{
		Normal, Editor
	};

	class Engine
	{
	private:
		static Engine * instance;
        AppLaunchParameters params;
		TimingMode timingMode = TimingMode::Natural;
		float fixedFrameDuration = 1.0f / 30.0f;
		unsigned int frameCounter = 0;
		bool inDataTransfer = false;
		bool isRunning = false;
		bool useSoftwareRenderer = false;
		TargetShadingLanguage targetShadingLanguage = TargetShadingLanguage::SPIRV;
		WindowBounds currentViewport;
		GraphicsSettings graphicsSettings;
		CoreLib::String levelToLoad;
		CoreLib::List<CoreLib::List<CoreLib::RefPtr<Fence>>> fencePool;
		CoreLib::List<CoreLib::List<Fence*>> syncFences;
	private:
		bool enableInput = true;
		CoreLib::Diagnostics::TimePoint startTime, lastGameLogicTime, lastRenderingTime;
		float gameLogicTimeDelta = 0.0f, renderingTimeDelta = 0.0f;
		CoreLib::String gameDir, engineDir;
		CoreLib::EnumerableDictionary<CoreLib::String, CoreLib::Func<Actor*>> actorClassRegistry;
		CoreLib::RefPtr<Level> level;
		CoreLib::RefPtr<Renderer> renderer;
		CoreLib::RefPtr<InputDispatcher> inputDispatcher;
		CoreLib::RefPtr<LevelEditor> levelEditor;
        CoreLib::RefPtr<SystemWindow> mainWindow;
        CoreLib::RefPtr<IVideoEncoder> videoEncoder;
        CoreLib::RefPtr<CoreLib::IO::Stream> videoEncodingStream;
        CoreLib::RefPtr<IShaderCompiler> shaderCompiler;
        CoreLib::RefPtr<DebugGraphics> debugGraphics;
		EngineMode engineMode = EngineMode::Normal;
		CoreLib::Array<RenderStat, 16> renderStats;
		GraphicsUI::CommandForm * uiCommandForm = nullptr;
		DrawCallStatForm * drawCallStatForm = nullptr;
		CoreLib::RefPtr<UISystemBase> uiSystemInterface;
        void MainLoop();
		bool OnToggleConsoleAction(const CoreLib::String & actionName, ActionInput input);
		void Resize();
		Engine() {};
		void InternalInit(const EngineInitArguments & args);
		~Engine();
	public:
		void RefreshUI();
		GraphicsSettings & GetGraphicsSettings()
		{
			return graphicsSettings;
		}
		void SaveGraphicsSettings();
		void SetTimingMode(TimingMode mode)
		{
			timingMode = mode;
		}
		// set fixed frame duration when TimingMode is Fixed
		void SetFrameDuration(float duration)
		{
			fixedFrameDuration = duration;
		}
		float GetTimeDelta(EngineThread thread);
		float GetTime()
		{
			if (timingMode == TimingMode::Natural)
				return CoreLib::Diagnostics::PerformanceCounter::EndSeconds(startTime);
			else
				return frameCounter * fixedFrameDuration;
		}
		bool IsRunning()
		{
			return isRunning;
		}
		bool UseSoftwareRenderer()
		{
			return useSoftwareRenderer;
		}
		int GetFrameId()
        {
            return frameCounter;
        }
        GraphicsUI::ISystemInterface* GetUISystemInterface()
        {
            return uiSystemInterface.Ptr();
        }
        
		Level * GetLevel()
		{
			return level.Ptr();
		}
		Renderer * GetRenderer()
		{
			return renderer.Ptr();
		}
		InputDispatcher * GetInputDispatcher()
		{
			return inputDispatcher.Ptr();
		}
        SystemWindow * GetMainWindow()
        {
            return mainWindow.Ptr();
        }
		GraphicsUI::UIEntry * GetUiEntry()
		{
			return mainWindow->GetUIEntry();
		}
		Texture2D * GetRenderResult(bool withUI);
		CoreLib::ArrayView<RenderStat> GetRenderStats()
		{
			return renderStats.GetArrayView();
		}
		void SetTargetShadingLanguage(TargetShadingLanguage lang)
		{
			targetShadingLanguage = lang;
		}
        TargetShadingLanguage GetTargetShadingLanguage()
        {
            return targetShadingLanguage;
        }
	public:
		Actor * CreateActor(const CoreLib::String & name);
		void RegisterActorClass(const CoreLib::String &name, const CoreLib::Func<Actor*> & actorCreator);
		bool IsRegisteredActorClass(const CoreLib::String &name);
		CoreLib::List<CoreLib::String> GetRegisteredActorClasses();
		void LoadLevel(const CoreLib::String & fileName);
		void LoadLevelFromText(const CoreLib::String & text);
		Level* NewLevel();
        GraphicsUI::IFont* LoadFont(Font f);
		void UpdateLightProbes();
		CoreLib::ObjPtr<Actor> ParseActor(GameEngine::Level * level, CoreLib::Text::TokenReader & parser);
	public:
		WindowBounds GetCurrentViewport()
		{
			return currentViewport;
		}
		Ray GetRayFromMousePosition(int x, int y);
	public:
		CoreLib::String FindFile(const CoreLib::String & fileName, ResourceType type);
		CoreLib::String GetDirectory(bool useEngineDir, ResourceType type);
	public:
		void Tick();
		void EnableInput(bool value);
		void OnCommand(CoreLib::String command);
		void UseEditor(LevelEditor * editor);
		LevelEditor * GetEditor()
		{
			return levelEditor.Ptr();
		}
		EngineMode GetEngineMode()
		{
			return engineMode;
		}
        void DoEvents();
		void SetEngineMode(EngineMode newMode);
        SystemWindow * CreateSystemWindow(int log2BufferSize = 20);
	public:
		int GpuId = 0;
		bool RecompileShaders = false;
		static Engine * Instance()
		{
			if (!instance)
				instance = new Engine();
			return instance;
		}
		static void Init(const EngineInitArguments & args);
        static void Run();
		static void Destroy();
        static DebugGraphics* GetDebugGraphics()
        {
            return Instance()->debugGraphics.Ptr();
        }
        static ComputeTaskManager* GetComputeTaskManager()
        {
            return Instance()->renderer->GetComputeTaskManager();
        }
        static IShaderCompiler* GetShaderCompiler()
        {
            if (!Instance()->shaderCompiler)
                Instance()->shaderCompiler = CreateShaderCompiler();
            return Instance()->shaderCompiler.Ptr();
        }
		template<typename ...Args>
		static void Print(const char * message, Args... args)
		{
			static char printBuffer[32768];
			static CoreLib::Diagnostics::TimePoint lastUIUpdate;
#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
			snprintf(printBuffer, 32768, message, args...);
#if __GNUC__
#pragma GCC diagnostic pop
#endif
			if (instance && instance->uiCommandForm && !instance->params.HeadlessMode)
			{
				instance->uiCommandForm->Write(printBuffer);
				float timeElapsed = CoreLib::Diagnostics::PerformanceCounter::EndSeconds(lastUIUpdate);
				if (timeElapsed > 0.2f)
				{
					instance->RefreshUI();
					lastUIUpdate = CoreLib::Diagnostics::PerformanceCounter::Start();
				}
			}
			else
			{
#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
				printf(message, args...);
#if __GNUC__
#pragma GCC diagnostic pop
#endif
			}
			if (!instance->params.HeadlessMode)
            	OsApplication::DebugPrint(printBuffer);
		}
		static void SaveImage(Texture2D * texture, CoreLib::String fileName);
	};

	template<typename ...Args>
	void Print(const char * message, Args... args)
	{
		Engine::Print(message, args...);
	}


}

#define EM(x) GraphicsUI::emToPixel(x)

#endif