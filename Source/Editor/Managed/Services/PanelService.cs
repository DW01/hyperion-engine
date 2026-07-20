using System;
using System.Windows.Input;
using Hyperion.Editor.Commands;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Services
{
    public sealed class PanelService
    {
        public static PanelService Instance { get; } = new PanelService();

        private EditorPanelViewModel? _activePanel;

        public EditorPanelViewModel? ActivePanel
        {
            get => _activePanel;
            private set
            {
                if (ReferenceEquals(_activePanel, value))
                    return;

                _activePanel = value;
                ActivePanelChanged?.Invoke(this, EventArgs.Empty);
            }
        }

        public event EventHandler? ActivePanelChanged;

        public ICommand CloseCommand { get; }

        private PanelService()
        {
            CloseCommand = new RelayCommand(ClosePanel);
        }

        public void OpenPanel(EditorPanelViewModel panel)
        {
            if (panel == null)
                throw new ArgumentNullException(nameof(panel));

            if (ReferenceEquals(_activePanel, panel))
                return;

            if (_activePanel != null)
            {
                var closing = _activePanel;
                _activePanel = null;
                closing.OnClosed?.Invoke();
            }

            ActivePanel = panel;
        }

        public void ClosePanel()
        {
            if (_activePanel == null)
                return;

            var closing = _activePanel;
            ActivePanel = null;
            closing.OnClosed?.Invoke();
        }
    }
}
