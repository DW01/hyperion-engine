using System;
using System.Collections.ObjectModel;

namespace Hyperion.Editor.ViewModels
{
    public class AddNewSceneDialogViewModel : ViewModelBase
    {
        private string _sceneName = "NewScene";

        public string SceneName
        {
            get => _sceneName;
            set => SetProperty(ref _sceneName, value);
        }

        public ObservableCollection<FlagsPropertyViewModel.EnumFlagEntry> FlagEntries { get; } = new();

        public SceneFlags SceneFlags
        {
            get
            {
                ulong combined = 0;

                foreach (var entry in FlagEntries)
                {
                    if (entry.IsSelected && entry.Value != null)
                    {
                        combined |= Convert.ToUInt64(entry.Value);
                    }
                }

                return (SceneFlags)combined;
            }
        }

        public AddNewSceneDialogViewModel()
        {
            BuildFlagEntries();
        }

        private void BuildFlagEntries()
        {
            Class? sceneFlagsClass = Class.TryGetClass(typeof(SceneFlags));

            if (sceneFlagsClass == null)
                return;

            ulong defaultVal = (ulong)SceneFlags.Default;

            foreach (StaticField staticField in sceneFlagsClass.Value.StaticFields)
            {
                ClassAttribute? attrEditor = staticField.GetAttribute("editor");

                if (attrEditor != null && attrEditor.Value.GetBool() == false)
                {
                    continue;
                }

                object? flagValue = staticField.ReadObject();

                string title = GetFlagEntryTitle(staticField);
                string description = GetFlagEntryDescription(staticField);

                var entry = new FlagsPropertyViewModel.EnumFlagEntry(title, description, flagValue, () => { });
                FlagEntries.Add(entry);

                if (flagValue != null)
                {
                    ulong flagVal = Convert.ToUInt64(flagValue);
                    entry.IsSelected = (defaultVal & flagVal) == flagVal && flagVal != 0;
                }
            }
        }

        private static string GetFlagEntryTitle(StaticField staticField)
        {
            ClassAttribute? titleAttribute = staticField.GetAttribute("title");

            return titleAttribute?.GetString() ?? staticField.Name.ToString();
        }

        private static string GetFlagEntryDescription(StaticField staticField)
        {
            ClassAttribute? descriptionAttribute = staticField.GetAttribute("description");

            return descriptionAttribute?.GetString() ?? string.Empty;
        }
    }
}
