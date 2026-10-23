// CPU-only fixture. methods.inc contains verbatim production method bodies.
// VBO below is an instrumented byte store, not an OpenGL or Metal execution.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/NativeBufferData.h"
#endif
using GLuint = unsigned;
using GLenum = unsigned;
using GLint = int;
using GLsizei = int;
using GLubyte = unsigned char;
using GLfloat = float;
using lua_Number = double;
constexpr GLenum GL_BYTE = 0x1400, GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_SHORT = 0x1402, GL_UNSIGNED_SHORT = 0x1403;
constexpr GLenum GL_INT = 0x1404, GL_UNSIGNED_INT = 0x1405, GL_FLOAT = 0x1406;
constexpr GLenum GL_FLOAT_VEC4 = 0x8B52, GL_INT_VEC4 = 0x8B55;
constexpr GLenum GL_UNSIGNED_INT_VEC4 = 0x8DC8, GL_FLOAT_MAT4 = 0x8B5C;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892, GL_UNIFORM_BUFFER = 0x8A11;
constexpr GLenum GL_SHADER_STORAGE_BUFFER = 0x90D2, GL_MAP_READ_BIT = 1;
#ifdef RECOIL_METAL4_LIVE_SCENE
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893, GL_R8UI = 0x8232, GL_RED_INTEGER = 0x8D94;
#endif
void glClearBufferData(GLenum, GLenum, GLenum, GLenum, const void*) {}
struct BindingLimits { int glslMaxUniformBufferBindings = 8, glslMaxStorageBufferBindings = 8; } bindingLimits;
auto* globalRendering = &bindingLimits;
#define LOG_L(...) ((void)0)

uint32_t AlignUp(uint32_t value, uint32_t alignment) { return (value + alignment - 1) / alignment * alignment; }
// Test values are representable in their destination types. Conversion policy
// is unchanged production SafeCast code and is outside this address oracle.
namespace spring { template<class Out, class In> Out SafeCast(In value) { return static_cast<Out>(value); } }
namespace LuaUtils {
template<class... Args> void SolLuaError(const char*, Args...) { throw std::runtime_error("Lua error"); }
}
namespace sol {
template<class T> struct optional {
    bool present = false;
    T value{};
    optional() = default;
    optional(T v): present(true), value(v) {}
    T value_or(T fallback) const { return present ? value : fallback; }
};
template<class T> struct as_table_t { T values; };
template<class T> as_table_t<T> as_table(T values) { return {std::move(values)}; }
}

struct VBO {
    std::vector<uint8_t> bytes;
    size_t logicalSize = 0, lastOffset = 0, lastSize = 0;
    unsigned binds = 0, unbinds = 0, uploads = 0, maps = 0, unmaps = 0;
    bool failMap = false, failBind = false;
    unsigned rangeBindings = 0; size_t rangeOffset = 0, rangeSize = 0;
    size_t GetSize() const { return logicalSize; }
    void Bind() { ++binds; }
    void Unbind() { ++unbinds; }
    void SetBufferSubData(size_t offset, size_t size, const void* source) {
        if (offset > logicalSize || size > logicalSize - offset) throw std::runtime_error("fixture upload overrun");
        ++uploads; lastOffset = offset; lastSize = size;
        std::memcpy(bytes.data() + offset, source, size);
    }
    GLubyte* MapBuffer(size_t offset, size_t size, GLenum) {
        if (offset > logicalSize || size > logicalSize - offset) throw std::runtime_error("fixture map overrun");
        ++maps;
        return failMap ? nullptr : bytes.data() + offset;
    }
    void UnmapBuffer() { ++unmaps; }
    bool BindBufferRange(GLenum, GLuint, size_t offset, size_t size) {
        ++rangeBindings; rangeOffset = offset; rangeSize = size; return !failBind;
    }
    bool UnbindBufferRange(GLenum target, GLuint slot, size_t offset, size_t size) { return BindBufferRange(target, slot, offset, size); }
};
void VBOExistenceCheck(const VBO* vbo, const char*) { if (!vbo) throw std::runtime_error("missing VBO"); }

#ifdef LAYOUT_PREFLIGHT
using ByteCount = size_t;
#else
using ByteCount = int;
#endif
class LuaVBOImpl {
public:
    struct BufferAttribDef {
        GLenum type;
        GLint size;
        int normalized;
        std::string name;
        GLsizei pointer, typeSizeInBytes, strideSizeInBytes;
    };
    GLenum defTarget = GL_ARRAY_BUFFER;
    uint32_t elementsCount = 0, elemSizeInBytes = 0, bufferSizeInBytes = 0;
    VBO* vbo = nullptr;
    void* bufferData = nullptr;
#ifdef RECOIL_METAL4_LIVE_SCENE
    bool vboOwner = true;
    rendering::NativeBufferSnapshotSource nativeSnapshotSource;
    rendering::NativeBufferLayout MakeNativeLayout() const;
    rendering::NativeBufferSnapshotRef CaptureNativeSnapshot() const;
    rendering::NativeBufferRange CaptureNativeRange(uint32_t, size_t, size_t, uint32_t, size_t = 1) const;
    void Clear();
    int BindBufferRangeImpl(GLuint, sol::optional<int>, sol::optional<int>, sol::optional<GLenum>, bool);
    static constexpr uint32_t uboMinIndex = 6, ssboMinIndex = 4;
#endif
    std::vector<std::pair<const int, const BufferAttribDef>> bufferAttribDefsVec;
    std::map<const int, BufferAttribDef> bufferAttribDefs;
    void GetTypePtr(GLenum, GLint, uint32_t&, uint32_t&, GLsizei&, GLsizei&);
    static bool GetAttributeScalarCount(const BufferAttribDef&, uint32_t, uint32_t&);
    static bool IsElementByteRangeValid(uint32_t, size_t, uint32_t, uint32_t);
    sol::as_table_t<std::vector<lua_Number>> Download(sol::optional<int>, sol::optional<int>, sol::optional<int>, sol::optional<bool>);
    template<class In, class Predicate> size_t UploadImpl(const std::vector<In>&, uint32_t, Predicate);
    template<class In> size_t UploadImpl(const std::vector<In>& values, uint32_t offset, int attribute) {
        return UploadImpl(values, offset, [attribute](int id) { return attribute == -1 || attribute == id; });
    }
    template<class In, class Out, class Iterator>
    bool TransformAndWrite(ByteCount&, GLubyte*&, ByteCount, int, Iterator&, const Iterator&, bool);
    template<class In> bool TransformAndRead(ByteCount&, GLubyte*&, ByteCount, int, std::vector<lua_Number>&, bool);
};
#include "methods.inc"

unsigned checks = 0, failures = 0, cases = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << message << '\n'; }
}
template<class T> void store(std::vector<uint8_t>& bytes, size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
struct Fixture {
    LuaVBOImpl impl;
    VBO vbo;
    std::vector<uint8_t> shadow;
    Fixture(uint32_t count, std::initializer_list<std::pair<GLenum, int>> attributes, GLenum target = GL_ARRAY_BUFFER) {
        ++cases; impl.defTarget = target; impl.elementsCount = count; impl.vbo = &vbo;
        uint32_t next = 0; int id = 0;
        for (auto [type, size] : attributes) {
            uint32_t offset = 0; GLsizei alignment = 0, bytes = 0;
            impl.GetTypePtr(type, size, offset, next, alignment, bytes);
            LuaVBOImpl::BufferAttribDef attr{type, size, 0, "", int(offset), bytes / size, bytes};
            impl.bufferAttribDefs.emplace(id, attr); impl.bufferAttribDefsVec.emplace_back(id++, attr);
        }
        impl.elemSizeInBytes = next; impl.bufferSizeInBytes = next * count;
        shadow.assign(impl.bufferSizeInBytes + 16, 0xC7);
        vbo.bytes = shadow; vbo.logicalSize = impl.bufferSizeInBytes; impl.bufferData = shadow.data();
    }
    void sameImage(const std::vector<uint8_t>& expected) {
        check(shadow == expected, "independent complete shadow byte image");
        check(vbo.bytes == expected, "instrumented upload byte image");
    }
};

void addressOracles() {
    Fixture mixed(3, {{GL_UNSIGNED_BYTE, 1}, {GL_FLOAT, 1}});
    check(mixed.impl.bufferAttribDefs.at(0).pointer == 0, "UBYTE declared offset0");
    check(mixed.impl.bufferAttribDefs.at(1).pointer == 4, "FLOAT declared offset4");
    check(mixed.impl.elemSizeInBytes == 8, "declared stride8");
    auto expected = mixed.shadow;
    expected[0] = 7; store(expected, 4, 1.25f);
    expected[8] = 9; store(expected, 12, -2.5f);
    expected[16] = 11; store(expected, 20, 3.75f);
    check(mixed.impl.UploadImpl(std::vector<double>{7, 1.25, 9, -2.5, 11, 3.75}, 0, -1) == 24, "three declared records upload24bytes");
    mixed.sameImage(expected);
    check(mixed.vbo.lastOffset == 0 && mixed.vbo.lastSize == 24, "upload covers declared record range");

    store(expected, 12, 6.5f);
    check(mixed.impl.UploadImpl(std::vector<float>{6.5f}, 1, 1) == 8, "middle float-only update covers one record");
    mixed.sameImage(expected);
    check(mixed.vbo.lastOffset == 8 && mixed.vbo.lastSize == 8, "middle record upload offset8");

    // Download starts from independently seeded bytes, never from Upload.
    Fixture seeded(3, {{GL_UNSIGNED_BYTE, 1}, {GL_FLOAT, 1}});
    seeded.shadow[0] = 21; store(seeded.shadow, 4, 2.25f);
    seeded.shadow[8] = 22; store(seeded.shadow, 12, -3.5f);
    seeded.shadow[16] = 23; store(seeded.shadow, 20, 4.75f);
    check(seeded.impl.Download(-1, 0, 3, false).values == std::vector<double>({21, 2.25, 22, -3.5, 23, 4.75}), "independent shadow download stride8");
    check(seeded.impl.Download(1, 1, 1, false).values == std::vector<double>({-3.5}), "independent selected middle float download");
    seeded.vbo.bytes[0] = 31; store(seeded.vbo.bytes, 4, 8.25f);
    seeded.vbo.bytes[8] = 32; store(seeded.vbo.bytes, 12, 9.5f);
    seeded.vbo.bytes[16] = 33; store(seeded.vbo.bytes, 20, 10.75f);
    check(seeded.impl.Download(-1, 0, 3, true).values == std::vector<double>({31, 8.25, 32, 9.5, 33, 10.75}), "independent mapped byte download stride8");
    check(seeded.vbo.maps == 1 && seeded.vbo.unmaps == 1 && seeded.vbo.binds == seeded.vbo.unbinds, "map lifecycle balanced");

    Fixture odd(3, {{GL_FLOAT, 1}, {GL_UNSIGNED_BYTE, 1}});
    check(odd.impl.elemSizeInBytes == 5 && odd.impl.bufferAttribDefs.at(1).pointer == 4, "legal odd stride remains5");
    expected = odd.shadow;
    store(expected, 0, 1.5f); expected[4] = 41;
    store(expected, 5, 2.5f); expected[9] = 42;
    store(expected, 10, 3.5f); expected[14] = 43;
    check(odd.impl.UploadImpl(std::vector<double>{1.5, 41, 2.5, 42, 3.5, 43}, 0, -1) == 15, "unaligned next records retain odd stride");
    odd.sameImage(expected);
    store(expected, 5, -4.5f); store(expected, 10, -5.5f);
    check(odd.impl.UploadImpl(std::vector<float>{-4.5f, -5.5f}, 1, 0) == 10, "odd stride selected update");
    odd.sameImage(expected);
    Fixture oddSeed(2, {{GL_FLOAT, 1}, {GL_UNSIGNED_BYTE, 1}});
    store(oddSeed.shadow, 0, 7.25f); oddSeed.shadow[4] = 51;
    store(oddSeed.shadow, 5, -8.5f); oddSeed.shadow[9] = 52;
    check(oddSeed.impl.Download(-1, 0, 2, false).values == std::vector<double>({7.25, 51, -8.5, 52}), "independent odd stride seed download");
}

#ifdef LAYOUT_PREFLIGHT
void rejectedUpload(Fixture& fixture, const std::vector<double>& values, uint32_t offset, int attribute) {
    const auto shadow = fixture.shadow, gpu = fixture.vbo.bytes;
    const auto uploads = fixture.vbo.uploads, binds = fixture.vbo.binds;
    check(fixture.impl.UploadImpl(values, offset, attribute) == 0, "invalid upload rejected");
    check(fixture.shadow == shadow && fixture.vbo.bytes == gpu, "invalid request changes no prefix bytes");
    check(fixture.vbo.uploads == uploads && fixture.vbo.binds == binds, "invalid request never submitted");
}
template<class Function> void throwsBeforeMap(Fixture& fixture, Function call) {
    const auto shadow = fixture.shadow, gpu = fixture.vbo.bytes;
    const auto maps = fixture.vbo.maps;
    bool threw = false;
    try { call(); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "invalid download rejected");
    check(fixture.vbo.maps == maps && fixture.shadow == shadow && fixture.vbo.bytes == gpu, "invalid download maps or modifies nothing");
}
void preflightOracles() {
    Fixture fixture(3, {{GL_UNSIGNED_BYTE, 1}, {GL_FLOAT, 2}});
    rejectedUpload(fixture, {1, 2, 3, 4}, 0, -1); // complete first record plus incomplete next one
    rejectedUpload(fixture, {1}, 0, 1); // incomplete float2 attribute
    rejectedUpload(fixture, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, 0, -1);
    rejectedUpload(fixture, {1, 2, 3, 4, 5, 6, 7, 8, 9}, 1, -1);
    rejectedUpload(fixture, {1, 2, 3}, 3, -1);
    rejectedUpload(fixture, {1, 2, 3}, std::numeric_limits<uint32_t>::max(), -1);
    rejectedUpload(fixture, {1, 2, 3}, 0, 999);
    check(fixture.impl.UploadImpl(std::vector<double>{}, 3, -1) == 0, "empty upload stays no-op");
    fixture.impl.bufferSizeInBytes -= 1;
    rejectedUpload(fixture, {1, 2, 3}, 2, -1);
    fixture.impl.bufferSizeInBytes += 1;
    fixture.vbo.logicalSize -= 1;
    rejectedUpload(fixture, {1, 2, 3}, 2, -1);
    fixture.vbo.logicalSize += 1;

    Fixture malformed(2, {{GL_UNSIGNED_BYTE, 1}, {GL_FLOAT, 1}});
    auto attr = malformed.impl.bufferAttribDefsVec.at(1).second;
    malformed.impl.bufferAttribDefsVec.clear(); attr.pointer = 7;
    malformed.impl.bufferAttribDefsVec.emplace_back(1, attr);
    rejectedUpload(malformed, {1}, 0, 1);
    throwsBeforeMap(malformed, [&] { malformed.impl.Download(1, 0, 1, true); });
    Fixture download(2, {{GL_UNSIGNED_BYTE, 1}, {GL_FLOAT, 1}});
    throwsBeforeMap(download, [&] { download.impl.Download(-1, 2, 1, true); });
    throwsBeforeMap(download, [&] { download.impl.Download(-1, 1, 2, true); });
    throwsBeforeMap(download, [&] { download.impl.Download(999, 0, 1, true); });
    download.impl.bufferSizeInBytes -= 1;
    throwsBeforeMap(download, [&] { download.impl.Download(-1, 1, 1, true); });
    download.impl.bufferSizeInBytes += 1;
    download.impl.bufferData = nullptr;
    throwsBeforeMap(download, [&] { download.impl.Download(-1, 0, 1, false); });
    download.vbo.failMap = true;
    bool threw = false;
    try { download.impl.Download(-1, 0, 1, true); } catch (const std::runtime_error&) { threw = true; }
    check(threw && download.vbo.binds == download.vbo.unbinds, "failed mapping releases binding");

    Fixture uniform(2, {{GL_FLOAT_VEC4, 1}, {GL_FLOAT_MAT4, 1}}, GL_UNIFORM_BUFFER);
    check(uniform.impl.elemSizeInBytes == 80 && uniform.impl.bufferAttribDefs.at(1).pointer == 16, "existing uniform layout unchanged");
    std::vector<double> values(40);
    for (size_t i = 0; i < values.size(); ++i) values[i] = double(i + 1);
    auto expected = uniform.shadow;
    for (size_t i = 0; i < values.size(); ++i) store(expected, i * 4, float(i + 1));
    check(uniform.impl.UploadImpl(values, 0, -1) == 160, "uniform basic-scalar count retained");
    uniform.sameImage(expected);

    Fixture shortTypes(2, {{GL_SHORT, 1}, {GL_UNSIGNED_INT, 1}});
    expected = shortTypes.shadow;
    store(expected, 0, int16_t(-123)); store(expected, 4, uint32_t(70000));
    store(expected, 8, int16_t(234)); store(expected, 12, uint32_t(80000));
    check(shortTypes.impl.UploadImpl(std::vector<double>{-123, 70000, 234, 80000}, 0, -1) == 16, "short integer alignment retained");
    shortTypes.sameImage(expected);

    std::vector<uint8_t> destination(16, 0xC7), original = destination;
    auto* pointer = destination.data(); auto* originalPointer = pointer;
    ByteCount written = 0;
    const std::vector<double> incomplete{1.25}; auto iterator = incomplete.cbegin();
    check(!uniform.impl.TransformAndWrite<double, float>(written, pointer, destination.size(), 2, iterator, incomplete.cend(), true), "individual partial attribute rejects");
    check(destination == original && pointer == originalPointer && written == 0 && iterator == incomplete.cbegin(), "partial attribute leaves cursor and bytes unchanged");
    check(!LuaVBOImpl::IsElementByteRangeValid(std::numeric_limits<uint32_t>::max(), 1, 8, 24), "offset overflow rejected");
    check(!LuaVBOImpl::IsElementByteRangeValid(0, std::numeric_limits<size_t>::max(), 8, 24), "count overflow rejected");
    check(!LuaVBOImpl::IsElementByteRangeValid(0, 1, 0, 24), "zero stride rejected");
}
#endif

#ifdef RECOIL_METAL4_LIVE_SCENE
void nativeSnapshotOracles() {
    Fixture mixed(3, {{GL_UNSIGNED_BYTE, 1}, {GL_FLOAT, 1}});
    mixed.impl.nativeSnapshotSource.Define(mixed.impl.MakeNativeLayout());
    const auto initial = mixed.impl.CaptureNativeSnapshot();
    check(initial->layout.stride == 8 && initial->layout.attributes[1].offset == 4, "actual producer declared typed layout captured");
    auto expected = initial->bytes;
    expected[0] = 7; store(expected, 4, 1.25f); expected[8] = 9; store(expected, 12, -2.5f); expected[16] = 11; store(expected, 20, 3.75f);
    check(mixed.impl.UploadImpl(std::vector<double>{7,1.25,9,-2.5,11,3.75},0,-1) == 24, "actual producer mutation accepted");
    const auto updated = mixed.impl.CaptureNativeSnapshot();
    check(updated->bytes == expected && initial->bytes != expected, "actual Upload retains old immutable generation");
    check(updated->revision == initial->revision+1 && updated->object == initial->object, "actual Upload advances revision once");
    check(mixed.impl.UploadImpl(std::vector<double>{9,1.5,8},0,-1) == 0, "actual incomplete upload rejects");
    check(mixed.impl.CaptureNativeSnapshot() == updated, "actual rejected upload preserves generation");
    mixed.impl.Clear();
    const auto cleared = mixed.impl.CaptureNativeSnapshot();
    check(std::all_of(cleared->bytes.begin(),cleared->bytes.end(),[](auto x){return x==0;}), "actual Clear updates native CPU shadow");
    check(cleared->revision == updated->revision+1 && updated->bytes == expected, "actual Clear retains previous generation and advances revision");
    check(mixed.shadow.back() == 0xC7, "actual Clear leaves backing guard untouched");
    mixed.impl.vboOwner = false; bool rejected = false;
    try { mixed.impl.CaptureNativeSnapshot(); } catch (...) { rejected = true; }
    check(rejected, "borrowed engine GPU-only buffer is not a successful native snapshot");

    Fixture storage(8, {{GL_FLOAT_VEC4,4}}, GL_SHADER_STORAGE_BUFFER);
    storage.impl.nativeSnapshotSource.Define(storage.impl.MakeNativeLayout());
    check(storage.impl.elemSizeInBytes == 64 && storage.impl.bufferSizeInBytes == 512, "actual GetTypePtr vec4 size4 retains64B records");
    std::vector<double> values(32); for(size_t i=0;i<values.size();++i) values[i]=i+1;
    auto rawExpected = storage.shadow; for(size_t i=0;i<values.size();++i) store(rawExpected,i*4,float(i+1));
    check(storage.impl.UploadImpl(values,0,-1) == 128, "actual padded polygon stream uploads two64B records");
    const auto ss = storage.impl.CaptureNativeSnapshot();
    check(ss->bytes == std::vector<uint8_t>(rawExpected.begin(),rawExpected.begin()+512), "actual SSBO prefix and unused capacity match independent bytes");
    check(ss->layout.attributes[0].declaredCount==4 && ss->layout.attributes[0].byteSize==64, "actual SSBO declaration metadata retained");
    auto range=storage.impl.CaptureNativeRange(4,2,3,8,64);
    check(range.offset==128 && range.size==192 && range.snapshot==ss, "actual native range retains requested logical interval");
    rejected=false; try { storage.impl.CaptureNativeRange(3,0,1,8); } catch (...) {rejected=true;}
    check(rejected && storage.impl.CaptureNativeSnapshot()==ss, "invalid native slot leaves generation unchanged");
    check(storage.impl.BindBufferRangeImpl(4,2,3,{},true)==4 && storage.vbo.rangeOffset==128 && storage.vbo.rangeSize==384, "legacy aligned GPU-tail binding extent preserved");
    const auto oldTarget=storage.impl.defTarget; const auto oldBinds=storage.vbo.rangeBindings;
    rejected=false; try { storage.impl.BindBufferRangeImpl(0,0,1,GL_UNIFORM_BUFFER,true); } catch(...) {rejected=true;}
    check(rejected && storage.impl.defTarget==oldTarget && storage.vbo.rangeBindings==oldBinds, "invalid binding validates before target mutation/backend call");
    storage.vbo.failBind=true; rejected=false;
    try {storage.impl.BindBufferRangeImpl(6,0,1,GL_UNIFORM_BUFFER,true);} catch(...) {rejected=true;}
    check(rejected && storage.impl.defTarget==oldTarget, "failed backend binding preserves target");
    storage.vbo.failBind=false; rejected=false;
    try {storage.impl.BindBufferRangeImpl(4,7,2,{},true);} catch(...) {rejected=true;}
    check(rejected && storage.impl.CaptureNativeSnapshot()==ss, "excess legacy element range leaves contents/generation unchanged");
}
#endif

int main() {
    addressOracles();
#ifdef LAYOUT_PREFLIGHT
    preflightOracles();
#endif
#ifdef RECOIL_METAL4_LIVE_SCENE
    nativeSnapshotOracles();
#endif
    std::cout << "cases=" << cases << " checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
