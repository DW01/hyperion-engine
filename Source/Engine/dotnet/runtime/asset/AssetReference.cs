using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetReference")]
    [StructLayout(LayoutKind.Explicit, Size = 24, Pack = 8)]
    public unsafe struct AssetReference
    {
        [FieldOffset(0)]
        private Handle<AssetObject> handle;

        [FieldOffset(0)]
        private AssetPath assetPath;

        public AssetReference()
        {
        }

        public AssetPath Path
        {
            get { return assetPath; }
            set { assetPath = value; }
        }

        public AssetObject? AssetObject
        {
            get { return handle.GetValue(); }
            set
            {
                if (value != null)
                {
                    handle = new Handle<AssetObject>(value);
                } else
                {
                    handle = Handle<AssetObject>.Empty;
                }
            }
        }
    }
}