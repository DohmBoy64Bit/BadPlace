using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Threading;
using System;
using System.IO;
using System.Xml;
using AvaloniaEdit.Highlighting;
using AvaloniaEdit.Highlighting.Xshd;
using AvaloniaEdit.CodeCompletion;
using BadPlaceUI.Injection;
using BadPlaceUI.IPC;

namespace BadPlaceUI;

public partial class MainWindow : Window
{
    private CompletionWindow? _completionWindow;
    private PipeClient?       _pipe;

    public MainWindow()
    {
        InitializeComponent();
        LoadLuaSyntax();

        Editor.TextArea.TextEntering += TextArea_TextEntering;
        Editor.TextArea.TextEntered += TextArea_TextEntered;
    }

    private void TextArea_TextEntering(object? sender, TextInputEventArgs e)
    {
        if (e.Text?.Length > 0 && _completionWindow != null)
        {
            if (!char.IsLetterOrDigit(e.Text[0]))
            {
                // Insert currently selected element on non-alphanumeric trigger
                _completionWindow.CompletionList.RequestInsertion(e);
            }
        }
    }

    private void TextArea_TextEntered(object? sender, TextInputEventArgs e)
    {
        if (e.Text == "." || char.IsLetter(e.Text?[0] ?? ' '))
        {
            if (_completionWindow == null)
            {
                _completionWindow = new CompletionWindow(Editor.TextArea);
                var data = _completionWindow.CompletionList.CompletionData;
                
                string[] keywords = { "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while", "game", "workspace", "script", "math", "string", "table", "coroutine", "print", "warn", "error", "getgenv", "getrawmetatable", "hookmetamethod", "require", "pcall", "tick", "wait", "task", "Instance", "Vector3", "Color3", "CFrame", "UDim2" };
                Array.Sort(keywords);
                
                foreach (var keyword in keywords)
                {
                    data.Add(new LuaCompletionData(keyword));
                }

                _completionWindow.Show();
                _completionWindow.Closed += delegate {
                    _completionWindow = null;
                };
            }
        }
    }

    private void LoadLuaSyntax()
    {
        try
        {
            var p = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Lua.xshd");
            if (File.Exists(p))
            {
                using (var stream = File.OpenRead(p))
                {
                    using (var reader = new XmlTextReader(stream))
                    {
                        Editor.SyntaxHighlighting = HighlightingLoader.Load(reader, HighlightingManager.Instance);
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine("Could not load syntax highlighting: " + ex.Message);
        }
    }

    private void TopBar_PointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            BeginMoveDrag(e);
        }
    }

    private void Minimize_Click(object? sender, RoutedEventArgs e)
    {
        WindowState = WindowState.Minimized;
    }

    private void Attach_Click(object? sender, RoutedEventArgs e)
    {
        string dllPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "BadPlace.dll");

        var (success, error) = Win32Injector.Inject("Polytoria Client", dllPath);

        if (success)
        {
            AttachBtn.Content  = "Attached!";
            AttachBtn.IsEnabled = false;
            AttachBtn.Classes.Remove("toolbarBtn");
            AttachBtn.Classes.Add("activeBtn");

            // Give the DLL a moment to boot its pipe server, then connect
            System.Threading.Tasks.Task.Delay(800).ContinueWith(_ =>
            {
                _pipe = new PipeClient();
                _pipe.LogReceived += msg =>
                    Dispatcher.UIThread.Post(() => AppendLog(msg));

                bool piped = _pipe.Connect(5000);
                Dispatcher.UIThread.Post(() =>
                {
                    if (piped)
                        AppendLog("[BadPlace] Pipe connected. Ready to execute.");
                    else
                        AppendLog("[BadPlace] Warning: pipe connection failed.");
                });
            });
        }
        else
        {
            AttachBtn.Content = "Failed";
            AttachBtn.Background = new SolidColorBrush(Color.FromRgb(100, 30, 30));
        }
    }

    private void Execute_Click(object? sender, RoutedEventArgs e)
    {
        string script = Editor.Text;
        if (string.IsNullOrWhiteSpace(script)) return;

        if (_pipe == null || !_pipe.IsConnected)
        {
            AppendLog("[BadPlace] Not connected — click Attach first.");
            return;
        }

        _pipe.SendScript(script);
        AppendLog("[BadPlace] Script sent.");
    }

    private void AppendLog(string message)
    {
        LogOutput.Text += message + "\n";
        // Auto scroll to bottom
        LogOutput.CaretIndex = LogOutput.Text?.Length ?? 0;
    }

    private void Close_Click(object? sender, RoutedEventArgs e)
    {
        Close();
    }
}