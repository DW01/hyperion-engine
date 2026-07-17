using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "BlendModeFactor")]
    public enum BlendModeFactor
    {
        None = 0,

        One,
        Zero,
        SrcColor,
        SrcAlpha,
        DstColor,
        DstAlpha,
        OneMinusSrcColor,
        OneMinusSrcAlpha,
        OneMinusDstColor,
        OneMinusDstAlpha,

        Count
    }

    [ClassBinding(Name = "BlendFunction")]
    public struct BlendFunction
    {
        private uint value;

        public BlendFunction(BlendModeFactor srcColor, BlendModeFactor dstColor)
        {
            value = ((uint)srcColor << 0) | ((uint)dstColor << 4) | ((uint)srcColor << 8) | ((uint)dstColor << 12);
        }

        public BlendFunction(BlendModeFactor srcColor, BlendModeFactor dstColor, BlendModeFactor srcAlpha, BlendModeFactor dstAlpha)
        {
            value = ((uint)srcColor << 0) | ((uint)dstColor << 4) | ((uint)srcAlpha << 8) | ((uint)dstAlpha << 12);
        }
    }
}