using System;
using Avalonia.Media;
using AvaloniaEdit.CodeCompletion;
using AvaloniaEdit.Document;
using AvaloniaEdit.Editing;

namespace BadPlaceUI
{
    public class LuaCompletionData : ICompletionData
    {
        public LuaCompletionData(string text, string description = "")
        {
            Text = text;
            Description = description;
        }

        public IImage? Image => null;

        public string Text { get; }

        public object Content => Text;

        public object Description { get; }

        public double Priority => 0;

        public void Complete(TextArea textArea, ISegment completionSegment, EventArgs insertionRequestEventArgs)
        {
            textArea.Document.Replace(completionSegment, Text);
        }
    }
}
