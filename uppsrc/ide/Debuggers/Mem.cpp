#include "Debuggers.h"

#ifdef PLATFORM_WIN32
#else
#include <sys/ptrace.h>
#endif

#define LLOG(x)   DLOG(x)

int    Pdb::Byte(adr_t addr)
{
#ifdef PLATFORM_WIN32
	if(!win64)
		addr &= 0xffffffff;
#endif
	int page = (int) (addr >> 10);
	if(invalidpage.Find(page) >= 0)
		return -1;
	int pos = (int) (addr & 1023);
	int q = mempage.Find(page);
//if (q>=0)	DLOG("Found memory page from 0x" << Hex(addr) << " thread:" << mainThreadId <<" page:" << page << " - " << strerror(errno));
	if(q >= 0)
		return (byte)mempage[q].data[pos];
	if(mempage.GetCount() > 1024)
		mempage.Clear();

#ifdef PLATFORM_WIN32
	byte data[1024];
	if(ReadProcessMemory(hProcess, (LPCVOID) (addr & ~1023), data, 1024, NULL)) {
		LLOG("ReadProcessMemory 0x" << Hex(addr) << " OK");
		memcpy(mempage.Add(page).data, data, 1024);
		return (byte)data[pos];
	}
	LLOG("ReadProcessMemory " << Hex(addr) << ": " << GetLastErrorMessage());
#else
	// Dwarf implementation
	errno = 0;
	byte data[1024];
	adr_t adr = (addr & ~1023);
	for (int i=0; i<sizeof data; ) {
		#ifdef CPU_64
		*(uint64*)(&data[i]) = ptrace(PTRACE_PEEKDATA, mainThreadId, adr, 0);
		adr += 8;
		i += 8;
		#else
		*(uint32*)(&data[i]) = ptrace(PTRACE_PEEKDATA, mainThreadId, adr, 0);
		adr += 4;
		i += 4;
		#endif
	}
	//LLOG("Read 1K memory from 0x" << Hex(addr) << " thread:" << mainThreadId <<" page:" << page << " - " << strerror(errno));
//LOGHEXDUMP(data,1024);
	memcpy(mempage.Add(page).data, data, 1024);
	return (byte)data[pos];
#endif

	invalidpage.Add(page);
	return -1;
}

bool    Pdb::Copy(adr_t addr, void *ptr, int count)
{
	byte *s = (byte *)ptr;
	while(count--) {
		int q = Byte(addr++);
		if(q < 0)
			return false;
		*s++ = (byte)q;
	}
	return true;
}

String Pdb::ReadString(adr_t addr, int maxlen, bool allowzero)
{
	String r;
	while(r.GetLength() < maxlen) {
		int q = Byte(addr++);
		if(q < 0)
			break;
		if(!q && !allowzero)
			break;
		r.Cat(q);
	}
	return r;
}

WString Pdb::ReadWString(adr_t addr, int maxlen, bool allowzero)
{
	WString r;
	while(r.GetLength() < maxlen) {
		int q = Byte(addr++);
		if(q < 0)
			break;
		int w = Byte(addr++);
		if(w < 0)
			break;
		w = MAKEWORD(q, w);
		if(w == 0 && !allowzero)
			break;
		r.Cat(w);
	}
	return r;
}

//#endif
