using System;
using System.Globalization;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class Vec3uViewModel : VectorPropertyViewModelBase<Vec3u>
    {
        public Vec3uViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  })
        {
        }

        public Vec3uViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  })
        {
        }

        // For delegated use (eg. transform subcomponents)
        public Vec3uViewModel(ObjectBase target, Property property, bool isReadOnly,
            System.Func<Vec3u> readOverride, System.Action<Vec3u> writeOverride)
            : base(target, property, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  },
                  readOverride,
                  writeOverride)
        {
        }

        // For delegated use with component targets
        public Vec3uViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly,
            System.Func<Vec3u> readOverride, System.Action<Vec3u> writeOverride)
            : base(classAddress, targetAddressResolver, property, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  },
                  readOverride,
                  writeOverride)
        {
        }

        public Vec3uViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  })
        {
        }

        protected override string FormatComponent(float value)
        {
            return ((uint)value).ToString(CultureInfo.InvariantCulture);
        }

        protected override bool TryParseComponent(string text, out float result)
        {
            if (uint.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out uint uintResult))
            {
                result = uintResult;
                return true;
            }

            result = 0;
            return false;
        }
    }
}
