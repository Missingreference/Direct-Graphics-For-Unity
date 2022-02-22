///Referenced native code and implementation from https://docs.unity3d.com/Manual/NativePlugins.html and the github repository.
///WARNING: The current programmer is not well versed in C/C++, Objective-C or Graphics APIs. There may be memory leaks or unforseen bugs.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
#if UNITY_EDITOR
using UnityEditor;
#endif
using UnityEngine;
using UnityEngine.Rendering;

using Elanetic.Graphics.Internal;

namespace Elanetic.Graphics
{
    /// <summary>
    /// Fast functions to do work on the GPU. Uses a native plugin for each Graphics API to send command buffers on the GPU.
    /// Platform support is limited.
    /// </summary>
    [InitializeOnLoad]
    static public class DirectGraphics
    {
        #region External Functions
#if(UNITY_IOS || UNITY_TVOS || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
        [DllImport("RenderingPlugin")]
#endif
        static private extern void CopyTextures(IntPtr texture, int x, int y, int w, int h, IntPtr texture2, int x2, int y2);

#if(UNITY_IOS || UNITY_TVOS || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
        [DllImport("RenderingPlugin")]
#endif
        static private extern int CreateNativeTexture(int width, int height, int format);

#if(UNITY_IOS || UNITY_TVOS || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
        [DllImport("RenderingPlugin")]
#endif
        static internal extern IntPtr DestroyNativeTexture(int textureIndex);

#if(UNITY_IOS || UNITY_TVOS || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
        [DllImport("RenderingPlugin")]
#endif
        static internal extern IntPtr GetNativeTexturePointer(int textureIndex);

#if(UNITY_IOS || UNITY_TVOS || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
        [DllImport("RenderingPlugin")]
#endif
        static private extern void SetTextureColor(float red, float green, float blue, float alpha, IntPtr targetTexture);
        #endregion

        static private readonly GraphicsDeviceType[] SUPPORTED_GRAPHICS_API =
        {
            GraphicsDeviceType.Vulkan,
            GraphicsDeviceType.Metal,
        };

        static private int[] TEXTURE_FORMAT_LOOKUP;
        static private List<DirectTexture2D> m_AllTextures = new List<DirectTexture2D>(500);

        static DirectGraphics()
        {
            switch(SystemInfo.graphicsDeviceType)
            {
                case GraphicsDeviceType.Metal:
                    TEXTURE_FORMAT_LOOKUP = NativeTextureFormatLookup.METAL_LOOKUP;
                    break;
                case GraphicsDeviceType.Vulkan:
                    TEXTURE_FORMAT_LOOKUP = NativeTextureFormatLookup.VULKAN_LOOKUP;
                    break;
                default:
                    //Native implementation of the target Graphics API needs to be implemented.
                    throw new NotSupportedException("DirectGraphics is not supported for Graphics API '" + SystemInfo.graphicsDeviceType + "'.");
            }
#if UNITY_EDITOR
            EditorApplication.playModeStateChanged += OnPlayModeChanged;
#endif
            Application.quitting += DestroyAllTextures;
        }

        /// <summary>
        /// Create DirectTexture2D and Texture2D. Just like normal Texture2Ds call DirectTexture2D.Destroy to cleanup.
        /// The initialized pixel data is different based on platform. For example Vulkan clears the data to all zero and Metal uses garbage data(whatever was left behind).
        /// </summary>
        static public DirectTexture2D CreateTexture(int width, int height, TextureFormat textureFormat)
        {
#if SAFE_EXECUTION
            if(((int)textureFormat) < 0 || ((int)textureFormat) > 74)
            {
                throw new ArgumentException("Inputted texture format '" + textureFormat.ToString() + "' is not a valid texture format.", nameof(textureFormat));
            }
            if(TEXTURE_FORMAT_LOOKUP[(int)textureFormat] == 0)
            {
                throw new ArgumentException("Inputted texture format '" + textureFormat.ToString() + "' is invalid.", nameof(textureFormat));
            }
            if(TEXTURE_FORMAT_LOOKUP[(int)textureFormat] == -1)
            {
                throw new ArgumentException("Inputted texture format '" + textureFormat.ToString() + "' has not been implemented in Elanetic.Graphics.Internal.NativeTextureFormatLookup.", nameof(textureFormat));
            }
            if(width <= 0 || height <= 0)
            {
                throw new ArgumentException("The width and height of the texture to be created must be more than zero. Inputted size: " + width.ToString() + ", " + height.ToString());
            }
#endif
            int textureIndex = CreateNativeTexture(width, height, TEXTURE_FORMAT_LOOKUP[(int)textureFormat]);
            if(textureIndex < 0)
            {
                //Comment out this exception if implementing a new graphics API for debugging purposes. Sometimes if your lucky you'll get a stack trace from Unity's editor log file for the dll.
                throw new SystemException("Texture creation failed. Usually occurs when graphics memory has run out or unsupported input texture size or texture format.");
            }

            DirectTexture2D directTexture = new DirectTexture2D(textureIndex, width, height, textureFormat, GetNativeTexturePointer(textureIndex));
            m_AllTextures.Add(directTexture);
            return directTexture;
        }

        static public void CopyTexture(Texture2D source, Texture2D destination)
        {
            CopyTexture(source.GetNativeTexturePtr(), 0, 0, source.width, source.height, destination.GetNativeTexturePtr(), 0, 0);
        }

        static public void CopyTexture(Texture2D source, int sourceX, int sourceY, int width, int height, Texture2D destination, int destinationX, int destinationY)
        {
            CopyTexture(source.GetNativeTexturePtr(), sourceX, sourceY, width, height, destination.GetNativeTexturePtr(), destinationX, destinationY);
        }

        /// <summary>
        /// Cache Texture2D.GetNativeTexturePtr() pointer and use this function since calling the other function is slower.
        /// Make sure to not alter the Texture2D using functions such as SetPixels or GetRawTextureData(Basically altering on the CPU side) otherwise the pointer will become invalid and these CopyTexture function results will be overidden by Texture2D.Apply that you do.
        /// Texture2D.Apply uploads to the GPU overwriting and previous copies.
        /// Make sure inputted pointers are valid texture pointers otherwise Unity will produce exceptions or crash.
        /// </summary>
        static public void CopyTexture(IntPtr sourceNativePointer, int sourceX, int sourceY, int width, int height, IntPtr destinationNativePointer, int destinationX, int destinationY)
        {
            CopyTextures(sourceNativePointer, sourceX, sourceY, width, height, destinationNativePointer, destinationX, destinationY);
        }

        static public void ClearTexture(Color color, Texture2D targetTexture)
        {
            SetTextureColor(color.r, color.g, color.b, color.a, targetTexture.GetNativeTexturePtr());
        }

        static public void ClearTexture(Texture2D targetTexture)
        {
            SetTextureColor(0.0f, 0.0f, 0.0f, 0.0f, targetTexture.GetNativeTexturePtr());
        }

        static public void ClearTexture(Color color, IntPtr targetTexturePointer)
        {
            SetTextureColor(color.r, color.g, color.b, color.a, targetTexturePointer);
        }

        static public void ClearTexture(IntPtr targetTexturePointer)
        {
            SetTextureColor(0.0f, 0.0f, 0.0f, 0.0f, targetTexturePointer);
        }

#if UNITY_EDITOR
        static private void OnPlayModeChanged(PlayModeStateChange state)
        {
            if(state == PlayModeStateChange.EnteredEditMode)
            {
                DestroyAllTextures();
            }
        }
#endif

        static private void DestroyAllTextures()
        {
            for(int i = 0; i < m_AllTextures.Count; i++)
            {
                m_AllTextures[i].Destroy();
            }
            m_AllTextures.Clear();
        }
    }

}
