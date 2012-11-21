#pragma once

namespace XScreenshot
{
	class Service
	{
	public:
		virtual ~Service() = 0 {}

	public:
		virtual std::string upload(const std::vector<uint8_t> &) = 0;
	};

	class ImgurService : public Service
	{
	private:
		struct CurlReadCallbackData
		{
			std::vector<uint8_t> data;
			size_t pos;
		};

		struct CurlWriteCallbackData
		{
			std::vector<uint8_t> data;
		};

		// CurlReadCallbackData rcbd;
		CurlWriteCallbackData wcbd;
		CURL *curl;

	public:
		ImgurService();
		virtual ~ImgurService();

	public:
		// static size_t curlReadCallback(void *, size_t, size_t, void *);
		static size_t curlWriteCallback(void *, size_t, size_t, void *);
		virtual std::string upload(const std::vector<uint8_t> &);
	};

	class ServiceFactory
	{
	protected:
		virtual ~ServiceFactory() = 0 {}

	public:
		virtual Service *create() = 0;
	};

	template<typename T>
	class ServiceFactoryImpl : public ServiceFactory
	{
	protected:
		virtual ~ServiceFactoryImpl() {}

	public:
		static ServiceFactoryImpl<T> &instance()
		{
			static ServiceFactoryImpl<T> inst;
			return inst;
		}

		virtual T *create()
		{
			return new T;
		}
	};

	extern HINSTANCE instance;

	extern std::string pluginName;
	extern std::string pluginVersion;
	extern std::string pluginDescription;

	extern const std::string usageXS;
	extern const std::string usageXSCFG;

	extern std::wstring pluginConfigPath;

	extern const std::string printConfigValue;
	extern const std::string succeededConfigSet;
	extern const std::string failedConfigSet;

	extern std::map<std::string, std::string> defaultOptionValue;
	extern std::map<std::string, ServiceFactory *> services;

	std::wstring readConfig(const std::wstring &, const std::wstring & = std::wstring());
	bool writeConfig(const std::wstring &, const std::wstring &);

	std::string utfConv(const std::wstring &);
	std::wstring utfConv(const std::string &);

	void initModuleInfo();
	void uninitModuleInfo();

	int xsCapture(char **, char **, void *);
	int xsConfig(char **, char **, void *);
}

extern xchat_plugin *ph;

extern "C"
{
	void xchat_plugin_get_info(char **, char **, char **, void **);
	void hexchat_plugin_get_info(char **, char **, char **, void **);
	int xchat_plugin_init(xchat_plugin *, char **, char **, char **, char *);
	int hexchat_plugin_init(xchat_plugin *, char **, char **, char **, char *);
	int xchat_plugin_deinit();
	int hexchat_plugin_deinit();
};
