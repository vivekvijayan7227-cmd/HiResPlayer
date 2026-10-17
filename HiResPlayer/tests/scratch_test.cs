using System;
using System.IO;
using HiResPlayer.Infrastructure.Native;

class Program {
    static void Main() {
        Console.WriteLine("Creating facade...");
        var facade = new NativeAudioFacade();
        string file = @"D:\Songs\antonio-vivaldi_the-four-seasons_202308\Antonio Vivaldi - The Four Seasons\11. 11 II Largo - Winter.flac";
        Console.WriteLine("Opening FLAC: " + file);
        bool ok = facade.OpenFlac(file);
        Console.WriteLine("OpenFlac returned: " + ok);
        var st = facade.GetStatus();
        Console.WriteLine($"Status: {st.SourceRate}Hz {st.SourceValidBits}bit {st.SourceChannels}ch total={st.SourceTotalSamples}");
        Console.WriteLine("Starting Play...");
        bool pok = facade.Play();
        Console.WriteLine("Play returned: " + pok);
        System.Threading.Thread.Sleep(2000);
        var st2 = facade.GetStatus();
        Console.WriteLine($"Rendered: {st2.RenderedSourceSamples} Underruns: {st2.UnderrunCount} Mode: {st2.ActualMode}");
        Console.WriteLine("Stopping...");
        facade.Stop();
        Console.WriteLine("Stopped!");
    }
}
