static cmVS7FlagTable cmVS10LinkFlagTable[] =
{

  //Enum Properties
  {"ShowProgress", "",
   "Not Set", "NotSet", 0},
  {"ShowProgress", "VERBOSE",
   "Display all progress messages", "LinkVerbose", 0},
  {"ShowProgress", "VERBOSE:Lib",
   "For Libraries Searched", "LinkVerboseLib", 0},
  {"ShowProgress", "VERBOSE:ICF",
   "About COMDAT folding during optimized linking", "LinkVerboseICF", 0},
  {"ShowProgress", "VERBOSE:REF",
   "About data removed during optimized linking", "LinkVerboseREF", 0},
  {"ShowProgress", "VERBOSE:SAFESEH",
   "About Modules incompatible with SEH", "LinkVerboseSAFESEH", 0},
  {"ShowProgress", "VERBOSE:CLR",
   "About linker activity related to managed code", "LinkVerboseCLR", 0},

  {"ForceFileOutput", "FORCE",
   "Enabled", "Enabled", 0},
  {"ForceFileOutput", "FORCE:MULTIPLE",
   "Multiply Defined Symbol Only", "MultiplyDefinedSymbolOnly", 0},
  {"ForceFileOutput", "FORCE:UNRESOLVED",
   "Undefined Symbol Only", "UndefinedSymbolOnly", 0},

  {"CreateHotPatchableImage", "FUNCTIONPADMIN",
   "Enabled", "Enabled", 0},
  {"CreateHotPatchableImage", "FUNCTIONPADMIN:5",
   "X86 Image Only", "X86Image", 0},
  {"CreateHotPatchableImage", "FUNCTIONPADMIN:6",
   "X64 Image Only", "X64Image", 0},
  {"CreateHotPatchableImage", "FUNCTIONPADMIN:16",
   "Itanium Image Only", "ItaniumImage", 0},

  {"UACExecutionLevel", "level='asInvoker'",
   "asInvoker", "AsInvoker", 0},
  {"UACExecutionLevel", "level='highestAvailable'",
   "highestAvailable", "HighestAvailable", 0},
  {"UACExecutionLevel", "level='requireAdministrator'",
   "requireAdministrator", "RequireAdministrator", 0},

  {"SubSystem", "",
   "Not Set", "NotSet", 0},
  {"SubSystem", "SUBSYSTEM:CONSOLE",
   "Console", "Console", 0},
  {"SubSystem", "SUBSYSTEM:WINDOWS",
   "Windows", "Windows", 0},
  {"SubSystem", "SUBSYSTEM:NATIVE",
   "Native", "Native", 0},
  {"SubSystem", "SUBSYSTEM:EFI_APPLICATION",
   "EFI Application", "EFI Application", 0},
  {"SubSystem", "SUBSYSTEM:EFI_BOOT_SERVICE_DRIVER",
   "EFI Boot Service Driver", "EFI Boot Service Driver", 0},
  {"SubSystem", "SUBSYSTEM:EFI_ROM",
   "EFI ROM", "EFI ROM", 0},
  {"SubSystem", "SUBSYSTEM:EFI_RUNTIME_DRIVER",
   "EFI Runtime", "EFI Runtime", 0},
  {"SubSystem", "SUBSYSTEM:WINDOWSCE",
   "WindowsCE", "WindowsCE", 0},
  {"SubSystem", "SUBSYSTEM:POSIX",
   "POSIX", "POSIX", 0},

  {"Driver", "",
   "Not Set", "NotSet", 0},
  {"Driver", "Driver",
   "Driver", "Driver", 0},
  {"Driver", "DRIVER:UPONLY",
   "UP Only", "UpOnly", 0},
  {"Driver", "DRIVER:WDM",
   "WDM", "WDM", 0},

  {"LinkTimeCodeGeneration", "",
   "Default", "Default", 0},
  {"LinkTimeCodeGeneration", "LTCG",
   "Use Link Time Code Generation", "UseLinkTimeCodeGeneration", 0},
  {"LinkTimeCodeGeneration", "LTCG:PGInstrument",
   "Profile Guided Optimization - Instrument", "PGInstrument", 0},
  {"LinkTimeCodeGeneration", "LTCG:PGOptimize",
   "Profile Guided Optimization - Optimization", "PGOptimization", 0},
  {"LinkTimeCodeGeneration", "LTCG:PGUpdate",
   "Profile Guided Optimization - Update", "PGUpdate", 0},

  {"TargetMachine", "",
   "Not Set", "NotSet", 0},
  {"TargetMachine", "MACHINE:ARM",
   "MachineARM", "MachineARM", 0},
  {"TargetMachine", "MACHINE:EBC",
   "MachineEBC", "MachineEBC", 0},
  {"TargetMachine", "MACHINE:IA64",
   "MachineIA64", "MachineIA64", 0},
  {"TargetMachine", "MACHINE:MIPS",
   "MachineMIPS", "MachineMIPS", 0},
  {"TargetMachine", "MACHINE:MIPS16",
   "MachineMIPS16", "MachineMIPS16", 0},
  {"TargetMachine", "MACHINE:MIPSFPU",
   "MachineMIPSFPU", "MachineMIPSFPU", 0},
  {"TargetMachine", "MACHINE:MIPSFPU16",
   "MachineMIPSFPU16", "MachineMIPSFPU16", 0},
  {"TargetMachine", "MACHINE:SH4",
   "MachineSH4", "MachineSH4", 0},
  {"TargetMachine", "MACHINE:THUMB",
   "MachineTHUMB", "MachineTHUMB", 0},
  {"TargetMachine", "MACHINE:X64",
   "MachineX64", "MachineX64", 0},
  {"TargetMachine", "MACHINE:X86",
   "MachineX86", "MachineX86", 0},

  {"CLRThreadAttribute", "CLRTHREADATTRIBUTE:MTA",
   "MTA threading attribute", "MTAThreadingAttribute", 0},
  {"CLRThreadAttribute", "CLRTHREADATTRIBUTE:STA",
   "STA threading attribute", "STAThreadingAttribute", 0},
  {"CLRThreadAttribute", "CLRTHREADATTRIBUTE:NONE",
   "Default threading attribute", "DefaultThreadingAttribute", 0},

  {"CLRImageType", "CLRIMAGETYPE:IJW",
   "Force IJW image", "ForceIJWImage", 0},
  {"CLRImageType", "CLRIMAGETYPE:PURE",
   "Force Pure IL Image", "ForcePureILImage", 0},
  {"CLRImageType", "CLRIMAGETYPE:SAFE",
   "Force Safe IL Image", "ForceSafeILImage", 0},
  {"CLRImageType", "",
   "Default image type", "Default", 0},

  {"LinkErrorReporting", "ERRORREPORT:PROMPT",
   "PromptImmediately", "PromptImmediately", 0},
  {"LinkErrorReporting", "ERRORREPORT:QUEUE",
   "Queue For Next Login", "QueueForNextLogin", 0},
  {"LinkErrorReporting", "ERRORREPORT:SEND",
   "Send Error Report", "SendErrorReport", 0},
  {"LinkErrorReporting", "ERRORREPORT:NONE",
   "No Error Report", "NoErrorReport", 0},

  {"CLRSupportLastError", "CLRSupportLastError",
   "Enabled", "Enabled", 0},
  {"CLRSupportLastError", "CLRSupportLastError:NO",
   "Disabled", "Disabled", 0},
  {"CLRSupportLastError", "CLRSupportLastError:SYSTEMDLL",
   "System Dlls Only", "SystemDlls", 0},


  //Bool Properties
  {"LinkIncremental", "INCREMENTAL:NO", "", "false", 0},
  {"LinkIncremental", "INCREMENTAL", "", "true", 0},
  {"SuppressStartupBanner", "NOLOGO", "", "true", 0},
  {"LinkStatus", "LTCG:NOSTATUS", "", "false", 0},
  {"LinkStatus", "LTCG:STATUS", "", "true", 0},
  {"PreventDllBinding", "ALLOWBIND:NO", "", "false", 0},
  {"PreventDllBinding", "ALLOWBIND", "", "true", 0},
  {"TreatLinkerWarningAsErrors", "WX:NO", "", "false", 0},
  {"TreatLinkerWarningAsErrors", "WX", "", "true", 0},
  {"IgnoreAllDefaultLibraries", "NODEFAULTLIB", "", "true", 0},
  {"GenerateManifest", "MANIFEST:NO", "", "false", 0},
  {"GenerateManifest", "MANIFEST", "", "true", 0},
  {"AllowIsolation", "ALLOWISOLATION:NO", "", "false", 0},
  {"UACUIAccess", "uiAccess='false'", "", "false", 0},
  {"UACUIAccess", "uiAccess='true'", "", "true", 0},
  {"GenerateDebugInformation", "DEBUG", "", "true", 0},
  {"MapExports", "MAPINFO:EXPORTS", "", "true", 0},
  {"AssemblyDebug", "ASSEMBLYDEBUG:DISABLE", "", "false", 0},
  {"AssemblyDebug", "ASSEMBLYDEBUG", "", "true", 0},
  {"LargeAddressAware", "LARGEADDRESSAWARE:NO", "", "false", 0},
  {"LargeAddressAware", "LARGEADDRESSAWARE", "", "true", 0},
  {"TerminalServerAware", "TSAWARE:NO", "", "false", 0},
  {"TerminalServerAware", "TSAWARE", "", "true", 0},
  {"SwapRunFromCD", "SWAPRUN:CD", "", "true", 0},
  {"SwapRunFromNET", "SWAPRUN:NET", "", "true", 0},
  {"OptimizeReferences", "OPT:NOREF", "", "false", 0},
  {"OptimizeReferences", "OPT:REF", "", "true", 0},
  {"EnableCOMDATFolding", "OPT:NOICF", "", "false", 0},
  {"EnableCOMDATFolding", "OPT:ICF", "", "true", 0},
  {"IgnoreEmbeddedIDL", "IGNOREIDL", "", "true", 0},
  {"NoEntryPoint", "NOENTRY", "", "true", 0},
  {"SetChecksum", "RELEASE", "", "true", 0},
  {"RandomizedBaseAddress", "DYNAMICBASE:NO", "", "false", 0},
  {"RandomizedBaseAddress", "DYNAMICBASE", "", "true", 0},
  {"FixedBaseAddress", "FIXED:NO", "", "false", 0},
  {"FixedBaseAddress", "FIXED", "", "true", 0},
  {"DataExecutionPrevention", "NXCOMPAT:NO", "", "false", 0},
  {"DataExecutionPrevention", "NXCOMPAT", "", "true", 0},
  {"TurnOffAssemblyGeneration", "NOASSEMBLY", "", "true", 0},
  {"SupportUnloadOfDelayLoadedDLL", "DELAY:UNLOAD", "", "true", 0},
  {"SupportNobindOfDelayLoadedDLL", "DELAY:NOBIND", "", "true", 0},
  {"Profile", "PROFILE", "", "true", 0},
  {"LinkDelaySign", "DELAYSIGN:NO", "", "false", 0},
  {"LinkDelaySign", "DELAYSIGN", "", "true", 0},
  {"CLRUnmanagedCodeCheck", "CLRUNMANAGEDCODECHECK:NO", "", "false", 0},
  {"CLRUnmanagedCodeCheck", "CLRUNMANAGEDCODECHECK", "", "true", 0},
  {"ImageHasSafeExceptionHandlers", "SAFESEH:NO", "", "false", 0},
  {"ImageHasSafeExceptionHandlers", "SAFESEH", "", "true", 0},
  {"LinkDLL", "DLL", "", "true", 0},

  //Bool Properties With Argument
  {"EnableUAC", "MANIFESTUAC:NO", "", "false",
   cmVS7FlagTable::UserValueIgnored | cmVS7FlagTable::Continue},
  {"EnableUAC", "MANIFESTUAC:NO", "Enable User Account Control (UAC)", "",
   cmVS7FlagTable::UserValueRequired},
  {"EnableUAC", "MANIFESTUAC:", "", "true",
   cmVS7FlagTable::UserValueIgnored | cmVS7FlagTable::Continue},
  {"UACUIAccess", "MANIFESTUAC:", "Enable User Account Control (UAC)", "",
   cmVS7FlagTable::UserValueRequired},
  {"GenerateMapFile", "MAP", "", "true",
   cmVS7FlagTable::UserValueIgnored | cmVS7FlagTable::Continue},
  {"MapFileName", "MAP", "Generate Map File", "",
   cmVS7FlagTable::UserValueRequired},

  //String List Properties
  {"AdditionalLibraryDirectories", "LIBPATH:",
   "Additional Library Directories",
   "", cmVS7FlagTable::UserValue | cmVS7FlagTable::SemicolonAppendable},
  // Skip [AdditionalDependencies] - no command line Switch.
  {"IgnoreSpecificDefaultLibraries", "NODEFAULTLIB:",
   "Ignore Specific Default Libraries",
   "", cmVS7FlagTable::UserValue | cmVS7FlagTable::SemicolonAppendable},
  {"AddModuleNamesToAssembly", "ASSEMBLYMODULE:",
   "Add Module to Assembly",
   "", cmVS7FlagTable::UserValue | cmVS7FlagTable::SemicolonAppendable},
  {"EmbedManagedResourceFile", "ASSEMBLYRESOURCE:",
   "Embed Managed Resource File",
   "", cmVS7FlagTable::UserValue | cmVS7FlagTable::SemicolonAppendable},
  {"ForceSymbolReferences", "INCLUDE:",
   "Force Symbol References",
   "", cmVS7FlagTable::UserValue | cmVS7FlagTable::SemicolonAppendable},
  {"DelayLoadDLLs", "DELAYLOAD:",
   "Delay Loaded Dlls",
   "", cmVS7FlagTable::UserValue | cmVS7FlagTable::SemicolonAppendable},
  {"AssemblyLinkResource", "ASSEMBLYLINKRESOURCE:",
   "Assembly Link Resource",
   "", cmVS7FlagTable::UserValue | cmVS7FlagTable::SemicolonAppendable},
  {"AdditionalManifestDependencies", "MANIFESTDEPENDENCY:",
   "Additional Manifest Dependencies",
   "", cmVS7FlagTable::UserValue | cmVS7FlagTable::SemicolonAppendable},

  //String Properties
  {"OutputFile", "OUT:",
   "Output File",
   "", cmVS7FlagTable::UserValue},
  {"Version", "VERSION:",
   "Version",
   "", cmVS7FlagTable::UserValue},
  {"SpecifySectionAttributes", "SECTION:",
   "Specify Section Attributes",
   "", cmVS7FlagTable::UserValue},
  {"MSDOSStubFileName", "STUB:",
   "MS-DOS Stub File Name",
   "", cmVS7FlagTable::UserValue},
  // Skip [TrackerLogDirectory] - no command line Switch.
  {"ModuleDefinitionFile", "DEF:",
   "Module Definition File",
   "", cmVS7FlagTable::UserValue},
  {"ManifestFile", "ManifestFile:",
   "Manifest File",
   "", cmVS7FlagTable::UserValue},
  {"ProgramDatabaseFile", "PDB:",
   "Generate Program Database File",
   "", cmVS7FlagTable::UserValue},
  {"StripPrivateSymbols", "PDBSTRIPPED:",
   "Strip Private Symbols",
   "", cmVS7FlagTable::UserValue},
  // Skip [MapFileName] - no command line Switch.
  // Skip [MinimumRequiredVersion] - no command line Switch.
  {"HeapReserveSize", "HEAP:",
   "Heap Reserve Size",
   "", cmVS7FlagTable::UserValue},
  // Skip [HeapCommitSize] - no command line Switch.
  {"StackReserveSize", "STACK:",
   "Stack Reserve Size",
   "", cmVS7FlagTable::UserValue},
  // Skip [StackCommitSize] - no command line Switch.
  {"FunctionOrder", "ORDER:@",
   "Function Order",
   "", cmVS7FlagTable::UserValue},
  {"ProfileGuidedDatabase", "PGD:",
   "Profile Guided Database",
   "", cmVS7FlagTable::UserValue},
  {"MidlCommandFile", "MIDL:@",
   "MIDL Commands",
   "", cmVS7FlagTable::UserValue},
  {"MergedIDLBaseFileName", "IDLOUT:",
   "Merged IDL Base File Name",
   "", cmVS7FlagTable::UserValue},
  {"TypeLibraryFile", "TLBOUT:",
   "Type Library",
   "", cmVS7FlagTable::UserValue},
  {"EntryPointSymbol", "ENTRY:",
   "Entry Point",
   "", cmVS7FlagTable::UserValue},
  {"BaseAddress", "BASE:",
   "Base Address",
   "", cmVS7FlagTable::UserValue},
  {"ImportLibrary", "IMPLIB:",
   "Import Library",
   "", cmVS7FlagTable::UserValue},
  {"MergeSections", "MERGE:",
   "Merge Sections",
   "", cmVS7FlagTable::UserValue},
  {"LinkKeyFile", "KEYFILE:",
   "Key File",
   "", cmVS7FlagTable::UserValue},
  {"KeyContainer", "KEYCONTAINER:",
   "Key Container",
   "", cmVS7FlagTable::UserValue},
  // Skip [AdditionalOptions] - no command line Switch.
  {0,0,0,0,0}
};
