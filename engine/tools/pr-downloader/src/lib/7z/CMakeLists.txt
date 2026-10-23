add_library(7zip
    7zAlloc.c
    7zArcIn.c
    7zBuf.c
    7zBuf2.c
    7zCrc.c
    7zCrcOpt.c
    7zDec.c
    7zFile.c
    7zStream.c
    Aes.c
    AesOpt.c
    Alloc.c
    Bcj2.c
    Bcj2Enc.c
    Bra.c
    Bra86.c
    BraIA64.c
    CpuArch.c
    Delta.c
    DllSecur.c
    LzFind.c
    LzFindMt.c
    LzFindOpt.c
    Lzma2Dec.c
    Lzma2DecMt.c
    Lzma2Enc.c
    Lzma86Dec.c
    Lzma86Enc.c
    LzmaDec.c
    LzmaEnc.c
    LzmaLib.c
    MtCoder.c
    MtDec.c
    Ppmd7.c
    Ppmd7Dec.c
    Ppmd7Enc.c
    Sha256.c
    Sha256Opt.c
    Sort.c
    Threads.c
    Xz.c
    XzCrc64.c
    XzCrc64Opt.c
    XzDec.c
    XzEnc.c
    XzIn.c
)

if(WIN32)
    target_compile_definitions(7zip 
        PRIVATE -D_7ZIP_LARGE_PAGES -DUNICODE -D_UNICODE)
endif ()

target_include_directories(7zip
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

set_target_properties(7zip PROPERTIES POSITION_INDEPENDENT_CODE ON)

# GCC -Wstringop-overflow can't prove the bound on Bcj2Enc.c's temp[8] copy loop
# and reports a bogus overflow; silence it on this vendored 7-Zip code.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(7zip PRIVATE -Wno-stringop-overflow)
endif()
