using System;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;

namespace BadPlaceUI.IPC;

public class PipeClient : IDisposable
{
    private NamedPipeClientStream? _execPipe;
    private NamedPipeClientStream? _logPipe;
    private BinaryWriter?          _execWriter;
    private Thread?                _logThread;
    private bool                   _disposed;

    /// <summary>Fired on the log reader thread — marshal to UI thread before touching controls.</summary>
    public event Action<string>? LogReceived;

    public bool IsConnected { get; private set; }

    // ── Connect ─────────────────────────────────────────────────────────────
    public bool Connect(int timeoutMs = 5000)
    {
        try
        {
            // Exec pipe: we write scripts TO the DLL
            _execPipe  = new NamedPipeClientStream(".", "BadPlaceExec", PipeDirection.Out);
            _execPipe.Connect(timeoutMs);
            _execWriter = new BinaryWriter(_execPipe, Encoding.UTF8, leaveOpen: true);

            // Log pipe: we read logs FROM the DLL
            _logPipe = new NamedPipeClientStream(".", "BadPlaceLogs", PipeDirection.In);
            _logPipe.Connect(timeoutMs);

            _logThread = new Thread(LogReaderLoop)
            {
                IsBackground = true,
                Name = "BadPlace.LogReader"
            };
            _logThread.Start();

            IsConnected = true;
            return true;
        }
        catch
        {
            IsConnected = false;
            return false;
        }
    }

    // ── Send script ─────────────────────────────────────────────────────────
    public void SendScript(string script)
    {
        if (!IsConnected || _execWriter == null) return;
        try
        {
            byte[] data = Encoding.UTF8.GetBytes(script);
            _execWriter.Write((uint)data.Length);
            _execWriter.Write(data);
            _execWriter.Flush();
        }
        catch
        {
            IsConnected = false;
        }
    }

    // ── Log reader (background thread) ──────────────────────────────────────
    private void LogReaderLoop()
    {
        try
        {
            using var reader = new BinaryReader(_logPipe!, Encoding.UTF8, leaveOpen: true);
            while (!_disposed && (_logPipe?.IsConnected ?? false))
            {
                uint len = reader.ReadUInt32();
                if (len == 0 || len > 1024 * 1024) break;

                byte[] data    = reader.ReadBytes((int)len);
                string message = Encoding.UTF8.GetString(data);
                LogReceived?.Invoke(message);
            }
        }
        catch { /* pipe closed or DLL detached */ }

        IsConnected = false;
    }

    // ── Cleanup ─────────────────────────────────────────────────────────────
    public void Dispose()
    {
        _disposed = true;
        _execWriter?.Dispose();
        _execPipe?.Dispose();
        _logPipe?.Dispose();
    }
}
