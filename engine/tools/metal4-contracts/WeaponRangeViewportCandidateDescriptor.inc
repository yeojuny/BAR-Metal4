MTL4LibraryDescriptor* RangeLibraryDescriptor(){
 auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Exact31657 original Attack/Defense range coverage and composite";const char* division233=std::getenv("BAR_NATIVE_RANGE_DIVISION233");
 const bool useDivision233=division233&&std::strcmp(division233,"1")==0;
 const char* exactInterior=std::getenv("BAR_NATIVE_WEAPON_RANGE_EXACT_INTERIOR");
 const bool useExactInterior=!useDivision233&&exactInterior&&std::strcmp(exactInterior,"1")==0;
 std::string text=std::string("#define RECOIL_RANGE_DIVISION233 ")+(useDivision233?"1\n":"0\n")+(useExactInterior?"#define RECOIL_WEAPON_RANGE_EXACT_INTERIOR 1\n":"")+NativeWeaponRangeShaderSource;
 descriptor.source=String(text);auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;return descriptor;
}
