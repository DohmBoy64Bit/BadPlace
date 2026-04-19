using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.Platform.Storage;
using System;
using System.IO;
using System.Diagnostics;
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
    private OptionsWindow? _optionsWindow;

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
        // Close options window if open
        _optionsWindow?.Close();
        Close();
    }

    private void Options_Click(object? sender, RoutedEventArgs e)
    {
        _optionsWindow = new OptionsWindow();
        _optionsWindow.Show();
        
        // Listen for Dump Scripts request
        _optionsWindow.DumpRequested += async () =>
        {
            if (_pipe == null || !_pipe.IsConnected)
            {
                AppendLog("BadPlace | Not connected — click Attach first.");
                return;
            }
            
            AppendLog("BadPlace | Starting dump...");
            
            // Send saveinstance command to DLL
            _pipe.SendScript("saveinstance()");
            
            // Wait for dump to complete (5 seconds for large games)
            await Task.Delay(5000);
            
            // If auto-decompile is enabled
            if (_optionsWindow.AutoDecompile)
            {
                AppendLog("BadPlace | Auto-decompiling bytecode...");
                await DecompileAllBinFiles(_optionsWindow.SaveBytecode);
            }
            
            AppendLog("BadPlace | Dump complete!");
        };
    }

    private async Task DecompileAllBinFiles(bool keepBinFiles)
    {
        try
        {
            string medalPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "thirdparty", "medal", "target", "release", "luau-lifter.exe");
            
            // Find workspace folder (most recent GameID folder)
            string workspacePath = Path.Combine(_baseDir, "Workspace");
            if (!Directory.Exists(workspacePath))
            {
                AppendLog("BadPlace | No Workspace folder found.");
                return;
            }
            
            // Find the most recent game folder (by last write time)
            var gameFolders = Directory.GetDirectories(workspacePath).OrderByDescending(d => Directory.GetLastWriteTime(d)).ToList();
            if (gameFolders.Count == 0)
            {
                AppendLog("BadPlace | No game folders in Workspace.");
                return;
            }
            
            string gameFolder = gameFolders[0];
            string gameName = Path.GetFileName(gameFolder);
            AppendLog($"BadPlace | Processing game: {gameName}");
            
            // Find all .bin files
            var binFiles = Directory.GetFiles(gameFolder, "*.bin", SearchOption.AllDirectories).ToList();
            if (binFiles.Count == 0)
            {
                AppendLog("BadPlace | No .bin files found to decompile.");
                return;
            }
            
            AppendLog($"BadPlace | Found {binFiles.Count} bytecode files...");
            
            int successCount = 0;
            int failCount = 0;
            
            foreach (var binFile in binFiles)
            {
                string luaFile = Path.ChangeExtension(binFile, ".lua");
                string relativePath = Path.GetRelativePath(gameFolder, binFile);
                
                try
                {
                    var startInfo = new ProcessStartInfo
                    {
                        FileName = medalPath,
                        Arguments = $"\"{binFile}\"",
                        UseShellExecute = false,
                        RedirectStandardOutput = true,
                        RedirectStandardError = true,
                        CreateNoWindow = true
                    };
                    
                    using var process = Process.Start(startInfo);
                    if (process != null)
                    {
                        string output = await process.StandardOutput.ReadToEndAsync();
                        await process.WaitForExitAsync();
                        
                        if (process.ExitCode == 0 && !string.IsNullOrWhiteSpace(output))
                        {
                            // Write decompiled output to .lua file
                            await File.WriteAllTextAsync(luaFile, output);
                            successCount++;
                            
                            // Delete .bin if user doesn't want to keep it
                            if (!keepBinFiles)
                            {
                                try { File.Delete(binFile); } catch { }
                            }
                        }
                        else
                        {
                            failCount++;
                        }
                    }
                    else
                    {
                        failCount++;
                    }
                }
                catch (Exception ex)
                {
                    failCount++;
                    AppendLog($"BadPlace | Error decompiling {relativePath}: {ex.Message}");
                }
            }
            
            AppendLog($"BadPlace | Decompile complete! Success: {successCount}, Failed: {failCount}");
        }
        catch (Exception ex)
        {
            AppendLog($"BadPlace | Decompile error: {ex.Message}");
        }
    }

    private List<int> _openTabs = new();

    private void Tab_Click(object? sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag != null)
        {
            Editor.Text = "";
            AppendLog($"BadPlace | Switched to {btn.Content}");
        }
    }

    private void NewTab_Click(object? sender, RoutedEventArgs e)
    {
        var nextTab = _openTabs.Count + 1;
        var newBtn = new Button
        {
            Content = $"Tab {nextTab}",
            Tag = nextTab,
            Margin = new Avalonia.Thickness(2),
            Padding = new Avalonia.Thickness(6, 3),
            Background = new SolidColorBrush(0xFF383838),
            Foreground = Brushes.White
        };
        newBtn.Click += Tab_Click;
        
        if (TabBar is StackPanel sp)
        {
            sp.Children.Insert(sp.Children.Count - 1, newBtn);
        }
        
        _openTabs.Add(nextTab);
        Editor.Text = "";
    }
}