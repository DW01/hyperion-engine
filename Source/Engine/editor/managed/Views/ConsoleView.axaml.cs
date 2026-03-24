using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Avalonia;

namespace Hyperion.Editor
{
    public partial class ConsoleView : UserControl
    {
        private TextBox? _commandTextBox;

        public ConsoleView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
            _commandTextBox = this.FindControl<TextBox>("CommandTextBox");
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            Dispatcher.UIThread.Post(() => _commandTextBox?.Focus(), DispatcherPriority.Loaded);
        }
    }
}
