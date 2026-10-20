using System;
using System.IO;

// Portable tests of the bounded PE metadata reader. Synthetic symbol names
// below are fixtures, not evidence of any vendor conversion capability.
public static class MetadataReaderTest
{
    private static void Word(byte[] data, int p, uint value)
    {
        Array.Copy(BitConverter.GetBytes(value), 0, data, p, 4);
    }
    private static void Short(byte[] data, int p, ushort value)
    {
        Array.Copy(BitConverter.GetBytes(value), 0, data, p, 2);
    }
    private static void Reject(string path, byte[] data)
    {
        File.WriteAllBytes(path, data);
        bool rejected = false;
        try { MiniCamTranslatorMetadata.ExportNames(path); }
        catch (Exception) { rejected = true; }
        if (!rejected) throw new Exception("Malformed PE accepted.");
    }
    public static int Main()
    {
        string folder = Path.Combine(Path.GetTempPath(), "minicam-metadata-test-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        string file = Path.Combine(folder, "fixture.bin");
        try
        {
            var data = new byte[1024];
            Short(data,0,0x5a4d); Word(data,0x3c,128); Word(data,128,0x4550);
            Short(data,132,0x8664); Short(data,134,1); Short(data,148,240);
            Short(data,152,0x20b); Word(data,264,4096);
            Word(data,404,4096); Word(data,408,512); Word(data,412,512);
            Word(data,536,1); Word(data,544,4184); Word(data,600,4204);
            Array.Copy(System.Text.Encoding.ASCII.GetBytes("FixtureExport\0"),0,data,620,14);
            File.WriteAllBytes(file,data);
            var names = MiniCamTranslatorMetadata.ExportNames(file);
            if (names.Length != 1 || names[0] != "FixtureExport") throw new Exception("Export table not read.");
            for (int n = 0; n < 634; n += 13)
            {
                var truncated = new byte[n]; Array.Copy(data,truncated,n); Reject(file,truncated);
            }
            var bad = (byte[])data.Clone(); Word(bad,536,uint.MaxValue); Reject(file,bad);
            bad = (byte[])data.Clone(); Word(bad,600,0xffffff00); Reject(file,bad);
            bad = (byte[])data.Clone(); bad[620] = 255; Reject(file,bad);
            Word(data,264,0); File.WriteAllBytes(file,data);
            if (MiniCamTranslatorMetadata.ExportNames(file).Length != 0) throw new Exception("Absent exports not recognized.");
            Console.WriteLine("PASS: export names, absent exports, truncated and malformed PE metadata.");
            return 0;
        }
        catch (Exception e) { Console.Error.WriteLine(e.Message); return 1; }
        finally { Directory.Delete(folder,true); }
    }
}
