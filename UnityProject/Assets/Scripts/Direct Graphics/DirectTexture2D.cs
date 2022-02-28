using System;
using System.Collections;
using System.Collections.Generic;

using UnityEngine;

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
        
        /// <summary>
        /// Be aware that a texture pointing to an invalid texture from destruction causes Unity to view the invalid data differently based on Graphics API used. 
        /// For example, Metal shows a destroyed texture as a clear texture while Vulkan shows as a black texture.
        /// </summary>
        public void Destroy()
        {
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

                if(ReferenceEquals(m_DestroyerObject,null))
                {
                    m_DestroyerObject = new GameObject("Direct Texture2D Destroyer").AddComponent<Direct2DTextureDestroyer>();
                }
                else if(m_DestroyerObject.unityIsQuitting)
                {
                    return;
                }
                m_DestructionCoroutine = m_DestroyerObject.StartCoroutine(OnEndFrame());
            }
            else
            {
                DoDestroy();
            }
        }

        private void DoDestroy()
        {
            UnityEngine.Object.Destroy(texture);
            DirectGraphics.DestroyNativeTexture(m_TextureIndex);
        }


        private IEnumerator OnEndFrame()
        {
            //Delay destroy by 2 frames to hopefully ensure the race condition has passed
            yield return new WaitForEndOfFrame();
            yield return new WaitForEndOfFrame();
            m_DestroyerObject.StopCoroutine(m_DestructionCoroutine);
            DoDestroy();
        }

        private static Direct2DTextureDestroyer m_DestroyerObject = null;

        private class Direct2DTextureDestroyer : MonoBehaviour
        {
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