newoption {
	trigger		= "disable-fingerprint",
	description	= "Don't add fingerprinting features."
}

solution "graalrelay"
	configurations { "Debug", "Release" }
	platforms { "native", "x32", "x64" }
	flags { "EnableSSE", "EnableSSE2", "StaticRuntime" }

	if _OPTIONS["disable-fingerprint"] then
	defines { "DISABLE_FINGERPRINT" }
	end

	project "client"
		kind "ConsoleApp"
		-- kind "WindowedApp"
		language "C++"
		location "projects"
		targetdir "../bin"
		targetname "graalrelay"
		flags { "Symbols", "Unicode" }
		files { "../client/include/**.h", "../client/src/**.cpp" }
		includedirs { "../client/include" }
		
		-- Dependencies.
		files { "../dependencies/zlib/**" }
		files { "../dependencies/bzip2/**" }
		includedirs { "../dependencies/zlib" }
		includedirs { "../dependencies/bzip2" }

		-- Libraries.
		configuration "windows"
			links { "ws2_32" }

		-- Windows defines.
		configuration "windows"
			defines { "WIN32", "_WIN32" }
		configuration { "windows", "x64" }
			defines { "WIN64", "_WIN64" }

		-- Linux static link.
		configuration { "linux or bsd or solaris" }
			linkoptions { "-static-libstdc++" }

		-- Debug options.
		configuration "Debug"
			defines { "DEBUG" }
			flags { "NoEditAndContinue" }
			targetsuffix "_d"
		
		-- Release options.
		configuration "Release"
			defines { "NDEBUG" }
			flags { "OptimizeSpeed" }
