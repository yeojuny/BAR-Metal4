if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    if(MSVC)
        add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    endif()
endif()

add_library(pr-downloader
    Downloader/CurlWrapper.cpp
    Downloader/Download.cpp
    Downloader/DownloadEnum.cpp
    Downloader/Http/DownloadData.cpp
    Downloader/Http/ETag.cpp
    Downloader/Http/HttpDownloader.cpp
    Downloader/Http/IOThreadPool.cpp
    Downloader/Http/Throttler.cpp
    Downloader/IDownloader.cpp
    Downloader/Rapid/RapidDownloader.cpp
    Downloader/Rapid/Repo.cpp
    Downloader/Rapid/Sdp.cpp
    FileSystem/File.cpp
    FileSystem/FileSystem.cpp
    FileSystem/HashGzip.cpp
    FileSystem/HashMD5.cpp
    FileSystem/IHash.cpp
    FileSystem/SevenZipArchive.cpp
    FileSystem/ZipArchive.cpp
    Logger.cpp
    Tracer.cpp
    Util.cpp
    Version.cpp
    pr-downloader.cpp
)

target_include_directories(pr-downloader
    PUBLIC
        ${pr-downloader_SOURCE_DIR}/src
)

set_source_files_properties(Version.cpp PROPERTIES COMPILE_DEFINITIONS "PR_DOWNLOADER_VERSION=${PR_DOWNLOADER_VERSION}")

target_link_libraries(pr-downloader
    PUBLIC
        prd::jsoncpp
        prd::libcurl
        ZLIB::ZLIB
        prd::minizip
        pr-base64
        pr-md5
        pr-sha1
        7zip
        readerwriterqueue
        Threads::Threads
)

add_executable(pr-downloader_cli main.cpp)
target_link_libraries(pr-downloader_cli pr-downloader)
set_target_properties(pr-downloader_cli PROPERTIES OUTPUT_NAME pr-downloader)

install(TARGETS pr-downloader_cli
        RUNTIME DESTINATION ${PRD_BINDIR})
