// Isolated x64 host for the verified ALPHACAM 2025 Feature.dll.
// ABI and CString layout ported from Feature_2025_TEST_4_9_KOMPLET.
#define NOMINMAX
#include <windows.h>
#include <oleauto.h>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QTextStream>
#include <array>
#include <vector>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace {
constexpr auto dllHash = "4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec";
constexpr auto poHash = "5ebfd4fedec46583fa72fd2e95862a0b3fbe7eb41b6a9e49ae12895176130776";
[[noreturn]] void fail(const QString& message) { throw std::runtime_error(message.toUtf8().constData()); }
[[noreturn]] HRESULT STDMETHODCALLTYPE unsupported(void*)
{
    QTextStream(stderr) << "Feature.dll requested an unsupported host method\n";
    ExitProcess(80);
}
void check(HRESULT hr, const char* operation)
{
    if (FAILED(hr)) fail(QString("%1 failed: 0x%2").arg(operation).arg(quint32(hr),8,16,QChar('0')));
}
QByteArray read(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) fail("Cannot read " + path);
    return f.readAll();
}
void verify(const QString& path, const char* hash)
{
    if (QCryptographicHash::hash(read(path), QCryptographicHash::Sha256).toHex() != hash)
        fail("Unsupported library/catalog build: " + path);
}
template<class T> T proc(HMODULE module, const char* name)
{
    auto p = GetProcAddress(module,name);
    if (!p) fail(QString("Missing export: %1").arg(name));
    return reinterpret_cast<T>(p);
}
template<class T> T imported(HMODULE module, size_t rva)
{
    auto p = *reinterpret_cast<void**>(reinterpret_cast<char*>(module) + rva);
    if (!p) fail("Unbound DLL import");
    return reinterpret_cast<T>(p);
}
struct Host;
struct Object {
    void** table;
    Host* host;
    int kind;
    GUID iid{};
    ULONG refs = 1;
    bool removed = false;
    QString name;
    ULONG color = 0;
    std::array<void*, 1024> entries;
    Object(Host* h, int k, const wchar_t* guid): host(h), kind(k) {
        table = entries.data();
        entries.fill(reinterpret_cast<void*>(&unsupported));
        check(CLSIDFromString(guid, &iid), "CLSIDFromString");
    }
    template<class T> void set(size_t offset, T fn) { entries.at(offset / 8) = reinterpret_cast<void*>(fn); }
};
struct Host {
    Object app{this,0,L"{88a2ff74-4309-11d2-9861-00104b4af281}"};
    Object drawing{this,1,L"{1a172592-4565-11d2-9866-00104b4af281}"};
    Object frame{this,2,L"{0691d8c8-4ae8-11d2-9876-00104b4af281}"};
    std::vector<std::unique_ptr<Object>> owned;
    std::vector<Object*> layers;
    Object* active = nullptr;
    VARIANT_BOOL screen = VARIANT_TRUE, project = VARIANT_TRUE, threeD = VARIANT_FALSE;
    LONG specialKey;
    QString undoName;
    explicit Host(LONG key);
};
HRESULT STDMETHODCALLTYPE qi(Object* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid != IID_IUnknown && iid != IID_IDispatch && iid != self->iid) return E_NOINTERFACE;
    *out=self; ++self->refs; return S_OK;
}
ULONG STDMETHODCALLTYPE addref(Object* self) { return ++self->refs; }
ULONG STDMETHODCALLTYPE release(Object* self) {
    if (self->refs <= 1) {
        QTextStream(stderr) << "Feature.dll released the host owner's reference\n";
        ExitProcess(81);
    }
    return --self->refs;
}
HRESULT STDMETHODCALLTYPE typeCount(Object*, UINT* n) { if(!n) return E_POINTER; *n=0; return S_OK; }
HRESULT STDMETHODCALLTYPE typeInfo(Object*,UINT,LCID,ITypeInfo** out) { if(!out)return E_POINTER;*out=nullptr;return DISP_E_BADINDEX; }
HRESULT STDMETHODCALLTYPE names(Object*,REFIID,LPOLESTR*,UINT,LCID,DISPID*) {return DISP_E_UNKNOWNNAME;}
HRESULT STDMETHODCALLTYPE invoke(Object*,DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*) {return DISP_E_MEMBERNOTFOUND;}
void standard(Object& o) {
    o.set(0,qi);o.set(8,addref);o.set(16,release);o.set(24,typeCount);o.set(32,typeInfo);o.set(40,names);o.set(48,invoke);
}
template<class T> HRESULT output(T* p,T value) {if(!p)return E_POINTER;*p=value;return S_OK;}
HRESULT objectOutput(Object** out,Object* value) { if(!out)return E_POINTER;*out=value;if(value)addref(value);return S_OK; }
HRESULT STDMETHODCALLTYPE drawing(Object* o,Object** p){return objectOutput(p,&o->host->drawing);}
HRESULT STDMETHODCALLTYPE frame(Object* o,Object** p){return objectOutput(p,&o->host->frame);}
HRESULT STDMETHODCALLTYPE letter(Object*,SHORT* p){return output(p,SHORT('R'));}
HRESULT STDMETHODCALLTYPE api(Object*,VARIANT_BOOL* p){return output(p,VARIANT_BOOL(VARIANT_TRUE));}
HRESULT STDMETHODCALLTYPE special(Object* o,LONG* p){return output(p,o->host->specialKey);}
// The isolated batch helper owns no undo history. Do not advertise enabling it.
HRESULT STDMETHODCALLTYPE undoDisabledGet(Object*,VARIANT_BOOL* p){return output(p,VARIANT_BOOL(VARIANT_TRUE));}
HRESULT STDMETHODCALLTYPE undoDisabledPut(Object* o,VARIANT_BOOL v){if(!v)return unsupported(o);return S_OK;}
HRESULT STDMETHODCALLTYPE undo(Object* o,BSTR s){o->host->undoName=QString::fromWCharArray(s,SysStringLen(s));return S_OK;}
HRESULT STDMETHODCALLTYPE screenGet(Object* o,VARIANT_BOOL* p){return output(p,o->host->screen);}
HRESULT STDMETHODCALLTYPE screenPut(Object* o,VARIANT_BOOL v){o->host->screen=v;return S_OK;}
HRESULT STDMETHODCALLTYPE projectGet(Object* o,VARIANT_BOOL* p){return output(p,o->host->project);}
HRESULT STDMETHODCALLTYPE projectPut(Object* o,VARIANT_BOOL v){o->host->project=v;return S_OK;}
HRESULT STDMETHODCALLTYPE threeGet(Object* o,VARIANT_BOOL* p){return output(p,o->host->threeD);}
HRESULT STDMETHODCALLTYPE threePut(Object* o,VARIANT_BOOL v){o->host->threeD=v;return S_OK;}
HRESULT STDMETHODCALLTYPE refresh(Object*){return S_OK;} // No display views owned by this host.
HRESULT STDMETHODCALLTYPE first(Object* o,Object** p){auto& a=o->host->layers;return objectOutput(p,a.empty()?nullptr:a.front());}
HRESULT STDMETHODCALLTYPE count(Object* o,LONG* p){return output(p,LONG(o->host->layers.size()));}
HRESULT STDMETHODCALLTYPE active(Object* o,Object** p){return objectOutput(p,o->host->active);}
HRESULT STDMETHODCALLTYPE setActive(Object* o,Object* p){auto& a=o->host->layers;if(std::find(a.begin(),a.end(),p)==a.end())return E_INVALIDARG;o->host->active=p;return S_OK;}
HRESULT STDMETHODCALLTYPE layerName(Object* o,BSTR* p){if(!p)return E_POINTER;if(o->removed)return E_FAIL;*p=SysAllocStringLen(reinterpret_cast<const OLECHAR*>(o->name.utf16()),o->name.size());return *p?S_OK:E_OUTOFMEMORY;}
HRESULT STDMETHODCALLTYPE setName(Object* o,BSTR s){if(o->removed)return E_FAIL;QString n=QString::fromWCharArray(s,SysStringLen(s));if(n.isEmpty()||n.contains(QChar(0)))return E_INVALIDARG;for(auto* l:o->host->layers)if(l!=o&&l->name==n)return E_INVALIDARG;o->name=n;return S_OK;}
HRESULT STDMETHODCALLTYPE colorGet(Object* o,ULONG* p){if(o->removed)return E_FAIL;return output(p,o->color);}
HRESULT STDMETHODCALLTYPE colorPut(Object* o,ULONG v){if(o->removed)return E_FAIL;o->color=v;return S_OK;}
HRESULT STDMETHODCALLTYPE next(Object* o,Object** p){auto& a=o->host->layers;auto i=std::find(a.begin(),a.end(),o);if(i==a.end())return E_FAIL;return objectOutput(p,++i==a.end()?nullptr:*i);}
HRESULT STDMETHODCALLTYPE remove(Object* o){auto& a=o->host->layers;auto i=std::find(a.begin(),a.end(),o);if(i==a.end())return E_FAIL;a.erase(i);o->removed=true;if(o->host->active==o)o->host->active=a.empty()?nullptr:a.front();return S_OK;}
HRESULT STDMETHODCALLTYPE create(Object* o,BSTR s,Object** p){
    if(!p)return E_POINTER;*p=nullptr;
    const QString n=QString::fromWCharArray(s,SysStringLen(s));
    if(n.isEmpty()||n.contains(QChar(0)))return E_INVALIDARG;
    for(auto* l:o->host->layers)if(l->name==n)return objectOutput(p,l);
    auto l=std::make_unique<Object>(o->host,3,L"{e1881172-47ba-11d2-986f-00104b4af281}");
    standard(*l);l->name=n;l->set(56,layerName);l->set(64,setName);l->set(144,next);l->set(160,remove);l->set(256,colorGet);l->set(264,colorPut);
    auto* result=l.get();o->host->owned.push_back(std::move(l));o->host->layers.push_back(result);if(!o->host->active)o->host->active=result;
    return objectOutput(p,result);
}
Host::Host(LONG key):specialKey(key){
    standard(app);standard(drawing);standard(frame);
    app.set(72,::drawing);app.set(112,::frame);app.set(272,letter);app.set(456,special);app.set(536,undo);app.set(864,api);
    app.set(528,undoDisabledPut);app.set(696,undoDisabledGet);
    drawing.set(80,threeGet);drawing.set(88,threePut);drawing.set(160,refresh);drawing.set(352,first);drawing.set(360,count);drawing.set(368,active);drawing.set(376,setActive);drawing.set(384,create);drawing.set(392,refresh);drawing.set(1032,screenGet);drawing.set(1040,screenPut);drawing.set(1840,refresh);
    frame.set(272,projectPut);frame.set(344,projectGet);
}
// Keep host objects alive until the DLL has released its fallback context on unload.
struct Library {
    HMODULE module = nullptr;
    std::vector<DLL_DIRECTORY_COOKIE> directories;
    ~Library(){if(module)FreeLibrary(module);for(auto d:directories)RemoveDllDirectory(d);}
};
struct Dispatch {
    IDispatch* value=nullptr;
    ~Dispatch(){if(value)value->Release();}
};
struct Variant {
    VARIANT v{};
    ~Variant(){VariantClear(&v);}
};
VARIANT call(IDispatch* object,const wchar_t* name,WORD kind,std::vector<VARIANT> args={}) {
    DISPID id;auto* n=const_cast<wchar_t*>(name);
    check(object->GetIDsOfNames(IID_NULL,&n,1,LOCALE_USER_DEFAULT,&id),"GetIDsOfNames");
    std::reverse(args.begin(),args.end());
    DISPPARAMS params{args.data(),nullptr,UINT(args.size()),0};
    VARIANT result{};EXCEPINFO error{};UINT arg=0;
    HRESULT hr=object->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,kind,&params,&result,&error,&arg);
    if(error.pfnDeferredFillIn)error.pfnDeferredFillIn(&error);
    const QString description=error.bstrDescription?QString::fromWCharArray(error.bstrDescription):QString();
    SysFreeString(error.bstrSource);SysFreeString(error.bstrDescription);SysFreeString(error.bstrHelpFile);
    if(FAILED(hr)){
        QString details=description;IErrorInfo* info=nullptr;
        if(GetErrorInfo(0,&info)==S_OK&&info){BSTR b=nullptr;info->GetDescription(&b);if(b){details+=" "+QString::fromWCharArray(b);SysFreeString(b);}info->Release();}
        VariantClear(&result);fail(QString::fromWCharArray(name)+": "+details+QString(" (0x%1)").arg(quint32(hr),8,16,QChar('0')));
    }
    return result;
}
LONG number(const VARIANT& v){if(v.vt==VT_I4)return v.lVal;if(v.vt==VT_I2)return v.iVal;fail("Unexpected numeric result type");}
VARIANT integer(LONG value){VARIANT v{};v.vt=VT_I4;v.lVal=value;return v;}

struct TextTable {
    HMODULE module;
    std::array<void*,26> groups{};
    std::vector<std::vector<quint64>> storage;
    std::vector<void*> objects;
    void** cell;void* previous;
    using Construct=void(*)(void*);
    using Assign=void*(*)(void*,const wchar_t*);
    using Get=const wchar_t*(*)(void*);
    Construct destroy;
    bool installed=false;
    TextTable(HMODULE m):module(m),cell(reinterpret_cast<void**>(reinterpret_cast<char*>(m)+0x186548)),previous(*cell),destroy(imported<Construct>(m,0x13D070)){}
    ~TextTable(){if(installed)*cell=previous;for(auto i=objects.rbegin();i!=objects.rend();++i)destroy(*i);}
    void install(const QString& path){
        verify(path,poHash);
        // Parse singular gettext entries using JSON string escaping plus gettext's octal-free subset.
        QMap<QString,QString> entries;QString context,id,text,field;
        auto finish=[&]{if(!id.isEmpty())entries[context+"|"+id]=text;context.clear();id.clear();text.clear();field.clear();};
        const QString contents=QString::fromUtf8(read(path));
        for(QString line:contents.split('\n')){
            line=line.trimmed();if(line.isEmpty()){finish();continue;}if(line.startsWith('#'))continue;
            if(line.startsWith("msgctxt ")){field="context";line=line.mid(8).trimmed();}
            else if(line.startsWith("msgid ")){field="id";line=line.mid(6).trimmed();}
            else if(line.startsWith("msgstr ")){field="text";line=line.mid(7).trimmed();}
            if(!line.startsWith('"'))fail("Unsupported Feature.po syntax");
            QJsonParseError e;const auto doc=QJsonDocument::fromJson(("["+line+"]").toUtf8(),&e);
            if(e.error!=QJsonParseError::NoError)fail("Invalid Feature.po quoted string");
            const QString s=doc.array().at(0).toString();
            if(field=="context")context+=s;else if(field=="id")id+=s;else if(field=="text")text+=s;else fail("Invalid Feature.po field");
        }finish();
        const int counts[]={25,6,8,31,36,34,162,19,83,10,7,29,5,7,17,19,9,6,7,2,11,10,25,2,6,7};
        const auto construct=imported<Construct>(module,0x13D068);const auto assign=imported<Assign>(module,0x13D8B0);const auto get=imported<Get>(module,0x13D060);
        storage.resize(26);
        for(int g=0;g<26;++g){storage[g].resize(counts[g]+1);storage[g][0]=counts[g];groups[g]=storage[g].data()+1;
            for(int i=0;i<counts[g];++i){void* p=storage[g].data()+i+1;const QString s=entries.value(QString("feature||%1-%2").arg((g+1)*10).arg(i+1));construct(p);objects.push_back(p);assign(p,reinterpret_cast<const wchar_t*>(s.utf16()));if(QString::fromWCharArray(get(p))!=s)fail("CString verification failed");}}
        if(*cell!=previous)fail("Feature text table changed during initialization");*cell=groups.data();installed=true;
    }
};
void run(const QStringList& args){
    if(args.size()!=6)fail("Usage: minicam-feature-host Feature.dll host_environment.json input.x_b output.bin Feature.po");
    static_assert(sizeof(void*)==8 && sizeof(VARIANT)==24,"x64 is required");
    const QString dll=QFileInfo(args[1]).absoluteFilePath();verify(dll,dllHash);
    const auto env=QJsonDocument::fromJson(read(args[2])).object();
    if(env.value("schema").toString()!="feature-host-environment-2025-v1"||env.value("feature_dll_sha256").toString()!=dllHash||env.value("source").toString()!="running_ALPHACAM_COM")fail("Invalid host environment snapshot");
    const auto app=env.value("application").toObject();
    if(app.value("ProgramLetter").toInt()!=82||!app.value("SpecialKey").isDouble())fail("Router host configuration is required");
    const LONG key=app.value("SpecialKey").toInt();
    check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED),"CoInitializeEx");
    struct ComCleanup{~ComCleanup(){CoUninitialize();}} cleanup;
    Host host(key);Library library;
    const QDir dir=QFileInfo(dll).absoluteDir();
    for(const QString& d:{dir.absolutePath(),dir.absoluteFilePath("../..")}){
        auto cookie=AddDllDirectory(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(d).utf16()));if(cookie)library.directories.push_back(cookie);
    }
    library.module=LoadLibraryExW(reinterpret_cast<const wchar_t*>(dll.utf16()),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS|LOAD_LIBRARY_SEARCH_USER_DIRS);
    if(!library.module)fail(QString("Cannot load Feature.dll; Windows error %1").arg(GetLastError()));
    if(proc<int(*)()>(library.module,"GetUIVersion")()!=2)fail("Unsupported Feature UI version");
    TextTable texts(library.module);Dispatch native,solid;
    proc<void(*)(IDispatch**)>(library.module,"GetSolidFeatures")(&native.value);
    if(!native.value)fail("GetSolidFeatures returned no object");
    check(native.value->QueryInterface(IID_IDispatch,reinterpret_cast<void**>(&solid.value)),"QueryInterface");
    Variant context;context.v.vt=VT_DISPATCH;context.v.pdispVal=reinterpret_cast<IDispatch*>(&host.app);addref(&host.app);
    proc<LONG(*)(VARIANT*,LONG)>(library.module,"ObjectNumberOf")(&context.v,0);
    texts.install(args[5]);
    const auto suffix=QFileInfo(args[3]).suffix().toLower();
    if(suffix!="x_b"&&suffix!="x_t"&&suffix!="step"&&suffix!="stp")fail("Only STEP or Parasolid input is supported");
    QTemporaryDir inputCopy;
    if(!inputCopy.isValid())fail("Cannot create input directory");
    const QString input=inputCopy.filePath("model."+suffix);
    QFile sourceFile(QFileInfo(args[3]).absoluteFilePath());
    if(!sourceFile.copy(input))fail("Cannot copy the input model: "+sourceFile.errorString());
    Variant path;path.v.vt=VT_BSTR;path.v.bstrVal=SysAllocString(reinterpret_cast<const wchar_t*>(input.utf16()));
    const LONG fileType=(suffix=="x_b"||suffix=="x_t")?6:11;
    Variant result;result.v=call(solid.value,L"ImportAs",DISPATCH_METHOD,{integer(fileType),path.v,integer(0)});
    if(number(result.v)<=0)fail("Feature.dll imported no solid bodies");
    Variant bodies;bodies.v=call(solid.value,L"Bodies",DISPATCH_PROPERTYGET);
    if(bodies.v.vt!=VT_DISPATCH||!bodies.v.pdispVal)fail("No Bodies collection");
    Variant bodyCount;bodyCount.v=call(bodies.v.pdispVal,L"Count",DISPATCH_PROPERTYGET);
    if(number(bodyCount.v)<=0)fail("Empty solid body collection");
    // Preserve the Python probe sequence: Bodies.Save precedes object serialization.
    QTemporaryDir roundtrip;
    if(!roundtrip.isValid())fail("Cannot create Parasolid roundtrip directory");
    Variant savePath;savePath.v.vt=VT_BSTR;
    savePath.v.bstrVal=SysAllocString(reinterpret_cast<const wchar_t*>(roundtrip.filePath("roundtrip.x_b").utf16()));
    Variant saved;saved.v=call(bodies.v.pdispVal,L"Save",DISPATCH_METHOD,{savePath.v});
    if(number(saved.v)<=0||!read(roundtrip.filePath("roundtrip.x_b")).left(4096).contains("FORMAT=binary"))fail("Bodies.Save failed");
    std::array<unsigned char,80> name{};
    std::fill_n(name.begin(),8,0xA5);std::fill_n(name.begin()+72,8,0x5A);
    const LONG query=proc<LONG(*)(VARIANT*,char*,LONG)>(library.module,"ObjectQueryWrite2")(&context.v,reinterpret_cast<char*>(name.data()+8),0);
    if(!std::all_of(name.begin(),name.begin()+8,[](auto b){return b==0xA5;})
       ||!std::all_of(name.begin()+72,name.end(),[](auto b){return b==0x5A;})
       ||!std::memchr(name.data()+8,0,64))fail("ObjectQueryWrite2 damaged its output buffer");
    if(query!=1||std::strcmp(reinterpret_cast<char*>(name.data()+8),"LicomUKRJMParaSolid"))fail("ObjectQueryWrite2 rejected the solid");
    using Open=int(*)(void**,const wchar_t*,const wchar_t*);using Flush=int(*)(void*);using Close=int(*)(void*);
    auto open=imported<Open>(library.module,0x13CE28);auto flush=imported<Flush>(library.module,0x13CE20);auto close=imported<Close>(library.module,0x13CE68);auto fwrite=imported<void*>(library.module,0x13CE38);
    void* stream=nullptr;const auto output=QFileInfo(args[4]).absoluteFilePath();
    if(open(&stream,reinterpret_cast<const wchar_t*>(output.utf16()),L"wbx")||!stream)fail("Cannot create object record");
    const LONG written=proc<LONG(*)(VARIANT*,void*,void*)>(library.module,"ObjectDoWrite2")(&context.v,stream,fwrite);
    const int flushed=flush(stream);const int closed=close(stream);
    if(written||flushed||closed||read(output).isEmpty())fail("ObjectDoWrite2 failed");
    QTextStream(stdout)<<"Saved "<<number(bodyCount.v)<<" solid bodies\n";
}
} // namespace
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{run(app.arguments());return 0;}catch(const std::exception& e){QTextStream(stderr)<<e.what()<<'\n';return 1;}}




