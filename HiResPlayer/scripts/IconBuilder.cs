using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

public static class IconBuilder
{
    public static void CreateIcoFromPng(string sourcePngPath, string outputIcoPath)
    {
        using var original = new Bitmap(sourcePngPath);
        int[] sizes = { 256, 128, 64, 48, 32, 16 };
        var pngStreams = new List<byte[]>();

        foreach (int size in sizes)
        {
            using var resized = new Bitmap(size, size, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(resized))
            {
                g.CompositingQuality = CompositingQuality.HighQuality;
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.SmoothingMode = SmoothingMode.HighQuality;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                g.Clear(Color.Transparent);
                g.DrawImage(original, new Rectangle(0, 0, size, size));
            }

            using var ms = new MemoryStream();
            resized.Save(ms, ImageFormat.Png);
            pngStreams.Add(ms.ToArray());
        }

        using var fs = new FileStream(outputIcoPath, FileMode.Create, FileAccess.Write);
        using var bw = new BinaryWriter(fs);

        // Header
        bw.Write((short)0);             // Reserved
        bw.Write((short)1);             // 1 = ICO
        bw.Write((short)sizes.Length);  // Count

        int offset = 6 + (16 * sizes.Length);

        // Directory Entries
        for (int i = 0; i < sizes.Length; i++)
        {
            int s = sizes[i];
            bw.Write((byte)(s >= 256 ? 0 : s)); // Width
            bw.Write((byte)(s >= 256 ? 0 : s)); // Height
            bw.Write((byte)0);                  // Colors
            bw.Write((byte)0);                  // Reserved
            bw.Write((short)1);                 // Planes
            bw.Write((short)32);                // Bit count
            bw.Write((int)pngStreams[i].Length);// Size of image
            bw.Write((int)offset);              // Offset of image
            offset += pngStreams[i].Length;
        }

        // Image Data
        for (int i = 0; i < sizes.Length; i++)
        {
            bw.Write(pngStreams[i]);
        }

        Console.WriteLine($"Icon created successfully with {sizes.Length} resolutions at: {outputIcoPath}");
    }
}
