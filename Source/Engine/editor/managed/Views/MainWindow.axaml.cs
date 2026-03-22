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

        private Grid? _bottomPanelGrid;
        private GridSplitter? _bottomPanelSplitter;
        private Control? _contentBrowserPanel;
        private Border? _contentBrowserCollapsedStrip;
        private Control? _consolePanel;
        private Border? _consoleCollapsedStrip;
        private bool _contentBrowserExpanded = true;
        private bool _consoleExpanded = true;

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

            _bottomPanelGrid = this.FindControl<Grid>("BottomPanelGrid");
            _bottomPanelSplitter = this.FindControl<GridSplitter>("BottomPanelSplitter");
            _contentBrowserPanel = this.FindControl<Control>("ContentBrowserPanel");
            _contentBrowserCollapsedStrip = this.FindControl<Border>("ContentBrowserCollapsedStrip");
            _consolePanel = this.FindControl<Control>("ConsolePanel");
            _consoleCollapsedStrip = this.FindControl<Border>("ConsoleCollapsedStrip");

            var collapseContentBrowser = this.FindControl<Button>("CollapseContentBrowser");
            var expandContentBrowser = this.FindControl<Button>("ExpandContentBrowser");
            var collapseConsole = this.FindControl<Button>("CollapseConsole");
            var expandConsole = this.FindControl<Button>("ExpandConsole");

            if (collapseContentBrowser != null) collapseContentBrowser.Click += OnCollapseContentBrowser;
            if (expandContentBrowser != null) expandContentBrowser.Click += OnExpandContentBrowser;
            if (collapseConsole != null) collapseConsole.Click += OnCollapseConsole;
            if (expandConsole != null) expandConsole.Click += OnExpandConsole;

            if (IsRenderingOnMainThread)
            {
                Opened += (s, e) =>
                {
                    var topLevel = TopLevel.GetTopLevel(this);
                    topLevel?.RequestAnimationFrame(OnFrame);
                };
            }
        }

        private void OnCollapseContentBrowser(object? sender, RoutedEventArgs e) { _contentBrowserExpanded = false; UpdateBottomPanelLayout(); }
        private void OnExpandContentBrowser(object? sender, RoutedEventArgs e) { _contentBrowserExpanded = true; UpdateBottomPanelLayout(); }
        private void OnCollapseConsole(object? sender, RoutedEventArgs e) { _consoleExpanded = false; UpdateBottomPanelLayout(); }
        private void OnExpandConsole(object? sender, RoutedEventArgs e) { _consoleExpanded = true; UpdateBottomPanelLayout(); }

        private void UpdateBottomPanelLayout()
        {
            if (_bottomPanelGrid == null) return;

            var cols = _bottomPanelGrid.ColumnDefinitions;
            cols[0].Width = _contentBrowserExpanded ? new GridLength(1, GridUnitType.Star) : new GridLength(30);
            cols[1].Width = (!_contentBrowserExpanded && !_consoleExpanded) ? new GridLength(0) : new GridLength(2);
            cols[2].Width = _consoleExpanded ? new GridLength(1, GridUnitType.Star) : new GridLength(30);

             bool bothExpanded = _contentBrowserExpanded && _consoleExpanded;
            if (_bottomPanelSplitter != null) _bottomPanelSplitter.IsEnabled = bothExpanded;

            if (_contentBrowserPanel != null) _contentBrowserPanel.IsVisible = _contentBrowserExpanded;
            if (_contentBrowserCollapsedStrip != null) _contentBrowserCollapsedStrip.IsVisible = !_contentBrowserExpanded;
            if (_consolePanel != null) _consolePanel.IsVisible = _consoleExpanded;
            if (_consoleCollapsedStrip != null) _consoleCollapsedStrip.IsVisible = !_consoleExpanded;
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
