#include "Debuggers.h"

#ifdef PLATFORM_WIN32
#else
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <dwarf.h>
#endif

#define STATUS_WX86_CONTINUE             0x4000001D
#define STATUS_WX86_SINGLE_STEP          0x4000001E
#define STATUS_WX86_BREAKPOINT           0x4000001F
#define STATUS_WX86_EXCEPTION_CONTINUE   0x40000020
#define STATUS_WX86_EXCEPTION_LASTCHANCE 0x40000021
#define STATUS_WX86_EXCEPTION_CHAIN      0x40000022

#define LLOG(x)   DLOG(x)

String Pdb::Hex(adr_t a)
{
	return Format64Hex(a);
}

void Pdb::Error(const char *s)
{
	String txt = "Error!&";
	if(s)
		txt << s << "&";
	LLOG("ERROR: " << DeQtf(GetLastErrorMessage()));
	Exclamation(txt + DeQtf(GetLastErrorMessage()));
	running = false;
	Stop();
}

void Pdb::UnloadModuleSymbols()
{
	for(int i = 0; i < module.GetCount(); i++) {
		ModuleInfo& f = module[i];
		if(f.symbols) {
#ifdef PLATFORM_WIN32
			SymUnloadModule64(hProcess, f.base);
			LLOG("Unloaded symbols for " << f.path << ' ' << Hex(module[i].base) << '/' << hProcess);
#endif
		}
	}
}

int  Pdb::FindModuleIndex(adr_t base)
{
	for(int i = 0; i < module.GetCount(); i++)
		if(module[i].base == base)
			return i;
	return -1;
}


void Pdb::LoadModuleInfo()
{
	fninfo_cache.Clear();

#ifdef PLATFORM_WIN32
	ModuleInfo f;
	dword cb = 1;
	HMODULE  h;
	if(!EnumProcessModules(hProcess, &h, sizeof(HMODULE), &cb)) {
		Error();
		return;
	}
	int n = cb / sizeof(HMODULE);
	Buffer<HMODULE> m(n);
	if(!EnumProcessModules(hProcess, m, cb, &cb)) {
		Error();
		return;
	}
	Vector<ModuleInfo> nm;
	for (int i = 0; i < n; i++) {
		MODULEINFO mi;
		if(GetModuleInformation(hProcess, m[i], &mi, sizeof(mi))) {
			ModuleInfo& f = nm.Add();
			f.base = (adr_t)mi.lpBaseOfDll;
			f.size = mi.SizeOfImage;
			int q = FindModuleIndex(f.base);
			if(q >= 0) {
				ModuleInfo& of = module[q];
				f.path = of.path;
				f.symbols = of.symbols;
				of.symbols = false;
				LLOG("Stable " << Hex(f.base) << " (" << Hex(f.size) << "): " << f.path);
			}
			else {
				char name[MAX_PATH];
				if(GetModuleFileNameEx(hProcess, m[i], name, MAX_PATH)) {
					f.path = name;
					adr_t w = (adr_t)SymLoadModule64(hProcess, NULL, name, 0, f.base, f.size);
					if(w) {
						LLOG("Loading symbols " << name << ' ' << Hex(f.base) << '/' << hProcess << " returned base " << Hex(w));
						f.symbols = true;
						LoadGlobals(w);
					}
				}
				LLOG(Hex(f.base) << " (" << Hex(f.size) << "): " << f.path);
			}
		}
	}
	UnloadModuleSymbols();
	module = pick(nm);
#else
	LoadGlobals(w);
#endif

	refreshmodules = false;
}

bool Pdb::AddBp(adr_t address)
{
	LLOG("AddBp: 0x" << Hex(address));
	if(bp_set.Find(address) >= 0)
		return true;

#ifdef PLATFORM_WIN32

	byte prev;
	if(!ReadProcessMemory(hProcess, (LPCVOID) address, &prev, 1, NULL))
		return false;
	LLOG("ReadProcessMemory OK");
	byte int3 = 0xcc;
	if(!WriteProcessMemory(hProcess, (LPVOID) address, &int3, 1, NULL))
		return false;
	LLOG("WriteProcessMemory OK");
//	FlushInstructionCache (hProcess, (LPCVOID)address, 1);

#else

#ifdef CPU_ARM

	// Dwarf ARM implementation
	uint64 prev;
	// Read original instruction
	uint64 peek64 = ptrace(PTRACE_PEEKDATA, mainThreadId, address, 0);
	//LLOG("\t Read memory at break point id:"<<mainThreadId<<" 0x"<<Hex(address)<<" of 0x"<<Hex(peek64));
	// Insert breakpoint instruction
	uint64 bp = (~(uint64)0xffffffff&peek64) | 0xd4200000; // AArch64 uses the BRK #<immediate> instruction 0xD4200000
	prev = 0xffffffff&peek64;
	if (ptrace(PTRACE_POKEDATA, mainThreadId, address, bp)==-1) {
		LLOG("\t Write memory failed id:"<<mainThreadId<<" at 0x"<<Hex(address)<<" of 0x"<<Hex(bp)<<" - "<<strerror(errno));
		return false;
	}
	LLOG("\t Write memory at break point id:"<<mainThreadId<<" 0x"<<Hex(address)<<" to 0x"<<Hex((byte)bp)<<" previous was 0x"<<Hex(prev));

#else

	// Dwarf implementation
	uint64 prev;
	// Read original instruction
	uint64 peek64 = ptrace(PTRACE_PEEKDATA, mainThreadId, address, 0);
	//LLOG("\t Read memory at break point id:"<<mainThreadId<<" 0x"<<Hex(address)<<" of 0x"<<Hex((byte)peek64));
	// Insert breakpoint instruction
	uint64 int3 = (~(uint64)0xff&peek64) | 0xcc;
	prev = (byte)peek64;
	if (ptrace(PTRACE_POKEDATA, mainThreadId, address, int3)==-1) {
		LLOG("\t Write memory failed id:"<<mainThreadId<<" at 0x"<<Hex(address)<<" of 0x"<<Hex(int3)<<" - "<<strerror(errno));
		return false;
	}
	LLOG("\t Write memory at break point id:"<<mainThreadId<<" 0x"<<Hex(address)<<" to 0x"<<Hex((byte)int3)<<" previous was 0x"<<Hex(prev));
	
#endif

#endif

	bp_set.Put(address, prev);
	return true;
}

bool Pdb::RemoveBp(adr_t address)
{
	LLOG("RemoveBp: " << Hex(address));
	int pos = bp_set.Find(address);
	if(pos < 0)
		return true;

#ifdef PLATFORM_WIN32

	if(!WriteProcessMemory(hProcess, (LPVOID)address, &bp_set[pos], 1, NULL))
		return false;
	FlushInstructionCache(hProcess, (LPCVOID)address, 1);

#else

#ifdef CPU_ARM

	// Dwarf ARM implementation
	int64 peek64 = ptrace(PTRACE_PEEKDATA, mainThreadId, address, 0);
	uint64 poke64 = (~(uint64)0xffffffffff&peek64) | bp_set[pos];
	if(ptrace(PTRACE_POKEDATA, mainThreadId, address, poke64, NULL)==-1) {
		LLOG("\t Poke id:"<<mainThreadId<<" at 0x"<<Hex(address)<<" of 0x"<<Hex(poke64)<<" failed - "<<strerror(errno));
		return false;
	}
	LLOG("\t Restore memory at break point 0x"<<Hex(address)<<" to 0x"<<Hex(bp_set[pos]));

#else

	// Dwarf implementation
	int64 peek64 = ptrace(PTRACE_PEEKDATA, mainThreadId, address, 0);
	uint64 poke64 = (~(uint64)0xff&peek64) | ((byte)bp_set[pos]);
	if(ptrace(PTRACE_POKEDATA, mainThreadId, address, poke64, NULL)==-1) {
		LLOG("\t Poke id:"<<mainThreadId<<" at 0x"<<Hex(address)<<" of 0x"<<Hex(poke64)<<" failed - "<<strerror(errno));
		return false;
	}
	LLOG("\t Restore memory at break point 0x"<<Hex(address)<<" to 0x"<<Hex(bp_set[pos]));

#endif

#endif

	bp_set.Unlink(pos);
	return true;
}

bool Pdb::RemoveBp()
{
	LLOG("RemoveBp: all " << bp_set.GetCount());
	for(int i = bp_set.GetCount(); --i >= 0;)
		if(!bp_set.IsUnlinked(i)) {
			adr_t address = bp_set.GetKey(i);

#ifdef PLATFORM_WIN32
			if(!WriteProcessMemory(hProcess, (LPVOID)address, &bp_set[i], 1, NULL))
				return false;
			FlushInstructionCache(hProcess, (LPCVOID)address, 1);

#else

#ifdef CPU_ARM

			// Dwarf ARM implementation
			int64 peek64 = ptrace(PTRACE_PEEKDATA, mainThreadId, address, 0);
			uint64 poke64 = (~(uint64)0xffffffff&peek64) | bp_set[i];
			if(ptrace(PTRACE_POKEDATA, mainThreadId, address, poke64, NULL)==-1) {
				LLOG("\t Poke id:"<<mainThreadId<<" at 0x"<<Hex(address)<<" index:"<<i<<" of 0x"<<Hex(poke64)<<" failed - "<<strerror(errno));
			}
			LLOG("\t Restore memory at break point 0x"<<Hex(address)<<" to 0x"<<Hex(bp_set[i]));

#else

			// Dwarf implementation
			uint64 peek64 = ptrace(PTRACE_PEEKDATA, mainThreadId, address, 0);
			uint64 poke64 = (~(uint64)0xff&peek64) | bp_set[i];
			if(ptrace(PTRACE_POKEDATA, mainThreadId, address, poke64, NULL)==-1) {
				LLOG("\t Poke id:"<<mainThreadId<<" at 0x"<<Hex(address)<<" index:"<<i<<" of 0x"<<Hex(poke64)<<" failed - "<<strerror(errno));
			}
			LLOG("\t Restore memory at break point 0x"<<Hex(address)<<" to 0x"<<Hex(bp_set[i]));

#endif

#endif
			bp_set.Unlink(i);

		}
	bp_set.Clear();
	return true;
}

void Pdb::SyncFrameButtons()
{
	int ii = framelist.GetIndex();
	frame_down.Enable(!lock && ii >= 0 && ii < framelist.GetCount() - 1);
	frame_up.Enable(!lock && ii > 0);
}

void Pdb::Lock()
{
	if(lock == 0) {
		IdeHidePtr();
		IdeDebugLock();
		watches.Disable();
		locals.Disable();
		framelist.Disable();
		SyncFrameButtons();
		frame_down.Disable();
		frame_up.Disable();
		dlock.Show();
		IdeSetBar();
	}
	lock++;
}

void Pdb::Unlock()
{
	lock--;
	if(lock == 0) {
		IdeDebugUnLock();
		watches.Enable();
		locals.Enable();
		framelist.Enable();
		SyncFrameButtons();
		dlock.Hide();
		IdeSetBar();
	}
}


Pdb::Context Pdb::ReadContext(Thread::Hnd hThread)
{
	DR_LOG("ReadContext");
	Context r;
#ifdef PLATFORM_WIN32

#ifdef CPU_64
	if(win64) {
		CONTEXT ctx;
		ctx.ContextFlags = CONTEXT_FULL;
		if(!GetThreadContext(hThread, &ctx))
			Error("GetThreadContext failed");
		memcpy(&r.context64, &ctx, sizeof(CONTEXT));
	}
	else {
		static BOOL (WINAPI *Wow64GetThreadContext)(HANDLE hThread, PWOW64_CONTEXT lpContext);
		ONCELOCK
			DllFn(Wow64GetThreadContext, "Kernel32.dll", "Wow64GetThreadContext");
		
		WOW64_CONTEXT ctx;
		ctx.ContextFlags = WOW64_CONTEXT_FULL;
		if(!Wow64GetThreadContext || !Wow64GetThreadContext(hThread, &ctx))
			Error("Wow64GetThreadContext failed");
		memcpy(&r.context32, &ctx, sizeof(WOW64_CONTEXT));
	}
#else
	CONTEXT ctx;
	ctx.ContextFlags = CONTEXT_FULL;
	if(!GetThreadContext(hThread, &ctx))
			Error("GetThreadContext failed");
	memcpy(&r.context32, &ctx, sizeof(CONTEXT));
#endif

#else

#ifdef CPU_ARM

	// Dwarf ARM implementation
	struct iovec iov;
	iov.iov_base = &r.regs;
	iov.iov_len = sizeof(r.regs);
	ptrace(PTRACE_GETREGSET, hThread, (void*)NT_PRSTATUS, &iov);

#else

	// Dwarf implementation
	ptrace(PTRACE_GETREGS, hThread, NULL, &r.regs);

#endif

#endif
	LLOG("ReadContext hThread:"<<hThread<<" IP:0x"<<Hex(r.GetIP(win64))<<" SP:0x"<<Hex(r.GetSP(win64))<<" flags:0x"<<Hex(r.GetFlags(win64)));
	return r;
}

void Pdb::WriteContext(Thread::Hnd  hThread, Context& context)
{
	LLOG("WriteContext hThread:"<<hThread<<" IP:0x"<<Hex(context.GetIP(win64))<<" SP:0x"<<Hex(context.GetSP(win64))<<" BP:0x"<<Hex(context.GetBP(win64)));

#ifdef PLATFORM_WIN32

#ifdef CPU_64
	if(win64) {
		CONTEXT ctx;
		memcpy(&ctx, &context.context64, sizeof(CONTEXT));
		ctx.ContextFlags = CONTEXT_CONTROL; // CONTEXT_CONTROL specifies SegSs, Rsp, SegCs, Rip, and EFlags. for x64, CONTEXT_CONTROL specifies Sp, Lr, Pc, and Cpsr for arm specifies FP, LR, SP, PC, and CPSR for arm64
		if(!SetThreadContext(hThread, &ctx))
			Error("SetThreadContext failed");
	}
	else {
		static BOOL (WINAPI *Wow64SetThreadContext)(HANDLE hThread, PWOW64_CONTEXT lpContext);
		DllFn(Wow64SetThreadContext, "Kernel32.dll", "Wow64SetThreadContext");

		WOW64_CONTEXT ctx;
		memcpy(&ctx, &context.context32, sizeof(WOW64_CONTEXT));
		ctx.ContextFlags = CONTEXT_CONTROL;
		if(!Wow64SetThreadContext || !Wow64SetThreadContext(hThread, &ctx))
			Error("Wow64SetThreadContext failed");
	}
#else
	CONTEXT ctx;
	memcpy(&ctx, &context.context32, sizeof(WOW64_CONTEXT));
	ctx.ContextFlags = CONTEXT_CONTROL;
	if(!SetThreadContext(hThread, &ctx))
		Error("SetThreadContext failed");
#endif

#else

#ifdef CPU_ARM

	// Dwarf implementation
	struct iovec iov;
	iov.iov_base = &context.regs;
	iov.iov_len = sizeof(context.regs);
	if(ptrace(PTRACE_SETREGSET, hThread, (void*)NT_PRSTATUS, &iov)==-1) {
		LLOG("Pdb::WriteContext PTRACE_SETREGSET id:"<<hThread<<" failed - "<<strerror(errno));
		Error("SetThreadContext failed");
	}

#else

	// Dwarf implementation
	if(ptrace(PTRACE_SETREGS, hThread, NULL, &context.regs)==-1) {
		LLOG("Pdb::WriteContext PTRACE_SETREGS id:"<<hThread<<" failed - "<<strerror(errno));
		Error("SetThreadContext failed");
	}
	
#endif

#endif
}

bool Pdb::AddThread(dword dwThreadId, Thread::Hnd  hThread)
{
	if(threads.Find(dwThreadId) >= 0)
		return false; // Already have this thread
	DR_LOG("AddThread");
	Thread& f = threads.GetAdd(dwThreadId);
	// Retrive "base-level" stack-pointer, to have limit for stackwalks:
	Context c = ReadContext(hThread);
	f.sp = c.GetSP(win64);
	f.hThread = hThread;
	LLOG("Adding thread " << dwThreadId << ", Thread SP: 0x" << Hex(f.sp) << ", handle: 0x" << FormatIntHex((dword)(uintptr_t)(hThread)));
	return true;
}

bool Pdb::RemoveThread(dword dwThreadId)
{
	int q = threads.Find(dwThreadId);
	if(q >= 0) {
		Thread& f = threads[q];
		LLOG("Closing thread " << dwThreadId << ", handle: 0x" << FormatIntHex((dword)(uintptr_t)(f.hThread)));
#ifdef PLATFORM_WIN32
		CloseHandle(f.hThread);
#endif
		threads.Remove(q);
		return true;
	}
	return false;
}

#define EXID(id)       { id, #id },

struct {
	adr_t code;
	const char *text;
}
ex_desc[] = {
#ifdef PLATFORM_WIN32
	EXID(EXCEPTION_ACCESS_VIOLATION)
	EXID(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
	EXID(EXCEPTION_DATATYPE_MISALIGNMENT)
	EXID(EXCEPTION_FLT_DENORMAL_OPERAND)
	EXID(EXCEPTION_FLT_DIVIDE_BY_ZERO)
	EXID(EXCEPTION_FLT_INEXACT_RESULT)
	EXID(EXCEPTION_FLT_INVALID_OPERATION)
	EXID(EXCEPTION_FLT_OVERFLOW)
	EXID(EXCEPTION_FLT_STACK_CHECK)
	EXID(EXCEPTION_FLT_UNDERFLOW)
	EXID(EXCEPTION_ILLEGAL_INSTRUCTION)
	EXID(EXCEPTION_IN_PAGE_ERROR)
	EXID(EXCEPTION_INT_DIVIDE_BY_ZERO)
	EXID(EXCEPTION_INT_OVERFLOW)
	EXID(EXCEPTION_INVALID_DISPOSITION)
	EXID(EXCEPTION_NONCONTINUABLE_EXCEPTION)
	EXID(EXCEPTION_PRIV_INSTRUCTION)
	EXID(EXCEPTION_STACK_OVERFLOW )
#endif
};

void Pdb::RestoreForeground()
{
#ifdef PLATFORM_WIN32
	if(hWnd) {
		SetForegroundWindow(hWnd);
		LLOG("Restored foreground window: " << FormatIntHex((dword)(uintptr_t)hWnd));
	}
	hWnd = NULL;
#else
	//NEVER(); // Todo Dwarf implementation
#endif
}

void Pdb::SaveForeground()
{
#ifdef PLATFORM_WIN32
	HWND hwnd = GetForegroundWindow();
	if(hwnd) {
		DWORD pid;
		GetWindowThreadProcessId(hwnd, &pid);
		if(pid == processid) {
			hWnd = hwnd;
			LLOG("Saved foreground window: " << Hex((adr_t)hWnd));
		}
	}
#else
	//NEVER(); // Todo Dwarf implementation
#endif
}

void Pdb::ToForeground()
{
	TopWindow *w = GetTopWindow();
	if(w && !w->IsForeground()) {
		LLOG("Setting theide as foreground");
		w->SetForeground();
	}
}

bool Pdb::RunToException()
{
	DR_LOG("RunToException");
	LLOG("RUN TO EXCEPTION");
	TimeStop ts;
	bool disasfocus = disas.HasFocus();
	bool locked = false;
	bool frestored = false;
//	int childPid = 0;
	invalidpage.Clear();
	mempage.Clear();
	int opn = 0;
	for(;;) {
		if(terminated) {
			if(locked)
				Unlock();
			return false;
		}
		opn++;
		DR_LOG("WaitForDebugEvent");
		
#ifdef PLATFORM_WIN32

		if(WaitForDebugEvent(&event, 0)) {
			DR_LOG("WaitForDebugEvent ended");
			debug_threadid = event.dwThreadId;
			opn = 0;
			running = false;
			switch(event.dwDebugEventCode) {
			case EXCEPTION_DEBUG_EVENT: {
				DR_LOG("EXCEPTION_DEBUG_EVENT");
				LLOG("Exception: " << FormatIntHex(event.u.Exception.ExceptionRecord.ExceptionCode) <<
				     " at: " << FormatIntHex(event.u.Exception.ExceptionRecord.ExceptionAddress) <<
				     " first: " << event.u.Exception.dwFirstChance);
				SaveForeground();
				const EXCEPTION_RECORD& x = event.u.Exception.ExceptionRecord;
				if(findarg(x.ExceptionCode, EXCEPTION_BREAKPOINT, EXCEPTION_SINGLE_STEP,
				                            STATUS_WX86_BREAKPOINT, STATUS_WX86_SINGLE_STEP) < 0)
				{
					LLOG("Non-debug EXCEPTION");
					String desc = Format("Exception: [* %lX] at [* %16llX]&",
					                     (int64)x.ExceptionCode, (int64)x.ExceptionAddress);
					bool known = false;
					for(int i = 0; i < __countof(ex_desc); i++)
						if(ex_desc[i].code == x.ExceptionCode) {
							known = true;
							desc << "[* " << DeQtf(ex_desc[i].text) << "]&";
							break;
						}
					if(event.u.Exception.dwFirstChance && !known) {
						LLOG("First chance " << FormatIntHex(x.ExceptionCode));
						break;
					}
					if(x.ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
						desc << (x.ExceptionInformation[0] ? "[*@3 writing]" : "[*@4 reading]");
						desc << Format(" at [* %08llX]", (int64)x.ExceptionInformation[1]);
					}
					ToForeground();
					BeepError();
					if(first_exception) {
						first_exception = false;
						ErrorOK(desc);
					}
					else
					if(!Prompt(Ctrl::GetAppName(), CtrlImg::error(), desc, t_("OK"), t_("Stop"))) {
						Stop();
						return false;
					}
				}
#ifdef CPU_64
				if(!win64 && x.ExceptionCode == EXCEPTION_BREAKPOINT && !break_running) // Ignore x64 breakpoint in wow64
					break;
#endif
				if(break_running)
					debug_threadid = mainThreadId;
				break_running = false;
				ToForeground();
				if(disasfocus)
					disas.SetFocus();
				if(locked)
					Unlock();
				if(refreshmodules)
					LoadModuleInfo();
				LLOG("event.dwThreadId = " << event.dwThreadId);
				bool isbreakpoint = findarg(x.ExceptionCode, EXCEPTION_BREAKPOINT, STATUS_WX86_BREAKPOINT) >= 0;
				for(int i = 0; i < threads.GetCount(); i++) {
					Thread& t = threads[i];
					(Context&)t = ReadContext(threads[i].hThread);
					if(event.dwThreadId == threads.GetKey(i)) {
						LLOG("Setting current context");
						if(isbreakpoint
#ifdef CPU_64
						   && bp_set.Find((win64 ? t.context64.Rip : t.context32.Eip) - 1) >= 0
#else
						   && bp_set.Find(t.context32.Eip - 1) >= 0
#endif
						) { // We have stopped at breakpoint, need to move address back
					#ifdef CPU_64
							if(win64)
								t.context64.Rip--;
							else
					#endif
								t.context32.Eip--;
						}
						context = t;
					}
				}
				RemoveBp();
				return true;
			}
			case CREATE_THREAD_DEBUG_EVENT:
				DR_LOG("CREATE_THREAD_DEBUG_EVENT");
				LLOG("Create thread: " << event.dwThreadId);
				AddThread(event.dwThreadId, event.u.CreateThread.hThread);
				break;
			case EXIT_THREAD_DEBUG_EVENT:
				DR_LOG("EXIT_THREAD_DEBUG_EVENT");
				LLOG("Exit thread: " << event.dwThreadId);
				RemoveThread(event.dwThreadId);
				break;
			case CREATE_PROCESS_DEBUG_EVENT:
				DR_LOG("CREATE_PROCESS_DEBUG_EVENT");
				LLOG("Create process: " << event.dwProcessId);
				processid = event.dwProcessId;
				AddThread(event.dwThreadId, event.u.CreateProcessInfo.hThread);
				CloseHandle(event.u.CreateProcessInfo.hFile);
				CloseHandle(event.u.CreateProcessInfo.hProcess);
				break;
			case EXIT_PROCESS_DEBUG_EVENT:
				DR_LOG("EXIT_PROCESS_DEBUG_EVENT");
				LLOG("Exit process: " << event.dwProcessId);
				if(locked)
					Unlock();
				Stop();
				return false;
			case LOAD_DLL_DEBUG_EVENT: {
				DR_LOG("LOAD_DLL_DEBUG_EVENT");
				LLOG("Load dll: " << event.u.LoadDll.lpBaseOfDll);
				CloseHandle(event.u.LoadDll.hFile);
				refreshmodules = true;
				break;
			}
			case UNLOAD_DLL_DEBUG_EVENT:
				DR_LOG("UNLOAD_DLL_DEBUG_EVENT");
				LLOG("UnLoad dll: " << event.u.UnloadDll.lpBaseOfDll);
				refreshmodules = true;
				break;
			case RIP_EVENT:
				DR_LOG("RIP_EVENT");
				LLOG("RIP!");
				Exclamation("Process being debugged died unexpectedly!");
				if(locked)
					Unlock();
				Stop();
				return false;
			}
			DR_LOG("ContinueDebugEvent");
			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
			running = true;
		}

#else

		// Dwarf implementation
		int status;
		pid_t pid = waitpid(mainThreadId, &status, WNOHANG|__WALL|__WCLONE); // Wait for the child debugee to stop
		if (pid>0) {
			DR_LOG("Debugee captured "<<pid);
			LLOG(">>> Debugee captured "<<pid);
			opn = 0;
			running = false;
			if(first_exception) {
				first_exception = false;
				// Read base address from /proc map table
				char line[256];
				String procfile;
				procfile << "/proc/"<<mainThreadId<<"/maps";
				FILE* fp = fopen(procfile, "r");
				if (fp) {
					if (fgets(line, sizeof(line), fp)) {
						baseAddress = strtoul(line, NULL, 16);
						LLOG("Extract base address 0x" << Hex(baseAddress) << " from map" << procfile);
					}
					fclose(fp);
				}
				if (AddThread(pid, pid)) {
					DR_LOG("Create main thread: " << pid);
					LLOG("Create main thread: " << pid);
				}
				ptrace(PTRACE_SETOPTIONS, mainThreadId, NULL, PTRACE_O_TRACECLONE|PTRACE_O_TRACEFORK); // Enable multi threading debugging
			}
			// Check if child process finished
			if(WIFEXITED(status)) {
				DR_LOG("ptrace debug event exited normally with status"<<WEXITSTATUS(status));
				if(locked)
					Unlock();
				Stop();
				if (RemoveThread(pid)) {
					DR_LOG("Exit thread: " << mainThreadId);
					LLOG("Exit thread: " << mainThreadId);
				}
				return false;
			}
			Context ctx = ReadContext(pid);
			uint64 ip = ctx.GetIP();
			// Check if child stopped - normal debugging
			if(WIFSTOPPED(status)) {
				siginfo_t info;
				SaveForeground();
				int stopSig = WSTOPSIG(status);
				LLOG("Debugee child stopped with code "<<stopSig<<" at 0x"<<Hex(ip));
				if(stopSig==SIGTRAP || stopSig==SIGSTOP) { // SIGTRAP(5) Trace/breakpoint trap / SIGSTOP(19) - TRAP_TRACE sent on single step
					bool halt = true;
					ptrace(PTRACE_GETSIGINFO, pid, NULL, &info);
					int statusBits = status>>8;
					if (statusBits == (SIGTRAP | (PTRACE_EVENT_CLONE << 8))) {
						// New thread
						unsigned long procId = 0;
						ptrace(PTRACE_GETEVENTMSG, pid, NULL, &procId);
						LLOG("New thread cloned "<<procId<<" pid:"<<pid<<" mainThreadId:"<<mainThreadId);
						if (AddThread(procId, pid)) {
							DR_LOG("Added thread: " << procId);
							LLOG("Added thread: " << procId);
						}
						debug_threadid = procId;
						halt = false;
						ptrace(PTRACE_CONT, procId, NULL, NULL);
					}
					else if (statusBits == (SIGTRAP | (PTRACE_EVENT_FORK << 8))) {
						// New thread
						unsigned long forkId = 0;
						ptrace(PTRACE_GETEVENTMSG, pid, NULL, &forkId);
						LLOG("New thread cloned "<<forkId<<" pid:"<<pid<<" mainThreadId:"<<mainThreadId);
						if (AddThread(forkId, pid)) {
							DR_LOG("Added process: " << forkId);
							LLOG("Added process: " << forkId);
						}
						debug_threadid = forkId;
						halt = false;
						ptrace(PTRACE_CONT, forkId, NULL, NULL);
					} else {
						debug_threadid = pid;
					}
					context = ctx;
					int q = threads.Find(debug_threadid);
					if(q >= 0) {
						Thread& f = threads[q];
						f.regs = context.regs;
					}
					if (halt) {
						bool singleStep = info.si_code==TRAP_TRACE;
						bool isbreakpoint = info.si_code==TRAP_BRKPT || info.si_code==SI_KERNEL;
						LLOG("\t Got "<<(stopSig==SIGTRAP?"SIGTRAP":"SIGSTOP")<<"("<<stopSig<<") trace/breakpoint break_running:"<<break_running<<" isbreakpoint:"<<isbreakpoint<<" mainThreadId:"<<mainThreadId<<" debug_threadid:"<<debug_threadid);
						ToForeground();
						if(disasfocus)
							disas.SetFocus();
						if(locked)
							Unlock();
						if(refreshmodules)
							LoadModuleInfo();
						if(isbreakpoint) {
							if (bp_set.Find(ip - 1) >= 0) {
								// We have stopped at breakpoint, need to move address back
								ip--;
								LLOG("\t SIGTRAP("<<info.si_signo<<") breakpoint - moved address back from ip:0x"<<Hex((ip+1))<<" to 0x"<<Hex(ip));
								context.SetIP(ip);
								#ifdef CPU_ARM
								struct iovec iov;
								iov.iov_base = &context.regs;
								iov.iov_len = sizeof(context.regs);
								ptrace(PTRACE_SETREGSET, debug_threadid, (void*)NT_PRSTATUS, &iov);
								#else
								ptrace(PTRACE_SETREGS, debug_threadid, NULL, &context.regs);
								#endif
							}
						}
						break_running = false;
						LLOG("<<< Debugee is paused "<<debug_threadid);
						RemoveBp();
						return true; // Target is paused
					}
				}
				if(stopSig==SIGILL) { // SIGILL(4) - Program counter is not pointing to an instruction
					ptrace(PTRACE_GETSIGINFO, pid, NULL, &info);
					DR_LOG("\t Got SIGILL("<<info.si_signo<<") "<<" code:"<<info.si_code<<" - program counter is not pointing to an instruction");
					LLOG("\t Got SIGILL("<<info.si_signo<<") "<<" code:"<<info.si_code<<" - program counter is not pointing to an instruction");
					return false; // It is not going to run
				}
				if(stopSig==SIGSEGV) { // SIGSEGV(11)
					ptrace(PTRACE_GETSIGINFO, pid, NULL, &info);
					DR_LOG("\t Got SIGSEGV("<<info.si_signo<<") code:"<<info.si_code);
					LLOG("\t Got SIGSEGV("<<info.si_signo<<") code:"<<info.si_code);
					ToForeground();
					BeepError();
					String desc = "Debug caught exeception";
					if (info.si_errno!=0)
						desc << " - " << strerror(info.si_errno);
					if(!Prompt(Ctrl::GetAppName(), CtrlImg::error(), desc, t_("OK"), t_("Stop"))) {
						Stop();
						if (RemoveThread(pid)) {
							DR_LOG("Exit thread: " << pid);
							LLOG("Exit thread: " << pid);
						}
						LLOG("<<< Debugee aborted "<<pid);
						return false; // Target has an exception, user has opted out
					}
				}
				if(stopSig==SIGCHLD) { // SIGCHLD(17)
					ptrace(PTRACE_GETSIGINFO, pid, NULL, &info);
					DR_LOG("\t Got SIGCHLD("<<info.si_signo<<") "<<" code:"<<info.si_code);
					LLOG("\t Got SIGCHLD("<<info.si_signo<<") "<<" code:"<<info.si_code);
					context = ctx;
					int q = threads.Find(debug_threadid);
					if(q >= 0) {
						Thread& f = threads[q];
						f.regs = context.regs;
					}
				}
			}
			else if(WIFSIGNALED(status)) {
				DR_LOG("Debugee child killed with signal "<<WTERMSIG(status)<<" at 0x"<<Hex(ip));
				LLOG("Debugee child killed with signal "<<WTERMSIG(status)<<" at 0x"<<Hex(ip));
				if(locked)
					Unlock();
				Stop();
				LLOG("<<< Debugee dead "<<pid);
				return false; // It has died
			}
			else if(WIFCONTINUED(status)) {
				LLOG("Continued");
			}
			LLOG("<<< Debugee continued "<<pid);
			ptrace(PTRACE_CONT, pid, NULL, NULL);
			running = true;
		}
#endif
		
		if(ts.Elapsed() > 200) {
			DR_LOG("ts.Elpsed() > 200");
			if(!lock) {
				Lock();
				locked = true;
			}
			if(!frestored) {
				RestoreForeground();
				frestored = true;
			}
		}
		if(lock) {
			DR_LOG("GuiSleep");
			GuiSleep(opn < 1000 ? 0 : 100);
			Ctrl::ProcessEvents();
		}
		else {
			DR_LOG("Sleep");
			Sleep(opn < 1000 ? 0 : 100);
		}
	}
}

/*
const CONTEXT& Pdb::CurrentContext()
{
	return threads.Get((int)~threadlist).context;
}
*/

void Pdb::WriteContext()
{
#ifdef PLATFORM_WIN32
	WriteContext(threads.Get(event.dwThreadId).hThread, context);
#else
	// Dwarf implementation
	WriteContext(threads.Get(debug_threadid).hThread, context);
#endif
}

bool Pdb::SingleStep()
{
	LLOG("SINGLE STEP 0");
#ifdef PLATFORM_WIN32

#if CPU_64
	if(win64)
		context.context64.EFlags |= 0x100;
	else
#endif
		context.context32.EFlags |= 0x100;
	WriteContext();
	running = true;
	ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
	if(!RunToException())
		return false;
#if CPU_64
	if(win64)
		context.context64.EFlags &= ~0x100;
	else
#endif
		context.context32.EFlags &= ~0x100;
	WriteContext();

#else

	// Dwarf implementation
	if (ptrace(PTRACE_SINGLESTEP, mainThreadId, NULL, NULL)==-1) {
		LOG("ptrace PTRACE_SINGLESTEP failed ID:"<<mainThreadId<<" - "<<strerror(errno));
		return false;
	}
	RunToException();

#endif
	return true;
}

bool Pdb::Continue()
{
	running = true;
#ifdef PLATFORM_WIN32
	LLOG("** Continue "<<event.dwProcessId<<" "<<event.dwThreadId);
	ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
#else
	// Dwarf implementation
	LLOG("*** Continue "<<debug_threadid);
	ptrace(PTRACE_CONT, debug_threadid, NULL, NULL);
#endif
	return RunToException();
}

void Pdb::SetBreakpoints()
{
	RemoveBp();
	for(int i = 0; i < breakpoint.GetCount(); i++)
		AddBp(breakpoint[i]);
}

void Pdb::BreakRunning() //TODO: Fix in wow64?
{
	stop = true;
	if(running) {
#ifdef PLATFORM_WIN32
		BOOL (WINAPI *debugbreak)(HANDLE Process);
		debugbreak = (BOOL (WINAPI *)(HANDLE))
		             GetProcAddress(GetModuleHandle("kernel32.dll"), "DebugBreakProcess");
		if(debugbreak) {
			LLOG("=== DebugBreakProcess");
			break_running = true;
			(*debugbreak)(hProcess);
		}
		else
			Exclamation("Operation is not supported on this OS");
#else
	// Dwarf implementation
	LLOG("Pdb::BreakRunning mainThreadId:"<<mainThreadId);
	kill(mainThreadId, SIGSTOP);
#endif
	}
}

//#endif
