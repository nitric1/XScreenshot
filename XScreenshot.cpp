#include "Common.h"
#include "XScreenshot.h"

namespace XScreenshot
{
	namespace
	{
		class CurlInitialize
		{
		public:
			CurlInitialize()
			{
				curl_global_init(CURL_GLOBAL_ALL);
			}

		public:
			static CurlInitialize &instance()
			{
				static CurlInitialize inst;
				return inst;
			}
		};
	}

	ImgurService::ImgurService()
	{
		CurlInitialize::instance();

		curl = curl_easy_init();
		if(curl == nullptr)
			; // TODO: Error

		// URL: PUT https://api.imgur.com/2/upload.json

		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1l);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, (pluginName + "/" + pluginVersion + " Image Uploader for Imgur").c_str());
		curl_easy_setopt(curl, CURLOPT_READDATA, static_cast<void *>(&rcbd));
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, &curlReadCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void *>(&wcbd));
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curlWriteCallback);

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0l);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0l);

		curl_easy_setopt(curl, CURLOPT_URL, "http://api.imgur.com/2/upload.json");
	}

	ImgurService::~ImgurService()
	{
		curl_easy_cleanup(curl);
	}

	size_t ImgurService::curlReadCallback(void *data, size_t size, size_t nmemb, void *param)
	{
		CurlReadCallbackData *cbd = static_cast<CurlReadCallbackData *>(param);
		size_t realSize = size * nmemb;
		uint8_t *dest = static_cast<uint8_t *>(data);

		if(realSize + cbd->pos > cbd->data.size())
			realSize = cbd->data.size() - cbd->pos;

		memcpy_s(data, size * nmemb, cbd->data.data() + cbd->pos, realSize);

		return realSize;
	}

	size_t ImgurService::curlWriteCallback(void *data, size_t size, size_t nmemb, void *param)
	{
		CurlWriteCallbackData *cbd = static_cast<CurlWriteCallbackData *>(param);
		size_t realSize = size * nmemb;
		uint8_t *src = static_cast<uint8_t *>(data);

		cbd->data.insert(cbd->data.end(), src, src + realSize);

		return realSize;
	}

	std::string ImgurService::upload(const std::vector<uint8_t> &data)
	{
		rcbd.data = data;
		rcbd.pos = 0;

		wcbd.data.clear();

		curl_httppost *post = nullptr, *lastpost = nullptr;
		curl_formadd(&post, &lastpost,
			CURLFORM_COPYNAME, "image",
			CURLFORM_BUFFER, "XScreenshot.png",
			CURLFORM_BUFFERPTR, data.data(),
			CURLFORM_BUFFERLENGTH, static_cast<long>(data.size()),
			CURLFORM_FILENAME, "XScreenshot.png",
			CURLFORM_END);
		curl_formadd(&post, &lastpost,
			CURLFORM_COPYNAME, "key",
			CURLFORM_COPYCONTENTS, utfConv(readConfig(L"KEY", utfConv(defaultOptionValue["KEY"]))).c_str(),
			CURLFORM_END);
		curl_formadd(&post, &lastpost,
			CURLFORM_COPYNAME, "type",
			CURLFORM_COPYCONTENTS, "file",
			CURLFORM_END);
		curl_formadd(&post, &lastpost,
			CURLFORM_COPYNAME, "name",
			CURLFORM_COPYCONTENTS, "XScreenshot.png",
			CURLFORM_END);

		curl_slist *header = nullptr;
		header = curl_slist_append(header, "Expect:");

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);

		CURLcode ret = curl_easy_perform(curl);

		curl_formfree(post);
		curl_slist_free_all(header);

		if(ret != CURLE_OK)
			return std::string();

		for(; !wcbd.data.empty() && wcbd.data.back() == '\0'; wcbd.data.pop_back());

		Json::Value root;
		Json::Reader reader;
		if(!reader.parse(std::string(std::begin(wcbd.data), std::end(wcbd.data)), root))
			return std::string();

		return root["upload"]["links"]["original"].asString();
	}

	HINSTANCE instance;

	std::string pluginName = "XScreenshot";
	std::string pluginVersion = "1.0.0";
	std::string pluginDescription = "Captures screenshot and shares it.";

	const std::string usageXS = "Usage: XS | \xE3\x85\x8C\xE3\x84\xB4, captures screenshot and says the shared image URI.";
	const std::string usageXSCFG = "Usage: XSCFG <name> [<value> | default], sets a XScreenshot option."
		" <name> can be: SERVICE, KEY."
		" If <value> is empty, shows the current value."
		" If <value> is \"default\", restores default value."
		" SERVICE option must be: IMGUR (currently only available service)."
		" KEY option is an API key or account information provided by the sharing service, which can be \"null\" if the service does not require any key."; // TODO: Image Services

	std::wstring pluginConfigPath;

	const std::string printConfigValue = "XS - Current value of option \"%s\" is \"%s\".";
	const std::string succeededConfigSet = "XS - Option \"%s\" is set successfully.";
	const std::string failedConfigSet = "XS - Failed to set option \"%s\".";

	std::map<std::string, std::string> defaultOptionValue;
	std::map<std::string, ServiceFactory *> services;

	std::wstring readConfig(const std::wstring &name, const std::wstring &def)
	{
		std::vector<wchar_t> buf(0x500);
		static std::wstring appName(utfConv(pluginName));
		GetPrivateProfileStringW(appName.c_str(), name.c_str(), def.c_str(), &*buf.begin(), 0x500, pluginConfigPath.c_str());
		return std::wstring(&*buf.begin());
	}

	bool writeConfig(const std::wstring &name, const std::wstring &value)
	{
		static std::wstring appName(utfConv(pluginName));
		return WritePrivateProfileStringW(appName.c_str(), name.c_str(), (L"\"" + value + L"\"").c_str(), pluginConfigPath.c_str()) != 0;
	}

	std::string utfConv(const std::wstring &str)
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
		if(size <= 0)
			return std::string();

		std::vector<char> buf(static_cast<size_t>(size));

		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &*buf.begin(), size, NULL, NULL);
		return std::string(buf.begin(), buf.end());
	}

	std::wstring utfConv(const std::string &str)
	{
		int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
		if(size <= 0)
			return std::wstring();

		std::vector<wchar_t> buf(static_cast<size_t>(size));

		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &*buf.begin(), size);
		return std::wstring(buf.begin(), buf.end());
	}

	void initModuleInfo()
	{
		wchar_t buf[512];

		if(GetModuleFileNameW(instance, buf, 512) == 0)
		{
			throw(std::runtime_error("GetModuleFileName failed"));
		}

		pluginConfigPath = buf;
		size_t pos;
		if((pos = pluginConfigPath.rfind(L'\\')) != pluginConfigPath.npos)
		{
			pluginConfigPath.erase(pos);
			pluginConfigPath += L"\\XS.ini";
		}
		else
			pluginConfigPath = L".\\XS.ini";

		defaultOptionValue.insert(std::make_pair("SERVICE", "IMGUR"));
		defaultOptionValue.insert(std::make_pair("KEY", "null"));

		services.insert(std::make_pair("IMGUR", &ServiceFactoryImpl<ImgurService>::instance()));
	}

	void uninitModuleInfo()
	{
	}

	namespace
	{
		std::vector<uint8_t> capture(int &w, int &h)
		{
			HWND desktop = GetDesktopWindow();
			RECT rc;
			HDC desktopdc, memdc;
			HBITMAP bitmap, oldBitmap;

			GetClientRect(desktop, &rc);
			desktopdc = GetDC(desktop);

			w = std::abs(rc.right - rc.left);
			h = std::abs(rc.bottom - rc.top);

			memdc = CreateCompatibleDC(desktopdc);
			bitmap = CreateCompatibleBitmap(desktopdc, w, h);

			oldBitmap = static_cast<HBITMAP>(SelectObject(memdc, bitmap));
			BitBlt(memdc, 0, 0, w, h, desktopdc, 0, 0, SRCCOPY);
			SelectObject(memdc, oldBitmap);

			BITMAPINFO bi = {0, };
			bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bi.bmiHeader.biWidth = w;
			bi.bmiHeader.biHeight = h;
			bi.bmiHeader.biPlanes = 1;
			bi.bmiHeader.biBitCount = 24;
			bi.bmiHeader.biCompression = BI_RGB;

			GetDIBits(memdc, bitmap, 0, h, nullptr, &bi, DIB_RGB_COLORS);

			std::vector<uint8_t> bitmapData(bi.bmiHeader.biSizeImage);

			GetDIBits(memdc, bitmap, 0, h, bitmapData.data(), &bi, DIB_RGB_COLORS);

			DeleteObject(bitmap);
			DeleteDC(memdc);
			ReleaseDC(desktop, desktopdc);

			return bitmapData;
		}

		void writeFile(png_structp pptr, png_bytep data, png_size_t len)
		{
			HANDLE file = png_get_io_ptr(pptr);
			DWORD written;
			WriteFile(file, data, static_cast<DWORD>(len), &written, nullptr);
		}

		void flushFile(png_structp)
		{
		}

		void writeVector(png_structp pptr, png_bytep data, png_size_t len)
		{
			std::vector<uint8_t> *vec = static_cast<std::vector<uint8_t> *>(png_get_io_ptr(pptr));
			vec->insert(vec->end(), data, data + len);
		}

		void flushVector(png_structp)
		{
		}
	}

	int xsCapture(char **word, char **eol, void *)
	{
		std::vector<uint8_t> data;

		int w, h;
		std::vector<uint8_t> bitmap = capture(w, h);
		uint8_t **writeData;

		size_t lineBytes = bitmap.size() / h;
		writeData = new uint8_t *[h];
		for(int i = 0; i < h; ++ i)
		{
			writeData[i] = new uint8_t[w * 3];
			memcpy(writeData[i], bitmap.data() + (h - i - 1) * lineBytes, w * 3);
		}

		png_structp pptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		png_infop iptr = png_create_info_struct(pptr);

		if(setjmp(png_jmpbuf(pptr)))
		{
			for(int i = 0; i < h; ++ i)
				delete [] writeData[i];
			delete [] writeData;

			png_destroy_write_struct(&pptr, &iptr);

			xchat_print(ph, "XS - Error during compressing the screenshot into PNG.");
			return XCHAT_EAT_ALL;
		}

		png_set_write_fn(pptr, &data, &writeVector, &flushVector);
		png_set_IHDR(pptr, iptr, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_bgr(pptr);
		png_set_rows(pptr, iptr, writeData);
		png_write_png(pptr, iptr, PNG_TRANSFORM_IDENTITY, nullptr);
		png_write_end(pptr, iptr);
		png_destroy_write_struct(&pptr, &iptr);

		for(int i = 0; i < h; ++ i)
			delete [] writeData[i];
		delete [] writeData;

		auto it = services.find(utfConv(readConfig(L"SERVICE", utfConv(defaultOptionValue["SERVICE"]))));
		if(it == std::end(services))
		{
			xchat_print(ph, "XS - The specified service is not supported. Check XSCFG.");
			return XCHAT_EAT_ALL;
		}

		std::shared_ptr<Service> svc(it->second->create());
		std::string uri = svc->upload(data);
		if(uri.empty())
		{
			xchat_print(ph, "XS - Cannot upload the screenshot.");
			return XCHAT_EAT_ALL;
		}

		xchat_commandf(ph, "SAY %s", uri.c_str()); // TODO: Method

		return XCHAT_EAT_ALL;
	}

	int xsConfig(char **word, char **eol, void *)
	{
		if(eol[2][0] == '\0') // XSCFG <name> [<value>]
		{
			xchat_print(ph, usageXSCFG.c_str());
			return XCHAT_EAT_ALL;
		}

		std::string name(word[2]);
		std::string value(eol[3]);

		std::transform(std::begin(name), std::end(name), std::begin(name), toupper);

		if(name == "SERVICE")
		{
			std::transform(std::begin(value), std::end(value), std::begin(value), toupper);
			if(!value.empty() && (value == "DEFAULT" || services.find(value) == std::end(services)))
				value = defaultOptionValue[name];
		}
		else if(name == "KEY")
		{
			if(value == "default")
				value = defaultOptionValue[name];
		}

		if(value.empty())
			xchat_printf(ph, printConfigValue.c_str(), name.c_str(), utfConv(readConfig(utfConv(name), utfConv(defaultOptionValue[name]))).c_str());
		else if(writeConfig(utfConv(name), utfConv(value)))
			xchat_printf(ph, succeededConfigSet.c_str(), name.c_str());
		else
			xchat_printf(ph, failedConfigSet.c_str(), name.c_str());

		return XCHAT_EAT_ALL;
	}
}

xchat_plugin *ph;

BOOL __stdcall DllMain(HINSTANCE inst, DWORD reason, void *)
{
	try
	{
		switch(reason)
		{
		case DLL_PROCESS_ATTACH:
			{
				XScreenshot::instance = inst;
				XScreenshot::initModuleInfo();
			}
			break;

		case DLL_PROCESS_DETACH:
			{
				XScreenshot::uninitModuleInfo();
			}
			break;
		}
	}
	catch(std::exception &ex)
	{
		MessageBoxA(nullptr, (std::string("Error: ") + ex.what()).c_str(), "Error", MB_ICONERROR | MB_OK);
		return FALSE;
	}

	return TRUE;
}

void xchat_plugin_get_info(char **name, char **desc, char **version, void **)
{
	*name = &*std::begin(XScreenshot::pluginName);
	*desc = &*std::begin(XScreenshot::pluginDescription);
	*version = &*std::begin(XScreenshot::pluginVersion);
}

int xchat_plugin_init(xchat_plugin *iph, char **name, char **desc, char **version, char *arg)
{
	ph = iph;

	*name = &*std::begin(XScreenshot::pluginName);
	*desc = &*std::begin(XScreenshot::pluginDescription);
	*version = &*std::begin(XScreenshot::pluginVersion);

	xchat_hook_command(ph, "XS", XCHAT_PRI_NORM, &XScreenshot::xsCapture, XScreenshot::usageXS.c_str(), nullptr);
	xchat_hook_command(ph, "XSCFG", XCHAT_PRI_NORM, &XScreenshot::xsConfig, XScreenshot::usageXSCFG.c_str(), nullptr);

	xchat_printf(ph, "%s %s loaded.", XScreenshot::pluginName.c_str(), XScreenshot::pluginVersion.c_str());

	return 1;
}

int xchat_plugin_deinit()
{
	return 1;
}
