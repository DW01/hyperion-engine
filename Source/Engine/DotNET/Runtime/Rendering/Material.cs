using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "MaterialParameters")]
    public struct MaterialParameters
    {
        public Vec4f albedo;
        public float metalness;
        public float roughness;
        public float alphaThreshold;
        public float parallaxHeightScale;
        public float transmission;
        public float ior;
        public Color emissiveColor;
        float emissiveIntensity;
        Vec4f userParams;

        [MarshalAs(UnmanagedType.I1)]
        bool unlit;
    }

    [ClassBinding(Name = "Material")]
    public class Material : AssetObject
    {
        public Material()
        {
        }
    }
}
