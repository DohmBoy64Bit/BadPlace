using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using System;
using System.Diagnostics;
using System.IO;

namespace BadPlaceUI;

public partial class OptionsWindow : Window
{
    private string _baseDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "TheBadPlace");
    private string _bytecodeCacheDir = "";

    // Options state
    public bool AutoDecompile { get; private set; }
    public bool SaveBytecode { get; private set; } = true;

    // Event fired when dump is requested
    public event Action? DumpRequested;

    public OptionsWindow()
    {
        InitializeComponent();
        _bytecodeCacheDir = Path.Combine(_baseDir, "BytecodeCache");
    }

    private void Close_Click(object? sender, RoutedEventArgs e)
    {
        // Save checkbox states
        AutoDecompile = AutoDecompileCheckBox?.IsChecked == true;
        SaveBytecode = SaveBytecodeCheckBox?.IsChecked == true;
        Close();
    }

    private void DumpScripts_Click(object? sender, RoutedEventArgs e)
    {
        // Save checkbox states
        AutoDecompile = AutoDecompileCheckBox?.IsChecked == true;
        SaveBytecode = SaveBytecodeCheckBox?.IsChecked == true;
        
        // Fire event and close
        DumpRequested?.Invoke();
        Close();
    }

    private void OpenBytecodeCache_Click(object? sender, RoutedEventArgs e)
    {
        try
        {
            if (!Directory.Exists(_bytecodeCacheDir))
                Directory.CreateDirectory(_bytecodeCacheDir);
            
            Process.Start(new ProcessStartInfo
            {
                FileName = _bytecodeCacheDir,
                UseShellExecute = true
            });
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error opening cache: {ex.Message}");
        }
    }

    private void ClearBytecodeCache_Click(object? sender, RoutedEventArgs e)
    {
        try
        {
            if (Directory.Exists(_bytecodeCacheDir))
            {
                foreach (var file in Directory.GetFiles(_bytecodeCacheDir, "*.bin", SearchOption.AllDirectories))
                {
                    File.Delete(file);
                }
                
                foreach (var dir in Directory.GetDirectories(_bytecodeCacheDir))
                {
                    if (Directory.GetFiles(dir).Length == 0)
                        Directory.Delete(dir, true);
                }
            }
            
            // Also clear cache dir itself to remove any leftover structure
            if (Directory.Exists(_bytecodeCacheDir))
            {
                Directory.Delete(_bytecodeCacheDir, true);
                Directory.CreateDirectory(_bytecodeCacheDir);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error clearing cache: {ex.Message}");
        }
    }
}