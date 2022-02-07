# Direct Graphics For Unity
A direct implementation of the Graphics API used in Unity. Used for Texture2D modification on the GPU as much as possible. 

### Features
 -Allows for copying Texture2Ds to other Texture2Ds, sorely missing from Unity itself. Even if the texture is set to not read/write enabled since that means it is not operating on the CPU side.
 -Instantiate Texture2D as fast as possible. Faster than Unity's default Texture2D instantiation since Unity clears the texture's pixels to white. Instead(depending on the Graphics API) garbage data is used instead.
 -Set Texture2D to a color or clear a Texture2D on the GPU as fast possible instead of using Texture2D.SetPixels.

 ### Notes
  Project setup has been forked from Unity's Native Plugin project here: https://github.com/Unity-Technologies/NativeRenderingPlugin

  As of this time support is limited to Metal for MacOS and Vulkan for Windows. Auto Graphics must be disabled in Edit -> Project Settings -> Auto Graphics for Mac / Windows and have it set to the only supported Graphics API.