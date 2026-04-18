using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.Platform.Storage;
using System;
using System.IO;
using System.Xml;
using System.Linq;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Threading.Tasks;
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
    private string            _baseDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "TheBadPlace");
    private ObservableCollection<string> _scripts = new();

    public MainWindow()
    {
        InitializeComponent();
        InitializeEnvironment();
        LoadLuaSyntax();

        ScriptList.ItemsSource = _scripts;

        Editor.TextArea.TextEntering += TextArea_TextEntering;
        Editor.TextArea.TextEntered += TextArea_TextEntered;

        RefreshScriptList();
    }

    private void InitializeEnvironment()
    {
        try
        {
            string[] subdirs = { "AutoExec", "Scripts", "Workspace" };
            if (!Directory.Exists(_baseDir)) Directory.CreateDirectory(_baseDir);
            foreach (var dir in subdirs)
            {
                string path = Path.Combine(_baseDir, dir);
                if (!Directory.Exists(path)) Directory.CreateDirectory(path);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Environment Error: {ex.Message}");
        }
    }

    private void RefreshScriptList()
    {
        try
        {
            _scripts.Clear();
            string scriptsPath = Path.Combine(_baseDir, "Scripts");
            if (Directory.Exists(scriptsPath))
            {
                var files = Directory.GetFiles(scriptsPath, "*.*")
                    .Where(f => f.EndsWith(".lua") || f.EndsWith(".txt") || f.EndsWith(".txt"))
                    .Select(Path.GetFileName)
                    .Cast<string>();

                foreach (var file in files) _scripts.Add(file);
            }
        }
        catch { }
    }

    private void ScriptList_DoubleTapped(object? sender, TappedEventArgs e)
    {
        if (ScriptList.SelectedItem is string fileName)
        {
            try
            {
                string fullPath = Path.Combine(_baseDir, "Scripts", fileName);
                Editor.Text = File.ReadAllText(fullPath);
                AppendLog($"BadPlace | Loaded {fileName}");
            }
            catch (Exception ex)
            {
                AppendLog($"BadPlace | Error loading script: {ex.Message}");
            }
        }
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
                        AppendLog("BadPlace | Pipe connected. Ready to execute.");
                    else
                        AppendLog("BadPlace | Warning: pipe connection failed.");
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
            AppendLog("BadPlace | Not connected — click Attach first.");
            return;
        }

        _pipe.SendScript(script);
        AppendLog("BadPlace | Script sent.");
    }

    private void AppendLog(string message)
    {
        LogOutput.Text += message + "\n";
        // Auto scroll to bottom
        LogOutput.CaretIndex = LogOutput.Text?.Length ?? 0;
    }

    private async void OpenFile_Click(object? sender, RoutedEventArgs e)
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel == null) return;

        var scriptsFolder = await topLevel.StorageProvider.TryGetFolderFromPathAsync(Path.Combine(_baseDir, "Scripts"));

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Open Lua Script",
            SuggestedStartLocation = scriptsFolder,
            FileTypeFilter = new[] 
            { 
                new FilePickerFileType("Lua Scripts") { Patterns = new[] { "*.lua", "*.txt" } } 
            },
            AllowMultiple = false
        });

        if (files.Count > 0)
        {
            using var stream = await files[0].OpenReadAsync();
            using var reader = new StreamReader(stream);
            Editor.Text = await reader.ReadToEndAsync();
            AppendLog($"BadPlace | Loaded {files[0].Name}");
        }
    }

    private async void SaveFile_Click(object? sender, RoutedEventArgs e)
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel == null) return;

        var scriptsFolder = await topLevel.StorageProvider.TryGetFolderFromPathAsync(Path.Combine(_baseDir, "Scripts"));

        var file = await topLevel.StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = "Save Lua Script",
            SuggestedStartLocation = scriptsFolder,
            SuggestedFileName = "Script.lua",
            FileTypeChoices = new[] 
            { 
                new FilePickerFileType("Lua Scripts") { Patterns = new[] { "*.lua" } } 
            }
        });

        if (file != null)
        {
            using var stream = await file.OpenWriteAsync();
            using var writer = new StreamWriter(stream);
            await writer.WriteAsync(Editor.Text);
            AppendLog($"BadPlace | Saved to {file.Name}");
            RefreshScriptList();
        }
    }

    private void Close_Click(object? sender, RoutedEventArgs e)
    {
        Close();
    }
}