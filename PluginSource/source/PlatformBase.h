#pragma once

// Standard base includes, defines that indicate our current platform, etc.

#include <stddef.h>


// Which platform we are on?
// UNITY_WIN - Windows (regular win32)
// UNITY_OSX - Mac OS X
// UNITY_LINUX - Linux
// UNITY_IOS - iOS
// UNITY_TVOS - tvOS
// UNITY_ANDROID - Android
// UNITY_METRO - WSA or UWP
// UNITY_WEBGL - WebGL
// UNITY_EMBEDDED_LINUX - EmbeddedLinux OpenGLES
// UNITY_EMBEDDED_LINUX_GL - EmbeddedLinux OpenGLCore
#if _MSC_VER
	#define UNITY_WIN 1
#elif defined(__APPLE__)
    #if TARGET_OS_TV
        #define UNITY_TVOS 1
    #elif TARGET_OS_IOS
        #define UNITY_IOS 1
	#else
		#define UNITY_OSX 1
	#endif
#elif defined(__ANDROID__)
	#define UNITY_ANDROID 1
#elif defined(UNITY_METRO) || defined(UNITY_LINUX) || defined(UNITY_WEBGL) || defined (UNITY_EMBEDDED_LINUX) || defined (UNITY_EMBEDDED_LINUX_GL)
	// these are defined externally
#elif defined(__EMSCRIPTEN__)
	// this is already defined in Unity 5.6
	#define UNITY_WEBGL 1
#else
	#error "Unknown platform!"
#endif



// Which graphics device APIs we possibly support?
#if UNITY_METRO
	#define SUPPORT_D3D11 0
	#if WINDOWS_UWP
		#define SUPPORT_D3D12 0
	#endif
#elif UNITY_WIN
	#define SUPPORT_D3D11 0
	#define SUPPORT_D3D12 0
	#define SUPPORT_OPENGL_UNIFIED 0
	#define SUPPORT_OPENGL_CORE 0
	#define SUPPORT_VULKAN 1 // Requires Vulkan SDK to be installed
#elif UNITY_IOS || UNITY_TVOS || UNITY_ANDROID || UNITY_WEBGL || UNITY_EMBEDDED_LINUX
	#ifndef SUPPORT_OPENGL_ES
		#define SUPPORT_OPENGL_ES 0
	#endif
	#define SUPPORT_OPENGL_UNIFIED SUPPORT_OPENGL_ES
	#ifndef SUPPORT_VULKAN
		#define SUPPORT_VULKAN 0
	#endif
#elif UNITY_OSX || UNITY_LINUX || UNITY_EMBEDDED_LINUX_GL
	#define SUPPORT_OPENGL_UNIFIED 0
	#define SUPPORT_OPENGL_CORE 0
#endif

#if UNITY_IOS || UNITY_TVOS || UNITY_OSX
	#define SUPPORT_METAL 1
#endif



// COM-like Release macro
#ifndef SAFE_RELEASE
	#define SAFE_RELEASE(a) if (a) { a->Release(); a = NULL; }
#endif

