using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;

using UnityEngine;
using Debug = UnityEngine.Debug;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace Elanetic.Graphics
{
    /// <summary>
    /// The reference to a native Graphics texture. This wrapper is required for deallocating the texture since Unity itself does not clean it up.
    /// Call DirectTexture2D.Destroy to cleanup the texture from GPU memory. Do not call Texture2D.Destroy on the Texture2D reference.
    /// Automatically destroyed upon Application.quitting event and in editor on exit play mode.
    /// </summary>
    public class DirectTexture2D
    {
        public Texture2D texture { get; private set; }
        public IntPtr nativePointer { get; private set; }

        public bool isDestroyed { get; private set; }

        private int m_TextureIndex;
        private int m_FrameCreated;

        internal DirectTexture2D(int textureIndex, int width, int height, TextureFormat format, IntPtr nativePointer)
        {
            m_TextureIndex = textureIndex;
            this.nativePointer = nativePointer;

            texture = Texture2D.CreateExternalTexture(width, height, format, false, true, nativePointer);

            texture.filterMode = FilterMode.Point;
            m_FrameCreated = Time.renderedFrameCount;
        }

        private Coroutine m_DestructionCoroutine;
#if UNITY_EDITOR
        static private Stopwatch m_EditorDestructionTimer = new Stopwatch();
#endif
        /// <summary>
        /// Be aware that a texture pointing to an invalid texture from destruction causes Unity to view the invalid data differently based on Graphics API used. 
        /// For example, Metal shows a destroyed texture as a clear texture while Vulkan shows as a black texture.
        /// </summary>
        public void Destroy()
        {
#if UNITY_EDITOR
            if(!Application.isPlaying)
            {
                if(!m_EditorDestructionTimer.IsRunning && m_EditorDestructionTimer.ElapsedMilliseconds < 3.0f) 
                    m_EditorDestructionTimer.Start();
                while(m_EditorDestructionTimer.ElapsedMilliseconds < 1.5f) continue;
                if(texture != null)
                    UnityEngine.Object.DestroyImmediate(texture);
                while(m_EditorDestructionTimer.ElapsedMilliseconds < 3.0f) continue;
                DirectGraphics.DestroyDirectTexture(m_TextureIndex);
                return;
            }
#endif

            if(isDestroyed) return;

#if SAFE_EXECUTION
            if(texture == null)
                throw new InvalidOperationException("DirectTexture2D should handle Texture2D destruction.");
#endif

            isDestroyed = true;
            if(Time.renderedFrameCount - m_FrameCreated < 2)
            {

                //We must wait to deallocate this texture. On Vulkan(untested on other Graphics APIs) where calling Texture2D.CreateExternalTexture, Unity creates another thread perceived from the following crash stacktrace:
                /*
                0x00007FFEA6FE8498(nvoglv64) vkGetInstanceProcAddr
                0x00007FFEA6FE7CDC(nvoglv64) vkGetInstanceProcAddr
                0x00007FF6F9123A96(Unity) vk::Image::CreateImageViews
                0x00007FF6F9163EBC(Unity) vk::ImageManager::CreateImageFromExternalNativeImage
                0x00007FF6F913558A(Unity) GfxDeviceVK::RegisterNativeTextureWithParams
                0x00007FF6FB01AC5F(Unity) GfxDeviceWorker::RunCommand
                0x00007FF6FB022BED(Unity) GfxDeviceWorker::RunExt
                0x00007FF6FB022D08(Unity) GfxDeviceWorker::RunGfxDeviceWorker
                0x00007FF6F96BF335(Unity) Thread::RunThreadWrapper
                0x00007FFF0E727034(KERNEL32) BaseThreadInitThunk
                0x00007FFF10562651(ntdll) RtlUserThreadStart
                */
                //This causes a race condition while Unity's Vulkan is creating an ImageView for our texture while we are trying to deallocate the memory.
                //Destruction of our VkImage reference too early causes a crash.
                //Wait a couple frames just to be safe.

                if(m_DestroyerObject.unityIsQuitting)
                {
                    return;
                }
                m_DestructionCoroutine = m_DestroyerObject.StartCoroutine(WaitAndDestroyUnityTexture());
            }
            else
            {
                DestroyUnityReference();
            }
        }

        private void DestroyUnityReference()
        {
            //We have to destroy Unity's texture first before freeing the memory of the texture in the plugin otherwise crashes occur.
            UnityEngine.Object.DestroyImmediate(texture);
            //But that also means we can't free the memory too quickly since Unity makes calls to their Vulkan API in seperate threads causing a race condition.
            //This means we have to delay freeing the GPU memory in hopes of getting past the race condition. See below for more info.
            //It sure would make way more sense to implement this logic in the plugin itself. A more experienced C/C++ programmer is welcome to do this.
            m_DestructionCoroutine = m_DestroyerObject.StartCoroutine(WaitAndDestroyNativeTexture());
        }

        //This clustersuck is a result of trying to get past race conditions.
        //On Vulkan(Windows), in testing on editor and standalone build it was found that calling vkFreeMemory in Vulkan plugin was causing Unity to freeze as if it was stuck in an infinite loop with no errors in log files.
        //Furthermore it was found when minimizing the standalone build(not the case in the editor) the FPS would shoot up to something like 800 frames per second if running in the background is enabled causing the race condition to be hit dispite the 2 frame window.
        //Thus as a result "if(Time.unscaledTimeAsDouble < 0.005)new WaitForSecondsRealtime(2.0f);" was born and appears to work. Try not to create and destroy too too many textures to fill up all the memory within 2 seconds alright?
        private int m_LastRenderedFrame = -1;
        private IEnumerator WaitAndDestroyUnityTexture()
        {
            yield return WaitSetTime();

            m_DestroyerObject.StopCoroutine(m_DestructionCoroutine);

            DestroyUnityReference();
        }

        private IEnumerator WaitAndDestroyNativeTexture()
        {
            yield return WaitSetTime();

            m_DestroyerObject.StopCoroutine(m_DestructionCoroutine);

            DirectGraphics.DestroyDirectTexture(m_TextureIndex);
        }

        //The amount of time to wait in hopes that we pass the race condition
        private IEnumerator WaitSetTime()
        {
            if(Time.unscaledTimeAsDouble < 0.005)
            {
                yield return new WaitForSecondsRealtime(2.0f);
            }
            else
            {
                m_LastRenderedFrame = Time.renderedFrameCount;
                while(m_LastRenderedFrame == Time.renderedFrameCount)
                    yield return new WaitForEndOfFrame();
                if(Time.unscaledTimeAsDouble < 0.005)
                {
                    yield return new WaitForSecondsRealtime(2.0f);
                }
                else
                {
                    m_LastRenderedFrame = Time.renderedFrameCount;
                    while(m_LastRenderedFrame == Time.renderedFrameCount)
                        yield return new WaitForEndOfFrame();
                }
            }
        }

        private static Direct2DTextureDestroyer m_DestroyerObject = null;

        private class Direct2DTextureDestroyer : MonoBehaviour
        {
            [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
            static private void OnRuntimeLoad()
            {
                if(ReferenceEquals(m_DestroyerObject, null))
                {
                    m_DestroyerObject = new GameObject("Direct Texture2D Destroyer").AddComponent<Direct2DTextureDestroyer>();
                }
            }

            public bool unityIsQuitting { get; private set; }

            void Awake()
            {
                m_DestroyerObject = this;
                GameObject.DontDestroyOnLoad(m_DestroyerObject.gameObject);
                gameObject.hideFlags = HideFlags.HideAndDontSave | HideFlags.HideInInspector;
            }


            void OnDisable()
            {
                if(unityIsQuitting) return;

                throw new InvalidOperationException("DirectTexture2DDestroyer should never be disabled or destroyed during runtime. This will interrupt DirectTexture2D destruction and cause memory leaks.");
            }

            void OnDestroy()
            {
                if(unityIsQuitting) return;

                throw new InvalidOperationException("DirectTexture2DDestroyer should never be destroyed during runtime. This will interrupt DirectTexture2D destruction and cause memory leaks.");
            }

            void OnApplicationQuit()
            {
                unityIsQuitting = true;
            }
        }

    }
}