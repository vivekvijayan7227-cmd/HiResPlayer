using System;
using HiResPlayer.Infrastructure.Native;

class Program {
    static void Main() {
        var facade = new NativeAudioFacade();
        var devices = facade.EnumerateDevices();
        Console.WriteLine($"Found {devices.Count} audio endpoints:");
        foreach (var d in devices) {
            Console.WriteLine($" - ID: {d.Id}");
            Console.WriteLine($"   Name: {d.FriendlyName}");
            Console.WriteLine($"   State: {d.State}");
            Console.WriteLine($"   IsDefault: {d.IsDefault}");
        }
    }
}
