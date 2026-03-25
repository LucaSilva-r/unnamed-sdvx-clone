#include "stdafx.h"
#ifdef ENABLE_LIVE2D
#include "Live2DManager.hpp"

namespace Live2D
{
	Live2DManager& Live2DManager::Get()
	{
		static Live2DManager s_instance;
		return s_instance;
	}

	bool Live2DManager::Initialize()
	{
		if (m_initialized)
			return true;

		// Placeholder initialization point for Cubism framework bootstrap.
		m_initialized = true;
		Logf("Live2DManager initialized", Logger::Severity::Info);
		return true;
	}

	void Live2DManager::Shutdown()
	{
		if (!m_initialized)
			return;

		// Placeholder shutdown point for Cubism framework cleanup.
		m_initialized = false;
		Logf("Live2DManager shutdown", Logger::Severity::Info);
	}
}

#endif // ENABLE_LIVE2D
