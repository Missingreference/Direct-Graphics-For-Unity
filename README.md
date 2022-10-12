# Direct Graphics For Unity
A direct implementation of the Graphics API used in Unity. Used for Texture2D modification on the GPU as much as possible. 

### Installation

To simply add Direct Graphics to your Unity project as a package:

-In the Unity Editor open up the Package Manager by going to Window -> Package Manager.

-At the top left of the Package Manager window press the plus button and press 'Add package from git URL' or similar.

-Submit ```https://github.com/Missingreference/Direct-Graphics-For-Unity.git?path=/UnityProject/Packages/com.elanetic.directgraphics``` as the URL and the Package Manager should add the package to your project.

Alternatively if you would like to view the code not as a package you can navigate to the subfolder in this repository located at 'UnityProject/Packages/com.elanetic.directgraphics'.

### Features
 -Allows for copying Texture2Ds to other Texture2Ds, sorely missing from Unity itself. Even if the texture is set to not read/write enabled since that means it is for the most part operating on the GPU side.
 
 -Instantiate Texture2D as fast as possible. Faster than Unity's default Texture2D instantiation since Unity clears the texture's pixels to white. Instead(depending on the Graphics API) garbage data is used instead.
 
 -Set Texture2D to a color or clear a Texture2D on the GPU as fast possible instead of using Texture2D.SetPixels.

 ### Notes
  Project setup has been forked from Unity's Native Plugin project here: https://github.com/Unity-Technologies/NativeRenderingPlugin

  As of this time support is limited to Metal for MacOS and Vulkan for Windows. Auto Graphics must be disabled in Edit -> Project Settings -> Auto Graphics for Mac / Windows and have it set to the only supported Graphics APIs in this case Metal and Vulkan. DirectX(all versions), OpenGL, etc are currently not supported but implementation of other Unity supported APIs are possible and pull requests are welcome.
  
  ### Code Walkthrough
  To start off the C++ plugins have their own projects that export their builds(bundle, DLL, etc) to the Unity Project plugins folder. Unity's editor needs a full restart if any changes to the plugins are made. All of the C# code is located in 'Assets/Scripts/Direct Graphics/' outside one GlobalCoroutine script used for a delayed destruction of textures.
  
  DirectGraphics script is the main interaction with the API with functions including CreateTexture, CopyTexture and ClearTexture. When creating a texture it returns a DirectTexture2D with a reference to a Texture2D. The reason this exists is because destroying the underlying Texture2D itself does not delete the native memory of the Graphics API, only any allocations on Unity's side. The plugins themselves manage the destruction of native graphics textures and delete any allocations as needed. This implementation uses Unity's Texture2D.CreateExternalTexture where any bad input will crash Unity. Upon exiting playmode and entering edit mode does the DirectGraphics API automatically destroy all created textures. Do not call Destroy on the created Texture2D, DirectTexture2D will handle it's destruction. Call DirectTexture2D.Destroy to clear the texture memory from the GPU. If the creation and destruction of DirectTexture2D are called too quickly(within a couple frames) it will wait a couple frames to destroy hence the the included GlobalCoroutine script.
  
  NativeTextureFormatLookup are arrays for each GraphicsAPI used to convert Unity's TextureFormat to the respective GraphicsAPI texture format equivalent. Most are unimplemented since it's tedious work to look up the native version of the enum so implement as needed. Use TextureFormat.RGBA32 for your first test with this project.
  
  DirectGraphics.CopyTexture currently does not work correctly with mismatched texture types for the most part depending on the data layout so your mileage may vary.
  
  DirectGraphics.ClearTexture currently does not work correctly with non-zero'd colors when the texture format is not common layouts like RGBA32. Your mileage may vary. For example, passing Color(0,0,0,0) to BC7 appears to clear the texture correctly but most likely a coincidence of the data layout. Passing any color does not give a correct result of the texture.
  
Texture to texture conversion/compression as a feature would be nice to see on the roadmap for this project.
