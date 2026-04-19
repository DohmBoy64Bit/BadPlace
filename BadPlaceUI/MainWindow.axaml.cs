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

public class DocumentTab
{
    public int Id { get; set; }
    public string Title { get; set; } = "Untitled";
    public string? FilePath { get; set; }
    public string Content { get; set; } = "";
    public bool IsModified { get; set; }
    public bool IsInitial { get; set; }
    public Control? TabPanel { get; set; }
    public Button? TabButton { get; set; }
}

public partial class MainWindow : Window
{
    private CompletionWindow? _completionWindow;
    private PipeClient?       _pipe;
    private string            _baseDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "TheBadPlace");
    private ObservableCollection<string> _scripts = new();
    private OptionsWindow? _optionsWindow;
    private List<DocumentTab> _documents = new();
    private int _tabCounter = 1;
    private DocumentTab? _activeDoc;

    public MainWindow()
    {
        InitializeComponent();
        InitializeEnvironment();
        LoadLuaSyntax();

        ScriptList.ItemsSource = _scripts;

        Editor.TextArea.TextEntering += TextArea_TextEntering;
        Editor.TextArea.TextEntered += TextArea_TextEntered;
        Editor.TextChanged += Editor_TextChanged;

        CreateNewTab();
        RefreshScriptList();
    }

    private void Editor_TextChanged(object? sender, EventArgs e)
    {
        if (_activeDoc != null)
        {
            _activeDoc.Content = Editor.Text;
            _activeDoc.IsModified = _activeDoc.Content != GetFileContent(_activeDoc.FilePath);
            UpdateTabTitle(_activeDoc);
        }
    }

    private string GetFileContent(string? path)
    {
        if (string.IsNullOrEmpty(path) || !File.Exists(path)) return "";
        return File.ReadAllText(path);
    }

    private void UpdateTabTitle(DocumentTab doc)
    {
        var title = doc.IsModified ? "● " : "";
        title += string.IsNullOrEmpty(doc.FilePath) ? doc.Title : Path.GetFileName(doc.FilePath);
        doc.TabButton.Content = title;
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

                var existing = _documents.FirstOrDefault(d => d.FilePath == fullPath);
                if (existing != null)
                {
                    SwitchToDocument(existing);
                }
                else
                {
                    var content = File.ReadAllText(fullPath);
                    _tabCounter++;
                    var doc = new DocumentTab
                    {
                        Id = _tabCounter,
                        Title = fileName,
                        FilePath = fullPath,
                        Content = content
                    };

                    var btn = new Button
                    {
                        Content = fileName,
                        Tag = doc,
                        Margin = new Avalonia.Thickness(2),
                        Padding = new Avalonia.Thickness(6, 3),
                        Background = new SolidColorBrush(0xFF383838),
                        Foreground = Brushes.White
                    };
                    btn.Click += Tab_Click;

                    var contextMenu = new ContextMenu();
                    var closeItem = new MenuItem { Header = "Close" };
                    closeItem.Click += (s, ev) => CloseTab_Click(doc);
                    contextMenu.ItemsSource = new[] { closeItem };
                    btn.ContextMenu = contextMenu;

                    var panel = new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Tag = doc };
                    panel.Children.Add(btn);
                    doc.TabButton = btn;
                    doc.TabPanel = panel;

                    if (TabBar is StackPanel sp)
                    {
                        sp.Children.Insert(sp.Children.Count - 1, panel);
                    }

                    _documents.Add(doc);
                    SwitchToDocument(doc);
                }
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
            var filePath = files[0].Path.LocalPath;
            var content = await File.ReadAllTextAsync(filePath);

            var existing = _documents.FirstOrDefault(d => d.FilePath == filePath);
            if (existing != null)
            {
                SwitchToDocument(existing);
            }
            else
            {
                _tabCounter++;
                var doc = new DocumentTab
                {
                    Id = _tabCounter,
                    Title = files[0].Name,
                    FilePath = filePath,
                    Content = content
                };

                var btn = new Button
                {
                    Content = files[0].Name,
                    Tag = doc,
                    Margin = new Avalonia.Thickness(2),
                    Padding = new Avalonia.Thickness(6, 3),
                    Background = new SolidColorBrush(0xFF383838),
                    Foreground = Brushes.White
                };
                btn.Click += Tab_Click;

                var contextMenu = new ContextMenu();
                var closeItem = new MenuItem { Header = "Close" };
                closeItem.Click += (s, ev) => CloseTab_Click(doc);
                contextMenu.ItemsSource = new[] { closeItem };
                btn.ContextMenu = contextMenu;

                var panel = new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Tag = doc };
                panel.Children.Add(btn);
                doc.TabButton = btn;
                doc.TabPanel = panel;

                if (TabBar is StackPanel sp)
                {
                    sp.Children.Insert(sp.Children.Count - 1, panel);
                }

                _documents.Add(doc);
                SwitchToDocument(doc);
            }
            AppendLog($"BadPlace | Loaded {files[0].Name}");
        }
    }

    private async void SaveFile_Click(object? sender, RoutedEventArgs e)
    {
        if (_activeDoc == null) return;

        if (!string.IsNullOrEmpty(_activeDoc.FilePath))
        {
            await File.WriteAllTextAsync(_activeDoc.FilePath, Editor.Text);
            _activeDoc.IsModified = false;
            UpdateTabTitle(_activeDoc);
            AppendLog($"BadPlace | Saved {_activeDoc.Title}");
            return;
        }

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
            var filePath = file.Path.LocalPath;
            await File.WriteAllTextAsync(filePath, Editor.Text);
            _activeDoc.FilePath = filePath;
            _activeDoc.Title = file.Name;
            _activeDoc.IsModified = false;
            UpdateTabTitle(_activeDoc);
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

private void CreateNewTab(bool hasCloseButton = true)
    {
        _tabCounter++;
        var doc = new DocumentTab
        {
            Id = _tabCounter,
            Title = $"Untitled-{_tabCounter - 1}",
            Content = "",
            IsInitial = !hasCloseButton
        };

        var btn = new Button
        {
            Content = doc.Title,
            Tag = doc,
            Margin = new Avalonia.Thickness(2),
            Padding = new Avalonia.Thickness(6, 3),
            Background = new SolidColorBrush(0xFF383838),
            Foreground = Brushes.White
        };
        btn.Click += Tab_Click;

        if (hasCloseButton)
        {
            var contextMenu = new ContextMenu();
            var closeItem = new MenuItem { Header = "Close" };
            closeItem.Click += (s, e) => CloseTab_Click(doc);
            contextMenu.ItemsSource = new[] { closeItem };
            btn.ContextMenu = contextMenu;
        }

        var container = new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Tag = doc };
        container.Children.Add(btn);

        doc.TabButton = btn;
        doc.TabPanel = container;

        if (TabBar is StackPanel sp)
        {
            sp.Children.Insert(sp.Children.Count - 1, container);
        }

        _documents.Add(doc);
        SwitchToDocument(doc);
    }

    private void Tab_Click(object? sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is DocumentTab doc)
        {
            SwitchToDocument(doc);
        }
    }

    private void SwitchToDocument(DocumentTab doc)
    {
        if (_activeDoc != null)
        {
            _activeDoc.Content = Editor.Text;
            _activeDoc.TabButton!.Background = new SolidColorBrush(0xFF383838);
        }
        _activeDoc = doc;
        Editor.Text = doc.Content;

        doc.TabButton!.Background = new SolidColorBrush(0xFF505050);
    }

    private void CloseTab_Click(DocumentTab doc)
    {
        int idx = _documents.IndexOf(doc);
        if (idx >= 0)
        {
            if (TabBar is StackPanel sp && doc.TabPanel != null)
            {
                sp.Children.Remove(doc.TabPanel);
            }
            _documents.Remove(doc);
            if (_documents.Count == 0)
            {
CreateNewTab(false);
            }
            else if (_activeDoc == doc)
            {
                var newIdx = Math.Min(idx, _documents.Count - 1);
                SwitchToDocument(_documents[newIdx]);
            }
        }
    }

    private void NewTab_Click(object? sender, RoutedEventArgs e)
    {
        CreateNewTab();
    }
}