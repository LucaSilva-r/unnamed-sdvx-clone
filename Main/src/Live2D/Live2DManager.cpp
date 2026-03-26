#include "stdafx.h"
#ifdef ENABLE_LIVE2D
#include "Live2DManager.hpp"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

using namespace Csm;

namespace Live2D
{
	// --- UscAllocator ---

	void* UscAllocator::Allocate(const csmSizeType size)
	{
		return malloc(size);
	}

	void UscAllocator::Deallocate(void* memory)
	{
		free(memory);
	}

	void* UscAllocator::AllocateAligned(const csmSizeType size, const csmUint32 alignment)
	{
		size_t offset = alignment - 1 + sizeof(void*);
		void* raw = malloc(size + offset);
		if (!raw) return nullptr;

		void** aligned = reinterpret_cast<void**>(
			(reinterpret_cast<size_t>(raw) + offset) & ~(static_cast<size_t>(alignment) - 1)
		);
		aligned[-1] = raw;
		return aligned;
	}

	void UscAllocator::DeallocateAligned(void* alignedMemory)
	{
		if (!alignedMemory) return;
		void* raw = (static_cast<void**>(alignedMemory))[-1];
		free(raw);
	}

	// --- File I/O callbacks for CubismFramework ---

	static csmByte* UscLoadFileAsBytes(const std::string filePath, csmSizeInt* outSize)
	{
		// Resolve relative paths against the game directory
		String resolvedPath = Path::Absolute(filePath);

		struct stat statBuf;
		if (stat(resolvedPath.c_str(), &statBuf) != 0 || statBuf.st_size == 0)
		{
			Logf("Live2D: Failed to stat file '%s' (resolved: '%s')", Logger::Severity::Warning, filePath.c_str(), resolvedPath.c_str());
			return nullptr;
		}

		std::ifstream file(resolvedPath.c_str(), std::ios::in | std::ios::binary);
		if (!file.is_open())
		{
			Logf("Live2D: Failed to open file '%s'", Logger::Severity::Warning, resolvedPath.c_str());
			return nullptr;
		}

		csmSizeInt size = statBuf.st_size;
		char* buf = new char[size];
		file.read(buf, size);
		file.close();

		*outSize = size;
		return reinterpret_cast<csmByte*>(buf);
	}

	static void UscReleaseBytes(csmByte* byteData)
	{
		delete[] byteData;
	}

	static void UscLogFunction(const csmChar* message)
	{
		Logf("Live2D: %s", Logger::Severity::Normal, message);
	}

	// --- Live2DManager ---

	Live2DManager& Live2DManager::Get()
	{
		static Live2DManager s_instance;
		return s_instance;
	}

	bool Live2DManager::Initialize()
	{
		if (m_initialized)
			return true;

		m_cubismOption.LogFunction = UscLogFunction;
		m_cubismOption.LoggingLevel = CubismFramework::Option::LogLevel_Warning;
		m_cubismOption.LoadFileFunction = UscLoadFileAsBytes;
		m_cubismOption.ReleaseBytesFunction = UscReleaseBytes;

		if (!CubismFramework::StartUp(&m_allocator, &m_cubismOption))
		{
			Logf("Live2DManager: CubismFramework::StartUp failed", Logger::Severity::Error);
			return false;
		}

		CubismFramework::Initialize();

		m_initialized = true;
		Logf("Live2DManager initialized (Cubism SDK)", Logger::Severity::Info);
		return true;
	}

	void Live2DManager::Shutdown()
	{
		if (!m_initialized)
			return;

		CubismFramework::Dispose();
		CubismFramework::CleanUp();

		m_initialized = false;
		Logf("Live2DManager shutdown", Logger::Severity::Info);
	}
}

#endif // ENABLE_LIVE2D
