#include "Debuggers.h"

#ifdef PLATFORM_WIN32
#else
#include <dwarf.h>
#include <cxxabi.h>
#include <sys/ptrace.h>
#endif

#define LLOG(x) // DLOG(x)

#ifdef _DEBUG

String SymTagAsString(int n) {
	static VectorMap<int, String> tagmap = {
		{ SymTagNull, "SymTagNull" },
		{ SymTagExe, "SymTagExe" },
		{ SymTagCompiland, "SymTagCompiland" },
		{ SymTagCompilandDetails, "SymTagCompilandDetails" },
		{ SymTagCompilandEnv, "SymTagCompilandEnv" },
		{ SymTagFunction, "SymTagFunction" },
		{ SymTagBlock, "SymTagBlock" },
		{ SymTagData, "SymTagData" },
		{ SymTagAnnotation, "SymTagAnnotation" },
		{ SymTagLabel, "SymTagLabel" },
		{ SymTagPublicSymbol, "SymTagPublicSymbol" },
		{ SymTagUDT, "SymTagUDT" },
		{ SymTagEnum, "SymTagEnum" },
		{ SymTagFunctionType, "SymTagFunctionType" },
		{ SymTagPointerType, "SymTagPointerType" },
		{ SymTagArrayType, "SymTagArrayType" },
		{ SymTagBaseType, "SymTagBaseType" },
		{ SymTagTypedef, "SymTagTypedef" },
		{ SymTagBaseClass, "SymTagBaseClass" },
		{ SymTagFriend, "SymTagFriend" },
		{ SymTagFunctionArgType, "SymTagFunctionArgType" },
		{ SymTagFuncDebugStart, "SymTagFuncDebugStart" },
		{ SymTagFuncDebugEnd, "SymTagFuncDebugEnd" },
		{ SymTagUsingNamespace, "SymTagUsingNamespace" },
		{ SymTagVTableShape, "SymTagVTableShape" },
		{ SymTagVTable, "SymTagVTable" },
		{ SymTagCustom, "SymTagCustom" },
		{ SymTagThunk, "SymTagThunk" },
		{ SymTagCustomType, "SymTagCustomType" },
		{ SymTagManagedType, "SymTagManagedType" },
		{ SymTagDimension, "SymTagDimension" },
	};
	return tagmap.Get(n, "");
}

const char * BaseTypeAsString( dword baseType )
{
	switch ( baseType )
	{
	case btNoType: return "btNoType";
	case btVoid: return "btVoid";
	case btChar: return "btChar";
	case btWChar: return "btWChar";
	case btInt: return "btInt";
	case btUInt: return "btUInt";
	case btFloat: return "btFloat";
	case btBCD: return "btBCD";
	case btBool: return "btBool";
	case btLong: return "btLong";
	case btULong: return "btULong";
	case btCurrency: return "btCurrency";
	case btDate: return "btDate";
	case btVariant: return "btVariant";
	case btComplex: return "btComplex";
	case btBit: return "btBit";
	case btBSTR: return "btBSTR";
	case btHresult: return "btHresult";
	default: return "???";
	}
}
#endif

adr_t Pdb::GetAddress(FilePos p)
{
#ifdef PLATFORM_WIN32

	LONG dummy;
	IMAGEHLP_LINE ln;
	ln.SizeOfStruct = sizeof(ln);
	char h[MAX_PATH];
	strcpy(h, p.path);
	if(SymGetLineFromName(hProcess, NULL, h, p.line + 1, &dummy, &ln)) {
		LLOG("GetAddress " << p.path << "(" << p.line << "): " << Hex(ln.Address));
		return ln.Address;
	}
	
#else

	// Dwarf implementation
	unsigned pline = p.line + 1;
	adr_t adr = 0;
	Dwarf_Line** matches = NULL;
	size_t match_count = 0;
	// Retrieve lines matching file and line
	int result = dwarf_getsrc_file(dwarf, p.path, pline, 0, &matches, &match_count);
	if (result == 0 && match_count > 0) {
		for (size_t i = 0; i < match_count; ++i) {
			Dwarf_Addr addr;
			if (dwarf_lineaddr(matches[i], &addr) == 0) {
				adr = addr;
				break;
			}
		}
	}
	if (!adr) {
		// Try looking using relative file path
		size_t plen = strlen(p.path);
		Dwarf_Off off = 0;
		Dwarf_Off next;
		size_t hdrSz;
		// Iterate over compilation units (CU)
		while (dwarf_nextcu(dwarf, off, &next, &hdrSz, NULL, NULL, NULL) == 0) {
			Dwarf_Die cu;
			if (dwarf_offdie(dwarf, off + hdrSz, &cu) != NULL) {
				Dwarf_Lines *lines;
				size_t nlines;
				if (dwarf_getsrclines(&cu, &lines, &nlines) == 0) {
					unsigned best = ~0;
					Dwarf_Addr a = 0;
					for (size_t i = 0; i < nlines; i++) {
						bool fileMatch = false;
						Dwarf_Line *line = dwarf_onesrcline(lines, i);
						size_t fi;
						Dwarf_Files *files;
						dwarf_line_file(line, &files, &fi);
						const char *filename = dwarf_filesrc(files, fi, NULL, NULL);
						if (filename[0]!='/') {
							// Only check relative path
							size_t len = strlen(filename);
							if (len < plen) {
								if (strcmp(~p.path+(plen-len),filename)==0) {
									fileMatch = true;
								}
							}
						} else {
							String fn = NormalizePath(filename);
							if (fn==p.path) {
								fileMatch = true;
							}
						}
						if (fileMatch) {
							int lineNum;
							dwarf_lineaddr(line, &a);
							dwarf_lineno(line, &lineNum);
							if (pline<=lineNum) {
								if (pline==lineNum) {
									adr = a;
									LLOG("GetAddress " << p.path << "(" << pline << "): 0x" << Hex(adr) << " line:"<<lineNum);
									best = 0;
									break;
								}
								unsigned diff = lineNum - pline;
								if (best>diff) {
									best = diff;
								}
							}
						}
						if (best != 0 && best != ~0) {
							adr = a;
						}
					}
				}
			}
			off = next;
		}
	}
	if (adr) {
		adr_t pc = adr + baseAddress;
		LLOG("GetAddress " << p.path << "(" << pline << "): 0x" << Hex(pc));
		return pc;
	} else {
		LLOG("Pdb::GetAddress failed to find " << p.path << "(" << pline << ")");
	}
	
#endif

return 0;
}

Pdb::FilePos Pdb::GetFilePos(adr_t address)
{
	FilePos fp;
	fp.address = address;

#ifdef PLATFORM_WIN32

	DWORD dummy;
	IMAGEHLP_LINE ln;
	ln.SizeOfStruct = sizeof(ln);
	if(SymGetLineFromAddr(hProcess, (uintptr_t)address, &dummy, &ln) && FileExists(ln.FileName)) {
		fp.line = ln.LineNumber - 1;
		fp.path = ln.FileName;
		fp.address = ln.Address;
	}

#else

	// Dwarf implementation
  Dwarf_Addr addr = address - baseAddress;
	Dwarf_Off off = 0;
	Dwarf_Off next;
	size_t hdrSz;
	// Iterate over compilation units (CU)
	while (dwarf_nextcu(dwarf, off, &next, &hdrSz, NULL, NULL, NULL) == 0) {
		Dwarf_Die cu;
		if (dwarf_offdie(dwarf, off + hdrSz, &cu) != NULL) {
			if(dwarf_haspc(&cu, addr)) {
				Dwarf_Lines *lines;
				size_t i, nlines = 0;
				if (dwarf_getsrclines(&cu, &lines, &nlines) == 0) {
					for (i = 0; i < nlines; i++) {
						Dwarf_Line *line = dwarf_onesrcline(lines, i);
						Dwarf_Addr adr;
						dwarf_lineaddr(line, &adr);
						if (adr<=addr) {
							int lineNum;
							dwarf_lineno(line, &lineNum);
							const char *src = dwarf_linesrc(line, NULL, NULL);
							fp.line = lineNum - 1;
							fp.path = src;
							fp.address = address;
							if (adr==addr) {
								LLOG("File position for local address 0x"<<Hex(addr)<<" address:0x"<<Hex(adr)<<" Line: "<<lineNum<<" Source:"<<src);
								break;
							}
						}
						else {
							break;
						}
					}
				}
				if (fp.line == -1) {
					// Dwarf gives 0 when it can not ascertain the line number so just take the next valid line
					for (++i; i < nlines; i++) {
						int lineNum;
						Dwarf_Line *line = dwarf_onesrcline(lines, i);
						dwarf_lineno(line, &lineNum);
						if (lineNum != 0) {
							const char *src = dwarf_linesrc(line, NULL, NULL);
							fp.line = lineNum - 1;
							fp.path = src;
							LLOG("Using next file position for local address 0x"<<Hex(addr)<<" Line: "<<lineNum<<" Source:"<<src);
							break;
						}
					}
				}
				break; // Has PC
			}
		}
		off = next;
	}
	
#endif

	LLOG("GetFilePos(0x" << Hex(address) << "): " << fp.path << ": " << fp.line);
	return fp;
}

#define MAX_SYMB_NAME 1024

Pdb::FnInfo Pdb::GetFnInfo0(adr_t address)
{
	FnInfo fn;
	LLOG("GetFnInfo 0x" << Hex(address));

#ifdef PLATFORM_WIN32

	ULONG64 buffer[(sizeof(SYMBOL_INFO) + MAX_SYMB_NAME + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
	SYMBOL_INFO *f = (SYMBOL_INFO*)buffer;

	f->SizeOfStruct = sizeof(SYMBOL_INFO);
	f->MaxNameLen = MAX_SYMB_NAME;

	DWORD64 h;
	if(SymFromAddr(hProcess, address, &h, f)) {
		LLOG("GetFnInfo " << f->Name
		     << ", type index: " << f->TypeIndex
		     << ", Flags: 0x" << FormatIntHex(f->Flags)
		     << ", Address: 0x" << Hex((dword)f->Address)
		     << ", Size: 0x" << FormatIntHex((dword)f->Size)
		     << ", Tag: " << SymTagAsString(f->Tag));
		fn.name = f->Name;
		fn.address = (adr_t)f->Address;
		fn.size = f->Size;
		fn.pdbtype = f->TypeIndex;
	}

#else
	// Dwarf implementation
	bool done = false;
  Dwarf_Addr addr = address - baseAddress;
	Dwarf_Off off = 0;
	Dwarf_Off next;
	size_t hdrSz;
	// Iterate over compilation units (CU)
	while (dwarf_nextcu(dwarf, off, &next, &hdrSz, NULL, NULL, NULL) == 0) {
		Dwarf_Die cu;
		if (dwarf_offdie(dwarf, off + hdrSz, &cu) != NULL) {
			// Iterate over children DIEs
			Dwarf_Die die; // Debugging Information Entry (DIE)
			if (dwarf_child(&cu, &die) == 0) {
				do {
					int tag = dwarf_tag(&die);
					Dwarf_Die kid;
					if (dwarf_child(&die, &kid) == 0) {
						//LOG("Has kids");
					}
					switch(tag) {
						case DW_TAG_subprogram: {
							if(dwarf_haspc(&die, addr)) {
								Dwarf_Addr loAdr=0;
								dwarf_lowpc(&die, &loAdr);
								Dwarf_Addr hiAdr=0;
								dwarf_highpc(&die, &hiAdr);
								unsigned size = hiAdr - loAdr;
								String name = dwarf_diename(&die);
								Dwarf_Attribute linkAttr;
								if (dwarf_attr(&die, DW_AT_linkage_name, &linkAttr)) {
									const char *mangledName = dwarf_formstring(&linkAttr);
									if (mangledName != NULL) {
										int status;
										char* demangled = abi::__cxa_demangle(mangledName, NULL, NULL, &status);
										if (status == 0) {
											name = demangled;
											std::free(demangled);
										}
									}
								}
								else {
									// Check the header file for the full name
									Dwarf_Attribute specAttr;
									if (dwarf_attr(&die, DW_AT_specification, &specAttr)) {
										Dwarf_Die specDie;
										if (dwarf_formref_die(&specAttr, &specDie)) {
											if (dwarf_attr(&specDie, DW_AT_linkage_name, &linkAttr)) {
												const char *mangledName = dwarf_formstring(&linkAttr);
												if (mangledName != NULL) {
													int status;
													char* demangled = abi::__cxa_demangle(mangledName, NULL, NULL, &status);
													if (status == 0) {
														name = demangled;
														std::free(demangled);
													}
												}
											}
										}
									}
								}
								LLOG("GetFnInfo " << name
								     << ", Address: 0x" << Hex(loAdr)
								     << ", Size: " << size
								     << ", Tag: " << tag);
								fn.name = name;
								fn.address = loAdr + baseAddress;
								fn.size = size;
								fn.pdbtype = tag;
								done = true;
							}
							break;
						}
					}
					if (done) break;
				} while (dwarf_siblingof(&die, &die) == 0);
			}
		}
		off = next;
	}

#endif
	return fn;
}

Pdb::FnInfo Pdb::GetFnInfo(adr_t address)
{
	int q = fninfo_cache.Find(address);
	if(q >= 0)
		return fninfo_cache[q];
	if(fninfo_cache.GetCount() > 100)
		fninfo_cache.Clear();
	FnInfo f = GetFnInfo0(address);
	fninfo_cache.Add(address, f);
	return f;
}

void Pdb::TypeVal(Pdb::Val& v, int typeId, adr_t modbase)
{
	adr_t tag;
#ifdef PLATFORM_WIN32

	BOOL reference;
	dword dw = 0;
#ifdef COMPILER_MINGW
	SymGetTypeInfo(hProcess, modbase, typeId, (IMAGEHLP_SYMBOL_TYPE_INFO)31, &reference);
#else
	SymGetTypeInfo(hProcess, modbase, typeId, TI_GET_IS_REFERENCE, &reference);
#endif
	v.reference = reference;

	for(;;) {
		tag = GetSymInfo(modbase, typeId, TI_GET_SYMTAG);
		if(tag == SymTagPointerType)
			v.ref++;
		else
		if(tag == SymTagArrayType)
			v.array = true;
		else {
			if(tag == SymTagUDT)
				v.udt = true;
			break;
		}
		typeId = GetSymInfo(modbase, typeId, TI_GET_TYPE); // follow pointer(s) to base type
	}
	v.type = UNKNOWN;
	if(tag == SymTagUDT)
		v.type = GetTypeIndex(modbase, typeId);
	else {
		ULONG64 sz = 0;
		SymGetTypeInfo(hProcess, modbase, typeId, TI_GET_LENGTH, &sz);
		dword size = (dword)sz;
		if(tag == SymTagEnum)
			v.type = size == 8 ? SINT8 : size == 4 ? SINT4 : size == 2 ? SINT2 : SINT1;
		else {
			switch(GetSymInfo(modbase, typeId, TI_GET_BASETYPE)) {
			case btBool:
				v.type = BOOL1;
				break;
			case btChar:
			case btWChar:
			case btInt:
			case btLong:
				v.type = size == 8 ? SINT8 : size == 4 ? SINT4 : size == 2 ? SINT2 : SINT1;
				break;
			case btUInt:
			case btULong:
				v.type = size == 8 ? UINT8 : size == 4 ? UINT4 : size == 2 ? UINT2 : UINT1;
				break;
			case btFloat:
				v.type = size == 8 ? DBL : FLT;
				break;
			}
		}
	}
#else
	NEVER(); // Todo Dwarf implementation
#endif
}

struct Pdb::LocalsCtx {
	adr_t                       frame;
	VectorMap<String, Pdb::Val> param;
	VectorMap<String, Pdb::Val> local;
	Pdb                        *pdb;
	Context                    *context;
};

int CALLBACK Pdb::EnumLocals(PSYMBOL_INFO pSym, unsigned long SymbolSize, void* UserContext)
{
	LocalsCtx& c = *(LocalsCtx *)UserContext;

	if(pSym->Tag == SymTagFunction)
		return TRUE;

#ifdef PLATFORM_WIN32

	bool param = pSym->Flags & IMAGEHLP_SYMBOL_INFO_PARAMETER;
	Val& v = (param ? c.param : c.local).Add(pSym->Name);
	v.address = (adr_t)pSym->Address;
	if(pSym->Flags & IMAGEHLP_SYMBOL_INFO_REGISTER)
		v.address = pSym->Register;
	else
	if(pSym->Flags & IMAGEHLP_SYMBOL_INFO_REGRELATIVE) {
		if(pSym->Register == CV_ALLREG_VFRAME) {
		#ifdef CPU_64
			if(c.pdb->win64)
				v.address += c.pdb->GetCpuRegister(*c.context, CV_AMD64_RBP);
			else
		#endif
			{
				adr_t ebp = (adr_t)c.pdb->GetCpuRegister(*c.context, CV_REG_EBP);
				if(c.pdb->clang)
					ebp &= ~(adr_t)7;  // Workaround for supposed clang/win32 issue
				v.address += ebp;
			}
		}
		else
			v.address += (adr_t)c.pdb->GetCpuRegister(*c.context, pSym->Register);
	}
	else
	if(pSym->Flags & IMAGEHLP_SYMBOL_INFO_FRAMERELATIVE)
		v.address += c.frame;
	
	c.pdb->TypeVal(v, pSym->TypeIndex, (adr_t)pSym->ModBase);
	if(param && v.udt && v.ref == 0 && c.pdb->win64) { // dbghelp.dll incorrectly does not report pointer for (copied) value struct params
		v.ref++;
		v.reference = true;
	}
	v.reported_size = pSym->Size;
	v.context = c.context;
#if 0
	DLOG("------");
	DDUMP(pSym->Name);
	DLOG("TYPE: " << (v.type >= 0 ? c.pdb->GetType(v.type).name : "primitive"));
	DDUMPHEX(pSym->Flags);
	DDUMP(pSym->Flags & IMAGEHLP_SYMBOL_INFO_PARAMETER);
	DDUMP(pSym->Flags & IMAGEHLP_SYMBOL_INFO_REGISTER);
	DDUMP(pSym->Flags & IMAGEHLP_SYMBOL_INFO_REGRELATIVE);
	DDUMP(pSym->Flags & IMAGEHLP_SYMBOL_INFO_FRAMERELATIVE);
	DDUMP(pSym->Register == CV_ALLREG_VFRAME);
	DDUMP(pSym->Register);
	DDUMP(pSym->Scope);
	DDUMP(pSym->Value);
	DDUMP(pSym->ModBase);
	DDUMPHEX((adr_t)pSym->Address);
#endif
	LLOG("LOCAL " << c.pdb->GetType(v.type).name << " " << pSym->Name << ": " << Format64Hex(v.address));
#endif
	return TRUE;
}

#ifdef PLATFORM_WIN32
#else

int Pdb::GetValType(Dwarf_Die& die, Dwarf_Die *valDie) {
	int valType = UNKNOWN; // enum { UNKNOWN = -99, BOOL1, SINT1, UINT1, SINT2, UINT2, SINT4, UINT4, SINT8, UINT8, FLT, DBL, PFUNC };
	if (valDie) {
		*valDie = die;
	}
	int tag = dwarf_tag(&die);
	if (tag==DW_TAG_base_type) {
		Dwarf_Word typeSz;
		dwarf_aggregate_size(&die, &typeSz);
		Dwarf_Word encoding;
		Dwarf_Attribute encodingAttr;
		if (dwarf_attr(&die, DW_AT_encoding, &encodingAttr)) {
			dwarf_formudata(&encodingAttr, &encoding); // DW_ATE_signed, DW_ATE_unsigned, DW_ATE_float, DW_ATE_boolean, DW_ATE_void etc.
			switch(encoding) {
				case DW_ATE_boolean:
					valType = BOOL1;
					break;
				case DW_ATE_signed_char:
					valType = SINT1;
					break;
				case DW_ATE_signed_fixed:
				case DW_ATE_signed:
					switch(typeSz) {
						case 1: valType = SINT1; break;
						case 2: valType = SINT2; break;
						case 4: valType = SINT4; break;
						case 8: valType = SINT8; break;
					}
					break;
				case DW_ATE_unsigned_char: valType = UINT1; break;
				case DW_ATE_unsigned_fixed:
				case DW_ATE_unsigned:
					switch(typeSz) {
						case 1: valType = UINT1; break;
						case 2: valType = UINT2; break;
						case 4: valType = UINT4; break;
						case 8: valType = UINT8; break;
					}
					break;
				case DW_ATE_decimal_float:
				case DW_ATE_float:
					switch(typeSz) {
						case 4: valType = FLT; break;
						case 8: valType = DBL; break;
					}
					break;
				case DW_ATE_address:
					valType = PFUNC;
					break;
			}
		}
	}
	else if (tag==DW_TAG_const_type) {
		// Get the baseType
		Dwarf_Attribute typeAttr;
		if (dwarf_attr(&die, DW_AT_type, &typeAttr)) {
			Dwarf_Die subTypeDie;
			if (dwarf_formref_die(&typeAttr, &subTypeDie)) {
				if (valDie) {
					*valDie = subTypeDie;
				}
				valType = GetValType(subTypeDie); // Const type - need to go deeper
			}
		}
	}
	else {
		const char *typeName = dwarf_diename(&die);
		LLOG("\t\t\t typeName:"<<(typeName?:""));
	}
	return valType;
}

bool Pdb::GetTypeVal(Pdb::Val* val, Dwarf_Die die) {
	bool ok = false;
	int tag = dwarf_tag(&die);
	const char *name = dwarf_diename(&die);
	const char *tagName = 0;
	switch(tag) {
		case DW_TAG_typedef:
			if (!tagName) tagName = "DW_TAG_typedef";
		case DW_TAG_member:
			if (!tagName) tagName = "DW_TAG_member";
		case DW_TAG_inheritance:
			if (!tagName) tagName = "DW_TAG_inheritance";
		case DW_TAG_formal_parameter:
			if (!tagName) tagName = "DW_TAG_formal_parameter";
		case DW_TAG_variable:
			if (!tagName) tagName = "DW_TAG_variable";
			if (tagName) {
				const char *file = dwarf_decl_file(&die);
				int line = -1;
				dwarf_decl_line(&die, &line);
				LLOG("\t\t "<<tagName<<" #"<<tag<<" Ref:"<<dwarf_dieoffset(&die)<<" line:"<<line<<" file:"<<(file ?:"")<<" Name:"<<(name ?: ""));
				// Get variable type info
				int valType = UNKNOWN; // enum { UNKNOWN = -99, BOOL1, SINT1, UINT1, SINT2, UINT2, SINT4, UINT4, SINT8, UINT8, FLT, DBL, PFUNC };
				int ref = 0;
				bool reference = false;
				bool udt = false; // User defined type
				bool ptr = false;
				bool self = false;
				bool array = false;
				bool constance = false;
				Dwarf_Word typeSz = 0;
				Dwarf_Word rptSz = 0;
				Dwarf_Word arrayCnt = 0;
				Dwarf_Attribute typeAttr;
				Dwarf_Die typeDie;
				if (dwarf_attr(&die, DW_AT_type, &typeAttr)) {
					if (dwarf_formref_die(&typeAttr, &typeDie)) {
						int typeTag = dwarf_tag(&typeDie);
						if (typeTag==DW_TAG_pointer_type || typeTag==DW_TAG_reference_type) {
							ref = 1;
							reference = typeTag==DW_TAG_reference_type;
							LLOG("\t\t\t "<<(reference?"reference":"pointer"));
							ptr = !reference;
							// Get the baseType of pointer
							if (dwarf_attr(&typeDie, DW_AT_type, &typeAttr)) {
								Dwarf_Die subTypeDie;
								if (dwarf_formref_die(&typeAttr, &subTypeDie)) {
									Dwarf_Die valDie;
									valType = GetValType(subTypeDie, &valDie);
									if (valType == UNKNOWN) {
										// Must be a struct type
										valType = GetTypeIndex(0,dwarf_dieoffset(&subTypeDie)); // Get custom val type
										Type& t = type[valType];
										t.die = valDie;
										Dwarf_Attribute sizeAttr;
										if (dwarf_attr(&subTypeDie, DW_AT_byte_size, &sizeAttr)) {
											dwarf_formudata(&sizeAttr, &rptSz);
										}
										udt = true;
										if (name && strcmp(name,"this")==0) {
											self = true;
										}
									}
								}
							}
						}
						else if (typeTag==DW_TAG_class_type || typeTag==DW_TAG_structure_type) {
							LLOG("\t\t\t "<<(typeTag==DW_TAG_class_type?"class":"struct")<<" Ref:"<<dwarf_dieoffset(&typeDie));
							valType = GetTypeIndex(0,dwarf_dieoffset(&typeDie)); // Get custom val type
							Type& t = type[valType];
							t.die = typeDie;
							Dwarf_Attribute sizeAttr;
							if (dwarf_attr(&typeDie, DW_AT_byte_size, &sizeAttr)) {
								dwarf_formudata(&sizeAttr, &rptSz);
							}
							Dwarf_Attribute ccAttr;
							Dwarf_Word cc;
							// Check calling convention is by reference
							if (dwarf_attr(&typeDie, DW_AT_calling_convention, &ccAttr) != NULL) {
								if (dwarf_formudata(&ccAttr, &cc) == 0) {
									if (cc == DW_CC_pass_by_reference) {
///										reference = true;
									}
								}
							}
							udt = true;
						}
						else if (typeTag==DW_TAG_array_type) {
							LLOG("\t\t\t array");
							ref = 1;
							Dwarf_Die typeKid;
							if (dwarf_child(&typeDie, &typeKid) == 0) {
								int typeKidTag = dwarf_tag(&typeKid);
								if (typeKidTag==DW_TAG_subrange_type) {
									Dwarf_Attribute cntAttr;
									if (dwarf_attr(&typeKid, DW_AT_count, &cntAttr)) {
										dwarf_formudata(&cntAttr, &arrayCnt);
									}
								}
							}
							// Get the baseType of array
							if (dwarf_attr(&typeDie, DW_AT_type, &typeAttr)) {
								Dwarf_Die subTypeDie;
								if (dwarf_formref_die(&typeAttr, &subTypeDie)) {
									Dwarf_Die valDie;
									valType = GetValType(subTypeDie, &valDie);
									if (valType == UNKNOWN) {
										// Must be a struct type
										valType = GetTypeIndex(0,dwarf_dieoffset(&subTypeDie)); // Get custom val type
										Type& t = type[valType];
										t.die = valDie;
										Dwarf_Attribute sizeAttr;
										if (dwarf_attr(&subTypeDie, DW_AT_byte_size, &sizeAttr)) {
											dwarf_formudata(&sizeAttr, &rptSz);
										}
										udt = true;
									}
								}
							}
							array = true;
						}
						else if (typeTag==DW_TAG_typedef) {
							// Get the real type
							if (dwarf_attr(&typeDie, DW_AT_type, &typeAttr)) {
								Dwarf_Die subTypeDie;
								if (dwarf_formref_die(&typeAttr, &subTypeDie)) {
									Dwarf_Die valDie;
									valType = GetValType(subTypeDie, &valDie);
									if (valType == UNKNOWN) {
										// Must be a struct type
										valType = GetTypeIndex(0,dwarf_dieoffset(&subTypeDie)); // Get custom val type
										Type& t = type[valType];
										t.die = valDie;
										udt = true;
									}
								}
							}
						}
						else if (typeTag==DW_TAG_base_type) {
							valType = GetValType(typeDie);
						}
					}
				}
				if (valType != UNKNOWN || ref != 0 || udt || array) {
						val->type = valType;
						val->array = array;
						val->udt = udt;
						val->ref = ref;
						val->reference = reference || self; // Self prevents visual from showing an array of this
						val->rvalue = !(reference || ptr);
						if (array) {
							unsigned sz = typeSz;
							if (sz == 0) {
								if (rptSz == 0) {
									sz = 1;
									switch(valType) {
										case UINT1: sz = 1; break;
										case UINT2: sz = 2; break;
										case UINT4: sz = 4; break;
										case UINT8: sz = 8; break;
										case SINT1: sz = 1; break;
										case SINT2: sz = 2; break;
										case SINT4: sz = 4; break;
										case SINT8: sz = 8; break;
										case FLT:   sz = 4; break;
										case DBL:   sz = 4; break;
										#ifdef CPU_64
										case PFUNC: sz = 8; break;
										#else
										case PFUNC: sz = 4; break;
										#endif
									}
								}
								else {
									sz = rptSz;
								}
							}
							rptSz = sz * arrayCnt;
						}
						val->reported_size = rptSz;
//						if (reference && ref<=0) {
//							val->ref = 1; // Ensure this is set for a reference type
//						}
						ok = true; // Set type
				}
				// Get variable location
				Dwarf_Op* expr;
				size_t cnt;
				Dwarf_Attribute locAttr;
				dwarf_attr(&die, DW_AT_location, &locAttr);
				if (dwarf_getlocation(&locAttr, &expr, &cnt) == 0) {
					uint64 adr = 0;
					// Evaluate Dwarf expressions
					for (size_t i = 0; i < cnt; ++i) {
						Dwarf_Op &exp = expr[i];
						LLOG("\t\t Op:" << (int)exp.atom << " Val1: " << exp.number << " Val2: " << exp.number2);
						if (exp.atom == DW_OP_fbreg) { // Variable at frame base register offset
							int64 reg = exp.number;
							int64 offset = exp.number2;
							adr_t fr;
//							switch(fbReg) {
//								case DW_OP_reg31: // ARM 64 stack pointer register
//									fr = context.GetSP();
//									break;
//								case DW_OP_fbreg:
//								default:
									fr = context.GetBP();
//									break;
//							}
							adr = fr+offset+reg;
						}
						else if (DW_OP_breg0 <= exp.atom && exp.atom <= DW_OP_breg31) { // Variable at the given register plus the given offset to the stack
							int64 reg = exp.number;
							int64 offset = exp.number2;
							user_regs_struct regs;
							#ifdef CPU_ARM
							struct iovec iov;
							iov.iov_base = &regs;
							iov.iov_len = sizeof(regs);
							ptrace(PTRACE_GETREGSET, debug_threadid, (void*)NT_PRSTATUS, &iov); // ARM does not use PTRACE_GETREGS - must use PTRACE_GETREGSET instead
							unsigned idx = exp.atom - DW_OP_breg0;
							uint64 r = regs.regs[idx];
							#else
							ptrace(PTRACE_GETREGS, debug_threadid, NULL, &regs);
							uint64 r;
							switch(exp.atom) {
								#ifdef CPU_64
								case DW_OP_breg0: r = regs.rax; break;
								case DW_OP_breg1: r = regs.rcx; break;
								case DW_OP_breg2: r = regs.rdx; break;
								case DW_OP_breg3: r = regs.rbx; break;
								case DW_OP_breg4: r = regs.rsp; break;
								case DW_OP_breg5: r = regs.rbx; break;
								case DW_OP_breg6: r = regs.rsi; break;
								case DW_OP_breg7: r = regs.rdx; break;
								#else
								case DW_OP_breg0: r = regs.Eax; break;
								case DW_OP_breg1: r = regs.Ecx; break;
								case DW_OP_breg2: r = regs.Edx; break;
								case DW_OP_breg3: r = regs.Ebx; break;
								case DW_OP_breg4: r = regs.Esp; break;
								case DW_OP_breg5: r = regs.Ebx; break;
								case DW_OP_breg6: r = regs.Esi; break;
								case DW_OP_breg7: r = regs.Edx; break;
								#endif
								default: r = 0; break;
							}
							#endif
							adr = r+offset+reg;
						}
						else if (exp.atom == DW_OP_addrx || exp.atom == DW_OP_GNU_addr_index) { // Variable in CU list at offset
							Dwarf_Attribute exprAttr;
							if (dwarf_getlocation_attr(&locAttr,expr,&exprAttr) == 0) {
								Dwarf_Addr eaddr;
								dwarf_formaddr(&exprAttr, &eaddr);
								adr = baseAddress + eaddr;
							}
						}
						else if (DW_OP_lit0 <= exp.atom && exp.atom <= DW_OP_lit31) { // Literal encoding, value is  on to the stack
							NEVER();
						}
						else if (exp.atom == DW_OP_addr) { // Literal encoding, value at address
							NEVER();
						}
						else if (exp.atom == DW_OP_constu) { // Literal encoding, unsigned value on to the stack
							NEVER();
						}
						else if (exp.atom == DW_OP_stack_value) { // Value is on top of stack
							NEVER();
						}
						else if (exp.atom == DW_OP_dup) { // Stack operations - Duplicate the value at the top of the stack
							NEVER();
						}
						else if (exp.atom == DW_OP_deref) { // Stack operations - Treats the top of the stack as a memory address, and replaces it with the contents of that address
							NEVER();
						}
						else if (exp.atom == DW_OP_and) { // Logical operations - Pops the top two values from the stack and pushes back the logical AND of them
							NEVER();
						}
						else if (exp.atom == DW_OP_plus) { // Arithmetic operations - Pops the top two values from the stack and pushes back the addition of them
							NEVER();
						}
						else if (exp.atom == DW_OP_le || exp.atom == DW_OP_eq || exp.atom == DW_OP_gt) { // Control flow operations - Pops the top two values, compares them, and pushes 1 if the condition is true and 0 otherwise
							NEVER();
						}
						else if (exp.atom == DW_OP_bra) { // Control flow operations - Conditional branch: if the top of the stack is not 0, skips back or forward in the expression by offset
							NEVER();
						}
						else if (exp.atom == DW_OP_convert) { // Type conversions - Converts value on the top of the stack to a different type, which is described by the Dwarf information entry at the given offset
							NEVER();
						}
						else {
							LLOG("\t\t Unknown variable location");
							NEVER();
						}
						if (ok) {
							uint64 v = ptrace(PTRACE_PEEKDATA, debug_threadid, adr, 0);
							LLOG("\t\t\t address: 0x" << Hex(adr) << " value:" << v << " 0x" << Hex(v));
							if (array | ref | udt) {
								val->address = adr;
							}
							else {
								if (valType==DBL) {
									#ifdef CPU_64
									#else
									uint64 v2 = ptrace(PTRACE_PEEKDATA, debug_threadid, adr+4, 0); // With older 32bit CPU double must be spread across 2 long words
									v |= v2<<4;
									#endif
									double d;
									memcpy(&d, &v, sizeof d);
									val->fval = d;
								}
								else if (valType==FLT) {
									float f;
									memcpy(&f, &v, sizeof f);
									val->fval = f;
								}
								else if (valType<0) {
									val->ival = (int64)v;
								}
							}
						}
					}
				}
			}
			break;
		}
	return ok;
}

bool Pdb::GetNestedLocals(VectorMap<String, Pdb::Val>& param, VectorMap<String, Pdb::Val>& local, Dwarf_Die *die, Dwarf_Addr addr) {
	bool ok = true;
	Dwarf_Die kid;
	if (dwarf_child(die, &kid) == 0) {
		do {
			int tag = dwarf_tag(&kid);
			if (tag == DW_TAG_lexical_block) {
				if(dwarf_haspc(&kid, addr)) {
					GetNestedLocals(param, local, &kid, addr);
				}
			}
			else {
				Pdb::Val val;
				if (GetTypeVal(&val, kid)) {
					int tag = dwarf_tag(&kid);
					const char *name = dwarf_diename(&kid);
					if(tag == DW_TAG_formal_parameter) {
						param.Add(name,val);
						LLOG("\t\t added parameter variable "<<name<<" val:"<<val<<" address:0x"<<Hex(val.address)<<" ival:"<<val.ival);
					} else {
						local.Add(name,val);
						LLOG("\t\t added local variable "<<name<<" val:"<<val<<" address:0x"<<Hex(val.address)<<" ival:"<<val.ival);
					}
				}
			}
		} while (dwarf_siblingof(&kid, &kid) == 0);
	}
	return ok;
}
#endif

void Pdb::GetLocals(Frame& frame, Context& context, VectorMap<String, Pdb::Val>& param,
                    VectorMap<String, Pdb::Val>& local)
{
	LLOG("GetLocals *****************");
#ifdef PLATFORM_WIN32
	static IMAGEHLP_STACK_FRAME f;
	f.InstructionOffset = frame.pc;
	SymSetContext(hProcess, &f, 0);
	LocalsCtx c;
	c.frame = frame.frame;
	c.pdb = this;
	c.context = &context;
	SymEnumSymbols(hProcess, 0, 0, &EnumLocals, &c);
	param = pick(c.param);
	local = pick(c.local);
#else
	// Dwarf implementation
	adr_t ipOrg = this->context.GetIP();
	adr_t bpOrg = this->context.GetBP();
	adr_t spOrg = this->context.GetSP();
	adr_t ip = context.GetIP();
	adr_t bp = context.GetBP();
	adr_t sp = context.GetSP();
	this->context.SetIP(ip);
	this->context.SetBP(bp);
	this->context.SetSP(sp);
  Dwarf_Addr addr = ip - baseAddress;
	LLOG("Pdb::GetLocals ip:0x"<<Hex(ip)<<" baseAddress:0x"<<Hex(baseAddress)<<" addr:0x"<<Hex(addr));
	Dwarf_Off off = 0;
	Dwarf_Off next;
	size_t hdrSz;
	// Iterate over compilation units (CU)
	while (dwarf_nextcu(dwarf, off, &next, &hdrSz, NULL, NULL, NULL) == 0) {
		Dwarf_Die cu;
		if (dwarf_offdie(dwarf, off + hdrSz, &cu) != NULL) {
			// Iterate over children DIEs
			Dwarf_Die die; // Debugging Information Entry (DIE)
			if (dwarf_child(&cu, &die) == 0) {
				do {
					int tag = dwarf_tag(&die);
					const char *fnName = dwarf_diename(&die);
					switch(tag) {
						case DW_TAG_subprogram:
							if(dwarf_haspc(&die, addr)) {
								// This must be the function the cursor is a in, now get all the children variables associated with it
								Dwarf_Addr loAdr=0;
								dwarf_lowpc(&die, &loAdr);
								Dwarf_Addr hiAdr=0;
								dwarf_highpc(&die, &hiAdr);
								unsigned size = hiAdr - loAdr;
								LLOG("\t Tag:DW_TAG_subprogram #"<<tag<<" loAdr:0x"<<Hex(loAdr)<<" size:"<<(hiAdr-loAdr)<< " Name:"<<(fnName ?: ""));
								GetNestedLocals(param, local, &die, addr);
							}
							break;
					}
				} while (dwarf_siblingof(&die, &die) == 0);
			}
		}
		off = next;
	}
	this->context.SetIP(ipOrg);
	this->context.SetBP(bpOrg);
	this->context.SetSP(spOrg);
#endif
	LLOG("===========================");
}

int CALLBACK Pdb::EnumGlobals(PSYMBOL_INFO pSym, unsigned long SymbolSize, void* UserContext)
{
	LocalsCtx& c = *(LocalsCtx *)UserContext;

	if(pSym->Tag != SymTagData)
		return TRUE;

	LLOG("GLOBAL: " << pSym->Name << " " << Format64Hex(pSym->Address));

	Val& v = c.pdb->global.GetAdd(pSym->Name);
	v.address = (adr_t)pSym->Address;
	v.reported_size = pSym->Size;
	c.pdb->TypeVal(v, pSym->TypeIndex, (adr_t)pSym->ModBase);
	return TRUE;
}

void Pdb::LoadGlobals(uint64 base)
{
#ifdef PLATFORM_WIN32

	LocalsCtx c;
	c.pdb = this;
	c.context = &context;
	SymEnumSymbols(hProcess, base, NULL, &EnumGlobals, &c);

#else

	LLOG("Pdb::LoadGlobals");
	Dwarf_Off off = 0;
	Dwarf_Off next;
	size_t hdrSz;
	// Iterate over compilation units (CU)
	while (dwarf_nextcu(dwarf, off, &next, &hdrSz, NULL, NULL, NULL) == 0) {
		Dwarf_Die cu;
		if (dwarf_offdie(dwarf, off + hdrSz, &cu) != NULL) {
			// Iterate over children DIEs
			Dwarf_Die die; // Debugging Information Entry (DIE)
			if (dwarf_child(&cu, &die) == 0) {
				do {
					int tag = dwarf_tag(&die);
					const char *fnName = dwarf_diename(&die);
					if (tag == DW_TAG_variable) {
						Dwarf_Attribute attrGlobal;
						// Check if it's external (global)
						if (dwarf_attr(&die, DW_AT_external, &attrGlobal)) {
							Pdb::Val val;
							if (GetTypeVal(&val, die)) {
								int tag = dwarf_tag(&die);
								const char *name = dwarf_diename(&die);
								global.Add(name,val);
								LLOG("Pdb::LoadGlobals - added global variable " << name);
							}
						}
					}
				} while (dwarf_siblingof(&die, &die) == 0);
			}
		}
		off = next;
	}

#endif
}

Pdb::Val Pdb::GetGlobal(const String& name)
{
	return global.Get(name, Val());
}

String Pdb::GetSymName(adr_t modbase, dword typeindex)
{
#ifdef PLATFORM_WIN32
	WCHAR *pwszTypeName;
	if(SymGetTypeInfo(hProcess, modbase, typeindex, TI_GET_SYMNAME, &pwszTypeName)) {
		WString w = pwszTypeName;
		LocalFree(pwszTypeName);
		return w.ToString();
	}
#else
	NEVER(); // Todo Dwarf implementation
#endif
	return Null;
}

dword Pdb::GetSymInfo(adr_t modbase, dword typeindex, IMAGEHLP_SYMBOL_TYPE_INFO info)
{
	dword dw = 0;
#ifdef PLATFORM_WIN32
	SymGetTypeInfo(hProcess, modbase, typeindex, info, &dw);
#else
	NEVER(); // Todo Dwarf implementation
	Dwarf_Die die;
	if (dwarf_die_addr_die (dwarf, &modbase, &die)) {
		int typeTag = dwarf_tag(&die);
		dw = typeTag;
	}
#endif
	return dw;
}

int Pdb::GetTypeIndex(adr_t modbase, dword typeindex)
{
	int q = type.Find(typeindex);
	if(q < 0) {
		q = type.GetCount();
		type.Add(typeindex).modbase = modbase;
		//LLOG("Pdb::GetTypeIndex added new type "<<type.GetCount() << " DIE Dwarf offset:"<<typeindex);
	}
	return q;
}

const Pdb::Type& Pdb::GetType(int ti)
{
	if(ti < 0 || ti >= type.GetCount())
		ThrowError("Invalid type");
	Type& t = type[ti];
#ifdef PLATFORM_WIN32

	int typeindex = type.GetKey(ti);
	if(t.size < 0) {
		t.name = GetSymName(t.modbase, typeindex);
		type_name.GetAdd(t.name) = ti;
		ULONG64 sz = 0;
		SymGetTypeInfo(hProcess, t.modbase, typeindex, TI_GET_LENGTH, &sz);
		t.size = (dword)sz;
		dword count = GetSymInfo(t.modbase, typeindex, TI_GET_CHILDRENCOUNT);
		if(count) {
			Buffer<byte> b(sizeof(TI_FINDCHILDREN_PARAMS) + sizeof(ULONG) * count);
			TI_FINDCHILDREN_PARAMS *children = (TI_FINDCHILDREN_PARAMS *) ~b;
			children->Count = count;
			children->Start = 0;
			if(SymGetTypeInfo(hProcess, t.modbase, typeindex, TI_FINDCHILDREN, children)) {
				for(dword i = 0; i < count; i++) {
					dword ch = children->ChildId[i];
					dword tag = GetSymInfo(t.modbase, ch, TI_GET_SYMTAG);
					dword kind = GetSymInfo(t.modbase, ch, TI_GET_DATAKIND);
					if(tag == SymTagUDT)
						t.member_type << GetTypeIndex(t.modbase, ch);
					else
					if(tag == SymTagData) {
						String name = GetSymName(t.modbase, ch);
						if(kind == DataIsMember) {
							Val& v = t.member.Add(name);
							TypeVal(v, GetSymInfo(t.modbase, ch, TI_GET_TYPEID), t.modbase);
							v.address = GetSymInfo(t.modbase, ch, TI_GET_OFFSET);
							ULONG64 bitcnt = 0;
							SymGetTypeInfo(hProcess, t.modbase, ch, TI_GET_LENGTH, &bitcnt);
							if(bitcnt) {
								v.bitcnt = (byte)bitcnt;
								v.bitpos = (byte)GetSymInfo(t.modbase, ch, TI_GET_BITPOSITION);
							}
						}
						if(kind == DataIsStaticMember || kind == DataIsGlobal) {
							Val& v = t.static_member.Add(name);
							TypeVal(v, GetSymInfo(t.modbase, ch, TI_GET_TYPEID), t.modbase);
							ULONG64 adr = 0;
							SymGetTypeInfo(hProcess, t.modbase, ch, TI_GET_ADDRESS, &adr);
							v.address = (adr_t)adr;
						}
					}
					else
					if(tag == SymTagBaseClass) {
						Val& v = t.base.Add();
						TypeVal(v, GetSymInfo(t.modbase, ch, TI_GET_TYPEID), t.modbase);
						v.address = GetSymInfo(t.modbase, ch, TI_GET_OFFSET);
					}
					else
					if(tag == SymTagVTable) {
						t.vtbl_offset = GetSymInfo(t.modbase, ch, TI_GET_OFFSET);
						dword typeId = GetSymInfo(t.modbase, ch, TI_GET_TYPEID);
						while(GetSymInfo(t.modbase, typeId, TI_GET_SYMTAG) == SymTagPointerType)
							typeId = GetSymInfo(t.modbase, typeId, TI_GET_TYPE);
						if((t.vtbl_typeindex = type.Find(typeId)) < 0) {
							t.vtbl_typeindex = type.GetCount();
							Type& vt = type.Add(typeId);
							vt.modbase = t.modbase;
							vt.size = 0;
							vt.vtbl_typeindex = -2;
						}
					}
				}
			}
		}
	}
	
#else

	// Dwarf implementation
	LLOG("PDB::GetType for \""<<t.name<<"\" "<<ti<<'/'<<type.GetCount());
	if(t.size < 0) {
		t.name = dwarf_diename(&t.die);
		Dwarf_Word size = -1;
		Dwarf_Attribute sizeAttr;
		if (dwarf_attr(&t.die, DW_AT_byte_size, &sizeAttr)) {
			dwarf_formudata(&sizeAttr, &size);
			t.size = size;
		}
		Dwarf_Die kid;
		if (dwarf_child(&t.die, &kid) == 0) {
			t.member.Clear();
			do {
				int tag = dwarf_tag(&kid);
				const char *name = dwarf_diename(&kid);
				switch(tag) {
					//LLOG("\t Tag:#" << tag<<" Ref:"<<dwarf_dieoffset(&kid)<<" name:"<<(name ?: ""));
					case DW_TAG_member: {
							Dwarf_Attribute locAttr;
							Dwarf_Word locOff = 0;
							if (dwarf_attr(&kid, DW_AT_data_member_location, &locAttr)) {
								dwarf_formudata(&locAttr, &locOff);
							}
							Pdb::Val val;
							if (GetTypeVal(&val, kid)) {
								val.address = locOff;
								val.rvalue = false;
								if (name && *name) {
									t.member.Add(name,val);
									LLOG("\t To '"<<t.name<<"' added new type member "<<t.member.GetCount()<<" '"<<name<<"' val:"<<val<<" ival:"<<val.ival<<" 0x:"<<Hex(val.ival));
								}
							}
						}
						break;
					case DW_TAG_inheritance: {
							Dwarf_Attribute locAttr;
							Dwarf_Word locOff = 0;
							if (dwarf_attr(&kid, DW_AT_data_member_location, &locAttr)) {
								dwarf_formudata(&locAttr, &locOff);
							}
							Pdb::Val val;
							if (GetTypeVal(&val, kid)) {
								val.address = locOff;
								val.rvalue = false;
								if (!(name && *name)) {
									Dwarf_Attribute typeAttr;
									if (dwarf_attr(&kid, DW_AT_type, &typeAttr)) {
										Dwarf_Die sub;
										if (dwarf_formref_die(&typeAttr, &sub)) {
											name = dwarf_diename(&sub);
										}
									}
								}
								if (name && *name) {
									t.member.Add(name,val);
									LLOG("\t To '"<<t.name<<"' added new inheritance type member "<<t.member.GetCount()<<" '"<<name<<"' val:"<<val<<" ival:"<<val.ival<<" 0x:"<<Hex(val.ival));
								}
							}
						}
						break;
					case DW_TAG_typedef: {
							Pdb::Val val;
							if (GetTypeVal(&val, kid)) {
								if (name && *name) {
									t.member.Add(name,val);
									LLOG("\t To '"<<t.name<<"' added new typedef type member "<<t.member.GetCount()<<" '"<<name<<"' value:"<<val);
								}
							}
						}
						break;
//					case DW_TAG_template_type_parameter: {
//							Pdb::Val val;
//							if (GetTypeVal(&val, kid)) {
//								if (name && *name) {
//									t.member.Add(name,val);
//									LLOG("\t To '"<<t.name<<"' added new template_type_parameter type member "<<t.member.GetCount()<<" '"<<name<<"' value:"<<val);
//								}
//							}
//						}
//						break;
				}
			} while (dwarf_siblingof(&kid, &kid) == 0);
		}
	}
	
#endif
	return t;
}

static int CALLBACK sSymEnum(PSYMBOL_INFO pSym, unsigned long SymbolSize, void* UserContext)
{
	auto type_index = (VectorMap<String, int> *)UserContext;
	type_index->GetAdd(pSym->Name) = pSym->TypeIndex;
	return TRUE;
}

int Pdb::FindType(adr_t modbase, const String& name)
{
	static VectorMap<String, int> primitive = {
		{ "bool", BOOL1 },
		{ "char", SINT1 },
		{ "unsigned char", UINT1 },
		{ "short", SINT2 },
		{ "unsigned short", UINT2 },
		{ "wchar_t", UINT2 },
		{ "int", SINT4 },
		{ "unsigned int", UINT4 },
		{ "long", SINT4 },
		{ "unsigned long", UINT4 },
		{ "float", FLT },
		{ "double", DBL },
		{ "int64", SINT8 },
		{ "uint64", UINT8 },
		{ "__int64", SINT8 },
		{ "unsigned __int64", UINT8 },
	};
	
	int q = primitive.Get(name, Null);
	if(!IsNull(q))
		return q;
	q = type_name.Get(name, Null);
	if(!IsNull(q))
		return q;
	if(type_bases.Find(modbase) < 0) {
		type_bases.Add(modbase);
#ifdef PLATFORM_WIN32
		SymEnumTypes(hProcess, current_modbase, sSymEnum, &type_index);
#else
	NEVER(); // Todo Dwarf implementation
#endif
		// DDUMPM(type_index);
	}
	int ndx = type_index.Get(name, Null);
	if(IsNull(ndx))
		return Null;
	return GetTypeIndex(modbase, ndx);
}

String Pdb::TypeInfoAsString(TypeInfo tf)
{
	static VectorMap<int, String> primitive = { // todo: UINT8
		{ BOOL1, "bool" },
		{ SINT1, "char" },
		{ UINT1, "unsigned char" },
		{ SINT2, "short" },
		{ UINT2, "unsigned short" },
		{ SINT4, "int" },
		{ UINT4, "unsigned int"  },
		{ SINT8, "int64" },
		{ UINT8, "uint64"  },
		{ FLT, "float" },
		{ DBL, "double" },
	};
	
	String r = primitive.Get(tf.type, Null);
	if(IsNull(r))
		r = GetType(tf.type).name;

	while(tf.ref > 0) {
		r << "*";
		tf.ref--;
	}
	
	return r;
}

Pdb::TypeInfo Pdb::GetTypeInfo(adr_t modbase, const String& name)
{
	int q = typeinfo_cache.Find(name);
	if(q >= 0)
		return typeinfo_cache[q];
	
	TypeInfo r;
	String tp = name;
	bool spc = false;
	for(;;)
		if(tp.TrimEnd("*") || tp.TrimEnd("&"))
			r.ref++;
		else
		if(!tp.TrimEnd(" ") && !tp.TrimEnd("const"))
			break;
	r.type = FindType(modbase, TrimBoth(tp));
	typeinfo_cache.Add(name, r);
	return r;
}

#ifdef _DEBUG

String Pdb::TypeAsString(int ti, bool deep)
{
	String r;
	#define sTYPE(x)     case x: return #x;
	switch(ti) {
	sTYPE(BOOL1)
	sTYPE(UINT1)
	sTYPE(SINT1)
	sTYPE(UINT2)
	sTYPE(SINT2)
	sTYPE(UINT4)
	sTYPE(SINT4)
	sTYPE(UINT8)
	sTYPE(SINT8)
	sTYPE(FLT)
	sTYPE(DBL)
	sTYPE(UNKNOWN)
	}
	if(ti < 0)
		return r;
	const Type& t = GetType(ti);
	r << t.name << "(sizeof = " << t.size << ") ";
	if(!deep)
		return r;
	if(t.member.GetCount()) {
		r << "{ ";
		for(int i = 0; i < t.member.GetCount(); i++) {
			if(i)
				r << ", ";
			r << t.member.GetKey(i) << " +" << t.member[i].address;
			if(t.member[i].ref)
				r << TypeAsString(t.member[i].type, false) << String('*', t.member[i].ref);
			else
				r << ": " << TypeAsString(t.member[i].type);
		}
		r << " }";
	}
	if(t.base.GetCount()) {
		r << " BASE: ";
		for(int i = 0; i < t.base.GetCount(); i++) {
			if(i)
				r << ", ";
			r << " +" << t.base[i].address;
			r << TypeAsString(t.base[i].type);
		}
	}
	return r;
}

#endif
