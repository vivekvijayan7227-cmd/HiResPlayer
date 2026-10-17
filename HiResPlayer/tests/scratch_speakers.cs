using System;
using System.IO;
using System.Threading;
using HiResPlayer.Infrastructure.Native;

class Program {
    static void Main() {
        using var facade = new NativeAudioFacade();
        string speakerId = "{0.0.0.00000000}.{4a4416e8-5851-4c22-9962-48d79fa06300}";
        Console.WriteLine("Selecting Speakers: " + speakerId);
        bool sOk = facade.SelectDevice(speakerId);
        Console.WriteLine("SelectDevice: " + sOk);

        string file = @"D:\Songs\03_Harris_Jayaraj_Lolita_From_Engeyum_Kadhal.flac";
        bool oOk = facade.OpenFlac(file);
        Console.WriteLine("OpenFlac: " + oOk);

        bool pOk = facade.Play();
        Console.WriteLine("Play: " + pOk);

        for (int i = 0; i < 4; i++) {
            Thread.Sleep(500);
            var st = facade.GetStatus();
            Console.WriteLine($"T+{(i+1)*500}ms: Rendered={st.RenderedSourceSamples}, Rate={st.OutputRate}, HResult=0x{st.LastHresult:X8}");
        }

        facade.Stop();
        Console.WriteLine("Finished!");
    }
}
