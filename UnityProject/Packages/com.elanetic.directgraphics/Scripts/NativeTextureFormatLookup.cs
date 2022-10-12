using System;
using System.Collections;
using System.Collections.Generic;

using UnityEngine;

namespace Elanetic.Graphics.Internal
{
    static public class NativeTextureFormatLookup
    {
        /// <summary>
        /// Convert Unity TextureFormat to Metal PixelFormat
        /// </summary>
        static public readonly int[] METAL_LOOKUP = new int[75]
        {
             0, // [0] Invalid
             1, // [1] Alpha8             -> MTLPixelFormatA8Unorm
            -1, // [2] ARGB4444           -> Unimplemented
            -1, // [3] RGB24              -> Unimplemented
            70, // [4] RGBA32             -> MTLPixelFormatRGBA8Unorm
            -1, // [5] ARGB32             -> Unimplemented
             0, // [6] Invalid            
            -1, // [7] RGB565             -> Unimplemented
             0, // [8] Invalid            
            -1, // [9] R16                -> Unimplemented
            -1, //[10] DXT1               -> Unimplemented
             0, //[11] Invalid            
           134, //[12] DXT5               -> MTLPixelFormatBC3_RGBA
            -1, //[13] RGBA4444           -> Unimplemented
            -1, //[14] BGRA32             -> Unimplemented
            -1, //[15] RHalf              -> Unimplemented
            -1, //[16] RGHalf             -> Unimplemented
            -1, //[17] RGBAHalf           -> Unimplemented
            -1, //[18] RFloat             -> Unimplemented
            -1, //[19] RGFloat            -> Unimplemented
            -1, //[20] RGBAFloat          -> Unimplemented
            -1, //[21] YUY2               -> Unimplemented
            -1, //[22] RGB9e5Float        -> Unimplemented
             0, //[23] Invalid            
            -1, //[24] BC6H               -> Unimplemented
           152, //[25] BC7                -> MTLPixelFormatBC7_RGBAUnorm
            -1, //[26] BC4                -> Unimplemented
            -1, //[27] BC5                -> Unimplemented
            -1, //[28] DXT1Crunched       -> Unimplemented
            -1, //[29] DXT5Crunched       -> Unimplemented
            -1, //[30] PVRTC_RGB2         -> Unimplemented
            -1, //[31] PVRTC_RGBA2        -> Unimplemented
            -1, //[32] PVRTC_RGB4         -> Unimplemented
            -1, //[33] PVRTC_RGBA4        -> Unimplemented
            -1, //[34] ETC_RGB4           -> Unimplemented
             0, //[35] Invalid            
             0, //[36] Invalid            
             0, //[37] Invalid            
             0, //[38] Invalid            
             0, //[39] Invalid            
             0, //[40] Invalid            
            -1, //[41] EAC_R              -> Unimplemented
            -1, //[42] EAC_R_SIGNED       -> Unimplemented
            -1, //[43] EAC_RG             -> Unimplemented
            -1, //[44] EAC_RG_SIGNED      -> Unimplemented
            -1, //[45] ETC2_RGB           -> Unimplemented
            -1, //[46] ETC2_RGBA1         -> Unimplemented
            -1, //[47] ETC2_RGBA8         -> Unimplemented
            -1, //[48] ASTC_4x4           -> Unimplemented
            -1, //[49] ASTC_5x5           -> Unimplemented
            -1, //[50] ASTC_6X6           -> Unimplemented
            -1, //[51] ASTC_8x8           -> Unimplemented
            -1, //[52] ASTC_10x10         -> Unimplemented
            -1, //[53] ASTC_12x12         -> Unimplemented
             0, //[54] Invalid
             0, //[55] Invalid
             0, //[56] Invalid
             0, //[57] Invalid
             0, //[58] Invalid
             0, //[59] Invalid
             0, //[60] Invalid
             0, //[61] Invalid
            -1, //[62] RG16               -> Unimplemented
            10, //[63] R8                 -> MTLPixelFormatR8Unorm
            -1, //[64] ETC_RGB4Crunched   -> Unimplemented
            -1, //[65] ETC2_RGBA8Crunched -> Unimplemented
            -1, //[66] ASTC_HDR_4x4       -> Unimplemented
            -1, //[67] ASTC_HDR_5x5       -> Unimplemented
            -1, //[68] ASTC_HDR_6x6       -> Unimplemented
            -1, //[69] ASTC_HDR_8x8       -> Unimplemented
            -1, //[70] ASTC_HDR_10x10     -> Unimplemented
            -1, //[71] ASTC_HDR_12x12     -> Unimplemented
            -1, //[72] RG32               -> Unimplemented
            -1, //[73] RGB48              -> Unimplemented
            -1, //[74] RGBA64             -> Unimplemented
        };

        /// <summary>
        /// Convert Unity TextureFormat to Vulkan VkFormat
        /// </summary>
        static public readonly int[] VULKAN_LOOKUP = new int[75]
        {
             0, // [0] Invalid
             1, // [1] Alpha8             -> Unimplemented
            -1, // [2] ARGB4444           -> Unimplemented
            -1, // [3] RGB24              -> Unimplemented
            37, // [4] RGBA32             -> VK_FORMAT_R8G8B8A8_UNORM
            -1, // [5] ARGB32             -> Unimplemented
             0, // [6] Invalid            
            -1, // [7] RGB565             -> Unimplemented
             0, // [8] Invalid            
            -1, // [9] R16                -> Unimplemented
            -1, //[10] DXT1               -> Unimplemented
             0, //[11] Invalid
           137, //[12] DXT5               -> VK_FORMAT_BC3_UNORM_BLOCK
            -1, //[13] RGBA4444           -> Unimplemented
            -1, //[14] BGRA32             -> Unimplemented
            -1, //[15] RHalf              -> Unimplemented
            -1, //[16] RGHalf             -> Unimplemented
            -1, //[17] RGBAHalf           -> Unimplemented
            -1, //[18] RFloat             -> Unimplemented
            -1, //[19] RGFloat            -> Unimplemented
            -1, //[20] RGBAFloat          -> Unimplemented
            -1, //[21] YUY2               -> Unimplemented
            -1, //[22] RGB9e5Float        -> Unimplemented
             0, //[23] Invalid            
            -1, //[24] BC6H               -> Unimplemented
           145, //[25] BC7                -> VK_FORMAT_BC7_UNORM_BLOCK 
            -1, //[26] BC4                -> Unimplemented
            -1, //[27] BC5                -> Unimplemented
            -1, //[28] DXT1Crunched       -> Unimplemented
            -1, //[29] DXT5Crunched       -> Unimplemented
            -1, //[30] PVRTC_RGB2         -> Unimplemented
            -1, //[31] PVRTC_RGBA2        -> Unimplemented
            -1, //[32] PVRTC_RGB4         -> Unimplemented
            -1, //[33] PVRTC_RGBA4        -> Unimplemented
            -1, //[34] ETC_RGB4           -> Unimplemented
             0, //[35] Invalid            
             0, //[36] Invalid            
             0, //[37] Invalid            
             0, //[38] Invalid            
             0, //[39] Invalid            
             0, //[40] Invalid            
            -1, //[41] EAC_R              -> Unimplemented
            -1, //[42] EAC_R_SIGNED       -> Unimplemented
            -1, //[43] EAC_RG             -> Unimplemented
            -1, //[44] EAC_RG_SIGNED      -> Unimplemented
            -1, //[45] ETC2_RGB           -> Unimplemented
            -1, //[46] ETC2_RGBA1         -> Unimplemented
            -1, //[47] ETC2_RGBA8         -> Unimplemented
            -1, //[48] ASTC_4x4           -> Unimplemented
            -1, //[49] ASTC_5x5           -> Unimplemented
            -1, //[50] ASTC_6X6           -> Unimplemented
            -1, //[51] ASTC_8x8           -> Unimplemented
            -1, //[52] ASTC_10x10         -> Unimplemented
            -1, //[53] ASTC_12x12         -> Unimplemented
             0, //[54] Invalid
             0, //[55] Invalid
             0, //[56] Invalid
             0, //[57] Invalid
             0, //[58] Invalid
             0, //[59] Invalid
             0, //[60] Invalid
             0, //[61] Invalid
            -1, //[62] RG16               -> Unimplemented
             9, //[63] R8                 -> VK_FORMAT_R8_UNORM
            -1, //[64] ETC_RGB4Crunched   -> Unimplemented
            -1, //[65] ETC2_RGBA8Crunched -> Unimplemented
            -1, //[66] ASTC_HDR_4x4       -> Unimplemented
            -1, //[67] ASTC_HDR_5x5       -> Unimplemented
            -1, //[68] ASTC_HDR_6x6       -> Unimplemented
            -1, //[69] ASTC_HDR_8x8       -> Unimplemented
            -1, //[70] ASTC_HDR_10x10     -> Unimplemented
            -1, //[71] ASTC_HDR_12x12     -> Unimplemented
            -1, //[72] RG32               -> Unimplemented
            -1, //[73] RGB48              -> Unimplemented
            -1, //[74] RGBA64             -> Unimplemented
        };
    }
}