using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.Platform;
using Avalonia.Threading;
using System;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Services;

namespace Hyperion.Editor
{
    public partial class MainWindow : Window
    {
        private const bool CappedFrameRate = true;
        private const bool IsRenderingOnMainThread = true;

        private ToggleButton? _toggleContentBrowser;
        private ToggleButton? _toggleConsole;
        private Grid? _bottomPanelGrid;
        private Control? _contentBrowserPanel;
        private Control? _consolePanel;

        public MainWindow()
        {
            InitializeComponent();

            // Provide engine window to the viewport control via factory
            EditorViewportControl? evc = this.FindControl<EditorViewportControl>("EditorViewportControl");

            if (evc == null)
            {
                throw new Exception("EditorViewportControl control not found in MainWindow.");
            }

            evc.Focus();

            DataContext = new MainWindowViewModel();

            _toggleContentBrowser = this.FindControl<ToggleButton>("ToggleContentBrowser");
            _toggleConsole = this.FindControl<ToggleButton>("ToggleConsole");
            _bottomPanelGrid = this.FindControl<Grid>("BottomPanelGrid");
            _contentBrowserPanel = this.FindControl<Control>("ContentBrowserPanel");
            _consolePanel = this.FindControl<Control>("ConsolePanel");

            if (_toggleContentBrowser != null)
                _toggleContentBrowser.IsCheckedChanged += OnBottomPanelToggleChanged;
            if (_toggleConsole != null)
                _toggleConsole.IsCheckedChanged += OnBottomPanelToggleChanged;

            if (IsRenderingOnMainThread)
            {
                Opened += (s, e) =>
                {
                    var topLevel = TopLevel.GetTopLevel(this);
                    topLevel?.RequestAnimationFrame(OnFrame);
                };
            }
        }

        private void OnBottomPanelToggleChanged(object? sender, RoutedEventArgs e)
        {
            if (_bottomPanelGrid == null) return;

            bool showContent = _toggleContentBrowser?.IsChecked == true;
            bool showConsole = _toggleConsole?.IsChecked == true;

            if (_contentBrowserPanel != null)
                _contentBrowserPanel.IsVisible = showContent;
            if (_consolePanel != null)
                _consolePanel.IsVisible = showConsole;

            var cols = _bottomPanelGrid.ColumnDefinitions;
            cols[0].Width = showContent ? new GridLength(1, GridUnitType.Star) : new GridLength(0);
            cols[1].Width = (showContent && showConsole) ? new GridLength(2) : new GridLength(0);
            cols[2].Width = showConsole ? new GridLength(1, GridUnitType.Star) : new GridLength(0);
        }

        // need to destroy the engine window when MainWindow is closed
        protected override void OnClosed(EventArgs e)
        {
            base.OnClosed(e);
        }

        private void OnFrame(TimeSpan time)
        {
            NativeBindings.Hyp_MainThreadUpdate();

            ConsoleService.Instance.ProcessLogQueue();

            var topLevel = GetTopLevel(this);
            topLevel?.RequestAnimationFrame(OnFrame);
        }

        // protected override void OnKeyDown(Avalonia.Input.KeyEventArgs e)
        // {
        //     base.OnKeyDown(e);

        //     var vm = DataContext as MainWindowViewModel;
        //     vm?.HandleKeyDown(e);
        // }
    }
}
