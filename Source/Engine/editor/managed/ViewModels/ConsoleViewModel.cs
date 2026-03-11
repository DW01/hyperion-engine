using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ConsoleViewModel : ViewModelBase
    {
        public ReadOnlyObservableCollection<LogEntry> Logs => ConsoleService.Instance.Logs;

        private string _commandText;
        public string CommandText
        {
            get => _commandText;
            set => SetProperty(ref _commandText, value);
        }

        public ICommand ExecuteCommand { get; }
        public ICommand ClearCommand { get; }

        public ConsoleViewModel()
        {
            _commandText = string.Empty;
            ExecuteCommand = new RelayCommand(Execute);
            ClearCommand = new RelayCommand(Clear);
        }

        private void Execute()
        {
            if (string.IsNullOrWhiteSpace(CommandText)) return;

            string[] args = CommandText.Split(' ', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);

            if (args.Length != 0)
            {
                try
                {
                    ConsoleService.Instance.ExecuteCommand(args);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Failed to execute command: {ex.Message}");
                }
                finally
                {
                    CommandText = string.Empty;
                }
            }
        }

        private void Clear()
        {
            ConsoleService.Instance.ClearLogs();
        }
    }
}
