// copyright britzl
// Extension lib defines
#define LIB_NAME "Delay"
#define MODULE_NAME "delay"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <stdlib.h>

struct Listener {
	Listener() {
		m_L = 0;
		m_Callback = LUA_NOREF;
		m_Self = LUA_NOREF;
	}
	lua_State* m_L;
	int        m_Callback;
	int        m_Self;
};

struct Delay {
	double seconds;
	double end;
	int repeating;
	unsigned int id;
	Listener listener;
};

static unsigned int g_SequenceId = 0;
static const int TIMERS_CAPACITY = 128;
static dmArray<Delay*> g_Delays;

static dmArray<int> g_DelaysToTrigger;
static dmArray<int> g_DelaysToRemove;


/**
 * Create a listener instance from a function on the stack
 */
static Listener CreateListener(lua_State* L, int index) {
	luaL_checktype(L, index, LUA_TFUNCTION);
	lua_pushvalue(L, index);
	int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

	Listener listener;
	listener.m_L = dmScript::GetMainThread(L);
	listener.m_Callback = cb;
	dmScript::GetInstance(L);
	listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
	return listener;
}

#ifdef DM_PLATFORM_WINDOWS
#include <windows.h>
// Timestamp calculation from luasocket timeout.c
static double GetTimestamp(void) {
	FILETIME ft;
	double t;
	GetSystemTimeAsFileTime(&ft);
	/* Windows file time (time since January 1, 1601 (UTC)) */
	t  = ft.dwLowDateTime/1.0e7 + ft.dwHighDateTime*(4294967296.0/1.0e7);
	/* convert to Unix Epoch time (time since January 1, 1970 (UTC)) */
	return (t - 11644473600.0);
}
#else
#include <sys/time.h>
static double GetTimestamp(void) {
	struct timeval v;
	gettimeofday(&v, (struct timezone *) NULL);
	/* Unix Epoch time (time since January 1, 1970 (UTC)) */
	return v.tv_sec + v.tv_usec/1.0e6;
}
#endif

/**
 * Create a new delay
 */
static Delay* CreateDelay(Listener listener, double seconds, int repeating) {
	Delay *delay = (Delay*)malloc(sizeof(Delay));
	delay->seconds = seconds;
	delay->id = g_SequenceId++;
	delay->listener = listener;
	delay->repeating = repeating;
	delay->end = GetTimestamp() + seconds;

	if (g_Delays.Full()) {
		g_Delays.SetCapacity(g_Delays.Capacity() + TIMERS_CAPACITY);
	}
	g_Delays.Push(delay);
	return delay;
}

/**
 * Get a delay
 */
static Delay* GetDelay(int id) {
	for (int i = g_Delays.Size() - 1; i >= 0; i--) {
		Delay* delay = g_Delays[i];
		if (delay->id == id) {
			return delay;
		}
	}
	return 0;
}

/**
 * Create a delay that will trigger after a certain number of seconds
 */
static int Seconds(lua_State* L) {
	int top = lua_gettop(L);

	const double seconds = luaL_checknumber(L, 1);
	const Listener listener = CreateListener(L, 2);

	Delay *delay = CreateDelay(listener, seconds, 0);

	lua_pushinteger(L, delay->id);

	assert(top + 1 == lua_gettop(L));
	return 1;
}

/**
 * Create a delay that will trigger repeatedly with a fixed interval
 */
static int Repeating(lua_State* L) {
	int top = lua_gettop(L);

	const double seconds = luaL_checknumber(L, 1);
	const Listener listener = CreateListener(L, 2);

	Delay *delay = CreateDelay(listener, seconds, 1);

	lua_pushinteger(L, delay->id);

	assert(top + 1 == lua_gettop(L));
	return 1;
}

/**
 * Remove a delay from the list of delays and free it's memory
 */
static void Remove(int id) {
	for (int i = g_Delays.Size() - 1; i >= 0; i--) {
		Delay* delay = g_Delays[i];
		if (delay->id == id) {
			g_Delays.EraseSwap(i);
			free(delay);
			break;
		}
	}
}

/**
 * Cancel a delay
 */
static int Cancel(lua_State* L) {
	int top = lua_gettop(L);

	int id = luaL_checkint(L, 1);

	Remove(id);

	// Remove the delay from the temporary lists used during Update()
	// This could be the case if a finished delay is canceling other
	// delays in it's callback function
	for (int i = g_DelaysToRemove.Size() - 1; i >= 0; i--) {
		int id_to_remove = g_DelaysToRemove[i];
		if (id_to_remove == id) {
			g_DelaysToRemove.EraseSwap(i);
			break;
		}
	}
	for (int i = g_DelaysToTrigger.Size() - 1; i >= 0; i--) {
		int id_to_trigger = g_DelaysToTrigger[i];
		if (id_to_trigger == id) {
			g_DelaysToTrigger.EraseSwap(i);
			break;
		}
	}

	assert(top + 0 == lua_gettop(L));
	return 0;
}

/**
 * Cancel all delays
 */
static int CancelAll(lua_State* L) {
	int top = lua_gettop(L);

	for (int i = g_Delays.Size() - 1; i >= 0; i--) {
		Delay* delay = g_Delays[i];
		g_Delays.EraseSwap(i);
		free(delay);
	}

	// Make sure to clear the temporary lists as well
	// In case of a delay callback calling delay.cancel_all()
	while(!g_DelaysToRemove.Empty()) {
		g_DelaysToRemove.Pop();
	}
	while(!g_DelaysToTrigger.Empty()) {
		g_DelaysToTrigger.Pop();
	}

	assert(top + 0 == lua_gettop(L));
	return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] = {
	{ "seconds", Seconds },
	{ "repeating", Repeating },
	{ "cancel", Cancel },
	{ "cancel_all", CancelAll },
	{ 0, 0 }
};

static void LuaInit(lua_State* L) {
	int top = lua_gettop(L);

	luaL_register(L, MODULE_NAME, Module_methods);

	lua_pop(L, 1);
	assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeDelayExtension(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeDelayExtension(dmExtension::Params* params) {
	LuaInit(params->m_L);
	printf("Registered %s Extension\n", MODULE_NAME);
	return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDelayExtension(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result UpdateDelayExtension(dmExtension::Params* params) {
	const double now = GetTimestamp();

	// Iterate over all delays to find the ones that should be triggered
	// and possibly also removed this frame (unless they are repeating)
	for (int i = g_Delays.Size() - 1; i >= 0; i--) {
		Delay* delay = g_Delays[i];
		if (now >= delay->end) {
			if (g_DelaysToTrigger.Full()) {
				g_DelaysToTrigger.SetCapacity(g_DelaysToTrigger.Capacity() + TIMERS_CAPACITY);
			}
			g_DelaysToTrigger.Push(delay->id);

			if (delay->repeating == 1) {
				delay->end += delay->seconds;
			} else {
				if (g_DelaysToRemove.Full()) {
					g_DelaysToRemove.SetCapacity(g_DelaysToRemove.Capacity() + TIMERS_CAPACITY);
				}
				g_DelaysToRemove.Push(delay->id);
			}
		}
	}

	// Trigger delay callbacks
	while(!g_DelaysToTrigger.Empty()) {
		int id = g_DelaysToTrigger.Back();
		g_DelaysToTrigger.Pop();
		Delay* delay = GetDelay(id);
		if (delay) {
			lua_State* L = delay->listener.m_L;
			int top = lua_gettop(L);

			lua_rawgeti(L, LUA_REGISTRYINDEX, delay->listener.m_Callback);
			lua_rawgeti(L, LUA_REGISTRYINDEX, delay->listener.m_Self);
			lua_pushvalue(L, -1);
			dmScript::SetInstance(L);
			if (!dmScript::IsInstanceValid(L)) {
				lua_pop(L, 2);
			} else {
				lua_pushinteger(L, delay->id);
				int ret = lua_pcall(L, 2, LUA_MULTRET, 0);
				if (ret != 0) {
					dmLogError("Error running delay callback: %s", lua_tostring(L, -1));
					lua_pop(L, 1);
				}
			}
			assert(top == lua_gettop(L));
		}
	}

	// Remove delays
	while(!g_DelaysToRemove.Empty()) {
		int id = g_DelaysToRemove.Back();
		g_DelaysToRemove.Pop();
		Remove(id);
	}

	return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDelayExtension(dmExtension::Params* params) {
	while(!g_Delays.Empty()) {
		Delay* delay = g_Delays.Back();
		Remove(delay->id);
	}
	return dmExtension::RESULT_OK;
}


// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

DM_DECLARE_EXTENSION(Delay, LIB_NAME, AppInitializeDelayExtension, AppFinalizeDelayExtension, InitializeDelayExtension, UpdateDelayExtension, 0, FinalizeDelayExtension)
