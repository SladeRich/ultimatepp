#include "Debuggers.h"

#ifdef PLATFORM_WIN32
#else
#include <capstone/capstone.h>
#endif

#define LLOG(x) // DLOG(x)

int Pdb::Disassemble(adr_t ip)
{
	char out[256];
	byte code[32];
	memset(code, 0, 32);
	int i;
	for(i = 0; i < 32; i++) {
		int q = Byte(i + ip);
		if(q < 0)
			break;
		code[i] = q;
	}
#ifdef PLATFORM_WIN32
	int sz = NDisassemble(out, code, ip, win64);
	if(sz > i)
		return -1;
#else
	int sz = 0;
	csh handle;
	cs_insn* insn;
#ifdef CPU_64
	cs_arch arch = CS_ARCH_X86;
	cs_mode mode = CS_MODE_64;
#else
	cs_arch arch = CS_ARCH_X86;
	cs_mode mode = CS_MODE_64;
#endif
	if(cs_open(arch, mode, &handle) != CS_ERR_OK)
		return 0;
	size_t count = cs_disasm(handle, code, i, ip, 1, &insn);
	if(count > 0) {
		cs_insn &ins = insn[0];
		sprintf(out,"%s\t%s", ins.mnemonic, ins.op_str);
		sz = (int)ins.size;
		//LLOG("Pdb::Disassemble count:"<<count<<" sz:"<<sz<<" ip:0x"<<Hex(ip)<<" asm:"<<out);
		cs_free(insn, count);
	}
	else {
		// This must be data not code
		sprintf(out, ".byte %02x",code[0]);
		sz = 1;
	}
	cs_close(&handle);
#endif
	disas.Add(ip, out, Null, String(code, sz));
	try {
		CParser p(out);
		while(!p.IsEof()) {
			try {
				if(p.Char2('0', 'x'))
					disas.AddT((adr_t)p.ReadNumber64(16));
			}
			catch(CParser::Error) {}
			p.SkipTerm();
		}
	}
	catch(CParser::Error) {}
	LLOG("Pdb::Disassemble sz:"<<sz<<" for ip:0x"<<Hex(ip)<<" "<<out);
	return sz;
}

bool Pdb::IsValidFrame(adr_t eip)
{
	for(int i = 0; i < module.GetCount(); i++) {
		const ModuleInfo& f = module[i];
		if(eip >= f.base && eip < f.base + f.size)
			return true;
	}
	return false;
}

adr_t Pdb::GetIP()
{
	return context.GetIP();
}

void Pdb::Sync()
{
	threadlist.Clear();
	for(int i = 0; i < threads.GetCount(); i++) {
		int thid = threads.GetKey(i);
		AttrText x(Format("0x%x", thid));
		if(thid == mainThreadId)
			x.Underline();
		if(thid == debug_threadid)
			x.Bold();
		threadlist.Add(thid, x);
	}
	threadlist <<= (int)debug_threadid;
	Thread& ctx = Current();
	Sync0(ctx);
	const VectorMap<int, CpuRegister>& reg = Pdb::GetRegisterList();
	cpu.Clear();
	for(int i = 0; i < reg.GetCount(); i++) {
		const CpuRegister& r = reg[i];
		if(r.name)
			cpu.Add(String().Cat() << " " << r.name << "|"
			                       << Hex(GetCpuRegister(ctx, r.sym)) << "|"
			                       << int(i & 1));
	}
	SetFrame();
	IdeActivateBottom();
	SyncFrameButtons();
	SyncTreeDisas();
}

void Pdb::SetThread()
{
	Sync0(Current());
	SetFrame();
	IdeActivateBottom();
}

void Pdb::SetFrame()
{
	int fi = ~framelist;
	frame_up.Enable(fi > 0 && lock == 0);
	frame_down.Enable(fi < framelist.GetCount() - 1 && lock == 0);
	if(fi >= 0 && fi < frame.GetCount()) {
		Frame f = frame[fi];
		current_frame = f;
		bool df = disas.HasFocus();
		FilePos fp = GetFilePos(f.pc);
		IdeHidePtr();
		autotext.Clear();
		Image ptrimg = fi == 0 ? DbgImg::IpLinePtr() : DbgImg::FrameLinePtr();
		if(fp) {
			IdeSetDebugPos(fp.path, fp.line, ptrimg, 0);
			autotext.Clear();
			for(int i = -4; i < 4; i++)
				autotext << ' ' << IdeGetLine(fp.line + i);
		}
		if(!disas.InRange(f.pc) || f.fn.name != disas_name) {
			disas_name = f.fn.name;
			disas.Clear();
			adr_t ip = f.fn.address;
			adr_t h = f.fn.address + f.fn.size;
			if(f.pc < ip || f.pc >= h) {
				ip = f.pc - 64;
				h = ip + 1024;
			}
			while(ip < h) {
				int sz = Disassemble(ip);
				if(sz < 0)
					break;
				ip += sz;
			}
		}
		disas.SetCursor(f.pc);
		disas.SetIp(f.pc, ptrimg);

		if(df)
			disas.SetFocus();
		
		Data();
		UpdateBTs();
	}
	SyncFrameButtons();
}

void Pdb::FrameUpDown(int dir)
{
	int q = framelist.GetIndex() + dir;
	if(q >= 0 && q < framelist.GetCount()) {
		framelist.SetIndex(q);
		SetFrame();
	}
}

bool Pdb::SetBreakpoint(const String& filename, int line, const String& bp)
{
	adr_t a = GetAddress(FilePos(filename, line));
	if(!a)
		return false;
	int q = breakpoint.Find(a);
	if(bp.IsEmpty()) {
		if(q >= 0) {
			if(!RemoveBp(a))
				return false;
			breakpoint.Remove(q);
			breakpoint_cond.Remove(q);
		}
	}
	else {
		if(q < 0) {
			if(!AddBp(a))
				return false;
			breakpoint.Add(a);
			breakpoint_cond.Add(bp);
		}
		else
			breakpoint_cond[q] = bp;
	}
	return true;
}

adr_t Pdb::CursorAdr()
{
	adr_t a = disas.HasFocus() ? disas.GetCursor() : GetAddress(FilePos(IdeGetFileName(), IdeGetFileLine()));
	if(!a)
		Exclamation("No code at choosen location !");
	return a;
}

bool Pdb::ConditionalPass()
{ // resolve conditional breakpoints
	String err;
	int q = breakpoint.Find(GetIP());
	if(q >= 0) {
		String exp = breakpoint_cond[q];
		if(exp != "1" && exp != "\xe") {
			try {
				q = threads.Find((int)debug_threadid);
				if(q >= 0) {
					Thread& ctx = threads[q];
					Array<Frame> frame = Backtrace(ctx, true);
					current_frame = frame[0];
					CParser p(exp);
					if(!GetInt64(Exp(p))) {
						current_frame.Clear();
						return true;
					}
				}
			}
			catch(CParser::Error e) {
				err = e;
			}
		}
	}
	current_frame.Clear();
	Sync();
	if(err.GetCount())
		Exclamation("Error in condition&\1" + err);
	return false;
}

bool Pdb::RunTo()
{
	LLOG("== RunTo");
	adr_t a = CursorAdr();
	if(!a)
		return false;
	do {
		if(!SingleStep())
			return false;
		if(GetIP() == a) {
			Sync();
			break;
		}
		SetBreakpoints();
		AddBp(a);
		if(!Continue())
			return false;
	}
	while(ConditionalPass());
	return true;
}

void Pdb::Run()
{
	LLOG("== Run");
	do {
		SingleStep();
		SetBreakpoints();
		if(!Continue()) {
			LLOG("Run: !Continue");
			return;
		}
		LLOG("Run: Sync");
	}
	while(ConditionalPass());
}

void Pdb::SetIp()
{
	adr_t a = CursorAdr();
	if(!a)
		return;

	context.SetIP(a);
	WriteContext();
	frame[0].pc = a;
	framelist <<= 0;
	SetFrame();
}

bool Pdb::Step(bool over)
{
	LLOG("== Step over: " << over);
	TimeStop ts;
	adr_t ip = GetIP();
	byte b = Byte(ip);
	byte b1 = (Byte(ip + 1) >> 3) & 7;
	if(b == 0xe8 || b == 0x9a || b == 0xff && (b1 == 2 || b1 == 3)) { // Various CALL forms
		LLOG("Stepping over CALL");
		if(over) {
			int l = 5;
			if(b != 0xe8) {
				char out[256];
				byte code[32];
				memset(code, 0, 32);
				adr_t ip = GetIP();
				int i;
				for(i = 0; i < 32; i++) {
					int q = Byte(ip + i);
					if(q < 0)
						break;
					code[i] = q;
				}
#ifdef PLATFORM_WIN32
				l = NDisassemble(out, code, GetIP(), win64);
#else
				// Dwarf implementation
				csh handle;
				cs_insn* insn;
				#ifdef CPU_ARM
				cs_arch arch = CS_ARCH_ARM64;
				cs_mode mode = CS_MODE_ARM;
				#else
				#ifdef CPU_64
				cs_arch arch = CS_ARCH_X86;
				cs_mode mode = CS_MODE_64;
				#else
				cs_arch arch = CS_ARCH_X86;
				cs_mode mode = CS_MODE_64;
				#endif
				#endif
				if(cs_open(arch, mode, &handle) != CS_ERR_OK)
					return 0;
				size_t count = cs_disasm(handle, code, i, ip, 0, &insn);
				char *pout = out;
				if(count > 0) {
					for(size_t j = 0; j < count; i++) {
						sprintf(pout,"0x%s:\t%s\t\t%s\n", ~Hex(insn[i].address), insn[i].mnemonic, insn[i].op_str);
						pout += strlen(pout);
					}
					cs_free(insn, count);
				}
				cs_close(&handle);
				int sz = strlen(out);
#endif
			}
			adr_t bp0 = GetIP();
			adr_t bp = bp0 + l;
			int lvl = 0;
			Lock();
			for(;;) {
				if(!SingleStep()) { // Step into function
					Unlock();
					return false;
				}
				AddBp(bp0); // Add breakpoint at CALL to detect recursion
				AddBp(bp);
				if(!Continue()) {
					Unlock();
					return false;
				}
				if(GetIP() == bp0)
					lvl++; // Recursive call
				else
				if(GetIP() == bp) {
					if(lvl <= 0) {
						Unlock(); // Base level - stepping ended
						return true;
					}
					lvl--;
				}
				else {
					Unlock(); // Any other breakpoint
					return true;
				}
				if(ts.Elapsed() > 100)
					Ctrl::ProcessEvents();
			}
		}
		else {
			if(!SingleStep())
				return false;
			byte b = Byte(GetIP());
			if(b == 0xeb || b == 0xe9) // Step over JMP following CALL (windows jump tables)
				return SingleStep();
			return true;
		}
	}
	else
		return SingleStep();
}

void Pdb::Trace(bool over)
{
	LLOG("== Trace over: " << over);
	adr_t ip0 = GetIP();
	FilePos p0 = GetFilePos(ip0);
	if(IsNull(p0.path) || disas.HasFocus()) {
		if(!Step(over))
			return;
		Sync();
		return;
	}
	bool locked = false;
	int n = 0;
	TimeStop ts;
	for(;;) {
		if(ts.Elapsed() > 100) {
			if(!locked) {
				Lock();
				locked = true;
			}
			ProcessEvents();
		}
		if(!Step(over))
			break;

		adr_t ip = GetIP();
		FilePos p = GetFilePos(ip);
		if(ip < ip0 || p.path != p0.path || p.line != p0.line || stop) {
			Sync();
			break;
		}
	}
	if(locked)
		Unlock();
}

void Pdb::StepOut()
{
	// TODO: Consider using stack frame info to optimize this
	LLOG("== StepOut");
	Lock();
	TimeStop ts;
	for(;;) {
		adr_t ip = GetIP();
		if(Byte(ip) == 0xc2 || Byte(ip) == 0xc3) { // RET instruction variants
			if(!SingleStep())
				break;
			Sync();
			break;
		}
		if(stop) {
			Sync();
			break;
		}
		if(!Step(true))
			break;
		if(ts.Elapsed() > 100)
			Ctrl::ProcessEvents();
	}
	Unlock();
}

//#endif
