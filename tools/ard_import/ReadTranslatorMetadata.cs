using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
using Com = System.Runtime.InteropServices.ComTypes;

// Read embedded type-library metadata only. REGKIND_NONE does not register a
// server, activate COM objects or invoke the translator's conversion methods.
public static class MiniCamTranslatorMetadata
{
    private static uint U32(byte[] data, int p)
    {
        if (p < 0 || p > data.Length - 4) throw new InvalidDataException("Truncated PE.");
        return BitConverter.ToUInt32(data, p);
    }
    private static int U16(byte[] data, int p)
    {
        if (p < 0 || p > data.Length - 2) throw new InvalidDataException("Truncated PE.");
        return BitConverter.ToUInt16(data, p);
    }
    private static int Offset(byte[] data, int sections, int count, uint rva, int size)
    {
        for (int i = 0; i < count; ++i)
        {
            int s = checked(sections + 40 * i);
            uint start = U32(data, s + 12), rawSize = U32(data, s + 16), raw = U32(data, s + 20);
            if (rva >= start && (ulong)(rva - start) + (ulong)size <= rawSize)
            {
                int p = checked((int)((ulong)raw + rva - start));
                if (p > data.Length - size) throw new InvalidDataException("PE span outside file.");
                return p;
            }
        }
        throw new InvalidDataException("PE span outside a section.");
    }
    public static string[] ExportNames(string path)
    {
        if (new FileInfo(path).Length > 256L * 1024 * 1024) throw new InvalidDataException("PE too large.");
        byte[] data = File.ReadAllBytes(path);
        if (U16(data, 0) != 0x5a4d) throw new InvalidDataException("Missing MZ signature.");
        int pe = checked((int)U32(data, 0x3c));
        if (U32(data, pe) != 0x4550 || U16(data, pe + 4) != 0x8664)
            throw new InvalidDataException("Expected AMD64 PE.");
        int count = U16(data, pe + 6), optionalSize = U16(data, pe + 20);
        int optional = checked(pe + 24);
        if (count < 1 || count > 96 || optionalSize < 120 || U16(data, optional) != 0x20b)
            throw new InvalidDataException("Unexpected PE headers.");
        uint exportRva = U32(data, optional + 112);
        if (exportRva == 0) return new string[0];
        int sections = checked(optional + optionalSize);
        int directory = Offset(data, sections, count, exportRva, 40);
        uint names = U32(data, directory + 24);
        if (names > 65536) throw new InvalidDataException("Too many PE exports.");
        if (names == 0) return new string[0];
        int table = Offset(data, sections, count, U32(data, directory + 32), checked((int)names * 4));
        var result = new List<string>();
        for (int i = 0; i < names; ++i)
        {
            uint rva = U32(data, table + i * 4);
            var text = new StringBuilder();
            bool terminated = false;
            for (int j = 0; j < 1024; ++j)
            {
                byte value = data[Offset(data, sections, count, checked(rva + (uint)j), 1)];
                if (value == 0) { terminated = true; break; }
                if (value > 127 || value < 32) throw new InvalidDataException("Invalid export name.");
                text.Append((char)value);
            }
            if (!terminated) throw new InvalidDataException("Unterminated export name.");
            result.Add(text.ToString());
        }
        return result.ToArray();
    }
    [DllImport("oleaut32.dll", CharSet = CharSet.Unicode, PreserveSig = false)]
    private static extern void LoadTypeLibEx(string path, int kind, out Com.ITypeLib library);

    public static object Read(string path)
    {
        Com.ITypeLib library = null;
        try
        {
            LoadTypeLibEx(path, 2, out library);
            int count = library.GetTypeInfoCount();
            if (count < 0 || count > 4096) throw new InvalidOperationException("Unexpected type count.");
            var types = new List<object>();
            for (int i = 0; i < count; ++i)
            {
                Com.ITypeInfo info = null;
                IntPtr attrPtr = IntPtr.Zero;
                try
                {
                    library.GetTypeInfo(i, out info);
                    string name, doc, help; int context;
                    info.GetDocumentation(-1, out name, out doc, out context, out help);
                    info.GetTypeAttr(out attrPtr);
                    var attr = (Com.TYPEATTR)Marshal.PtrToStructure(attrPtr, typeof(Com.TYPEATTR));
                    if (attr.cFuncs < 0 || attr.cFuncs > 4096) throw new InvalidOperationException("Unexpected method count.");
                    var methods = new List<object>();
                    for (int j = 0; j < attr.cFuncs; ++j)
                    {
                        IntPtr funcPtr = IntPtr.Zero;
                        try
                        {
                            info.GetFuncDesc(j, out funcPtr);
                            var func = (Com.FUNCDESC)Marshal.PtrToStructure(funcPtr, typeof(Com.FUNCDESC));
                            string methodName;
                            info.GetDocumentation(func.memid, out methodName, out doc, out context, out help);
                            if (func.cParams < 0 || func.cParams > 256) throw new InvalidOperationException("Unexpected parameter count.");
                            var parameters = new List<object>();
                            for (int k = 0; k < func.cParams; ++k)
                            {
                                var ptr = IntPtr.Add(func.lprgelemdescParam, k * Marshal.SizeOf(typeof(Com.ELEMDESC)));
                                var p = (Com.ELEMDESC)Marshal.PtrToStructure(ptr, typeof(Com.ELEMDESC));
                                parameters.Add(new { vartype = p.tdesc.vt, flags = (int)p.desc.paramdesc.wParamFlags });
                            }
                            methods.Add(new { name = methodName, dispid = func.memid,
                                invocation = func.invkind.ToString(), result_vartype = func.elemdescFunc.tdesc.vt,
                                parameters = parameters });
                        }
                        finally { if (funcPtr != IntPtr.Zero) info.ReleaseFuncDesc(funcPtr); }
                    }
                    types.Add(new { name = name, guid = attr.guid.ToString(),
                        kind = attr.typekind.ToString(), methods = methods });
                }
                finally
                {
                    if (attrPtr != IntPtr.Zero && info != null) info.ReleaseTypeAttr(attrPtr);
                    if (info != null) Marshal.ReleaseComObject(info);
                }
            }
            return types;
        }
        finally { if (library != null) Marshal.ReleaseComObject(library); }
    }
}
