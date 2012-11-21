#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

HMODULE mainLib, instance;

BOOL __stdcall DllMain(HINSTANCE inst, DWORD reason, void *)
{
	instance = inst;
	return TRUE;
}

namespace
{
	void load()
	{
		wchar_t curPath[512], *p;
		GetModuleFileNameW(instance, curPath, 512);
		p = wcsrchr(curPath, L'\\');
		if(p != nullptr)
			*++ p = L'\0';
		else // TODO: Failed
		{
		}
		wcscat_s(curPath, L"XScreenshot\\");
		SetDllDirectoryW(curPath);
		mainLib = LoadLibraryW(L"XScreenshot.dll");
	}
}

typedef void (*GetInfoFn)(char **, char **, char **, void **);
typedef int (*InitFn)(void *, char **, char **, char **, char *);
typedef int (*DeinitFn)();

extern "C"
{
	void xchat_plugin_get_info(char **name, char **desc, char **version, void **reserved)
	{
		if(!mainLib)
			load();
		GetInfoFn getInfo = reinterpret_cast<GetInfoFn>(GetProcAddress(mainLib, "xchat_plugin_get_info"));
		getInfo(name, desc, version, reserved);
	}

	void hexchat_plugin_get_info(char **name, char **desc, char **version, void **reserved)
	{
		xchat_plugin_get_info(name, desc, version, reserved);
	}

	int xchat_plugin_init(void *ph, char **name, char **desc, char **version, char *arg)
	{
		if(!mainLib)
			load();
		InitFn init = reinterpret_cast<InitFn>(GetProcAddress(mainLib, "xchat_plugin_init"));
		return init(ph, name, desc, version, arg);
	}

	int hexchat_plugin_init(void *ph, char **name, char **desc, char **version, char *arg)
	{
		return xchat_plugin_init(ph, name, desc, version, arg);
	}

	int xchat_plugin_deinit()
	{
		if(!mainLib)
			load();
		DeinitFn deinit = reinterpret_cast<DeinitFn>(GetProcAddress(mainLib, "xchat_plugin_deinit"));
		int ret = deinit();
		if(mainLib)
		{
			FreeLibrary(mainLib);
			mainLib = nullptr;
		}
		return ret;
	}

	int hexchat_plugin_deinit()
	{
		return xchat_plugin_deinit();
	}
}
