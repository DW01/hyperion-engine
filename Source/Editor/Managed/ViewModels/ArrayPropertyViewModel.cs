using System;
using System.Collections.ObjectModel;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ArrayPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const int MaxDepth = 4;

        private readonly int _depth;
        private readonly TypeInfo _elementTypeInfo;
        private readonly bool _isPolymorphic;

        // Sim-thread copy of the current array value.
        private BoxedValue? _currentArrayValue;

        // Virtual element not yet committed to the real array.
        private ObjectPropertyViewModel? _pendingElement;

        private int _isRefreshing;

        public ICommand AddElementCommand { get; }
        public ICommand RemoveElementCommand { get; }

        public ObservableCollection<InspectorPropertyViewModelBase> Elements { get; } = new();

        private bool _hasElements;
        public bool HasElements
        {
            get => _hasElements;
            private set => SetProperty(ref _hasElements, value);
        }

        private bool _isExpanded = true;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        public bool CanAddElement { get; }
        public bool CanRemoveElement { get; }

        public override bool ShowInlineLabel => false;


        public ArrayPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = property.TypeInfo.GetElementTypeInfo();
            _isPolymorphic = DetectPolymorphic();

            Value = property.TypeInfo.Name.ToString();

            bool isFixedArray = property.TypeInfo.Name.ToString().Contains("FixedArray");
            CanAddElement = !isReadOnly && !isFixedArray;
            CanRemoveElement = !isReadOnly && !isFixedArray;

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = property.TypeInfo.GetElementTypeInfo();
            _isPolymorphic = DetectPolymorphic();

            Value = property.TypeInfo.Name.ToString();

            bool isFixedArray = property.TypeInfo.Name.ToString().Contains("FixedArray");
            CanAddElement = !isReadOnly && !isFixedArray;
            CanRemoveElement = !isReadOnly && !isFixedArray;

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(string label, TypeInfo typeInfoHint, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfoHint, getter, setter, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = typeInfoHint.GetElementTypeInfo();
            _isPolymorphic = DetectPolymorphic();

            Value = _elementTypeInfo.Name.ToString();

            CanAddElement = !isReadOnly;
            CanRemoveElement = !isReadOnly;

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }


        private bool DetectPolymorphic()
        {
            Class? elementClass = _elementTypeInfo.Class;
            if (elementClass == null)
                return false;

            string className = elementClass.Value.Name.ToString();
            bool found = false;
            NameCallbackDelegate cb = (_, _) => found = true;
            NativeBindings.Hyp_GetAllDerivedClassNames(className, cb, IntPtr.Zero);
            return found;
        }


        private BoxedValue GetElementValue(int index)
        {
            if (_currentArrayValue == null)
                throw new InvalidOperationException("Array value not yet loaded");

            return _currentArrayValue.GetArrayElement(index);
        }

        private void SetElementValue(int index, BoxedValue value)
        {
            if (_currentArrayValue == null)
                throw new InvalidOperationException("Array value not yet loaded");

            _currentArrayValue.SetArrayElement(index, value);
        }

        private void WriteArrayToParent()
        {
            if (_currentArrayValue == null)
                return;

            SetPropertyValue(_currentArrayValue);
        }


        private InspectorPropertyViewModelBase CreateElementViewModel(int index)
        {
            int capturedIndex = index;

            return InspectorViewModelFactory.CreateForValue(
                $"[{capturedIndex}]",
                _elementTypeInfo,
                getter: () => GetElementValue(capturedIndex),
                setter: v => SetElementValue(capturedIndex, v),
                isReadOnly: _isReadOnly,
                depth: _depth + 1,
                initialize: false,
                postWriteCallback: WriteArrayToParent);
        }


        public void AddElement()
        {
            if (_depth >= MaxDepth || _currentArrayValue == null)
                return;

            // @TODO should all arrays do this

            if (_isPolymorphic)
            {
                if (_pendingElement != null)
                    return;

                int nextIndex = _currentArrayValue.GetArraySize();

                var vm = new ObjectPropertyViewModel(
                    $"[{nextIndex}]",
                    _elementTypeInfo,
                    getter: () => throw new InvalidOperationException("Pending element"),
                    setter: _ => { },
                    isReadOnly: _isReadOnly,
                    depth: _depth + 1);

                vm.IsPending = true;
                vm.OnPendingCommitted = CommitPendingElement;
                _pendingElement = vm;
                Elements.Add(vm);

                HasElements = Elements.Count > 0;
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    int sz = _currentArrayValue!.GetArraySize();
                    _currentArrayValue.ResizeArray(sz + 1);
                    WriteArrayToParent();

                    BoxedValue refreshed = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _currentArrayValue = refreshed;
                        RebuildElementVMs(_currentArrayValue);
                        foreach (var vm in Elements) vm.RefreshValue();
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: AddElement failed: {ex.Message}");
                }
            });
        }


        private void CommitPendingElement(string className)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    // Create the instance.
                    BoxedValueInternal result;

                    unsafe
                    {
                        if (!Hyp_CreateInstanceOfClass(className, &result))
                        {
                            Logger.Log(LogLevel.Warning, $"Failed to create instance of '{className}'");
                            return;
                        }
                    }

                    // Push back the element
                    using (BoxedValue instance = BoxedValue.FromBuffer(result))
                    {
                        _currentArrayValue!.PushBackArrayElement(instance);
                    }

                    WriteArrayToParent();

                    BoxedValue refreshed = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _currentArrayValue = refreshed;
                        _pendingElement = null;
                        RebuildElementVMs(_currentArrayValue);
                        foreach (var vm in Elements) vm.RefreshValue();
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"CommitPendingElement('{className}') failed: {ex.Message}");
                }
            });
        }


        public void RemoveElementAt(int index)
        {
            if (index < 0 || index >= Elements.Count)
                return;

            // Removing the virtual element: just drop it, no array work.
            if (_pendingElement != null && Elements[index] == _pendingElement)
            {
                Elements.RemoveAt(index);
                _pendingElement = null;
                HasElements = Elements.Count > 0;
                return;
            }

            if (_currentArrayValue == null)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _currentArrayValue!.RemoveArrayElement(index);
                    WriteArrayToParent();

                    BoxedValue refreshed = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _currentArrayValue = refreshed;
                        RebuildElementVMs(_currentArrayValue);
                        foreach (var vm in Elements) vm.RefreshValue();
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: RemoveElementAt({index}) failed: {ex.Message}");
                }
            });
        }


        private void RebuildElementVMs(BoxedValue arrayValue)
        {
            Elements.Clear();

            int count = 0;

            if (_depth < MaxDepth)
            {
                try
                {
                    count = arrayValue.GetArraySize();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: Failed to get array size: {ex.Message}");
                }

                for (int i = 0; i < count; i++)
                {
                    var vm = CreateElementViewModel(i);
                    Elements.Add(vm);
                }
            }

            // Re-attach the pending element if it still exists.
            if (_pendingElement != null)
            {
                Elements.Add(_pendingElement);
                count++; // show it in the summary
            }

            HasElements = Elements.Count > 0;
            Value = $"(array, {count} elem{(count != 1 ? "s" : "")})";
        }

        public override void RefreshValue()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    BoxedValue newArrayValue = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;
                        _currentArrayValue = newArrayValue;
                        RebuildElementVMs(_currentArrayValue);
                        foreach (var vm in Elements) vm.RefreshValue();
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: RefreshValue failed: {ex.Message}");
                }
            });
        }

        public override void CommitValue()
        {
        }


        [DllImport("hyperion")]
        private static extern unsafe bool Hyp_CreateInstanceOfClass(string className, BoxedValueInternal* pOutBoxed);
    }
}
