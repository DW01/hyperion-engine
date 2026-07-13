using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "Prefab")]
    public class Prefab : AssetObject
    {
        public Prefab()
        {
        }

        public Node? Root
        {
            get => this.GetRoot();      // Extension
            set => this.SetRoot(value); // Extension
        }
    }
}