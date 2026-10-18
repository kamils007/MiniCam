using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;

// Terminal, disposable diagnostic. CLS and native callbacks run on one MFC
// worker thread. No Acam image is loaded. Unsupported host calls terminate
// only after recording the boundary and closing our CLS connection.
public sealed class MiniCamSolidImportBridge
{
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    private static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    private static extern IntPtr AddDllDirectory(string path);
    [DllImport("kernel32.dll", EntryPoint="GetProcAddress", SetLastError=true)]
    private static extern IntPtr GetOrdinal(IntPtr module, IntPtr ordinal);
    [DllImport("kernel32.dll", CharSet=CharSet.Ansi, ExactSpelling=true, SetLastError=true)]
    private static extern IntPtr GetProcAddress(IntPtr module, string name);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    private static extern bool GetModuleHandleExW(uint flags, IntPtr address, out IntPtr module);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    private static extern uint GetModuleFileNameW(IntPtr module, StringBuilder path, uint count);
    [DllImport("kernel32.dll")] private static extern uint GetCurrentThreadId();
    [DllImport("kernel32.dll")] private static extern uint SetErrorMode(uint mode);
    [DllImport("ole32.dll")] private static extern int CoInitializeEx(IntPtr reserved, uint mode);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate IntPtr GetThread();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate IntPtr BeginThread(IntPtr entry, IntPtr parameter, int priority, uint stack, uint flags, IntPtr security);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate uint WorkerEntry(IntPtr unused);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] private delegate int Initializer(IntPtr context);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] private delegate int GetVersion();

    private static MiniCamSolidImportBridge active;
    private readonly Dictionary<string, object> state = new Dictionary<string, object>();
    private readonly JavaScriptSerializer json = new JavaScriptSerializer();
    private readonly string root, output;
    private IntPtr module, applicationPointer, variant;
    private WorkerEntry entryPin;
    private GetThread getThread;
    private Type licenseType;
    private object silent;
    private MethodInfo valid, close, release;
    private bool openAttempted, sessionReady, finishing;
    private uint mfcThread;
    private MiniCamClsLicenseAdapter licenseAdapter;
    private MiniCamCoreInitApp application;
    private MiniCamCoreInitFrame frame;
    private int eventCount;
    public string Root { get { return root; } }
    public string ImporterPath { get { return Path.Combine(root, "Add-Ins", "SolidImport", "SolidImport.dll"); } }

    private MiniCamSolidImportBridge(string rootPath, string outputPath)
    {
        root = Path.GetFullPath(rootPath); output = Path.GetFullPath(outputPath);
        state["schema"] = "solidimport-cls-bridge-worker-10-v1";
        state["phase"] = "start";
        state["init_started"] = false; state["init_returned"] = false;
        state["init_succeeded"] = false;
        state["host_profile"] = "acamLevelCORE"; state["host_program_level"] = 14;
        state["program_level_is_selected_host_profile"] = true;
        state["special_key_supplied"] = false;
        state["step_conversion_attempted"] = false;
        state["full_host_verified"] = false; state["release_version_verified"] = false;
        state["normal_native_unload_verified"] = false;
        state["cls_close_completed"] = false;
    }

    private static string Hash(string path)
    {
        using (var sha = SHA256.Create()) using (var stream = File.OpenRead(path))
            return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }

    private void Save(string phase)
    {
        state["phase"] = phase;
        File.WriteAllText(Path.Combine(output, "worker_result.json"), json.Serialize(state), new UTF8Encoding(false));
        var checkpoint = new Dictionary<string, object> { { "phase", phase }, { "utc", DateTime.UtcNow.ToString("o") } };
        File.AppendAllText(Path.Combine(output, "worker_checkpoints.jsonl"), json.Serialize(checkpoint)+"\n", new UTF8Encoding(false));
    }

    public void Record(string method)
    {
        eventCount++;
        var record = new Dictionary<string, object> { {"method",method}, {"thread_id",GetCurrentThreadId()}, {"phase","native_init"} };
        File.AppendAllText(Path.Combine(output, "native_host_calls.jsonl"), json.Serialize(record)+"\n", new UTF8Encoding(false));
        if (eventCount > 256) Stop("event_limit");
    }

    public IMiniCamClsLicense License
    {
        get { Record("IAlphaCamApp.License"); state["license_getter_called"] = true; return licenseAdapter; }
    }
    public IMiniCamCoreInitFrame Frame
    {
        get { Record("IAlphaCamApp.Frame"); state["frame_getter_called"] = true; return frame; }
    }

    public int Stop(string method)
    {
        RecordBoundary(method);
        Finish(80, "host_boundary");
        return unchecked((int)0x80004001); // Environment.Exit does not return.
    }

    private void RecordBoundary(string method)
    {
        state["host_boundary"] = method;
        state["boundary_thread_id"] = GetCurrentThreadId();
        var record = new Dictionary<string, object> { {"method",method}, {"thread_id",GetCurrentThreadId()},
            {"phase","native_init"}, {"unsupported",true} };
        File.AppendAllText(Path.Combine(output, "native_host_calls.jsonl"), json.Serialize(record)+"\n", new UTF8Encoding(false));
        // A later host request demonstrates return from the original native
        // licensing guard. No separate direct adapter self-test is made here.
        state["post_license_host_request"] = licenseAdapter != null && licenseAdapter.RequestAttempted;
    }

    private MethodInfo Method(string name, Type result, params Type[] parameters)
    {
        var method = licenseType.GetMethod(name, BindingFlags.Public | BindingFlags.Static, null, parameters, null);
        if (method == null || method.ReturnType != result) throw new InvalidOperationException("Unexpected CLS contract: "+name);
        return method;
    }

    private void OpenCls()
    {
        Save("before_cls_load");
        licenseType = Assembly.LoadFrom(Path.Combine(root,"CLS","LicManaged.dll")).GetType("PlanitCls.License", true);
        if (!licenseType.IsPublic) throw new InvalidOperationException("CLS type is not public");
        Type productType=licenseType.GetNestedType("Product",BindingFlags.Public);
        Type showType=licenseType.GetNestedType("ShowError",BindingFlags.Public);
        Type openedType=licenseType.GetNestedType("OpenLicenseConnectionResult",BindingFlags.Public);
        Type validType=licenseType.GetNestedType("ValidLicenseConnectionResult",BindingFlags.Public);
        Type releaseType=licenseType.GetNestedType("ReleaseAccessCodeResult",BindingFlags.Public);
        if (productType==null || showType==null || openedType==null || validType==null || releaseType==null)
            throw new InvalidOperationException("Missing public CLS enum");
        object product=Enum.Parse(productType,"Alphacam"), quiet=Enum.Parse(showType,"Silent");
        if (Convert.ToInt32(product)!=2 || Convert.ToInt32(quiet)!=2 ||
            Convert.ToInt32(Enum.Parse(openedType,"Ok"))!=1 || Convert.ToInt32(Enum.Parse(validType,"Ok"))!=101 ||
            Convert.ToInt32(Enum.Parse(releaseType,"Ok"))!=201 || Convert.ToInt32(Enum.Parse(releaseType,"NotOptional"))!=202 ||
            Convert.ToInt32(Enum.Parse(releaseType,"CapacityReleasedOk"))!=207)
            throw new InvalidOperationException("Unexpected CLS enum values");
        silent=quiet;
        var setPath=Method("SetClsPath",typeof(bool),typeof(string));
        var session=Method("GetCurrentSessionId",typeof(int));
        var open=Method("OpenLicenseConnection",openedType,typeof(string),showType,productType,typeof(int),typeof(int));
        valid=Method("ValidLicenseConnection",validType,showType);
        close=Method("CloseLicenseConnection",typeof(void));
        release=Method("ReleaseAccessCode",releaseType,typeof(int));
        var productMethod=Method("GetClsProduct",productType);
        if (!(bool)setPath.Invoke(null,new object[]{Path.Combine(root,"CLS")})) throw new InvalidOperationException("SetClsPath failed");
        state["cls_contract_checked"]=true;
        state["cls_observed_session_id"]=(int)session.Invoke(null,new object[0]);
        state["cls_check_month"]=-1;state["cls_check_year"]=-1;
        Save("before_cls_open");openAttempted=true;
        int opened=Convert.ToInt32(open.Invoke(null,new object[]{"MiniCam original importer diagnostic",quiet,product,-1,-1}));
        state["cls_open_return_code"]=opened;
        if(opened!=1)throw new InvalidOperationException("CLS open did not return Ok");
        int validated=Convert.ToInt32(valid.Invoke(null,new object[]{quiet}));
        state["cls_validation_before_init"]=validated;
        state["cls_reported_product"]=Convert.ToInt32(productMethod.Invoke(null,new object[0]));
        if(validated!=101 || (int)state["cls_reported_product"]!=2)throw new InvalidOperationException("CLS validation failed");
        sessionReady=true;
        licenseAdapter=new MiniCamClsLicenseAdapter(licenseType,quiet);
        Save("cls_ready");
    }

    private void Finish(int code, string phase)
    {
        if (finishing) Environment.Exit(1);
        finishing=true;
        if(licenseAdapter!=null) {
            state["license_request_count"]=licenseAdapter.RequestCount;
            state["license_request_attempted"]=licenseAdapter.RequestAttempted;
            state["license_request_granted"]=licenseAdapter.Granted;
            state["license_validation_before"]=licenseAdapter.BeforeValidation;
            state["license_validation_after"]=licenseAdapter.AfterValidation;
            state["license_error_type"]=licenseAdapter.ErrorType;
        }
        try {
            if(sessionReady) {
                state["cls_validation_after_init"]=Convert.ToInt32(valid.Invoke(null,new object[]{silent}));
                if((int)state["cls_validation_after_init"]!=101)code=1;
            }
            if(licenseAdapter!=null && licenseAdapter.Granted) {
                object released=release.Invoke(null,new object[]{390});
                int value=Convert.ToInt32(released);
                state["individual_release_return_code"]=value;
                state["individual_release_return_name"]=Enum.GetName(released.GetType(),released);
                state["individual_release_confirmed"]=value==201 || value==207;
                if(value!=201 && value!=202 && value!=207)code=1;
            }
        } catch(Exception error) { state["cls_cleanup_error_type"]=(error.InnerException??error).GetType().FullName;code=1; }
        finally {
            if(openAttempted && close!=null) {
                state["cls_close_attempted"]=true;
                try { close.Invoke(null,new object[0]);state["cls_close_completed"]=true; }
                catch(Exception error) { state["cls_close_error_type"]=(error.InnerException??error).GetType().FullName;code=1; }
            }
        }
        state["terminal_exit_code"]=code;
        Save(code==1 ? "failed" : phase);
        // Callback objects, CCWs and native DLL stay pinned until process end.
        Environment.Exit(code);
    }

    private uint Worker(IntPtr unused)
    {
        try {
            mfcThread=GetCurrentThreadId();
            state["mfc_thread_id"]=mfcThread;
            state["mfc_thread_ready"]=getThread()!=IntPtr.Zero;
            if(!(bool)state["mfc_thread_ready"])throw new InvalidOperationException("MFC worker is missing");
            int apartment=CoInitializeEx(IntPtr.Zero,2);
            state["sta_hresult"]=apartment;
            if(apartment<0)throw new InvalidOperationException("Cannot initialize worker STA");
            OpenCls();
            frame=new MiniCamCoreInitFrame(this);
            application=new MiniCamCoreInitApp(this);
            applicationPointer=Marshal.GetComInterfaceForObject(application,typeof(IMiniCamCoreInitApp));
            variant=Marshal.AllocCoTaskMem(24);
            for(int i=0;i<24;i++)Marshal.WriteByte(variant,i,0);
            Marshal.WriteInt16(variant,0,9);Marshal.WriteIntPtr(variant,8,applicationPointer);
            var version=(GetVersion)Marshal.GetDelegateForFunctionPointer(GetProcAddress(module,"GetUIVersion"),typeof(GetVersion));
            state["ui_version"]=version();
            if((int)state["ui_version"]!=1)throw new InvalidOperationException("Unexpected UI contract");
            IntPtr init=GetProcAddress(module,"InitAlphacamAddIn");
            if(init.ToInt64()-module.ToInt64()!=0x78c0)throw new InvalidOperationException("Unexpected init export RVA");
            var call=(Initializer)Marshal.GetDelegateForFunctionPointer(init,typeof(Initializer));
            state["init_started"]=true;state["init_thread_id"]=GetCurrentThreadId();
            Save("before_original_init");
            int result=call(variant);
            state["init_returned"]=true;state["init_return_code"]=result;state["init_succeeded"]=result==0;
            Finish(result==0?0:1,result==0?"completed":"failed");
        } catch(Exception error) {
            state["error_type"]=(error.InnerException??error).GetType().FullName;
            state["error_message"]=(error.InnerException??error).Message;
            Finish(1,"failed");
        }
        return 1;
    }

    public static void Run(string root, string output)
    {
        active=new MiniCamSolidImportBridge(root,output);
        try { active.Start(); }
        catch(Exception error) {
            active.state["error_type"]=(error.InnerException??error).GetType().FullName;
            active.state["error_message"]=(error.InnerException??error).Message;
            active.Finish(1,"failed");
        }
    }

    private void Start()
    {
        Save("preflight");
        if(IntPtr.Size!=8 || Environment.Version.Major!=4)throw new PlatformNotSupportedException("Windows x64 CLR v4 required");
        var expected=new Dictionary<string,string>{
            {"Acam.exe","02997d0f6c15d0e6c807d1be19621e128bdbf3a55ae1846e0196872766bf205d"},
            {Path.Combine("CLS","LicManaged.dll"),"0b30282755e72feefdc3b43e50c0628749e49a5bd97fa06dabc002a0851a4827"},
            {Path.Combine("CLS","CLS.dll"),"5d1e001654690774794b2d6093491e6b0b51aa670b6694f6c98883c29b8f02c8"},
            {Path.Combine("CLS","ClsInit.exe"),"d594c321383c33ca4eb12eedf3f95cb2adfc269484cdea986fbe5973737ef252"},
            {Path.Combine("Add-Ins","SolidImport","SolidImport.dll"),"b143da4e3f8771e04aebb7d20334244900866148dc83d242a3e0ad6938c5f82c"}};
        foreach(var pair in expected)if(Hash(Path.Combine(root,pair.Key))!=pair.Value)throw new InvalidOperationException("Unexpected installed build: "+pair.Key);
        state["source_fingerprints_checked"]=true;
        SetErrorMode(0x8003);
        if(AddDllDirectory(root)==IntPtr.Zero || AddDllDirectory(Path.GetDirectoryName(ImporterPath))==IntPtr.Zero)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        Save("before_importer_load");
        module=LoadLibraryExW(ImporterPath,IntPtr.Zero,0x1100);
        if(module==IntPtr.Zero)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        state["importer_loaded"]=true;
        IntPtr getter=Marshal.ReadIntPtr(module,0x18550),mfc;
        if(!GetModuleHandleExW(6,getter,out mfc))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        var mfcPath=new StringBuilder(32768);
        uint length=GetModuleFileNameW(mfc,mfcPath,(uint)mfcPath.Capacity);
        if(length==0 || length>=mfcPath.Capacity || !String.Equals(Path.GetFileName(mfcPath.ToString()),"mfc140u.dll",StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Unexpected MFC runtime");
        if(GetOrdinal(mfc,new IntPtr(2222))!=getter)throw new InvalidOperationException("MFC getter differs from public ordinal");
        state["mfc_entry_import_matches_getter"]=true;
        state["mfc_sha256"]=Hash(mfcPath.ToString());
        getThread=(GetThread)Marshal.GetDelegateForFunctionPointer(getter,typeof(GetThread));
        IntPtr begin=GetOrdinal(mfc,new IntPtr(2149));
        if(begin==IntPtr.Zero)throw new InvalidOperationException("Missing AfxBeginThread");
        var start=(BeginThread)Marshal.GetDelegateForFunctionPointer(begin,typeof(BeginThread));
        entryPin=new WorkerEntry(Worker);
        Save("before_mfc_thread");
        if(start(Marshal.GetFunctionPointerForDelegate(entryPin),IntPtr.Zero,0,0,0,IntPtr.Zero)==IntPtr.Zero)
            throw new InvalidOperationException("AfxBeginThread failed");
        // Parent watchdog owns timeout and terminates only this worker.
        Thread.Sleep(Timeout.Infinite);
    }
}
