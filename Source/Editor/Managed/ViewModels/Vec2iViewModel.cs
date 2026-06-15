using System;
using System.Globalization;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class Vec2iViewModel : VectorPropertyViewModelBase<Vec2i>
    {
        public Vec2iViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly, 2,
                  (v, i) => i == 0 ? v.x : v.y,
                  (v, i, val) => i switch
                  {
                      0 => new Vec2i((int)val, v.y),
                      1 => new Vec2i(v.x, (int)val),
                      _ => v
                  })
        {
        }

        public Vec2iViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly, 2,
                  (v, i) => i == 0 ? v.x : v.y,
                  (v, i, val) => i switch
                  {
                      0 => new Vec2i((int)val, v.y),
                      1 => new Vec2i(v.x, (int)val),
                      _ => v
                  })
        {
        }

        public Vec2iViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly, 2,
                  (v, i) => i == 0 ? v.x : v.y,
                  (v, i, val) => i switch
                  {
                      0 => new Vec2i((int)val, v.y),
                      1 => new Vec2i(v.x, (int)val),
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
