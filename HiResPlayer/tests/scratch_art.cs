using System;
using HiResPlayer.Application.Services;

class Program {
    static void Main() {
        string path = @"D:\Songs\A.R. Rahman - Aao Balma (From Coke Studio @ MTV Se.flac";
        var meta = LibraryScanner.ReadFlacMetadata(path);
        Console.WriteLine($"Title: {meta.Title}");
        Console.WriteLine($"Artist: {meta.Artist}");
        Console.WriteLine($"Album: {meta.Album}");
        Console.WriteLine($"ArtworkBytes: {(meta.ArtworkBytes == null ? "NULL" : meta.ArtworkBytes.Length + " bytes")}");
    }
}
