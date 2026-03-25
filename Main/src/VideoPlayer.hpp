#pragma once
#ifdef ENABLE_VIDEO

#include <Graphics/VideoDecoder.hpp>
#include "lua.hpp"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class VideoPlayer
{
public:
	VideoPlayer();
	~VideoPlayer();

	bool OpenVideo(const String& path);
	void CloseVideo();

	// Playback control
	void Play();
	void Pause();
	void Seek(double seconds);
	void SetLoop(bool loop) { m_loop = loop; }
	void SetVolume(float vol) { m_volume = Math::Clamp(vol, 0.0f, 1.0f); }

	// State queries
	double GetPosition() const { return m_currentPts; }
	double GetDuration() const { return m_decoder.GetDuration(); }
	bool IsPlaying() const { return m_playing.load(); }
	bool HasEnded() const { return m_ended.load(); }
	int GetWidth() const { return m_decoder.GetWidth(); }
	int GetHeight() const { return m_decoder.GetHeight(); }
	bool HasAudio() const { return m_decoder.HasAudioTrack(); }

	// Called each frame to advance playback and update NVG texture
	void Tick(float deltaTime);

	// Returns NVG image handle for rendering
	int GetImageHandle() const { return m_nvgImage; }

	// Lua bindings
	static int lNew(lua_State* L);
	static int lNewSkin(lua_State* L);

	// Called from GUI disposal path to ensure state reload cannot leak video resources.
	static void DisposeState(lua_State* L);

private:
	static int l__index(lua_State* L);
	static int l__gc(lua_State* L);

	// Lua method implementations
	static int lPlay(lua_State* L);
	static int lPause(lua_State* L);
	static int lSeek(lua_State* L);
	static int lSetLoop(lua_State* L);
	static int lSetVolume(lua_State* L);
	static int lTick(lua_State* L);
	static int lGetImage(lua_State* L);
	static int lGetPosition(lua_State* L);
	static int lGetDuration(lua_State* L);
	static int lIsPlaying(lua_State* L);
	static int lHasEnded(lua_State* L);
	static int lGetSize(lua_State* L);
	static int lDispose(lua_State* L);

	static VideoPlayer* GetSelf(lua_State* L);
	static int CreateAndPush(lua_State* L, const String& path);

	void m_RegisterInLuaState(lua_State* L);
	void m_UnregisterFromLuaState();

	Graphics::VideoDecoder m_decoder;
	int m_nvgImage = 0;
	Vector<uint8> m_frameBuffer;

	std::atomic<bool> m_playing{false};
	std::atomic<bool> m_ended{false};
	bool m_loop = false;
	float m_volume = 1.0f;

	double m_currentPts = 0.0;
	double m_playbackClock = 0.0;

	bool m_initialized = false;
	bool m_hasPendingFrame = false;
	Graphics::VideoFrame m_pendingFrame;

	lua_State* m_ownerState = nullptr;

	static std::mutex s_registryMutex;
	static std::unordered_map<lua_State*, std::unordered_set<VideoPlayer*>> s_registry;
};

#endif // ENABLE_VIDEO
