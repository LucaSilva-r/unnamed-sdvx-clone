#include "stdafx.h"
#ifdef ENABLE_VIDEO
#include "VideoPlayer.hpp"
#include "Application.hpp"
#include "nanovg.h"
#include <vector>

std::mutex VideoPlayer::s_registryMutex;
std::unordered_map<lua_State*, std::unordered_set<VideoPlayer*>> VideoPlayer::s_registry;

VideoPlayer::VideoPlayer()
{
}

VideoPlayer::~VideoPlayer()
{
	CloseVideo();
	m_UnregisterFromLuaState();
}

bool VideoPlayer::OpenVideo(const String& path)
{
	CloseVideo();

	if (!m_decoder.Open(path))
		return false;

	const int w = m_decoder.GetWidth();
	const int h = m_decoder.GetHeight();
	if (w <= 0 || h <= 0)
	{
		Logf("VideoPlayer: Invalid video dimensions", Logger::Severity::Error);
		m_decoder.Close();
		return false;
	}

	m_frameBuffer.resize(w * h * 4, 0);

	NVGcontext* vg = g_application ? g_application->GetVGContext() : nullptr;
	if (!vg)
	{
		Logf("VideoPlayer: No NanoVG context", Logger::Severity::Error);
		m_decoder.Close();
		return false;
	}

	m_nvgImage = nvgCreateImageRGBA(vg, w, h, 0, m_frameBuffer.data());
	if (m_nvgImage == 0)
	{
		Logf("VideoPlayer: Failed to create NVG image", Logger::Severity::Error);
		m_decoder.Close();
		return false;
	}

	m_playing.store(false);
	m_ended.store(false);
	m_currentPts = 0.0;
	m_playbackClock = 0.0;
	m_hasPendingFrame = false;

	m_decoder.StartDecoding();
	m_initialized = true;
	return true;
}

void VideoPlayer::CloseVideo()
{
	m_playing.store(false);
	m_ended.store(false);
	m_hasPendingFrame = false;
	m_pendingFrame = Graphics::VideoFrame();

	m_decoder.Close();

	if (m_nvgImage != 0)
	{
		NVGcontext* vg = g_application ? g_application->GetVGContext() : nullptr;
		if (vg)
		{
			nvgDeleteImage(vg, m_nvgImage);
		}
		m_nvgImage = 0;
	}

	m_frameBuffer.clear();
	m_currentPts = 0.0;
	m_playbackClock = 0.0;
	m_initialized = false;
}

void VideoPlayer::Play()
{
	if (!m_initialized)
		return;

	if (m_ended.load() && m_loop)
	{
		Seek(0.0);
		m_ended.store(false);
	}

	m_playing.store(true);
}

void VideoPlayer::Pause()
{
	m_playing.store(false);
}

void VideoPlayer::Seek(double seconds)
{
	if (!m_initialized)
		return;

	const double duration = GetDuration();
	if (duration > 0.0)
		seconds = Math::Clamp(seconds, 0.0, duration);
	else
		seconds = std::max(0.0, seconds);

	m_decoder.Seek(seconds);
	m_playbackClock = seconds;
	m_currentPts = seconds;
	m_ended.store(false);
	m_hasPendingFrame = false;
}

void VideoPlayer::Tick(float deltaTime)
{
	if (!m_initialized || !m_playing.load())
		return;

	m_playbackClock += std::max(0.0f, deltaTime);
	bool uploadedFrame = false;

	constexpr double FrameEpsilon = 0.0005;
	while (true)
	{
		if (!m_hasPendingFrame)
		{
			if (!m_decoder.GetVideoFrame(m_pendingFrame))
				break;
			m_hasPendingFrame = true;
		}

		if (m_pendingFrame.pts > m_playbackClock + FrameEpsilon)
			break;

		if ((int)m_pendingFrame.data.size() == m_decoder.GetWidth() * m_decoder.GetHeight() * 4)
		{
			memcpy(m_frameBuffer.data(), m_pendingFrame.data.data(), m_pendingFrame.data.size());
			NVGcontext* vg = g_application ? g_application->GetVGContext() : nullptr;
			if (vg && m_nvgImage != 0)
			{
				nvgUpdateImage(vg, m_nvgImage, m_frameBuffer.data());
			}
			uploadedFrame = true;
		}

		m_currentPts = m_pendingFrame.pts;
		m_hasPendingFrame = false;
	}

	if (m_decoder.IsEof() && !m_hasPendingFrame && !m_decoder.HasPendingVideoFrames())
	{
		if (m_loop)
		{
			Seek(0.0);
		}
		else
		{
			m_ended.store(true);
			m_playing.store(false);
			if (GetDuration() > 0.0)
				m_currentPts = GetDuration();
		}
		return;
	}

	if (!uploadedFrame && GetDuration() > 0.0 && m_playbackClock >= GetDuration() && !m_loop)
	{
		m_ended.store(true);
		m_playing.store(false);
	}
}

void VideoPlayer::m_RegisterInLuaState(lua_State* L)
{
	if (!L)
		return;
	m_ownerState = L;

	std::lock_guard<std::mutex> lock(s_registryMutex);
	s_registry[L].insert(this);
}

void VideoPlayer::m_UnregisterFromLuaState()
{
	if (!m_ownerState)
		return;

	std::lock_guard<std::mutex> lock(s_registryMutex);
	auto stateIt = s_registry.find(m_ownerState);
	if (stateIt != s_registry.end())
	{
		stateIt->second.erase(this);
		if (stateIt->second.empty())
		{
			s_registry.erase(stateIt);
		}
	}

	m_ownerState = nullptr;
}

void VideoPlayer::DisposeState(lua_State* L)
{
	if (!L)
		return;

	std::vector<VideoPlayer*> players;
	{
		std::lock_guard<std::mutex> lock(s_registryMutex);
		auto it = s_registry.find(L);
		if (it == s_registry.end())
			return;

		players.reserve(it->second.size());
		for (VideoPlayer* player : it->second)
		{
			players.push_back(player);
			if (player)
			{
				player->m_ownerState = nullptr;
			}
		}
		s_registry.erase(it);
	}

	for (VideoPlayer* player : players)
	{
		if (player)
		{
			player->CloseVideo();
		}
	}
}

// ---- Lua Bindings ----

VideoPlayer* VideoPlayer::GetSelf(lua_State* L)
{
	VideoPlayer** ud = static_cast<VideoPlayer**>(lua_touserdata(L, 1));
	if (!ud || !*ud)
	{
		luaL_error(L, "Invalid VideoPlayer object");
		return nullptr;
	}
	return *ud;
}

int VideoPlayer::CreateAndPush(lua_State* L, const String& path)
{
	VideoPlayer** place = (VideoPlayer**)lua_newuserdata(L, sizeof(VideoPlayer*));
	*place = new VideoPlayer();

	if (!(*place)->OpenVideo(path))
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

int VideoPlayer::lNew(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	String absPath = Path::Absolute(path);
	return CreateAndPush(L, absPath);
}

int VideoPlayer::lNewSkin(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	String fullPath = "skins/" + g_application->GetCurrentSkin() + "/videos/" + path;
	fullPath = Path::Absolute(fullPath);
	return CreateAndPush(L, fullPath);
}

int VideoPlayer::l__index(lua_State* L)
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
		{"Play", lPlay},
		{"Pause", lPause},
		{"Seek", lSeek},
		{"SetLoop", lSetLoop},
		{"SetVolume", lSetVolume},
		{"Tick", lTick},
		{"GetImage", lGetImage},
		{"GetPosition", lGetPosition},
		{"GetDuration", lGetDuration},
		{"IsPlaying", lIsPlaying},
		{"HasEnded", lHasEnded},
		{"GetSize", lGetSize},
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

	return luaL_error(L, "VideoPlayer has no method: '%s'", fname);
}

int VideoPlayer::l__gc(lua_State* L)
{
	VideoPlayer** ud = static_cast<VideoPlayer**>(lua_touserdata(L, 1));
	if (ud && *ud)
	{
		delete *ud;
		*ud = nullptr;
	}
	return 0;
}

int VideoPlayer::lPlay(lua_State* L)
{
	GetSelf(L)->Play();
	return 0;
}

int VideoPlayer::lPause(lua_State* L)
{
	GetSelf(L)->Pause();
	return 0;
}

int VideoPlayer::lSeek(lua_State* L)
{
	double seconds = luaL_checknumber(L, 2);
	GetSelf(L)->Seek(seconds);
	return 0;
}

int VideoPlayer::lSetLoop(lua_State* L)
{
	bool loop = lua_toboolean(L, 2) != 0;
	GetSelf(L)->SetLoop(loop);
	return 0;
}

int VideoPlayer::lSetVolume(lua_State* L)
{
	float vol = (float)luaL_checknumber(L, 2);
	GetSelf(L)->SetVolume(vol);
	return 0;
}

int VideoPlayer::lTick(lua_State* L)
{
	float dt = (float)luaL_checknumber(L, 2);
	GetSelf(L)->Tick(dt);
	return 0;
}

int VideoPlayer::lGetImage(lua_State* L)
{
	lua_pushinteger(L, GetSelf(L)->GetImageHandle());
	return 1;
}

int VideoPlayer::lGetPosition(lua_State* L)
{
	lua_pushnumber(L, GetSelf(L)->GetPosition());
	return 1;
}

int VideoPlayer::lGetDuration(lua_State* L)
{
	lua_pushnumber(L, GetSelf(L)->GetDuration());
	return 1;
}

int VideoPlayer::lIsPlaying(lua_State* L)
{
	lua_pushboolean(L, GetSelf(L)->IsPlaying());
	return 1;
}

int VideoPlayer::lHasEnded(lua_State* L)
{
	lua_pushboolean(L, GetSelf(L)->HasEnded());
	return 1;
}

int VideoPlayer::lGetSize(lua_State* L)
{
	VideoPlayer* self = GetSelf(L);
	lua_pushinteger(L, self->GetWidth());
	lua_pushinteger(L, self->GetHeight());
	return 2;
}

int VideoPlayer::lDispose(lua_State* L)
{
	VideoPlayer* self = GetSelf(L);
	self->CloseVideo();
	return 0;
}

#endif // ENABLE_VIDEO
