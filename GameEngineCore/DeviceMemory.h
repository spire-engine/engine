#ifndef GAME_ENGINE_DEVICE_MEMORY_H
#define GAME_ENGINE_DEVICE_MEMORY_H

#include "CoreLib/MemoryPool.h"
#include "HardwareRenderer.h"

namespace GameEngine
{
	class DeviceMemory
	{
	private:
		CoreLib::MemoryPool memory;
		CoreLib::RefPtr<Buffer> buffer;
		unsigned char * bufferPtr = nullptr;
		bool isMapped;
	public:
		DeviceMemory() {}
		~DeviceMemory();
		void Init(HardwareRenderer * hwRenderer, BufferUsage usage, bool isMapped, int log2BufferSize, int alignment,
			BufferStructureInfo* structInfo);
		void * Alloc(int size);
		void Free(void * ptr, int size);
		void Sync(void * ptr, int size);
		Buffer * GetBuffer()
		{
			return buffer.Ptr();
		}
		void * BufferPtr()
		{
			return bufferPtr;
		}
		void SetDataAsync(int offset, void * data, int length);
	};
}

#endif