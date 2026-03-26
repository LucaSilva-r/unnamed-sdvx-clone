// Include glew.h before stdafx.h to avoid "gl.h included before glew.h" on macOS
// (stdafx.h -> nuklear_sdl_gl3.h -> SDL_opengl.h -> gl.h)
#ifdef __APPLE__
#include <GL/glew.h>
#endif
#include "stdafx.h"
#ifdef ENABLE_LIVE2D
#include "Live2DModel.hpp"
#include "Application.hpp"
#include "Live2DManager.hpp"
#include "nanovg_gl_utils.h"

#include <CubismModelSettingJson.hpp>
#include <CubismDefaultParameterId.hpp>
#include <Motion/CubismMotion.hpp>
#include <Physics/CubismPhysics.hpp>
#include <Id/CubismIdManager.hpp>
#include <Utils/CubismString.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <Math/CubismMatrix44.hpp>

#include <fstream>
#include <vector>

// stb_image for loading model textures (PNG)
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#include "stb_image.h"

using namespace Csm;
using namespace Live2D::Cubism::Framework::DefaultParameterId;

namespace Live2D
{
	std::mutex Live2DModel::s_registryMutex;
	std::unordered_map<lua_State*, std::unordered_set<Live2DModel*>> Live2DModel::s_registry;

	// --- Helper: load file to byte buffer ---
	static csmByte* LoadFileBytes(const String& path, csmSizeInt* outSize)
	{
		auto loadFn = CubismFramework::GetLoadFileFunction();
		if (loadFn)
			return loadFn(path.c_str(), outSize);
		return nullptr;
	}

	static void ReleaseFileBytes(csmByte* buf)
	{
		auto releaseFn = CubismFramework::GetReleaseBytesFunction();
		if (releaseFn)
			releaseFn(buf);
	}

	// --- Live2DModel ---

	Live2DModel::Live2DModel()
		: CubismUserModel()
	{
		m_idParamAngleX = CubismFramework::GetIdManager()->GetId(ParamAngleX);
		m_idParamAngleY = CubismFramework::GetIdManager()->GetId(ParamAngleY);
		m_idParamAngleZ = CubismFramework::GetIdManager()->GetId(ParamAngleZ);
		m_idParamBodyAngleX = CubismFramework::GetIdManager()->GetId(ParamBodyAngleX);
		m_idParamEyeBallX = CubismFramework::GetIdManager()->GetId(ParamEyeBallX);
		m_idParamEyeBallY = CubismFramework::GetIdManager()->GetId(ParamEyeBallY);
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

		// Extract directory from path
		size_t lastSlash = path.find_last_of("/\\");
		if (lastSlash != String::npos)
			m_modelHomeDir = path.substr(0, lastSlash + 1);
		else
			m_modelHomeDir = "./";

		String fileName = (lastSlash != String::npos) ? String(path.substr(lastSlash + 1)) : path;

		m_SetupModel(m_modelHomeDir, fileName);

		if (_model == nullptr)
		{
			Logf("Live2DModel: Failed to load model from '%s'", Logger::Severity::Error, path);
			return false;
		}

		CreateRenderer();
		m_SetupTextures();

		m_loaded = true;
		m_userTimeSeconds = 0.0f;

		return m_RecreateFramebuffer(m_width, m_height);
	}

	void Live2DModel::m_SetupModel(const String& dir, const String& fileName)
	{
		_updating = true;
		_initialized = false;

		csmSizeInt size;
		String fullPath = dir + fileName;
		csmByte* buffer = LoadFileBytes(fullPath, &size);
		if (!buffer)
		{
			Logf("Live2DModel: Failed to load model3.json '%s'", Logger::Severity::Error, fullPath);
			return;
		}

		m_modelSetting = CSM_NEW CubismModelSettingJson(buffer, size);
		ReleaseFileBytes(buffer);

		// Load .moc3
		if (strcmp(m_modelSetting->GetModelFileName(), "") != 0)
		{
			String mocPath = dir + m_modelSetting->GetModelFileName();
			buffer = LoadFileBytes(mocPath, &size);
			if (buffer)
			{
				LoadModel(buffer, size);
				ReleaseFileBytes(buffer);
			}
		}

		// Load expressions
		for (csmInt32 i = 0; i < m_modelSetting->GetExpressionCount(); i++)
		{
			csmString name = m_modelSetting->GetExpressionName(i);
			String exprPath = dir + m_modelSetting->GetExpressionFileName(i);
			buffer = LoadFileBytes(exprPath, &size);
			if (buffer)
			{
				ACubismMotion* motion = LoadExpression(buffer, size, name.GetRawString());
				if (motion)
				{
					if (m_expressions[name] != nullptr)
						ACubismMotion::Delete(m_expressions[name]);
					m_expressions[name] = motion;
				}
				ReleaseFileBytes(buffer);
			}
		}

		// Load physics
		if (strcmp(m_modelSetting->GetPhysicsFileName(), "") != 0)
		{
			String physPath = dir + m_modelSetting->GetPhysicsFileName();
			buffer = LoadFileBytes(physPath, &size);
			if (buffer)
			{
				LoadPhysics(buffer, size);
				ReleaseFileBytes(buffer);
			}
		}

		// Load pose
		if (strcmp(m_modelSetting->GetPoseFileName(), "") != 0)
		{
			String posePath = dir + m_modelSetting->GetPoseFileName();
			buffer = LoadFileBytes(posePath, &size);
			if (buffer)
			{
				LoadPose(buffer, size);
				ReleaseFileBytes(buffer);
			}
		}

		// Eye blink
		if (m_modelSetting->GetEyeBlinkParameterCount() > 0)
			_eyeBlink = CubismEyeBlink::Create(m_modelSetting);

		// Breath
		{
			_breath = CubismBreath::Create();
			csmVector<CubismBreath::BreathParameterData> breathParams;
			breathParams.PushBack(CubismBreath::BreathParameterData(m_idParamAngleX, 0.0f, 15.0f, 6.5345f, 0.5f));
			breathParams.PushBack(CubismBreath::BreathParameterData(m_idParamAngleY, 0.0f, 8.0f, 3.5345f, 0.5f));
			breathParams.PushBack(CubismBreath::BreathParameterData(m_idParamAngleZ, 0.0f, 10.0f, 5.5345f, 0.5f));
			breathParams.PushBack(CubismBreath::BreathParameterData(m_idParamBodyAngleX, 0.0f, 4.0f, 15.5345f, 0.5f));
			breathParams.PushBack(CubismBreath::BreathParameterData(
				CubismFramework::GetIdManager()->GetId(ParamBreath), 0.5f, 0.5f, 3.2345f, 0.5f));
			_breath->SetParameters(breathParams);
		}

		// User data
		if (strcmp(m_modelSetting->GetUserDataFile(), "") != 0)
		{
			String udPath = dir + m_modelSetting->GetUserDataFile();
			buffer = LoadFileBytes(udPath, &size);
			if (buffer)
			{
				LoadUserData(buffer, size);
				ReleaseFileBytes(buffer);
			}
		}

		// Eye blink IDs
		for (csmInt32 i = 0; i < m_modelSetting->GetEyeBlinkParameterCount(); i++)
			m_eyeBlinkIds.PushBack(m_modelSetting->GetEyeBlinkParameterId(i));

		// Lip sync IDs
		for (csmInt32 i = 0; i < m_modelSetting->GetLipSyncParameterCount(); i++)
			m_lipSyncIds.PushBack(m_modelSetting->GetLipSyncParameterId(i));

		// Layout
		if (_modelMatrix)
		{
			csmMap<csmString, csmFloat32> layout;
			m_modelSetting->GetLayoutMap(layout);
			_modelMatrix->SetupFromLayout(layout);
		}

		if (_model)
			_model->SaveParameters();

		// Preload motions
		for (csmInt32 i = 0; i < m_modelSetting->GetMotionGroupCount(); i++)
		{
			const csmChar* group = m_modelSetting->GetMotionGroupName(i);
			csmInt32 count = m_modelSetting->GetMotionCount(group);
			for (csmInt32 j = 0; j < count; j++)
			{
				csmString name = Csm::Utils::CubismString::GetFormatedString("%s_%d", group, j);
				String motionPath = dir + m_modelSetting->GetMotionFileName(group, j);
				buffer = LoadFileBytes(motionPath, &size);
				if (buffer)
				{
					CubismMotion* motion = static_cast<CubismMotion*>(
						LoadMotion(buffer, size, name.GetRawString(), nullptr, nullptr, m_modelSetting, group, j));
					if (motion)
					{
						motion->SetEffectIds(m_eyeBlinkIds, m_lipSyncIds);
						if (m_motions[name] != nullptr)
							ACubismMotion::Delete(m_motions[name]);
						m_motions[name] = motion;
					}
					ReleaseFileBytes(buffer);
				}
			}
		}

		if (_motionManager)
			_motionManager->StopAllMotions();

		_updating = false;
		_initialized = true;
	}

	void Live2DModel::m_SetupTextures()
	{
		if (!m_modelSetting) return;

		for (csmInt32 i = 0; i < m_modelSetting->GetTextureCount(); i++)
		{
			if (strcmp(m_modelSetting->GetTextureFileName(i), "") == 0)
				continue;

			String texPath = m_modelHomeDir + m_modelSetting->GetTextureFileName(i);

			csmSizeInt fileSize;
			csmByte* fileData = LoadFileBytes(texPath, &fileSize);
			if (!fileData)
			{
				Logf("Live2DModel: Failed to load texture '%s'", Logger::Severity::Warning, texPath);
				continue;
			}

			int w, h, channels;
			unsigned char* png = stbi_load_from_memory(fileData, fileSize, &w, &h, &channels, STBI_rgb_alpha);
			ReleaseFileBytes(fileData);

			if (!png)
			{
				Logf("Live2DModel: Failed to decode texture '%s'", Logger::Severity::Warning, texPath);
				continue;
			}

			// Premultiply alpha
			unsigned int* pixels = reinterpret_cast<unsigned int*>(png);
			for (int p = 0; p < w * h; p++)
			{
				unsigned char* c = png + p * 4;
				unsigned char a = c[3];
				c[0] = (unsigned char)((c[0] * a) / 255);
				c[1] = (unsigned char)((c[1] * a) / 255);
				c[2] = (unsigned char)((c[2] * a) / 255);
			}

			GLuint texId;
			glGenTextures(1, &texId);
			glBindTexture(GL_TEXTURE_2D, texId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, png);
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glBindTexture(GL_TEXTURE_2D, 0);

			stbi_image_free(png);

			auto* renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
			if (renderer)
				renderer->BindTexture(i, texId);

			m_textureIds.Add(texId);
		}

		auto* renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
		if (renderer)
			renderer->IsPremultipliedAlpha(true);
	}

	void Live2DModel::Dispose()
	{
		m_loaded = false;
		m_userTimeSeconds = 0.0f;

		// Release GL textures
		for (size_t i = 0; i < m_textureIds.size(); i++)
			glDeleteTextures(1, &m_textureIds[i]);
		m_textureIds.clear();

		// Release motions
		for (auto iter = m_motions.Begin(); iter != m_motions.End(); ++iter)
			ACubismMotion::Delete(iter->Second);
		m_motions.Clear();

		// Release expressions
		for (auto iter = m_expressions.Begin(); iter != m_expressions.End(); ++iter)
			ACubismMotion::Delete(iter->Second);
		m_expressions.Clear();

		if (m_modelSetting)
		{
			CSM_DELETE(m_modelSetting);
			m_modelSetting = nullptr;
		}

		// Delete VAO
		if (m_vao)
		{
			glDeleteVertexArrays(1, &m_vao);
			m_vao = 0;
		}

		// Delete NVG framebuffer
		if (m_fbo)
		{
			nvgluDeleteFramebuffer(m_fbo);
			m_fbo = nullptr;
		}

		m_eyeBlinkIds.Clear();
		m_lipSyncIds.Clear();
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

		return true;
	}

	void Live2DModel::SetSize(int width, int height)
	{
		m_RecreateFramebuffer(width, height);
	}

	void Live2DModel::Update(float deltaTime)
	{
		if (!m_loaded || !_model || !m_fbo)
			return;

		m_userTimeSeconds += std::max(0.0f, deltaTime);

		// Update drag manager
		_dragManager->Update(deltaTime);
		_dragX = _dragManager->GetX();
		_dragY = _dragManager->GetY();

		csmBool motionUpdated = false;

		_model->LoadParameters();
		if (_motionManager->IsFinished())
		{
			// Auto-play idle motion if available
			if (m_modelSetting && m_modelSetting->GetMotionCount("Idle") > 0)
			{
				csmInt32 no = rand() % m_modelSetting->GetMotionCount("Idle");
				PlayMotion("Idle", no, 1);
			}
		}
		else
		{
			motionUpdated = _motionManager->UpdateMotion(_model, deltaTime);
		}
		_model->SaveParameters();

		// Eye blink (only when no motion is updating parameters)
		if (!motionUpdated && _eyeBlink)
			_eyeBlink->UpdateParameters(_model, deltaTime);

		// Expressions
		if (_expressionManager)
			_expressionManager->UpdateMotion(_model, deltaTime);

		// Breath
		if (_breath)
			_breath->UpdateParameters(_model, deltaTime);

		// Physics
		if (m_physicsEnabled && _physics)
			_physics->Evaluate(_model, deltaTime);

		// Pose
		if (_pose)
			_pose->UpdateParameters(_model, deltaTime);

		_model->Update();

		// Render to FBO
		m_RenderToFBO();
	}

	void Live2DModel::m_RenderToFBO()
	{
		if (!m_fbo || !_model) return;

		auto* renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
		if (!renderer) return;

		// Save GL state
		GLint previousFbo = 0;
		GLint previousViewport[4] = { 0, 0, 0, 0 };
		GLboolean previousBlend = GL_FALSE;
		GLint previousBlendSrc = 0, previousBlendDst = 0;
		GLint previousVao = 0;

		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
		glGetIntegerv(GL_VIEWPORT, previousViewport);
		previousBlend = glIsEnabled(GL_BLEND);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrc);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDst);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);

		// Bind our FBO
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo->fbo);
		glViewport(0, 0, m_width, m_height);

		// Clear to transparent
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		// Set blending for premultiplied alpha
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		// Build MVP matrix: ortho projection fitting the model
		CubismMatrix44 projection;
		if (m_width > m_height)
		{
			float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
			projection.Scale(1.0f, aspect);
		}
		else
		{
			float aspect = static_cast<float>(m_height) / static_cast<float>(m_width);
			projection.Scale(aspect, 1.0f);
		}

		if (_modelMatrix)
			projection.MultiplyByMatrix(_modelMatrix);

		renderer->SetMvpMatrix(&projection);
		renderer->DrawModel();

		// Restore GL state
		glBindVertexArray(previousVao);
		glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
		glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

		if (previousBlend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
		glBlendFunc(previousBlendSrc, previousBlendDst);
	}

	int Live2DModel::GetImageHandle() const
	{
		return m_fbo ? m_fbo->image : 0;
	}

	Vector<String> Live2DModel::GetParameterNames() const
	{
		Vector<String> names;
		if (_model)
		{
			csmInt32 count = _model->GetParameterCount();
			for (csmInt32 i = 0; i < count; i++)
				names.Add(_model->GetParameterId(i)->GetString().GetRawString());
		}
		return names;
	}

	void Live2DModel::SetParameterByName(const String& name, float value)
	{
		if (!_model) return;
		const CubismId* id = CubismFramework::GetIdManager()->GetId(name.c_str());
		_model->SetParameterValue(id, value);
	}

	void Live2DModel::PlayMotion(const String& group, int index, int priority)
	{
		if (!m_modelSetting || !_motionManager) return;

		csmString name = Csm::Utils::CubismString::GetFormatedString("%s_%d", group.c_str(), index);
		CubismMotion* motion = static_cast<CubismMotion*>(m_motions[name.GetRawString()]);

		if (!motion)
		{
			// Try to load on-demand
			if (index < m_modelSetting->GetMotionCount(group.c_str()))
			{
				String motionPath = m_modelHomeDir + m_modelSetting->GetMotionFileName(group.c_str(), index);
				csmSizeInt size;
				csmByte* buffer = LoadFileBytes(motionPath, &size);
				if (buffer)
				{
					motion = static_cast<CubismMotion*>(
						LoadMotion(buffer, size, nullptr, nullptr, nullptr, m_modelSetting, group.c_str(), index));
					if (motion)
						motion->SetEffectIds(m_eyeBlinkIds, m_lipSyncIds);
					ReleaseFileBytes(buffer);
				}
			}
		}

		if (motion)
			_motionManager->StartMotionPriority(motion, false, priority);
	}

	void Live2DModel::SetExpression(const String& expression)
	{
		if (!_expressionManager) return;

		csmString exprName(expression.c_str());
		ACubismMotion* motion = m_expressions[exprName];
		if (motion)
			_expressionManager->StartMotion(motion, false);
	}

	// --- Lua lifecycle ---

	void Live2DModel::m_RegisterInLuaState(lua_State* L)
	{
		if (!L) return;
		m_ownerState = L;
		std::lock_guard<std::mutex> lock(s_registryMutex);
		s_registry[L].insert(this);
	}

	void Live2DModel::m_UnregisterFromLuaState()
	{
		if (!m_ownerState) return;
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
		if (!L) return;

		std::vector<Live2DModel*> models;
		{
			std::lock_guard<std::mutex> lock(s_registryMutex);
			auto it = s_registry.find(L);
			if (it == s_registry.end()) return;

			models.reserve(it->second.size());
			for (Live2DModel* model : it->second)
			{
				models.push_back(model);
				if (model) model->m_ownerState = nullptr;
			}
			s_registry.erase(it);
		}

		for (Live2DModel* model : models)
		{
			if (model) model->Dispose();
		}
	}

	// --- Lua bindings ---

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
		if (!fname) return 0;

		String name = fname;
		struct MethodEntry { const char* name; lua_CFunction func; };

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
		GetSelf(L)->SetParameterByName(name, value);
		return 0;
	}

	int Live2DModel::lPlayMotion(lua_State* L)
	{
		const char* group = luaL_checkstring(L, 2);
		const int index = luaL_checkinteger(L, 3);
		const int priority = luaL_optinteger(L, 4, 2);
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
