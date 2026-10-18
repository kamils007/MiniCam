using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Com = System.Runtime.InteropServices.ComTypes;

// Metadata only: REGKIND_NONE neither registers nor starts the EXE.
public static class MiniCamSolidLinkMetadata
{
    [DllImport("oleaut32.dll", CharSet = CharSet.Unicode, PreserveSig = false)]
    private static extern void LoadTypeLibEx(string file, int regKind,
        [MarshalAs(UnmanagedType.Interface)] out Com.ITypeLib library);

    public static string Verify(string file)
    {
        Com.ITypeLib library = null;
        try
        {
            LoadTypeLibEx(file, 2, out library);
            int count = library.GetTypeInfoCount();
            if (count < 1 || count > 256) throw new InvalidOperationException("Unexpected type library size.");
            for (int i = 0; i < count; ++i)
            {
                Com.ITypeInfo info = null;
                IntPtr attrPtr = IntPtr.Zero;
                try
                {
                    library.GetTypeInfo(i, out info);
                    string name, doc, help;
                    int context;
                    info.GetDocumentation(-1, out name, out doc, out context, out help);
                    if (name != "ISolidLink") continue;
                    info.GetTypeAttr(out attrPtr);
                    Com.TYPEATTR attr = (Com.TYPEATTR)Marshal.PtrToStructure(attrPtr, typeof(Com.TYPEATTR));
                    if (attr.typekind != Com.TYPEKIND.TKIND_DISPATCH || attr.cFuncs > 256)
                        throw new InvalidOperationException("Unexpected ISolidLink type kind.");
                    Dictionary<string, int[]> expected = new Dictionary<string, int[]>();
                    // DISPID, result VARTYPE, then parameter VARTYPEs.
                    expected.Add("TranslateSolid", new int[] { 1, 3, 8, 8, 8 });
                    expected.Add("SetOutputLogFile", new int[] { 8, 24, 11 });
                    expected.Add("GetInteropResult", new int[] { 10, 3 });
                    expected.Add("GetFileType", new int[] { 12, 3, 8 });
                    HashSet<string> seen = new HashSet<string>();
                    for (int j = 0; j < attr.cFuncs; ++j)
                    {
                        IntPtr funcPtr = IntPtr.Zero;
                        try
                        {
                            info.GetFuncDesc(j, out funcPtr);
                            Com.FUNCDESC f = (Com.FUNCDESC)Marshal.PtrToStructure(funcPtr, typeof(Com.FUNCDESC));
                            info.GetDocumentation(f.memid, out name, out doc, out context, out help);
                            if (!expected.ContainsKey(name)) continue;
                            int[] contract = expected[name];
                            if (!seen.Add(name) || f.invkind != Com.INVOKEKIND.INVOKE_FUNC ||
                                f.memid != contract[0] || f.elemdescFunc.tdesc.vt != contract[1] ||
                                f.cParams != contract.Length - 2 || f.cParamsOpt != 0)
                                throw new InvalidOperationException("Unexpected public method contract: " + name);
                            int stride = Marshal.SizeOf(typeof(Com.ELEMDESC));
                            for (int k = 0; k < f.cParams; ++k)
                            {
                                IntPtr paramPtr = IntPtr.Add(f.lprgelemdescParam, k * stride);
                                Com.ELEMDESC p = (Com.ELEMDESC)Marshal.PtrToStructure(paramPtr, typeof(Com.ELEMDESC));
                                int flags = (int)p.desc.paramdesc.wParamFlags;
                                if (p.tdesc.vt != contract[k + 2] || (flags & 10) != 0)
                                    throw new InvalidOperationException("Unexpected parameter contract: " + name);
                            }
                        }
                        finally { if (funcPtr != IntPtr.Zero) info.ReleaseFuncDesc(funcPtr); }
                    }
                    if (seen.Count != expected.Count) throw new InvalidOperationException("Required SolidLink methods are missing.");
                    return "ISolidLink: TranslateSolid, SetOutputLogFile, GetInteropResult, GetFileType verified";
                }
                finally
                {
                    if (attrPtr != IntPtr.Zero && info != null) info.ReleaseTypeAttr(attrPtr);
                    if (info != null) Marshal.ReleaseComObject(info);
                }
            }
            throw new InvalidOperationException("ISolidLink was not found in the selected executable's type library.");
        }
        finally { if (library != null) Marshal.ReleaseComObject(library); }
    }
}
