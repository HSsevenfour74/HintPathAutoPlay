import ctypes, struct, sys
from ctypes import wintypes as w
k = ctypes.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.restype = w.HANDLE
k.ReadProcessMemory.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
k.VirtualQueryEx.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
class M(ctypes.Structure):
    _fields_ = [('base',ctypes.c_void_p),('alloc',ctypes.c_void_p),('ap',w.DWORD),('size',ctypes.c_size_t),('state',w.DWORD),('protect',w.DWORD),('type',w.DWORD)]
h = k.OpenProcess(0x410, False, int(sys.argv[1]))
if not h: raise OSError(ctypes.get_last_error())
def read(a,n):
    b=ctypes.create_string_buffer(n); got=ctypes.c_size_t()
    if not k.ReadProcessMemory(h,a,b,n,ctypes.byref(got)): return b''
    return b.raw[:got.value]
def regions():
    a=0
    while a<0x7fffffffffff:
        m=M()
        if not k.VirtualQueryEx(h,a,ctypes.byref(m),ctypes.sizeof(m)): break
        if m.state==4096 and m.protect & 255 in (2,4,8,32,64,128) and not m.protect&256:
            yield m.base,m.size
        a=(m.base or 0)+m.size
if __name__=='__main__':
    needles=[s.encode() for s in sys.argv[2:]]
    for a,n in regions():
        for off in range(0,n,16*1024*1024):
            b=read(a+off,min(n-off,16*1024*1024))
            for q in needles:
                start=0
                for _ in range(20):
                    i=b.find(q,start)
                    if i<0:break
                    print(hex(a+off+i),repr(b[max(0,i-32):i+100]))
                    start=i+len(q)
