#ifndef HARDWARE_RENDERER_H
#define HARDWARE_RENDERER_H

#include "CoreLib/Basic.h"
#include "CoreLib/VectorMath.h"
#include "OS.h"

namespace GameEngine
{
	/*
	 * Exceptions
	 */
	class HardwareRendererException : public CoreLib::Exception
	{
	public:
		HardwareRendererException()
		{}
		HardwareRendererException(const CoreLib::String & message)
			: CoreLib::Exception(message)
		{
		}
	};

	/*
	 * Enum Classes
	 */
	enum class BlendMode
	{
		Replace, Add, AlphaBlend
	};

	enum class BufferAccess
	{
		//TODO: Add more types
		Read, Write, ReadWrite, ReadWritePersistent
	};

	enum BufferStorageFlag
	{
		DynamicStorage = 0x1,
		MapRead = 0x2,
		MapWrite = 0x4,
		MapPersistent = 0x8,
		MapCoherent = 0xf,
		ClientStorage = 0x10
	};

	enum class BufferType
	{
		ArrayBuffer, ElementBuffer, UniformBuffer, StorageBuffer
	};

	enum class BufferUsage
	{
		ArrayBuffer, IndexBuffer, UniformBuffer, StorageBuffer
	};

	enum class CompareFunc
	{
		Disabled, Greater, GreaterEqual, Less, LessEqual, Equal, NotEqual, Always, Never
	};

	enum class CullMode
	{
		Disabled, CullBackFace, CullFrontFace
	};

	enum class BindingType
	{
		Unused, UniformBuffer, StorageBuffer, RWStorageBuffer, Texture, StorageTexture, Sampler
	};

	enum class DataType
	{
		Byte = 0x10, Byte2 = 0x11, Byte3 = 0x12, Byte4 = 0x13,
		Char = 0x60, Char2 = 0x61, Char3 = 0x62, Char4 = 0x63,
		Short = 0x20, Short2 = 0x21, Short3 = 0x22, Short4 = 0x23,
		UShort = 0x70, UShort2 = 0x71, UShort3 = 0x72, UShort4 = 0x73,
		Half = 0x90, Half2 = 0x91, Half3 = 0x92, Half4 = 0x93,
		Int = 0x40, Int2 = 0x41, Int3 = 0x42, Int4 = 0x43,
		UInt = 0x100,
		Float = 0x50, Float2 = 0x51, Float3 = 0x52, Float4 = 0x53,
		UInt4_10_10_10_2 = 0x83
	};

	enum class FeedbackStorageMode
	{
		Interleaved, Split
	};

	enum class PrimitiveType
	{
		Points = 0, Lines = 1, LineStrips = 3, Triangles = 4, TriangleStrips = 5, Patches = 14
	};

	enum class ShaderDataType
	{
		Float, Int, Float2, Int2, Float3, Int3, Float4, Int4,
		Float3x3, Float4x4, Sampler2D, SamplerCube, Sampler2DMS, SamplerBuffer
	};

	enum class ShaderType
	{
		VertexShader, FragmentShader, HullShader, DomainShader, ComputeShader
	};

	enum class StencilOp : char
	{
		Keep, Zero, Replace, Increment, IncrementWrap, Decrement, DecrementWrap, Invert
	};

	enum class StorageFormat
	{
		Invalid = -1,
		R_8, R_I8, R_16, R_I16, Int32_Raw,
		R_F16, R_F32,
		RG_8, RG_16, RG_I8, RG_I16, RG_I32_Raw,
		RG_F16, RG_F32,
		RGBA_8, RGBA_8_SRGB, RGBA_I8, RGBA_16, RGBA_I16, RGBA_I32_Raw,
		RGBA_F16, RGBA_F32,
		R11F_G11F_B10F, RGB10_A2,
		Depth24, Depth32, Depth24Stencil8,
        RGBA_Compressed,
        BC1,
        BC5,
        BC3,
        BC1_SRGB,
        BC6H
	};

	inline bool isDepthFormat(StorageFormat format)
	{
		switch (format)
		{
		case StorageFormat::Depth24:
		case StorageFormat::Depth24Stencil8:
		case StorageFormat::Depth32:
			return true;
		default:
			return false;
		}
	}

	enum class TextureCubeFace
	{
		PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ
	};

	enum class TextureFilter
	{
		Nearest, Linear, Trilinear, Anisotropic4x, Anisotropic8x, Anisotropic16x
	};

    enum class TextureUsage
    {
        Unused = 0x0,
        Sampled = 0x1,
        Storage = 0x10,
        SampledStorage = 0x11,
		ColorAttachment = 0x2,
		SampledColorAttachment = 0x2 | 0x1,
		DepthAttachment = 0x4,
		SampledDepthAttachment = 0x4 | 0x1,
		StencilAttachment = 0x8,
		SampledStencilAttachment = 0x8 | 0x1,
		DepthStencilAttachment = 0x8 | 0x4,
		SampledDepthStencilAttachment = 0x8 | 0x4 | 0x1,
	};

	inline constexpr TextureUsage operator&(TextureUsage lhs, TextureUsage rhs)
	{
		return static_cast<TextureUsage>(static_cast<int>(lhs) & static_cast<int>(rhs));
	}

	inline constexpr TextureUsage operator|(TextureUsage lhs, TextureUsage rhs)
	{
		return static_cast<TextureUsage>(static_cast<int>(lhs) | static_cast<int>(rhs));
	}

	inline constexpr bool operator!(TextureUsage x)
	{
		return x == TextureUsage::Unused;
	}

	struct Viewport
    {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, minZ = 0.0f, maxZ = 1.0F;
        Viewport() = default;
        Viewport(int _x, int _y, int _w, int _h) : x((float)_x), y((float)_y), w((float)_w), h((float)_h)
        {
        }
        Viewport(float _x, float _y, float _w, float _h) : x(_x), y(_y), w(_w), h(_h)
        {
        }
        bool operator==(const Viewport &vp)
        {
            return x == vp.x && y == vp.y && w == vp.w && h == vp.h && minZ == vp.minZ && maxZ == vp.maxZ;
        }
        bool operator!=(const Viewport &vp)
        {
            return !this->operator==(vp);
        }
    };

	struct BufferStructureInfo
	{
		int StructureStride = 0;
		int NumElements = 0;
        BufferStructureInfo() = default;
        BufferStructureInfo(int stride, int numElements)
        {
			StructureStride = stride;
            NumElements = numElements;
        }
	};

	struct AttachmentLayout
	{
		TextureUsage Usage;
		StorageFormat ImageFormat;
		AttachmentLayout() {}
		AttachmentLayout(TextureUsage usage, StorageFormat format)
		{
			Usage = usage;
			ImageFormat = format;
		}
	};

	enum class WrapMode
	{
		Repeat, Clamp, Mirror
	};

	enum class LoadOp
	{
		Load, Clear, DontCare
	};

	enum class StoreOp
	{
		Store, DontCare
	};

	// Helper Functions
	// Returns size in bytes of a StorageFormat
	inline int StorageFormatSize(StorageFormat format)
	{
		switch (format)
		{
		case StorageFormat::R_8:
		case StorageFormat::R_I8:
			return 1;
		case StorageFormat::RG_I8:
		case StorageFormat::RG_8:
		case StorageFormat::R_F16:
			return 2;
		case StorageFormat::RGBA_8:
		case StorageFormat::RGBA_I8:
		case StorageFormat::RG_16:
		case StorageFormat::RG_F16:
		case StorageFormat::RG_I16:
		case StorageFormat::Int32_Raw:
		case StorageFormat::R_F32:
		case StorageFormat::R11F_G11F_B10F:
		case StorageFormat::RGB10_A2:
		case StorageFormat::Depth32:
		case StorageFormat::Depth24Stencil8:
			return 4;
		case StorageFormat::RGBA_I16:
		case StorageFormat::RGBA_F16:
		case StorageFormat::RGBA_16:
		case StorageFormat::RG_I32_Raw:
		case StorageFormat::RG_F32:
			return 8;
		case StorageFormat::RGBA_I32_Raw:
		case StorageFormat::RGBA_F32:
			return 16;
		case StorageFormat::BC1:
		case StorageFormat::BC5:
        case StorageFormat::BC3:
		case StorageFormat::RGBA_Compressed:
		default: throw HardwareRendererException("Unsupported storage format.");
		}
	}
	// Returns size in bytes of a DataType
	inline int DataTypeSize(DataType type)
	{
		switch (type)
		{
		case DataType::Byte:
		case DataType::Char:
			return 1;
		case DataType::Byte2:
		case DataType::Char2:
		case DataType::Short:
		case DataType::UShort:
		case DataType::Half:
			return 2;
		case DataType::Byte3:
		case DataType::Char3:
			return 3;
		case DataType::Byte4:
		case DataType::Char4:
		case DataType::Short2:
		case DataType::UShort2:
		case DataType::Half2:
		case DataType::Int:
		case DataType::UInt:
		case DataType::Float:
		case DataType::UInt4_10_10_10_2:
			return 4;
		case DataType::Short3:
		case DataType::UShort3:
		case DataType::Half3:
			return 6;
		case DataType::Short4:
		case DataType::UShort4:
		case DataType::Half4:
		case DataType::Int2:
		case DataType::Float2:
			return 8;
		case DataType::Int3:
		case DataType::Float3:
			return 12;
		case DataType::Int4:
		case DataType::Float4:
			return 16;
		default:
			throw HardwareRendererException("Unsupported data type.");
		}
	}

	inline DataType GetDataTypeElementType(DataType type)
	{
		return (DataType)((int)(type) & (~3));
	}

	// Returns number of elements for a specific DataType
	inline int NumDataTypeElems(DataType type)
	{
		switch (type)
		{
		case DataType::Byte:
		case DataType::Char:
		case DataType::Short:
		case DataType::UShort:
		case DataType::Half:
		case DataType::Int:
		case DataType::UInt:
		case DataType::Float:
			return 1;
		case DataType::Byte2:
		case DataType::Char2:
		case DataType::Short2:
		case DataType::UShort2:
		case DataType::Half2:
		case DataType::Int2:
		case DataType::Float2:
			return 2;
		case DataType::Byte3:
		case DataType::Char3:
		case DataType::Short3:
		case DataType::UShort3:
		case DataType::Half3:
		case DataType::Int3:
		case DataType::Float3:
			return 3;
		case DataType::Byte4:
		case DataType::Char4:
		case DataType::Short4:
		case DataType::UShort4:
		case DataType::Half4:
		case DataType::Int4:
		case DataType::UInt4_10_10_10_2:
		case DataType::Float4:
			return 4;
		default:
			throw HardwareRendererException("Unsupported data type.");
		}
	}

	/*
	* ???
	*/
	class VertexAttributeDesc
	{
	public:
        CoreLib::String Semantic;
        int SemanticIndex;
		DataType Type;
		int Normalized : 1;
		int StartOffset : 31;
		int Location;
		VertexAttributeDesc()
		{
			Location = -1;
		}
		VertexAttributeDesc(DataType type, int normalized, int offset, int location, CoreLib::String semantic, int semanticIndex)
		{
			this->Type = type;
			this->Normalized = normalized;
			this->StartOffset = offset;
			this->Location = location;
            this->Semantic = semantic;
            this->SemanticIndex = semanticIndex;
		}
	};
	
	class VertexFormat
	{
	public:
		CoreLib::List<VertexAttributeDesc> Attributes;
		inline int Size()
		{
			if (Attributes.Count())
				return Attributes.Last().StartOffset + DataTypeSize(Attributes.Last().Type);
			return 0;
		}
	};

	/*
	 * Object Classes
	 */
	class Buffer : public CoreLib::RefObject
	{
	protected:
		Buffer() {};
	public:
		virtual void SetDataAsync(int offset, void * data, int size) = 0;
		virtual void SetData(int offset, void* data, int size) = 0;
		virtual void SetData(void* data, int size) = 0;
		virtual void GetData(void * buffer, int offset, int size) = 0;
		virtual int GetSize() = 0;
		virtual void* Map(int offset, int size) = 0;
		virtual void* Map() = 0;
		virtual void Flush(int offset, int size) = 0;
		virtual void Flush() = 0;
		virtual void Unmap() = 0;
	};

	enum class TextureAspect
	{
		Color, Depth, Stencil
	};

	class Texture : public CoreLib::RefObject
	{
	protected:
		Texture() {};
    public:
        virtual bool IsDepthStencilFormat() = 0;
		virtual void* GetInternalPtr() = 0;
	};

	class Texture2D : public Texture
	{
	protected:
		Texture2D() {};
	public:
		virtual void GetSize(int& width, int& height) = 0;
		virtual void SetData(int level, int width, int height, int samples, DataType inputType, void* data) = 0;
		virtual void SetData(int width, int height, int samples, DataType inputType, void* data) = 0;
		virtual void GetData(int mipLevel, void* data, int bufSize) = 0;
		virtual void BuildMipmaps() = 0;
	};

	class Texture2DArray : public Texture
	{
	protected:
		Texture2DArray() {};
	public:
		virtual void GetSize(int &width, int &height, int &layers) = 0;
		virtual void SetData(int mipLevel, int xOffset, int yOffset, int layerOffset, int width, int height, int layerCount, DataType inputType, void * data) = 0;
		virtual void BuildMipmaps() = 0;
	};

	class Texture3D : public Texture
	{
	protected:
		Texture3D() {};
	public:
		virtual void GetSize(int &width, int &height, int &depth) = 0;
		virtual void SetData(int mipLevel, int xOffset, int yOffset, int zOffset, int width, int height, int depth, DataType inputType, void * data) = 0;
	};

	class TextureCube : public Texture
	{
	protected:
		TextureCube() {};
	public:
		virtual void GetSize(int & size) = 0;
		virtual void SetData(int mipLevel, int xOffset, int yOffset, int layerOffset, int width, int height, int layerCount, DataType inputType, void* data) = 0;
	};

	class TextureCubeArray : public Texture
	{
	protected:
		TextureCubeArray() {}
	public:
		virtual void GetSize(int & size, int & layerCount) = 0;
		virtual void SetData(int mipLevel, int xOffset, int yOffset, int layerOffset, int width, int height, int layerCount, DataType inputType, void* data) = 0;
	};

	class TextureSampler : public CoreLib::RefObject
	{
	protected:
		TextureSampler() {};
	public:
		virtual TextureFilter GetFilter() = 0;
		virtual void SetFilter(TextureFilter filter) = 0;
		virtual WrapMode GetWrapMode() = 0;
		virtual void SetWrapMode(WrapMode wrap) = 0;
		virtual CompareFunc GetCompareFunc() = 0;
		virtual void SetDepthCompare(CompareFunc op) = 0;
	};

	class Shader : public CoreLib::RefObject
	{
	protected:
		Shader() {};
	};

	class RenderAttachments
	{
	public:
		int width = -1;
		int height = -1;
		struct Attachment
		{
			struct
			{
				GameEngine::Texture2D* tex2D = nullptr;
				GameEngine::Texture2DArray* tex2DArray = nullptr;
				GameEngine::TextureCube * texCube = nullptr;
				GameEngine::TextureCubeArray * texCubeArray = nullptr;
			} handle;
			int layer = -1;
			int level = -1;
			TextureCubeFace face = TextureCubeFace::NegativeX;
			Attachment(GameEngine::Texture2D * tex)
			{
				handle.tex2D = tex;
				layer = -1;
				level = 0;
			}
			Attachment(GameEngine::Texture2DArray* texArr, int l)
			{
				handle.tex2DArray = texArr;
				layer = l;
				level = 0;
			}
			Attachment(GameEngine::TextureCube* texCube, TextureCubeFace pface, int pLevel)
			{
				handle.texCube = texCube;
				face = pface;
				level = pLevel;
				layer = 0;
			}
			Attachment(GameEngine::TextureCubeArray* texCubeArr, int cubeId, TextureCubeFace pface, int pLevel)
			{
				handle.texCubeArray = texCubeArr;
				face = pface;
				layer = cubeId;
				level = pLevel;
			}
			Attachment() = default;
		};
		CoreLib::List<Attachment> attachments;
	private:
		void Resize(int size)
		{
			if (attachments.Count() < size)
			{
				attachments.SetSize(size);
			}
		}
	public:
		RenderAttachments() {}
		RenderAttachments(CoreLib::ArrayView<Texture2D*> pAttachments)
		{
			for (auto tex : pAttachments)
			{
				attachments.Add(Attachment(tex));
			}
			attachments[0].handle.tex2D->GetSize(width, height);
		}
		void SetAttachment(int binding, GameEngine::Texture2DArray * attachment, int layer)
		{
			if (width == -1 && height == -1)
			{
				int layers = 0;
				attachment->GetSize(width, height, layers);
			}
#if _DEBUG
			else
			{
				int thiswidth;
				int thisheight;
				int thislayers;
				attachment->GetSize(thiswidth, thisheight, thislayers);
				if (thiswidth != width || thisheight != height)
					throw HardwareRendererException("Attachment images must have the same dimensions.");
			}
#endif
			Resize(binding + 1);

			attachments[binding] = Attachment(attachment, layer);
		}
		void SetAttachment(int binding, GameEngine::Texture2D* attachment)
		{
			if (width == -1 && height == -1)
			{
				attachment->GetSize(width, height);
			}
#if _DEBUG
			else
			{
				int thiswidth;
				int thisheight;
				attachment->GetSize(thiswidth, thisheight);
				if (thiswidth != width || thisheight != height)
					throw HardwareRendererException("Attachment images must have the same dimensions.");
			}
#endif
			Resize(binding + 1);

			attachments[binding] = attachment;
		}
		void SetAttachment(int binding, GameEngine::TextureCube* attachment, TextureCubeFace face, int level)
		{
			if (width == -1 && height == -1)
			{
				attachment->GetSize(width);
				width >>= level;
				height = width;
			}
#if _DEBUG
			else
			{
				int thiswidth;
				int thisheight;
				attachment->GetSize(thiswidth);
				thiswidth >>= level;
				thisheight = thiswidth;
				if (thiswidth != width || thisheight != height)
					throw HardwareRendererException("Attachment images must have the same dimensions.");
			}
#endif
			Resize(binding + 1);

			attachments[binding] = Attachment(attachment, face, level);
		}
		void SetAttachment(int binding, GameEngine::TextureCubeArray* attachment, int cubeId, TextureCubeFace face, int level)
		{
			if (width == -1 && height == -1)
			{
				int count;
				attachment->GetSize(width, count);
				width >>= level;
				height = width;
			}
#if _DEBUG
			else
			{
				int thiswidth;
				int thisheight;
				int count;
				attachment->GetSize(thiswidth, count);
				thiswidth >>= level;
				thisheight = thiswidth;
				if (thiswidth != width || thisheight != height)
					throw HardwareRendererException("Attachment images must have the same dimensions.");
			}
#endif
			Resize(binding + 1);

			attachments[binding] = Attachment(attachment, cubeId, face, level);
		}
		void GetTextures(CoreLib::Array<Texture*, 8> & textures)
		{
			textures.Clear();
			for (auto & a : attachments)
			{
				if (a.handle.tex2D)
					textures.Add(a.handle.tex2D);
				else if (a.handle.tex2DArray)
					textures.Add(a.handle.tex2DArray);
				else if (a.handle.texCube)
					textures.Add(a.handle.texCube);
				else if (a.handle.texCubeArray)
					textures.Add(a.handle.texCubeArray);
			}
		}
	};

	class FrameBuffer : public CoreLib::RefObject
	{
	protected:
		FrameBuffer() {};
	public:
		virtual RenderAttachments& GetRenderAttachments() = 0;
	};

	class Fence : public CoreLib::RefObject 
	{
	protected:
		Fence() {}
	public:
		virtual void Reset() = 0;
		virtual void Wait() = 0;
	};

	class RenderTargetLayout : public CoreLib::RefObject
	{
	protected:
		RenderTargetLayout() {}
	public:
		virtual FrameBuffer* CreateFrameBuffer(const RenderAttachments& attachments) = 0;
	};

	struct TextureBinding
	{
		Texture* texture;
		TextureSampler* sampler;
	};

	struct BufferBinding
	{
		Buffer* buffer;
		int offset;
		int range;
	}; 

	enum StageFlags
	{
		sfNone = 0, sfGraphics = 3, sfVertex = 1, sfFragment = 2, sfCompute = 8, sfGraphicsAndCompute = 11
	};

	struct DescriptorLayout
	{
		int Location; //> location in the descritpor set
		StageFlags Stages = sfGraphics;
		BindingType Type; //< type of the resource binding this descriptor is about
        CoreLib::String Name;
        int ArraySize = 1;
		DescriptorLayout() {}
		DescriptorLayout(StageFlags stage, int loc, BindingType type, int arraySize = 1)
		{
			Stages = stage;
			Location = loc;
			Type = type;
            ArraySize = arraySize;
		}
	};

	// API specific class that holds internal representation of DescriptorSetLayout
	class DescriptorSetLayout : public CoreLib::RefObject
	{
	protected:
		DescriptorSetLayout() {}
	};

	class DescriptorSet : public CoreLib::RefObject
	{
	protected:
		DescriptorSet() {}
	public:
		virtual void BeginUpdate() = 0;
		virtual void Update(int location, Texture* texture, TextureAspect aspect) = 0;
        virtual void Update(int location, CoreLib::ArrayView<Texture*> texture, TextureAspect aspect) = 0;
        virtual void UpdateStorageImage(int location, CoreLib::ArrayView<Texture*> texture, TextureAspect aspect) = 0;
		virtual void Update(int location, TextureSampler* sampler) = 0;
		virtual void Update(int location, Buffer* buffer, int offset = 0, int length = -1) = 0;
		virtual void EndUpdate() = 0;
	};

	// API specific class that holds the pipeline representation
	enum class PipelineType
	{
		Graphics, Compute
	};
	class Pipeline : public CoreLib::RefObject
	{
	protected:
		Pipeline() {}
	};

    enum class PolygonMode
    {
        Fill, Line, Point
    };

	class FixedFunctionPipelineStates
	{
	public:
		bool PrimitiveRestartEnabled = false;
		bool EnablePolygonOffset = false;
        bool ConsevativeRasterization = false;
		float PolygonOffsetFactor = 0.4f;
		float PolygonOffsetUnits = 1.0f;
		PrimitiveType PrimitiveTopology = GameEngine::PrimitiveType::Triangles;
		int PatchSize = 3;
		CompareFunc DepthCompareFunc = CompareFunc::Disabled, StencilCompareFunc = CompareFunc::Disabled;
		StencilOp StencilFailOp = StencilOp::Keep, StencilDepthFailOp = StencilOp::Keep, StencilDepthPassOp = StencilOp::Keep;
		BlendMode blendMode = GameEngine::BlendMode::Replace;
        PolygonMode PolygonFillMode = PolygonMode::Fill;
		unsigned int StencilMask = 0xFFFFFFFF;
		unsigned int StencilReference = 0;
		CullMode cullMode = GameEngine::CullMode::CullBackFace;

	};

	class PipelineBuilder : public CoreLib::RefObject
	{
	protected:
		PipelineBuilder() {}
	public:
		FixedFunctionPipelineStates FixedFunctionStates;
		virtual void SetShaders(CoreLib::ArrayView<Shader*> shaders) = 0;
		virtual void SetVertexLayout(VertexFormat vertexFormat) = 0;
		virtual void SetBindingLayout(CoreLib::ArrayView<DescriptorSetLayout*> descriptorSets) = 0;
		virtual void SetDebugName(CoreLib::String name) = 0;
		virtual Pipeline* ToPipeline(RenderTargetLayout* renderTargetLayout) = 0;
		virtual Pipeline* CreateComputePipeline(CoreLib::ArrayView<DescriptorSetLayout*> descriptorSets, Shader* shader) = 0;
	};

	class CommandBuffer : public CoreLib::RefObject
	{
	protected:
		CommandBuffer() {};
	public:
		virtual void BeginRecording(FrameBuffer* frameBuffer) = 0;
		virtual void EndRecording() = 0;
        virtual void SetEventMarker(const char* name, uint32_t colorARGB) = 0;
        virtual void SetViewport(Viewport viewport) = 0;
		virtual void BindVertexBuffer(Buffer* vertexBuffer, int byteOffset) = 0;
		virtual void BindIndexBuffer(Buffer* indexBuffer, int byteOffset) = 0;
		virtual void BindPipeline(Pipeline* pipeline) = 0;
		virtual void BindDescriptorSet(int binding, DescriptorSet* descSet) = 0;
		virtual void Draw(int firstVertex, int vertexCount) = 0;
		virtual void DrawInstanced(int numInstances, int firstVertex, int vertexCount) = 0;
		virtual void DrawIndexed(int firstIndex, int indexCount) = 0;
		virtual void DrawIndexedInstanced(int numInstances, int firstIndex, int indexCount) = 0;
		virtual void DispatchCompute(int groupCountX, int groupCountY, int groupCountZ) = 0;
	};

    class WindowSurface : public CoreLib::RefObject
    {
    public:
        virtual WindowHandle GetWindowHandle() = 0;
        virtual void Resize(int width, int height) = 0;
        virtual void GetSize(int & width, int & height) = 0;
    };

	struct WindowBounds
	{
		int x, y;
		int width, height;
	};

    enum class TargetShadingLanguage
    {
        SPIRV, HLSL
    };

	enum class PipelineBarriers
	{
		None,
		ExecutionOnly,
		Memory,
		MemoryAndImage
	};

	enum class SourceFlipMode
    {
		None,
		Flip,
		ForPresent
    };

	class HardwareRenderer : public CoreLib::RefObject
	{
	public:
        virtual void ThreadInit(int threadId) = 0;
        virtual void BeginJobSubmission() = 0;
        virtual void QueueRenderPass(FrameBuffer *frameBuffer,
			bool clearFrameBuffer,
            CoreLib::ArrayView<CommandBuffer *> commands,
            PipelineBarriers barriers = PipelineBarriers::MemoryAndImage) = 0;
        virtual void QueueComputeTask(Pipeline* computePipeline, DescriptorSet* descriptorSet, int x, int y, int z, PipelineBarriers barriers = PipelineBarriers::MemoryAndImage) = 0;
        virtual void EndJobSubmission(GameEngine::Fence* fence) = 0;
		virtual void Present(WindowSurface * surface, Texture2D* srcImage) = 0;
		virtual void Blit(Texture2D* dstImage, Texture2D* srcImage, VectorMath::Vec2i destOffset, SourceFlipMode flipSrc) = 0;
		virtual void Wait() = 0;
		virtual void Init(int versionCount) = 0;
		virtual void ResetTempBufferVersion(int version) = 0;
		virtual Fence* CreateFence() = 0;
		virtual Buffer* CreateBuffer(BufferUsage usage, int sizeInBytes, const BufferStructureInfo* structInfo = nullptr) = 0;
		virtual Buffer* CreateMappedBuffer(BufferUsage usage, int sizeInBytes, const BufferStructureInfo* structInfo = nullptr) = 0;
		// Automatically builds mipmaps with supplied data
		virtual Texture2D* CreateTexture2D(CoreLib::String name, int width, int height, StorageFormat format, DataType type, void* data) = 0;
		// Allocates resources for a texture with supplied parameters
		virtual Texture2D* CreateTexture2D(CoreLib::String name, TextureUsage usage, int width, int height, int mipLevelCount, StorageFormat format) = 0;
		// Populates the created texture with the data supplied for each mipLevel
		virtual Texture2D* CreateTexture2D(CoreLib::String name, TextureUsage usage, int width, int height, int mipLevelCount, StorageFormat format, DataType type, CoreLib::ArrayView<void*> mipLevelData) = 0;
		virtual Texture2DArray* CreateTexture2DArray(CoreLib::String name, TextureUsage usage, int width, int height, int layers, int mipLevelCount, StorageFormat format) = 0;
		virtual TextureCube* CreateTextureCube(CoreLib::String name, TextureUsage usage, int size, int mipLevelCount, StorageFormat format) = 0;
		virtual TextureCubeArray* CreateTextureCubeArray(CoreLib::String name, TextureUsage usage, int size, int mipLevelCount, int cubemapCount, StorageFormat format) = 0;
		virtual Texture3D* CreateTexture3D(CoreLib::String name, TextureUsage usage, int width, int height, int depth, int mipLevelCount, StorageFormat format) = 0;
		virtual TextureSampler* CreateTextureSampler() = 0;
		virtual Shader* CreateShader(ShaderType stage, const char* data, int size) = 0;
		virtual RenderTargetLayout* CreateRenderTargetLayout(CoreLib::ArrayView<AttachmentLayout> bindings, bool ignoreInitialContent) = 0;
		virtual PipelineBuilder* CreatePipelineBuilder() = 0;
		virtual DescriptorSetLayout* CreateDescriptorSetLayout(CoreLib::ArrayView<DescriptorLayout> descriptors) = 0;
		virtual DescriptorSet* CreateDescriptorSet(DescriptorSetLayout* layout) = 0;
		virtual CommandBuffer* CreateCommandBuffer() = 0;
		virtual TargetShadingLanguage GetShadingLanguage() = 0;
		virtual int UniformBufferAlignment() = 0;
		virtual int StorageBufferAlignment() = 0;
        virtual WindowSurface * CreateSurface(WindowHandle windowHandle, int width, int height) = 0;
		virtual CoreLib::String GetRendererName() = 0;
	};

	// HardwareRenderer instance constructors
	HardwareRenderer* CreateVulkanHardwareRenderer(int gpuId, CoreLib::String cacheLocation);
	HardwareRenderer* CreateD3DHardwareRenderer(int gpuId, bool useSoftwareRenderer, CoreLib::String cacheLocation);
	HardwareRenderer* CreateDummyHardwareRenderer();
}

#endif