using System;
using System.Globalization;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class Vec4iViewModel : VectorPropertyViewModelBase<Vec4i>
    {
        public Vec4iViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly, 4,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      3 => v.w,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec4i((int)val, v.y, v.z, v.w),
                      1 => new Vec4i(v.x, (int)val, v.z, v.w),
                      2 => new Vec4i(v.x, v.y, (int)val, v.w),
                      3 => new Vec4i(v.x, v.y, v.z, (int)val),
                      _ => v
                  })
        {
        }

        public Vec4iViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly, 4,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      3 => v.w,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec4i((int)val, v.y, v.z, v.w),
                      1 => new Vec4i(v.x, (int)val, v.z, v.w),
                      2 => new Vec4i(v.x, v.y, (int)val, v.w),
                      3 => new Vec4i(v.x, v.y, v.z, (int)val),
                      _ => v
                  })
        {
        }

        public Vec4iViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly, 4,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      3 => v.w,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec4i((int)val, v.y, v.z, v.w),
                      1 => new Vec4i(v.x, (int)val, v.z, v.w),
                      2 => new Vec4i(v.x, v.y, (int)val, v.w),
                      3 => new Vec4i(v.x, v.y, v.z, (int)val),
                      _ => v
                  })
        {
        }

        protected override string FormatComponent(float value)
        {
            return ((int)value).ToString(CultureInfo.InvariantCulture);
        }

        protected override bool TryParseComponent(string text, out float result)
        {
            if (int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out int intResult))
            {
                result = intResult;
                return true;
            }

            result = 0;
            return false;
        }
    }
}
