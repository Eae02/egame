#ifdef __linux__

#include "FontConfig.hpp"

#include <fontconfig/fontconfig.h>
#include <dlfcn.h>

namespace eg
{
	static FcConfig* g_fontConfig = nullptr;
	static void* g_fontConfigLibrary = nullptr;
	
	namespace fc
	{
		FcConfig* (*InitLoadConfigAndFonts)();
		void (*ConfigDestroy)(FcConfig* config);
		FcPattern* (*NameParse)(const FcChar8* name);
		void (*PatternDestroy)(FcPattern* pattern);
		
		FcBool (*ConfigSubstitute)(FcConfig* config, FcPattern* p, FcMatchKind kind);
		void (*DefaultSubstitute)(FcPattern* pattern);
		FcPattern* (*FontMatch)(FcConfig* config, FcPattern* p, FcResult* result);
		FcResult (*PatternGetString)(const FcPattern* p, const char* object, int n, FcChar8** s);
	}
	
#define LOAD_FC_ENTRY_POINT(name) fc::name = reinterpret_cast<decltype(fc::name)>(dlsym(g_fontConfigLibrary, "Fc" #name));
	
	void InitPlatformFontConfig()
	{
		if (g_fontConfigLibrary != nullptr)
			return;
		
		g_fontConfigLibrary = dlopen("libfontconfig.so", RTLD_LAZY);
		if (g_fontConfigLibrary == nullptr)
			return;
		
		LOAD_FC_ENTRY_POINT(InitLoadConfigAndFonts)
		LOAD_FC_ENTRY_POINT(ConfigDestroy)
		LOAD_FC_ENTRY_POINT(NameParse)
		LOAD_FC_ENTRY_POINT(PatternDestroy)
		LOAD_FC_ENTRY_POINT(ConfigSubstitute)
		LOAD_FC_ENTRY_POINT(DefaultSubstitute)
		LOAD_FC_ENTRY_POINT(FontMatch)
		LOAD_FC_ENTRY_POINT(PatternGetString)
		
		g_fontConfig = fc::InitLoadConfigAndFonts();
	}
	
	void DestroyPlatformFontConfig()
	{
		if (g_fontConfigLibrary != nullptr)
		{
			fc::ConfigDestroy(g_fontConfig);
			dlclose(g_fontConfigLibrary);
			g_fontConfigLibrary = nullptr;
		}
	}
	
	struct FcPatternDeleter
	{
		inline void operator()(FcPattern* pattern) const
		{
			fc::PatternDestroy(pattern);
		}
	};
	
	std::string GetFontPathByName(const char* name)
	{
		if (g_fontConfigLibrary == nullptr)
			return { };
		
		using FcPatternPtr = std::unique_ptr<FcPattern, FcPatternDeleter>;
		
		FcPatternPtr pattern(fc::NameParse(reinterpret_cast<const FcChar8*>(name)));
		fc::ConfigSubstitute(g_fontConfig, pattern.get(), FcMatchPattern);
		fc::DefaultSubstitute(pattern.get());
		
		std::string result;
		
		FcResult matchResult;
		FcPatternPtr font(fc::FontMatch(g_fontConfig, pattern.get(), &matchResult));
		if (font)
		{
			FcChar8* file = nullptr;
			if (fc::PatternGetString(font.get(), FC_FILE, 0, &file) == FcResultMatch)
			{
				return reinterpret_cast<const char*>(file);
			}
		}
		
		return { };
	}
}

#endif
