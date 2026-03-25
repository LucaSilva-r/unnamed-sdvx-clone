#include "stdafx.h"
#ifdef ENABLE_LIVE2D
#include "Live2DModel.hpp"
#include "Application.hpp"
#include "Live2DManager.hpp"
#include <vector>

namespace Live2D
{
	std::mutex Live2DModel::s_registryMutex;
	std::unordered_map<lua_State*, std::unordered_set<Live2DModel*>> Live2DModel::s_registry;

	Live2DModel::Live2DModel()
	{
		m_parameterNames.Add("ParamAngleX");
		m_parameterNames.Add("ParamAngleY");
		m_parameterNames.Add("ParamMouthOpenY");
	}

	Live2DModel::~Live2DModel()
	{
		Dispose();
		m_UnregisterFromLuaState();
	}

	bool Live2DModel::OpenModel(const String& path)
	{
		Dispose();

		if (!Live2DManager::Get().IsInitialized())
		{
			Logf("Live2DModel: manager is not initialized", Logger::Severity::Warning);
			return false;
		}
		if (!Path::FileExists(path))
		{
			Logf("Live2DModel: model file not found '%s'", Logger::Severity::Warning, path);
			return false;
		}

		m_modelPath = path;
		m_loaded = true;
		m_time = 0.0f;
		return m_RecreateFramebuffer(m_width, m_height);
	}

	void Live2DModel::Dispose()
	{
		if (m_fbo)
		{
			nvgluDeleteFramebuffer(m_fbo);
			m_fbo = nullptr;
		}
		m_loaded = false;
		m_time = 0.0f;
	}

	bool Live2DModel::m_RecreateFramebuffer(int width, int height)
	{
		NVGcontext* vg = g_application ? g_application->GetVGContext() : nullptr;
		if (!vg)
			return false;

		width = std::max(1, width);
		height = std::max(1, height);
		m_width = width;
		m_height = height;

		if (m_fbo)
		{
			nvgluDeleteFramebuffer(m_fbo);
			m_fbo = nullptr;
		}

		m_fbo = nvgluCreateFramebuffer(vg, m_width, m_height, 0);
		if (!m_fbo)
		{
			Logf("Live2DModel: failed to create framebuffer (%dx%d)", Logger::Severity::Warning, m_width, m_height);
			return false;
		}

		m_RenderPlaceholder();
		return true;
	}

	void Live2DModel::SetSize(int width, int height)
	{
		m_RecreateFramebuffer(width, height);
	}

	void Live2DModel::m_RenderPlaceholder()
	{
		if (!m_fbo)
			return;

		GLint previousFbo = 0;
		GLint previousViewport[4] = { 0, 0, 0, 0 };
		GLfloat previousClearColor[4] = { 0, 0, 0, 0 };

		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
		glGetIntegerv(GL_VIEWPORT, previousViewport);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor);

		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo->fbo);
		glViewport(0, 0, m_width, m_height);

		const float pulse = 0.5f + 0.5f * sinf(m_time * 2.0f);
		glClearColor(0.08f + 0.10f * pulse, 0.12f, 0.18f + 0.08f * pulse, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
		glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
		glClearColor(previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]);
	}

	void Live2DModel::Update(float deltaTime)
	{
		if (!m_loaded || !m_fbo)
			return;

		m_time += std::max(0.0f, deltaTime);
		m_RenderPlaceholder();
	}

	int Live2DModel::GetImageHandle() const
	{
		return m_fbo ? m_fbo->image : 0;
	}

	Vector<String> Live2DModel::GetParameterNames() const
	{
		return m_parameterNames;
	}

	void Live2DModel::SetParameter(const String& name, float value)
	{
		if (!m_parameters.Contains(name))
		{
			m_parameterNames.Add(name);
		}
		m_parameters[name] = value;
	}

	void Live2DModel::PlayMotion(const String& group, int index, int priority)
	{
		Logf("Live2DModel: PlayMotion group=%s index=%d priority=%d", Logger::Severity::Info, group, index, priority);
	}

	void Live2DModel::SetExpression(const String& expression)
	{
		Logf("Live2DModel: SetExpression %s", Logger::Severity::Info, expression);
	}

	void Live2DModel::m_RegisterInLuaState(lua_State* L)
	{
		if (!L)
			return;

		m_ownerState = L;
		std::lock_guard<std::mutex> lock(s_registryMutex);
		s_registry[L].insert(this);
	}

	void Live2DModel::m_UnregisterFromLuaState()
	{
		if (!m_ownerState)
			return;

		std::lock_guard<std::mutex> lock(s_registryMutex);
		auto stateIt = s_registry.find(m_ownerState);
		if (stateIt != s_registry.end())
		{
			stateIt->second.erase(this);
			if (stateIt->second.empty())
				s_registry.erase(stateIt);
		}
		m_ownerState = nullptr;
	}

	void Live2DModel::DisposeState(lua_State* L)
	{
		if (!L)
			return;

		std::vector<Live2DModel*> models;
		{
			std::lock_guard<std::mutex> lock(s_registryMutex);
			auto it = s_registry.find(L);
			if (it == s_registry.end())
				return;

			models.reserve(it->second.size());
			for (Live2DModel* model : it->second)
			{
				models.push_back(model);
				if (model)
					model->m_ownerState = nullptr;
			}
			s_registry.erase(it);
		}

		for (Live2DModel* model : models)
		{
			if (model)
				model->Dispose();
		}
	}

	Live2DModel* Live2DModel::GetSelf(lua_State* L)
	{
		Live2DModel** ud = static_cast<Live2DModel**>(lua_touserdata(L, 1));
		if (!ud || !*ud)
		{
			luaL_error(L, "Invalid Live2DModel object");
			return nullptr;
		}
		return *ud;
	}

	int Live2DModel::CreateAndPush(lua_State* L, const String& path)
	{
		Live2DModel** place = (Live2DModel**)lua_newuserdata(L, sizeof(Live2DModel*));
		*place = new Live2DModel();

		if (!(*place)->OpenModel(path))
		{
			delete *place;
			*place = nullptr;
			lua_pop(L, 1);
			return 0;
		}

		(*place)->m_RegisterInLuaState(L);

		lua_newtable(L);
		lua_pushcfunction(L, l__index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, l__gc);
		lua_setfield(L, -2, "__gc");
		lua_setmetatable(L, -2);
		return 1;
	}

	int Live2DModel::lNew(lua_State* L)
	{
		const char* path = luaL_checkstring(L, 1);
		String absPath = Path::Absolute(path);
		return CreateAndPush(L, absPath);
	}

	int Live2DModel::lNewSkin(lua_State* L)
	{
		const char* path = luaL_checkstring(L, 1);
		String fullPath = "skins/" + g_application->GetCurrentSkin() + "/live2d/" + path;
		fullPath = Path::Absolute(fullPath);
		return CreateAndPush(L, fullPath);
	}

	int Live2DModel::l__index(lua_State* L)
	{
		const char* fname = lua_tostring(L, 2);
		if (!fname)
			return 0;

		String name = fname;
		struct MethodEntry
		{
			const char* name;
			lua_CFunction func;
		};

		static const MethodEntry methods[] = {
			{"SetSize", lSetSize},
			{"Update", lUpdate},
			{"GetImage", lGetImage},
			{"GetParameterNames", lGetParameterNames},
			{"SetParameter", lSetParameter},
			{"PlayMotion", lPlayMotion},
			{"SetExpression", lSetExpression},
			{"SetPhysicsEnabled", lSetPhysicsEnabled},
			{"Dispose", lDispose},
			{nullptr, nullptr}
		};

		for (int i = 0; methods[i].name != nullptr; i++)
		{
			if (name == methods[i].name)
			{
				lua_pushcfunction(L, methods[i].func);
				return 1;
			}
		}

		return luaL_error(L, "Live2DModel has no method: '%s'", fname);
	}

	int Live2DModel::l__gc(lua_State* L)
	{
		Live2DModel** ud = static_cast<Live2DModel**>(lua_touserdata(L, 1));
		if (ud && *ud)
		{
			delete *ud;
			*ud = nullptr;
		}
		return 0;
	}

	int Live2DModel::lSetSize(lua_State* L)
	{
		const int w = luaL_checkinteger(L, 2);
		const int h = luaL_checkinteger(L, 3);
		GetSelf(L)->SetSize(w, h);
		return 0;
	}

	int Live2DModel::lUpdate(lua_State* L)
	{
		const float dt = (float)luaL_checknumber(L, 2);
		GetSelf(L)->Update(dt);
		return 0;
	}

	int Live2DModel::lGetImage(lua_State* L)
	{
		lua_pushinteger(L, GetSelf(L)->GetImageHandle());
		return 1;
	}

	int Live2DModel::lGetParameterNames(lua_State* L)
	{
		Vector<String> names = GetSelf(L)->GetParameterNames();
		lua_newtable(L);
		int i = 1;
		for (auto&& name : names)
		{
			lua_pushinteger(L, i++);
			lua_pushstring(L, name.c_str());
			lua_settable(L, -3);
		}
		return 1;
	}

	int Live2DModel::lSetParameter(lua_State* L)
	{
		const char* name = luaL_checkstring(L, 2);
		const float value = (float)luaL_checknumber(L, 3);
		GetSelf(L)->SetParameter(name, value);
		return 0;
	}

	int Live2DModel::lPlayMotion(lua_State* L)
	{
		const char* group = luaL_checkstring(L, 2);
		const int index = luaL_checkinteger(L, 3);
		const int priority = luaL_optinteger(L, 4, 0);
		GetSelf(L)->PlayMotion(group, index, priority);
		return 0;
	}

	int Live2DModel::lSetExpression(lua_State* L)
	{
		const char* expression = luaL_checkstring(L, 2);
		GetSelf(L)->SetExpression(expression);
		return 0;
	}

	int Live2DModel::lSetPhysicsEnabled(lua_State* L)
	{
		const bool enabled = lua_toboolean(L, 2) != 0;
		GetSelf(L)->SetPhysicsEnabled(enabled);
		return 0;
	}

	int Live2DModel::lDispose(lua_State* L)
	{
		GetSelf(L)->Dispose();
		return 0;
	}
}

#endif // ENABLE_LIVE2D
