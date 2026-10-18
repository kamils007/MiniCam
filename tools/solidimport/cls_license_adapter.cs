using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

// Native order and signatures: Acam.exe 2025.4.1.104 TYPELIB/694/2057.
// This is an in-process CCW only. It is never registered as the vendor coclass.
[ComVisible(true)]
[Guid("214DE706-D211-4A06-84C3-6CB3044A4685")]
[InterfaceType(ComInterfaceType.InterfaceIsDual)]
public interface IMiniCamClsLicense
{
    [DispId(1), PreserveSig] int GetAcamModuleLicense([MarshalAs(UnmanagedType.BStr)] string name, out int value);
    [DispId(2), PreserveSig] int ReleaseLicense(int handle);
    [DispId(3), PreserveSig] int IsFeatureLicensed([MarshalAs(UnmanagedType.BStr)] string name, [MarshalAs(UnmanagedType.VariantBool)] out bool value);
    [DispId(4), PreserveSig] int GetLastError(out int value);
    [DispId(5), PreserveSig] int GetCustomerName([MarshalAs(UnmanagedType.BStr)] out string value);
    [DispId(6), PreserveSig] int GetAcamOptionLicense([MarshalAs(UnmanagedType.BStr)] string fileName, out int value);
    [DispId(7), PreserveSig] int GetLicenseServerName([MarshalAs(UnmanagedType.BStr)] out string value);
    [DispId(8), PreserveSig] int GetUpdateExpiryMonth(out int value);
    [DispId(9), PreserveSig] int IsOptionLicensed(int accessCode, [MarshalAs(UnmanagedType.VariantBool)] out bool value);
    [DispId(10), PreserveSig] int GetLicenseDaysToExpire(out int value);
    [DispId(11), PreserveSig] int GetMaintenanceExpired([MarshalAs(UnmanagedType.VariantBool)] out bool value);
    [DispId(12), PreserveSig] int IsNamedOptionLicensed(int accessCode, [MarshalAs(UnmanagedType.BStr)] string name, [MarshalAs(UnmanagedType.VariantBool)] out bool value);
}

[ComVisible(true)]
[ClassInterface(ClassInterfaceType.None)]
[ComDefaultInterface(typeof(IMiniCamClsLicense))]
public sealed class MiniCamClsLicenseAdapter : IMiniCamClsLicense
{
    private const int NotImplemented = unchecked((int)0x80004001);
    private const int InvalidArgument = unchecked((int)0x80070057);
    private const int Failure = unchecked((int)0x80004005);
    private readonly MethodInfo getAccess;
    private readonly MethodInfo validate;
    private readonly object silent;
    public bool RequestAttempted { get; private set; }
    public bool Granted { get; private set; }
    public int BeforeValidation { get; private set; }
    public int AfterValidation { get; private set; }
    public int RequestCount { get; private set; }
    public string ErrorType { get; private set; }

    public MiniCamClsLicenseAdapter(Type licenseType, object silentValue)
    {
        if (licenseType == null || !licenseType.IsPublic || licenseType.FullName != "PlanitCls.License")
            throw new ArgumentException("Unexpected public CLS type");
        silent = silentValue;
        getAccess = licenseType.GetMethod("GetAccessCode", BindingFlags.Public | BindingFlags.Static,
            null, new Type[] { typeof(int) }, null);
        validate = licenseType.GetMethod("ValidLicenseConnection", BindingFlags.Public | BindingFlags.Static,
            null, new Type[] { silent.GetType() }, null);
        if (getAccess == null || getAccess.ReturnType != typeof(bool) || validate == null || !validate.ReturnType.IsEnum)
            throw new InvalidOperationException("Unexpected CLS method signatures");
    }

    public int IsFeatureLicensed(string name, out bool value)
    {
        value = false;
        if (String.IsNullOrEmpty(name)) return InvalidArgument;
        if (!String.Equals(name, "Core:Solids", StringComparison.Ordinal)) return NotImplemented;
        // Do not reuse a grant from a previous invocation or issue other codes.
        if (RequestAttempted) return NotImplemented;
        try {
            BeforeValidation = Convert.ToInt32(validate.Invoke(null, new object[] { silent }));
            if (BeforeValidation != 101) return Failure;
            RequestAttempted = true;
            RequestCount++;
            Granted = (bool)getAccess.Invoke(null, new object[] { 390 });
            AfterValidation = Convert.ToInt32(validate.Invoke(null, new object[] { silent }));
            if (AfterValidation != 101) return Failure;
            value = Granted;
            return 0;
        } catch (Exception error) {
            ErrorType = (error.InnerException ?? error).GetType().FullName;
            return Failure;
        }
    }

    // No fabricated module handles, customer details, expiry or option grants.
    public int GetAcamModuleLicense(string name, out int value) { value = 0; return NotImplemented; }
    public int ReleaseLicense(int handle) { return NotImplemented; }
    public int GetLastError(out int value) { value = 0; return NotImplemented; }
    public int GetCustomerName(out string value) { value = null; return NotImplemented; }
    public int GetAcamOptionLicense(string name, out int value) { value = 0; return NotImplemented; }
    public int GetLicenseServerName(out string value) { value = null; return NotImplemented; }
    public int GetUpdateExpiryMonth(out int value) { value = 0; return NotImplemented; }
    public int IsOptionLicensed(int code, out bool value) { value = false; return NotImplemented; }
    public int GetLicenseDaysToExpire(out int value) { value = 0; return NotImplemented; }
    public int GetMaintenanceExpired(out bool value) { value = false; return NotImplemented; }
    public int IsNamedOptionLicensed(int code, string name, out bool value) { value = false; return NotImplemented; }
}

public static class MiniCamClsLicenseNativeProbe
{
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int FeatureCall(IntPtr self, IntPtr name, IntPtr answer);

    private static short Call(FeatureCall method, IntPtr self, string name, out int hr, out bool guards)
    {
        IntPtr input = name == null ? IntPtr.Zero : Marshal.StringToBSTR(name);
        IntPtr buffer = Marshal.AllocCoTaskMem(12);
        try {
            Marshal.WriteInt32(buffer, 0, 0x12345678);
            Marshal.WriteInt32(buffer, 4, unchecked((int)0x7ABCDEAD));
            Marshal.WriteInt32(buffer, 8, unchecked((int)0x87654321));
            hr = method(self, input, IntPtr.Add(buffer, 4));
            short answer = Marshal.ReadInt16(buffer, 4);
            guards = Marshal.ReadInt32(buffer, 0) == 0x12345678 &&
                Marshal.ReadInt16(buffer, 6) == 0x7ABC &&
                Marshal.ReadInt32(buffer, 8) == unchecked((int)0x87654321);
            return answer;
        } finally {
            Marshal.FreeCoTaskMem(buffer);
            if (input != IntPtr.Zero) Marshal.FreeBSTR(input);
        }
    }

    public static Dictionary<string, object> Run(Type licenseType, object silent)
    {
        if (IntPtr.Size != 8) throw new PlatformNotSupportedException("Windows x64 required");
        var state = new Dictionary<string, object>();
        var adapter = new MiniCamClsLicenseAdapter(licenseType, silent);
        IntPtr original = IntPtr.Zero, queried = IntPtr.Zero;
        try {
            original = Marshal.GetComInterfaceForObject(adapter, typeof(IMiniCamClsLicense));
            Guid iid = new Guid("214DE706-D211-4A06-84C3-6CB3044A4685");
            int qi = Marshal.QueryInterface(original, ref iid, out queried);
            state["query_interface_hresult"] = qi;
            state["interface_identity_matches"] = original != IntPtr.Zero && original == queried;
            if (qi != 0 || queried == IntPtr.Zero || original != queried)
                throw new InvalidOperationException("ILicense QueryInterface failed");
            IntPtr vtable = Marshal.ReadIntPtr(queried);
            // Seven IUnknown/IDispatch entries precede method 3 of ILicense.
            var call = (FeatureCall)Marshal.GetDelegateForFunctionPointer(
                Marshal.ReadIntPtr(vtable, 0x48), typeof(FeatureCall));
            int hr; bool guards;
            short answer = Call(call, queried, "Core:Solids", out hr, out guards);
            state["core_hresult"] = hr;
            state["core_variant_bool"] = answer;
            state["core_output_width_verified"] = guards;
            state["access_code"] = 390;
            state["checkout_attempted"] = adapter.RequestAttempted;
            state["checkout_granted"] = adapter.Granted;
            state["request_count"] = adapter.RequestCount;
            state["validation_before"] = adapter.BeforeValidation;
            state["validation_after"] = adapter.AfterValidation;
            state["upstream_error_type"] = adapter.ErrorType;
            short unsupported = Call(call, queried, "MiniCam:Unsupported", out hr, out guards);
            state["unsupported_hresult"] = hr;
            state["unsupported_variant_bool"] = unsupported;
            state["unsupported_output_width_verified"] = guards;
            short empty = Call(call, queried, null, out hr, out guards);
            state["null_name_hresult"] = hr;
            state["null_name_variant_bool"] = empty;
            state["null_name_output_width_verified"] = guards;
            state["unsupported_did_not_request_cls"] = adapter.RequestCount == 1;
            state["native_roundtrip_verified"] =
                (int)state["core_hresult"] == 0 && (bool)state["core_output_width_verified"] &&
                answer == (adapter.Granted ? (short)-1 : (short)0) &&
                (int)state["unsupported_hresult"] == unchecked((int)0x80004001) && unsupported == 0 &&
                (int)state["null_name_hresult"] == unchecked((int)0x80070057) && empty == 0 &&
                (bool)state["unsupported_output_width_verified"] && guards && adapter.RequestCount == 1;
            return state;
        } finally {
            // Release only the two interface references acquired above.
            if (queried != IntPtr.Zero) Marshal.Release(queried);
            if (original != IntPtr.Zero) Marshal.Release(original);
            GC.KeepAlive(adapter);
            state["owned_com_references_released"] = true;
        }
    }
}
