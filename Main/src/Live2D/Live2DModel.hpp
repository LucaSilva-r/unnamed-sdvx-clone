#pragma once
#ifdef ENABLE_LIVE2D

#include <CubismFramework.hpp>
#include <Model/CubismUserModel.hpp>
#include <ICubismModelSetting.hpp>
#include <Motion/CubismMotion.hpp>
#include <Type/csmMap.hpp>

#include "lua.hpp"
#include <mutex>
#include <unordered_map>
#include <unordered_set>

// Forward declarations to avoid pulling GL headers (glew.h order issues on macOS)
struct NVGLUframebuffer;
typedef unsigned int GLuint;

namespace Live2D
{
	class Live2DModel : public Csm::CubismUserModel
	{
	public:
		Live2DModel();
		~Live2DModel() override;

		bool OpenModel(const String& path);
		void Dispose();

		void SetSize(int width, int height);
		void Update(float deltaTime);
		int GetImageHandle() const;

		void SetParameterByName(const String& name, float value);
		Vector<String> GetParameterNames() const;
		void PlayMotion(const String& group, int index, int priority);
		void SetExpression(const String& expression);
		void SetPhysicsEnabled(bool enabled) { m_physicsEnabled = enabled; }

		static int lNew(lua_State* L);
		static int lNewSkin(lua_State* L);
		static void DisposeState(lua_State* L);

	private:
		// Lua binding methods
		static int l__index(lua_State* L);
		static int l__gc(lua_State* L);
		static int lSetSize(lua_State* L);
		static int lUpdate(lua_State* L);
		static int lGetImage(lua_State* L);
		static int lGetParameterNames(lua_State* L);
		static int lSetParameter(lua_State* L);
		static int lPlayMotion(lua_State* L);
		static int lSetExpression(lua_State* L);
		static int lSetPhysicsEnabled(lua_State* L);
		static int lDispose(lua_State* L);

		static Live2DModel* GetSelf(lua_State* L);
		static int CreateAndPush(lua_State* L, const String& path);

		void m_RegisterInLuaState(lua_State* L);
		void m_UnregisterFromLuaState();
		bool m_RecreateFramebuffer(int width, int height);
		void m_SetupModel(const String& dir, const String& fileName);
		void m_SetupTextures();
		void m_RenderToFBO();

		String m_modelPath;
		String m_modelHomeDir;
		bool m_loaded = false;
		bool m_physicsEnabled = true;
		int m_width = 512;
		int m_height = 512;
		float m_userTimeSeconds = 0.0f;

		NVGLUframebuffer* m_fbo = nullptr;

		// Cubism model data
		Csm::ICubismModelSetting* m_modelSetting = nullptr;
		Csm::csmMap<Csm::csmString, Csm::ACubismMotion*> m_motions;
		Csm::csmMap<Csm::csmString, Csm::ACubismMotion*> m_expressions;
		Csm::csmVector<Csm::CubismIdHandle> m_eyeBlinkIds;
		Csm::csmVector<Csm::CubismIdHandle> m_lipSyncIds;

		// Cached parameter IDs
		const Csm::CubismId* m_idParamAngleX = nullptr;
		const Csm::CubismId* m_idParamAngleY = nullptr;
		const Csm::CubismId* m_idParamAngleZ = nullptr;
		const Csm::CubismId* m_idParamBodyAngleX = nullptr;
		const Csm::CubismId* m_idParamEyeBallX = nullptr;
		const Csm::CubismId* m_idParamEyeBallY = nullptr;

		// GL texture handles for model textures
		Vector<GLuint> m_textureIds;

		// VAO required for desktop GL 3.3+ (Cubism targets ES2 and doesn't create one)
		GLuint m_vao = 0;

		lua_State* m_ownerState = nullptr;
		static std::mutex s_registryMutex;
		static std::unordered_map<lua_State*, std::unordered_set<Live2DModel*>> s_registry;
	};
}

#endif // ENABLE_LIVE2D
