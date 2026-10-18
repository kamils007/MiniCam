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
// SpecialKey implements only the exact mask consumed by this Init export.
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
    private MethodInfo valid, close, release, getAccess;
    private readonly List<Dictionary<string, object>> classificationQueries = new List<Dictionary<string, object>>();
    private readonly List<int> classificationGrants = new List<int>();
    private bool specialKeyRequested;
    private bool openAttempted, sessionReady, finishing;
    private uint mfcThread;
    private MiniCamClsLicenseAdapter licenseAdapter;
    private MiniCamCoreInitApp application;
    private MiniCamCoreInitFrame frame;
    private int eventCount;
    private MiniCamTextCatalogMap textMap;
    private readonly List<Dictionary<string,object>> textReads=new List<Dictionary<string,object>>();
    public string Root { get { return root; } }
    public string ImporterPath { get { return Path.Combine(root, "Add-Ins", "SolidImport", "SolidImport.dll"); } }

    private MiniCamSolidImportBridge(string rootPath, string outputPath)
    {
        root = Path.GetFullPath(rootPath); output = Path.GetFullPath(outputPath);
        json.MaxJsonLength=16*1024*1024;
        state["schema"] = "solidimport-text-catalog-worker-12-v1";
        state["phase"] = "start";
        state["init_started"] = false; state["init_returned"] = false;
        state["init_succeeded"] = false;
        state["host_profile"] = "acamLevelCORE"; state["host_program_level"] = 14;
        state["program_level_is_selected_host_profile"] = true;
        state["special_key_supplied"] = false;
        state["special_key_full_value_verified"] = false;
        state["special_key_supported_mask"] = 0x12;
        state["special_key_consumer_mask"] = 0x12;
        state["special_key_scope"] = "InitAlphacamAddIn projection only; other bits unknown";
        state["special_key_cls_queries"] = classificationQueries;
        state["step_conversion_attempted"] = false;
        state["full_host_verified"] = false; state["release_version_verified"] = false;
        state["normal_native_unload_verified"] = false;
        state["text_catalog_reads"]=textReads;
        state["text_catalog_served_count"]=0;
        state["text_catalog_sources_unchanged"]=false;
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
        state["post_text_catalog_host_request"]=textReads.Count>0;
        // A later host request demonstrates return from the original native
        // licensing guard. No separate direct adapter self-test is made here.
        state["post_license_host_request"] = licenseAdapter != null && licenseAdapter.RequestAttempted;
        state["post_special_key_host_request"] = state.ContainsKey("special_key_projection_ready") &&
            (bool)state["special_key_projection_ready"];
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
        getAccess=Method("GetAccessCode",typeof(bool),typeof(int));
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

    private bool QueryClassification(int accessCode)
    {
        if(accessCode!=348 && accessCode!=347 && accessCode!=349)
            throw new InvalidOperationException("Unsupported classification query");
        var query=new Dictionary<string,object>{{"access_code",accessCode},{"thread_id",GetCurrentThreadId()}};
        classificationQueries.Add(query);
        Save("before_special_key_cls_query");
        int before=Convert.ToInt32(valid.Invoke(null,new object[]{silent}));
        query["validation_before"]=before;
        if(before!=101)throw new InvalidOperationException("CLS invalid before classification query");
        bool granted=(bool)getAccess.Invoke(null,new object[]{accessCode});
        query["granted"]=granted;
        if(granted)classificationGrants.Add(accessCode);
        int after=Convert.ToInt32(valid.Invoke(null,new object[]{silent}));
        query["validation_after"]=after;
        Save("after_special_key_cls_query");
        if(after!=101)throw new InvalidOperationException("CLS invalid after classification query");
        return granted;
    }

    public int SpecialKeyProjection
    {
        get {
            Record("IAlphaCamApp.SpecialKey");
            if(specialKeyRequested || GetCurrentThreadId()!=mfcThread || !sessionReady ||
                licenseAdapter==null || !licenseAdapter.Granted)
                throw new InvalidOperationException("SpecialKey projection requested outside guarded Init");
            specialKeyRequested=true;
            // Exact original getter order for the two consumed bits: 348,
            // then 347 OR (short-circuit) 349. No guessed feature-code search.
            bool flag16=QueryClassification(348);
            bool flag2=QueryClassification(347);
            if(!flag2)flag2=QueryClassification(349);
            int value=(flag16?16:0)|(flag2?2:0);
            state["special_key_projection_value"]=value;
            state["special_key_projection_ready"]=true;
            state["special_key_supplied"]=true;
            Save("special_key_projection_supplied");
            return value;
        }
    }

    private void LoadTextCatalogs()
    {
        string mapPath=Path.Combine(output,"temp","catalog_map.json");
        textMap=json.Deserialize<MiniCamTextCatalogMap>(File.ReadAllText(mapPath,Encoding.UTF8));
        if(textMap==null || textMap.schema!="solidimport-text-map-12-v1" || textMap.catalogs==null ||
            !textMap.catalogs.ContainsKey("solidimport"))throw new InvalidOperationException("Missing text catalog map");
        var sources=new List<Dictionary<string,object>>();
        foreach(var pair in textMap.catalogs) {
            var c=pair.Value;
            if((pair.Key!="solidimport" && pair.Key!="aedit") || c==null || c.entries==null ||
                c.context!=pair.Key+"|" || !String.Equals(Path.GetFileName(c.path),pair.Key+".po",StringComparison.OrdinalIgnoreCase) ||
                c.entries.Count==0 || c.sha256==null || c.sha256.Length!=64 || Hash(c.path)!=c.sha256)
                throw new InvalidOperationException("Unexpected text catalog source");
            foreach(var value in c.entries.Values)
                if(String.IsNullOrEmpty(value) || value.Length>32768 || value.IndexOf('\0')>=0)
                    throw new InvalidOperationException("Invalid text catalog value");
            sources.Add(new Dictionary<string,object>{{"catalog",pair.Key},{"path",c.path},{"sha256",c.sha256},
                {"language",c.language},{"entry_count",c.entries.Count}});
        }
        state["text_catalog_sources"]=sources;
        state["text_catalog_map_checked"]=true;
        state["text_catalog_sources_unchanged"]=true;
        Save("text_catalog_ready");
    }

    public string ReadCatalogText(string name, int dollar, short index)
    {
        Record("IFrame.ReadTextFile");
        state["text_file_request"]=new Dictionary<string,object>{{"file_name",name},
            {"dollar_number",dollar},{"index",index}};
        string stem=null;
        if(String.Equals(name,"SolidImport.po",StringComparison.OrdinalIgnoreCase))stem="solidimport";
        // Explicit diagnostic resource alias; only an actual numeric PO entry
        // may satisfy this request. No CTX decoding or made-up fallback text.
        if(String.Equals(name,"aedit.ctx",StringComparison.OrdinalIgnoreCase) ||
            String.Equals(name,"aedit.po",StringComparison.OrdinalIgnoreCase))stem="aedit";
        if(stem==null || textMap==null || !textMap.catalogs.ContainsKey(stem)) {
            Stop("IFrame.ReadTextFile.UnsupportedCatalog");return null;
        }
        var catalog=textMap.catalogs[stem];string value;
        string key=dollar.ToString(System.Globalization.CultureInfo.InvariantCulture)+"-"+
            index.ToString(System.Globalization.CultureInfo.InvariantCulture);
        if(dollar<0 || index<1 || !catalog.entries.TryGetValue(key,out value)) {
            Stop("IFrame.ReadTextFile.MissingEntry");return null;
        }
        if(GetCurrentThreadId()!=mfcThread || Hash(catalog.path)!=catalog.sha256) {
            state["text_catalog_sources_unchanged"]=false;
            throw new InvalidOperationException("Text source changed or wrong caller thread");
        }
        string valueHash;
        using(var sha=SHA256.Create())
            valueHash=BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(value))).Replace("-","").ToLowerInvariant();
        textReads.Add(new Dictionary<string,object>{{"catalog",stem},{"key",key},{"requested_file",name},
            {"source_sha256",catalog.sha256},{"value_utf8_sha256",valueHash},{"thread_id",GetCurrentThreadId()},
            {"character_count",value.Length},{"legacy_alias_used",name.EndsWith(".ctx",StringComparison.OrdinalIgnoreCase)}});
        state["text_catalog_served_count"]=textReads.Count;
        Save("text_catalog_value_supplied");
        return value;
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
            var releases=new List<Dictionary<string,object>>();
            state["special_key_release_results"]=releases;
            foreach(int accessCode in classificationGrants) {
                var item=new Dictionary<string,object>{{"access_code",accessCode}};
                releases.Add(item);
                try {
                    object released=release.Invoke(null,new object[]{accessCode});
                    int value=Convert.ToInt32(released);
                    item["return_code"]=value;item["return_name"]=Enum.GetName(released.GetType(),released);
                    item["individual_release_confirmed"]=value==201 || value==207;
                    if(value!=201 && value!=202 && value!=207)code=1;
                } catch(Exception error) { item["error_type"]=(error.InnerException??error).GetType().FullName;code=1; }
            }
            if(openAttempted && close!=null) {
                state["cls_close_attempted"]=true;
                try { close.Invoke(null,new object[0]);state["cls_close_completed"]=true; }
                catch(Exception error) { state["cls_close_error_type"]=(error.InnerException??error).GetType().FullName;code=1; }
            }
        }
        if(textMap!=null) {
            try {
                foreach(var catalog in textMap.catalogs.Values)
                    if(Hash(catalog.path)!=catalog.sha256)throw new InvalidOperationException("Text source changed");
            } catch(Exception error) { state["text_catalog_sources_unchanged"]=false;
                state["text_catalog_error_type"]=error.GetType().FullName;code=1; }
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
        LoadTextCatalogs();
        SetErrorMode(0x8003);
        if(AddDllDirectory(root)==IntPtr.Zero || AddDllDirectory(Path.GetDirectoryName(ImporterPath))==IntPtr.Zero)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        Save("before_importer_load");
        module=LoadLibraryExW(ImporterPath,IntPtr.Zero,0x1100);
        if(module==IntPtr.Zero)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        state["importer_loaded"]=true;
        // Hash-gated original Init reads SpecialKey exactly once and only
        // consumes mask 0x12 at RVA 0x7b83. Guard those instruction bytes too.
        byte[] expectedMask={0xf6,0x45,0x30,0x12};
        for(int i=0;i<expectedMask.Length;i++)
            if(Marshal.ReadByte(module,0x7b83+i)!=expectedMask[i])
                throw new InvalidOperationException("Unexpected SpecialKey consumer");
        state["special_key_consumer_checked"]=true;
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

// Private map generated from the installed PO files by the bounded Python reader.
public sealed class MiniCamTextCatalogMap
{
    public string schema;
    public Dictionary<string,MiniCamTextCatalogFile> catalogs;
}
public sealed class MiniCamTextCatalogFile
{
    public string path,sha256,language,context;
    public Dictionary<string,string> entries;
}
