using System;
using System.Windows.Input;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class AssetObjectEditPanelViewModel : EditorPanelViewModel
    {
        public string Heading { get; }
        public string SubHeading { get; }
        public bool HasSubHeading { get; }
        public ComponentSubObjectViewModel SubObject { get; }

        public AssetObjectEditPanelViewModel(string label, string assetPath, ComponentSubObjectViewModel subObject)
            : base($"Edit {label}")
        {
            Heading = label ?? throw new ArgumentNullException(nameof(label));
            SubObject = subObject ?? throw new ArgumentNullException(nameof(subObject));

            bool hasPath = !string.IsNullOrEmpty(assetPath) && assetPath != "(None)";
            SubHeading = hasPath ? assetPath : string.Empty;
            HasSubHeading = hasPath;
        }
    }
}
