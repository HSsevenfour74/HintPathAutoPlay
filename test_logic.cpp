#include "auto_turn.cpp"
#include <cassert>
#include <string>
struct FakeName { void* a;void* b;int length;wchar_t text[32]; };
struct FakeObject { void* klass;void* monitor;void* native;FakeName n;FakeObject* up;bool active; };
struct FakeArray {void* k;void* m;void* bounds;size_t length;void* values[4];};
static FakeArray scene;static int activations;static bool includeInactive;
static void* fakeType(void*,void*) {return (void*)0x1234;}
static void* fakeGameObject(void* o){return o;}
static void* fakeTransform(void* o){return o;}
static void* fakeParent(void* o){return ((FakeObject*)o)->up;}
static String* fakeName(void* o){return (String*)&((FakeObject*)o)->n;}
static void fakeActive(void* o,bool v){((FakeObject*)o)->active=v;++activations;}
static void* fakeFindAll(void* t,bool inactive){assert(t==(void*)0x1234);includeInactive=inactive;return &scene;}
static void setName(FakeObject& o,const wchar_t* n){o.n.length=(int)wcslen(n);wcscpy_s(o.n.text,n);}
int main(){
 float t=0;
 assert(segmentXZ({0,0,0},{2,0,0},{1,10000,0},t)&&t==0.5f);
 assert(segmentXZ({0,500,0},{2,-500,0},{1,-10000,0},t));
 assert(!segmentXZ({0,0,0},{2,0,0},{1,0,0.19f},t));
 assert(!segmentXZ({0,0,0},{2,0,0},{3,0,0},t));
 assert(!segmentXZ({0,0,0},{0,100,0},{0,0,0},t));
 assert(!segmentXZ({0,0,0},{6,0,0},{1,0,0},t));
 assert(nearXZ({0,0,0},{0,999,0}));
 startedAt=1000;assert(startProtection(1000));assert(startProtection(1299));assert(!startProtection(1300));
 startedAt=5000;assert(startProtection(5000));assert(!startProtection(5300));
 points={{1,{0,800,0},false},{2,{1,-800,0},false},{3,{4,0,0},false}};
 havePrevious=false;consumeProtectedContacts({0,0,0});
 assert(points[0].used && !points[1].used && !points[2].used);
 consumeProtectedContacts({2,0,0});
 assert(points[0].used && points[1].used && !points[2].used && previous.x==2);
 // Guard ends with contacts already consumed; no deferred spawn turn.
 startedAt=1000;assert(!startProtection(1300) && points[0].used && points[1].used);
 points.clear();havePrevious=false; // Same reset performed on restart.
 points.push_back({1,{0,0,0},false});assert(!points[0].used);
 // Synthetic supported-build metadata: inherited Object.GetType, zero parameters.
 alignas(void*) unsigned char method[0x60]={},base[0x130]={},derived[0x130]={};
 *(void**)method=(void*)fakeType;*(const char**)(method+0x18)="GetType";
 void* methods[]={method};*(void***)(base+0x98)=methods;*(unsigned short*)(base+0x11c)=1;
 *(void**)(derived+0x58)=base;
 FakeObject hero={},root={},hint={},clone={},other={};hero.klass=derived;
 setName(root,L"Level");setName(hint,L"HintPath");setName(clone,L"HintPath(Clone)");setName(other,L"HintPathOther");
 hint.up=&root;clone.up=&root;
 scene.length=4;scene.values[0]=&root;scene.values[1]=&hint;scene.values[2]=&clone;scene.values[3]=&other;
 gameObject=fakeGameObject;transform=fakeTransform;parent=fakeParent;name=fakeName;setActive=fakeActive;findAll=fakeFindAll;
 assert(objectType(&hero)==(void*)0x1234);assert(objectType(nullptr)==nullptr);
 forceHintPath(&hero);
 assert(includeInactive && root.active && hint.active && clone.active && !other.active);
 assert(activations==4 && frames==0);
 puts("PASS: XZ ignores Y, miss/segment/teleport bounds, 300ms boundary/reset/consumed contacts, inherited GetType lookup, inactive HintPath + clone + parent activation, unrelated name excluded.");
}
