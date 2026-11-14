#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
struct Metadata { unsigned version; const wchar_t* name; const wchar_t* versionText; const wchar_t* description; };
struct Host { unsigned version; const wchar_t* root; void (*log)(const wchar_t*); HMODULE (*module)(const wchar_t*); };
struct Vec { float x,y,z; };
struct String { void* klass; void* monitor; int length; wchar_t text[1]; };
struct Point { int id; Vec pos; bool used; };
static Host host; static char* game; static void (*original)(void*,void*);
static void* (*resolve)(const char*,bool); static void* (*str)(const char*);
static void* (*find)(void*); static void* (*transform)(void*); static void* (*componentTransform)(void*);
static void* (*parent)(void*); static int (*children)(void*); static void* (*child)(void*,int);
static String* (*name)(void*); static void (*position)(void*,Vec*); static int (*id)(void*);
static std::vector<Point> points; static void* lastPlayer; static bool wasStarted; static Vec previous; static bool havePrevious;
static int frames; static int rootId; static bool enabled=true;
static void* (*gameObject)(void*); static void (*setActive)(void*,bool);
static void* (*findAll)(void*,bool); static void* (*getComponentByName)(void*,void*);
static ULONGLONG startedAt; static std::vector<void*> boundHintToggles; static Vec spawnPosition; static bool haveSpawn;
static constexpr float radiusSquared=0.18f*0.18f;
static bool nearXZ(const Vec& a,const Vec& b) {
    float x=a.x-b.x,z=a.z-b.z;return x*x+z*z<=radiusSquared;
}
static bool segmentXZ(const Vec& a,const Vec& b,const Vec& q,float& t) {
    float dx=b.x-a.x,dz=b.z-a.z,dist=dx*dx+dz*dz;
    if(dist<=0.00000001f || dist>=25.f)return false;
    t=((q.x-a.x)*dx+(q.z-a.z)*dz)/dist;
    if(t<0.f || t>1.f)return false;
    float x=a.x+t*dx-q.x,z=a.z+t*dz-q.z;
    return x*x+z*z<=radiusSquared;
}
static bool startProtection(ULONGLONG now) { return now-startedAt<300; }
static void consumeProtectedContacts(const Vec& current) {
    for(auto& item:points){float t;
        if(nearXZ(item.pos,current) || (havePrevious && segmentXZ(previous,current,item.pos,t)))item.used=true;
    }
    previous=current;havePrevious=true;
}
static bool pressed(int vk,bool& held) {
    bool down=(GetAsyncKeyState(vk)&0x8000)!=0;
    bool edge=down&&!held;held=down;
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    return edge && pid==GetCurrentProcessId();
}
template<class T> static T field(void* obj,size_t offset) { return *(T*)((char*)obj+offset); }
static void log(const wchar_t* s) { if(host.log)host.log(s); }
static void scan(void* t,int depth) {
    if(!t || depth>128)return;
    String* n=name(t);
    if(n && n->length==19 && wmemcmp(n->text,L"GuidePathTap(Clone)",19)==0) {
        int key=id(t); Vec p; position(t,&p);
        bool found=false; for(auto& item:points)if(item.id==key){item.pos=p;found=true;break;}
        if(!found)points.push_back({key,p,false});
    }
    int count=children(t); for(int i=0;i<count;++i)scan(child(t,i),depth+1);
}
// GetType is found on the live object's IL2CPP class hierarchy, not a guessed RVA.
// Layout matches the supported game's MethodInfo/class traversal used by its mod menu.
static void* objectType(void* obj) {
    if(!obj)return nullptr;
    for(void* klass=field<void*>(obj,0);klass;klass=field<void*>(klass,0x58)) {
        auto methods=field<void**>(klass,0x98);
        unsigned count=field<unsigned short>(klass,0x11c);
        if(!methods || count>5000)continue;
        for(unsigned i=0;i<count;++i) {
            void* m=methods[i];if(!m)continue;
            const char* n=field<const char*>(m,0x18);
            if(n && strcmp(n,"GetType")==0 && field<unsigned char>(m,0x52)==0) {
                auto fn=field<void*(*)(void*,void*)>(m,0);
                return fn?fn(obj,m):nullptr;
            }
        }
    }
    return nullptr;
}

static void* findMethod(void* obj,const char* wanted) {
    if(!obj)return nullptr;
    for(void* klass=field<void*>(obj,0);klass;klass=field<void*>(klass,0x58)) {
        auto methods=field<void**>(klass,0x98); unsigned count=field<unsigned short>(klass,0x11c);
        if(!methods || count>5000)continue;
        for(unsigned i=0;i<count;++i) { void* m=methods[i]; if(!m)continue;
            const char* n=field<const char*>(m,0x18);
            if(n && strcmp(n,wanted)==0) return m;
        }
    }
    return nullptr;
}

void* hintPathsContainerClass;

static void openHintPathsContainer(void* hero) {
    if (!hero || !findAll || !gameObject || !setActive) {
        log(L"GuidePath AutoTurn: HintPathsContainer interfaces unavailable.");
        return;
    }

    // FindObjectsOfType expects a managed System.Type, not an Il2CppClass*.
    // Discover one live component first; include inactive scene GameObjects.
    if (!hintPathsContainerClass) {
        if (!getComponentByName || !str) {
            log(L"GuidePath AutoTurn: HintPathsContainer component lookup unavailable.");
            return;
        }
        void* heroGo = gameObject(hero);
        void* goType = heroGo ? objectType(heroGo) : nullptr;
        void* objects = goType ? findAll(goType, true) : nullptr;
        if (!objects) return;
        size_t count = field<size_t>(objects, 0x18);
        if (count > 1000000) {
            log(L"GuidePath AutoTurn: invalid scene object count.");
            return;
        }

        // This game's runtime metadata names the component HintPathContainer.
        // Also accept the plural spellings used by other versions/mod sources.
        const char* componentNames[] = {
            "DancingLine.Level.HintPath.HintPathContainer",
            "HintPathContainer", "HintPathsContainer", "hintPathsContainer"
        };
        void* typeNames[4];
        for (size_t i = 0; i < 4; ++i) typeNames[i] = str(componentNames[i]);
        for (size_t i = 0; i < count && !hintPathsContainerClass; ++i) {
            void* go = field<void*>(objects, 0x20 + i * sizeof(void*));
            if (!go) continue;
            for (void* typeName : typeNames) {
                if (!typeName) continue;
                void* script = getComponentByName(go, typeName);
                if (!script) continue;
                hintPathsContainerClass = objectType(script);
                if (hintPathsContainerClass) break;
            }
        }
        if (!hintPathsContainerClass) {
            log(L"GuidePath AutoTurn: HintPathsContainer component not found in loaded scenes.");
            return;
        }
    }

    void* containers = findAll(hintPathsContainerClass, true);
    if (!containers) return;
    size_t count = field<size_t>(containers, 0x18);
    if (count > 1000000) {
        log(L"GuidePath AutoTurn: invalid HintPathsContainer count.");
        return;
    }

    unsigned activated = 0;
    for (size_t i = 0; i < count; ++i) {
        void* script = field<void*>(containers, 0x20 + i * sizeof(void*));
        if (!script) continue;
        void* go = gameObject(script);
        if (!go) continue;
        setActive(go, true);
        ++activated;
    }
    if (activated) frames = 0; // Refresh guide points after enabling containers.
    wchar_t msg[160];
    swprintf_s(msg, L"GuidePath AutoTurn: activated %u HintPathsContainer GameObject(s).", activated);
    log(msg);
}

/*static void bindHintPathToggle(void* hero) {
    if(!findAll || !gameObject) { log(L"GuidePath AutoTurn: HintPathsToggle interfaces unavailable (object scan)."); return; }
    if(!getComponentByName) { log(L"GuidePath AutoTurn: GetComponentByName unresolved; listener repair skipped."); return; }
    void* goType=objectType(gameObject(hero)); if(!goType)return;
    void* array=findAll(goType,true); if(!array)return;
    size_t count=field<size_t>(array,0x18), found=0, bound=0;
    if(count>1000000)return;
    for(size_t i=0;i<count;++i) {
        void* go=field<void*>(array,0x20+i*sizeof(void*)); if(!go)continue;
        void* toggle=getComponentByName(go,str("HintPathsToggle")); if(!toggle)continue;
        ++found; bool seen=false; for(void* x:boundHintToggles)if(x==toggle){seen=true;break;}
        if(seen)continue;
        void* method=findMethod(toggle,"Start");
        if(method) { ((void(*)(void*,void*))field<void*>(method,0))(toggle,method); boundHintToggles.push_back(toggle); ++bound; }
    }
    wchar_t msg[180]; swprintf_s(msg,L"GuidePath AutoTurn: HintPathsToggle found %u; initialized %u (button.onClick -> ButtonClicked).",(unsigned)found,(unsigned)bound); log(msg);
}*/
static void forceHintPath(void* hero) {
    if(!gameObject || !setActive || !findAll) {
        log(L"GuidePath AutoTurn: HintPath interfaces unavailable.");return;
    }
    void* type=objectType(gameObject(hero));
    if(!type){log(L"GuidePath AutoTurn: cannot resolve GameObject type.");return;}
    void* array=findAll(type,true);if(!array)return;
    size_t count=field<size_t>(array,0x18);
    if(count>1000000){log(L"GuidePath AutoTurn: invalid scene object count.");return;}
    unsigned activated=0;
    for(size_t i=0;i<count;++i) {
        void* go=field<void*>(array,0x20+i*sizeof(void*));if(!go)continue;
        String* n=name(go);
        if(!n || !((n->length==8 && wmemcmp(n->text,L"HintPath",8)==0) ||
                   (n->length==15 && wmemcmp(n->text,L"HintPath(Clone)",15)==0) || 
                   (n->length==8 && wmemcmp(n->text,L"hintpath",8)==0) || 
                   (n->length==8 && wmemcmp(n->text,L"Hintpath",8)==0) || 
                   (n->length==8 && wmemcmp(n->text,L"hintPath",8)==0)))continue;
        // Activate the ancestor chain first, including inactive scene roots.
        void* chain[129];int depth=0;
        for(void* t=transform(go);t && depth<129;t=parent(t))chain[depth++]=t;
        while(depth)setActive(gameObject(chain[--depth]),true);
        ++activated;
    }
    frames=0; // Refresh guide positions on the same frame after activation.
    wchar_t msg[160];swprintf_s(msg,L"GuidePath AutoTurn: F9 activated %u HintPath object(s).",activated);log(msg);
}
static void safeForceHintPath(void* hero) {
    __try {forceHintPath(hero);}
    __except(EXCEPTION_EXECUTE_HANDLER) {log(L"GuidePath AutoTurn: HintPath activation failed; auto turn remains available.");}
}
static bool initUnity() {
#define IC(dst,sig) dst=(decltype(dst))resolve(sig,false); if(!dst)return false
    IC(find,"UnityEngine.GameObject::Find(System.String)");
    IC(transform,"UnityEngine.GameObject::get_transform()");
    IC(componentTransform,"UnityEngine.Component::get_transform()");
    IC(parent,"UnityEngine.Transform::GetParent()");
    IC(children,"UnityEngine.Transform::get_childCount()");
    IC(child,"UnityEngine.Transform::GetChild(System.Int32)");
    IC(name,"UnityEngine.Object::GetName(UnityEngine.Object)");
    IC(position,"UnityEngine.Transform::get_position_Injected(UnityEngine.Vector3&)");
    // HintPath is optional: a missing interface must not disable existing auto turn.
    gameObject=(decltype(gameObject))resolve("UnityEngine.Component::get_gameObject()",false);
    setActive=(decltype(setActive))resolve("UnityEngine.GameObject::SetActive(System.Boolean)",false);
    findAll=(decltype(findAll))resolve("UnityEngine.Object::FindObjectsOfType(System.Type,System.Boolean)",false);
    getComponentByName=(decltype(getComponentByName))resolve("UnityEngine.GameObject::GetComponentByName(System.String)",false);
    if(!getComponentByName) getComponentByName=(decltype(getComponentByName))resolve("UnityEngine.GameObject::GetComponentByName(System.String)",true);
    // GetInstanceID is implemented in managed code in this Unity build; native pointer identifies scene objects.
#undef IC
    return true;
}
static int instance(void* t) { auto p=field<unsigned long long>(t,0x10); return (int)(p^(p>>32)); }
static void tick() {
    static bool ready=false,failed=false,f8Held=false,f9Held=false;
    if(failed)return;
    if(!ready){ if(!initUnity()){failed=true;log(L"GuidePath AutoTurn: Unity interface unavailable; disabled.");return;} id=instance;ready=true;log(L"GuidePath AutoTurn: ready. F8 auto turn; F9 activate HintPath; XZ only; 300ms start guard."); }
    if(pressed(VK_F8,f8Held)){enabled=!enabled;log(enabled?L"GuidePath AutoTurn: ON":L"GuidePath AutoTurn: OFF");}
    bool showHint=pressed(VK_F9,f9Held);
    void* klass=*(void**)(game+0x1ed4d08);
    void* statics=klass?field<void*>(klass,0xb8):nullptr;
    void* player=statics?field<void*>(statics,8):nullptr;
    if(!player){havePrevious=false;haveSpawn=false;wasStarted=false;lastPlayer=nullptr;return;}
    bool started=field<bool>(player,0x9c);
    if(player!=lastPlayer || (!wasStarted && started)) {
        points.clear();frames=0;rootId=0;havePrevious=false;haveSpawn=false;
        lastPlayer=player;startedAt=GetTickCount64();
        if(started)log(L"GuidePath AutoTurn: level started; 300ms turn protection.");
    }
    wasStarted=started;
    void* hero=field<void*>(player,0xc8);
    // Allow F9 before starting and while paused or auto turn is off.
    if(showHint && hero){ safeForceHintPath(hero); __try { openHintPathsContainer(hero); } __except(EXCEPTION_EXECUTE_HANDLER) { log(L"GuidePath AutoTurn: HintPathsToggle binding failed."); } }
    if(!started || !hero || field<bool>(hero,0xd0)){havePrevious=false;return;}
    Vec p;position(componentTransform(hero),&p);
    if(!haveSpawn){spawnPosition=p;haveSpawn=true;}
    if(frames++%60==0){
        void* go=find(str("GuidePathTap(Clone)"));
        if(go){void* t=transform(go);while(void* up=parent(t))t=up;
            int key=id(t);if(rootId!=key){points.clear();rootId=key;havePrevious=false;}scan(t,0);
            // Also skip a spawn guide discovered later when HintPath becomes active.
            for(auto& item:points)if(nearXZ(item.pos,spawnPosition))item.used=true;
            if(frames==1){wchar_t msg[128];swprintf_s(msg,L"GuidePath AutoTurn: found %u guide points.",(unsigned)points.size());log(msg);}
        }
    }
    if(startProtection(GetTickCount64())) {
        // Consume contacts during grace so they cannot trigger belatedly at 0.3s.
        consumeProtectedContacts(p);return;
    }
    if(!enabled || !field<bool>(player,0x108) || !field<bool>(player,0x10b)){havePrevious=false;return;}
    if(!havePrevious){previous=p;havePrevious=true;return;}
    Point* best=nullptr;float bestT=2.f;
    for(auto& item:points){if(item.used)continue;float t;
        if(segmentXZ(previous,p,item.pos,t) && t<bestT){best=&item;bestT=t;}
    }
    if(best){best->used=true;int before=field<int>(player,0x104);
        ((void(*)(void*,bool,void*))(game+0x5ae7a0))(player,false,nullptr);
        if(before!=field<int>(player,0x104))log(L"GuidePath AutoTurn: guide reached, turned once.");
    }
    previous=p;
}
static void update(void* self,void* method) { original(self,method); __try { tick(); } __except(EXCEPTION_EXECUTE_HANDLER) { static bool reported=false;if(!reported){log(L"GuidePath AutoTurn: runtime error; restart game and check version.");reported=true;} enabled=false; } }
extern "C" __declspec(dllexport) const Metadata* DLMod_GetMetadata() {static Metadata m={1,L"HintPathAutoPlayMod",L"V1.1",L"F8 On/Off AutoPlay,F9 ForceOpenHintPathInLevel"};return &m;}
extern "C" __declspec(dllexport) int DLMod_Initialize(const Host* h) {
    if(!h||h->version!=1)return 0;host=*h;game=(char*)h->module(L"GameAssembly.dll");if(!game)return 0;
    // Exact build guard: compare the function prologue and embedded SHA-256 via the build script/report.
    unsigned char expected[]={0x40,0x53,0x48,0x83,0xec,0x30,0xf3,0x0f,0x10,0x89,0x40,0x01,0x00,0x00,0x0f,0x57,0xc0};
    char* target=game+0x5b7f60;if(memcmp(target,expected,17)!=0){log(L"GuidePath AutoTurn: unsupported game build.");return 0;}
    resolve=(decltype(resolve))(game+0x1e8420);str=(decltype(str))(game+0x24e1c0);
    auto trampoline=(unsigned char*)VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!trampoline)return 0;
    memcpy(trampoline,target,17);unsigned char jump[]={0xff,0x25,0,0,0,0,0,0,0,0,0,0,0,0};
    void* back=target+17;memcpy(jump+6,&back,8);memcpy(trampoline+17,jump,14);original=(decltype(original))trampoline;
    void* hook=(void*)update;memcpy(jump+6,&hook,8);DWORD old;if(!VirtualProtect(target,17,PAGE_EXECUTE_READWRITE,&old))return 0;
    memcpy(target,jump,14);memset(target+14,0x90,3);VirtualProtect(target,17,old,&old);FlushInstructionCache(GetCurrentProcess(),target,17);
    log(L"HintPathAutoPlayMod V1.1: loaded by:HSsevenfour.");return 1;
}
