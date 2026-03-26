#pragma once
#ifdef ENABLE_LIVE2D

#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>

namespace Live2D
{
	class UscAllocator : public Csm::ICubismAllocator
	{
	public:
		void* Allocate(const Csm::csmSizeType size) override;
		void Deallocate(void* memory) override;
		void* AllocateAligned(const Csm::csmSizeType size, const Csm::csmUint32 alignment) override;
		void DeallocateAligned(void* alignedMemory) override;
	};

	class Live2DManager
	{
	public:
		static Live2DManager& Get();

		bool Initialize();
		void Shutdown();
		bool IsInitialized() const { return m_initialized; }

	private:
		Live2DManager() = default;
		~Live2DManager() = default;
		Live2DManager(const Live2DManager&) = delete;
		Live2DManager& operator=(const Live2DManager&) = delete;

		bool m_initialized = false;
		UscAllocator m_allocator;
		Csm::CubismFramework::Option m_cubismOption;
	};
}

#endif // ENABLE_LIVE2D
