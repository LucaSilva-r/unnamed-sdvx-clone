#pragma once
#ifdef ENABLE_LIVE2D

namespace Live2D
{
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
	};
}

#endif // ENABLE_LIVE2D
