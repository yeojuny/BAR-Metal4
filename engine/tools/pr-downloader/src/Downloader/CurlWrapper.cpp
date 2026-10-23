/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include <array>
#include <cstdint>
#include <cstring>
#include <curl/curl.h>

#include "CurlWrapper.h"
#include "FileSystem/File.h"
#include "FileSystem/FileSystem.h"
#include "IDownloader.h"
#include "Logger.h"
#include "Util.h"
#include "Version.h"

#ifndef CURL_VERSION_BITS
#define CURL_VERSION_BITS(x, y, z) ((x) << 16 | (y) << 8 | (z))
#endif

#ifndef CURL_AT_LEAST_VERSION
#define CURL_AT_LEAST_VERSION(x, y, z) (LIBCURL_VERSION_NUM >= CURL_VERSION_BITS(x, y, z))
#endif

static bool verify_certificate = true;
static std::optional<std::string> certDir = std::nullopt;
static std::optional<std::string> certFile = std::nullopt;

static void DumpVersion()
{
	const curl_version_info_data* ver = curl_version_info(CURLVERSION_NOW);
	if ((ver != nullptr) && (ver->age > 0)) {
		LOG_INFO("libcurl %s %s", ver->version, ver->ssl_version);
	}
}

static void ConfigureCertificates()
{
	certFile = getEnvVar("PRD_SSL_CERT_FILE");
	certDir = getEnvVar("PRD_SSL_CERT_DIR");

#if defined(__linux__)
	// This code is needed because curl library can be statically linked and then
	// the default, determined during build, certificate location can be
	// incorrect, see https://curl.se/mail/lib-2022-05/0038.html for more details.
	// To resolve this, we go over known paths in different linux distributions
	// to identify certificate file location.

	// Paths from https://go.dev/src/crypto/x509/root_linux.go
	std::array<const char*, 6> certFiles = {
		"/etc/ssl/certs/ca-certificates.crt",                 // Debian/Ubuntu/Gentoo etc.
		"/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora/RHEL 6
		"/etc/ssl/ca-bundle.pem",                             // OpenSUSE
		"/etc/pki/tls/cacert.pem",                            // OpenELEC
		"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7
		"/etc/ssl/cert.pem",                                  // Alpine Linux
	};
	for (auto cf = certFiles.begin(); !certFile && cf != certFiles.end(); ++cf) {
		if (fileSystem->fileExists(*cf)) {
			certFile = *cf;
		}
	}
	if (!certFile) {
		LOG_WARN("Not found cert file in any of known locations, download will likely fail");
	}

	std::array<const char*, 2> certDirs = {
		"/etc/ssl/certs",      // SLES10/SLES11, https://golang.org/issue/12139
		"/etc/pki/tls/certs",  // Fedora/RHEL
	};
	for (auto cd = certDirs.begin(); !certDir && cd != certDirs.end(); ++cd) {
		if (fileSystem->directoryExists(*cd)) {
			certDir = *cd;
		}
	}
	if (!certDir) {
		LOG_WARN("Not found cert dir in any of known locations, download will likely fail");
	}
#endif

	LOG_INFO("CURLOPT_CAINFO is %s (can be overriden by PRD_SSL_CERT_FILE env variable)",
	         !certFile ? "nullptr" : certFile.value().c_str());
	LOG_INFO("CURLOPT_CAPATH is %s (can be overriden by PRD_SSL_CERT_DIR env variable)",
	         !certDir ? "nullptr" : certDir.value().c_str());
}

static void SetCAOptions(CURL* handle)
{
#ifdef _WIN32
	if (!certFile && !certDir) {
		// CURLSSLOPT_NATIVE_CA was added in curl 7.71.0
		curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
	}
#endif

	if (certFile) {
		const int res = curl_easy_setopt(handle, CURLOPT_CAINFO, certFile.value().c_str());
		if (res != CURLE_OK) {
			LOG_WARN("Error setting CURLOPT_CAINFO to %s: %d", certFile.value().c_str(), res);
		}
	}
	if (certDir) {
		const int res = curl_easy_setopt(handle, CURLOPT_CAPATH, certDir.value().c_str());
		if (res != CURLE_OK) {
			LOG_WARN("Error setting CURLOPT_CAPATH to %s: %d", certDir.value().c_str(), res);
		}
	}
}

CurlWrapper::CurlWrapper()
{
	handle = curl_easy_init();
	errbuf = (char*)malloc(sizeof(char) * CURL_ERROR_SIZE);
	errbuf[0] = 0;
	curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errbuf);

	SetCAOptions(handle);

	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 30);

	// if transfer is slower this bytes/s than this for CURLOPT_LOW_SPEED_TIME
	// then its aborted
	curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 10);
	curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, 30);
	curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
	curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
	curl_easy_setopt(handle, CURLOPT_USERAGENT, getAgent());
	curl_easy_setopt(handle, CURLOPT_FAILONERROR, true);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, verify_certificate ? 1 : 0);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, verify_certificate ? 2 : 0);
}

CurlWrapper::~CurlWrapper()
{
	curl_slist_free_all(list); /* free the list again */
	curl_easy_cleanup(handle);
	handle = nullptr;
	list = nullptr;
	free(errbuf);
	errbuf = nullptr;
}

void CurlWrapper::AddHeader(const std::string& header)
{
	list = curl_slist_append(list, header.c_str());
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, list);
}

std::string CurlWrapper::EscapeUrl(const std::string& url)
{
// Just in case we compile with some old version of curl.
#if CURL_AT_LEAST_VERSION(7, 82, 0)
	CURL* handle = nullptr;
#else
	CURL* handle = curl_easy_init();
#endif

	char* s = curl_easy_escape(handle, url.c_str(), url.size());
	std::string out(s);
	curl_free(s);

#if !CURL_AT_LEAST_VERSION(7, 82, 0)
	curl_easy_cleanup(handle);
#endif

	return out;
}

// We want to reuse curl multi handle to reuse open connections across transfers.
CURLM* global_curlm_handle = nullptr;

CURLM* CurlWrapper::GetMultiHandle()
{
	return global_curlm_handle;
}

void CurlWrapper::InitCurl()
{
	DumpVersion();
	ConfigureCertificates();
	curl_global_init(CURL_GLOBAL_ALL);
	const char* cert_check_env = std::getenv("PRD_DISABLE_CERT_CHECK");
	if (cert_check_env != nullptr && std::string(cert_check_env) == "true") {
		verify_certificate = false;
	}
	global_curlm_handle = curl_multi_init();
	curl_multi_setopt(global_curlm_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
	curl_multi_setopt(global_curlm_handle, CURLMOPT_MAX_HOST_CONNECTIONS, 5);
}

void CurlWrapper::KillCurl()
{
	curl_multi_cleanup(global_curlm_handle);
	curl_global_cleanup();
}

std::string CurlWrapper::GetError() const
{
	if (errbuf == nullptr)
		return "";
	return std::string(errbuf, strlen(errbuf));
}
