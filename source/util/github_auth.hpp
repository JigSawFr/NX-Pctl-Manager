// github_auth — signs PlayGuard in to GitHub with the device flow, for the
// developer tools' development builds (dev_builds.hpp: GitHub hands Actions
// artifacts to signed-in users only) and the reports sent as gists.
//
// PlayGuard shows a short code and github.com/login/device (a QR code); the
// developer enters the code on a phone and authorises the "PlayGuard dev
// builds" GitHub App; PlayGuard polls until GitHub hands it a token. The app
// asks for two permissions only: Actions (read), as GitHub hands artifacts
// only to a token allowed to read Actions, and Gists (write), so that "Send a
// report online" can make a secret gist (log_upload.hpp); otherwise the token
// can only read what is public. It is revoked from GitHub (Settings ›
// Applications › Authorized GitHub Apps) or by Disconnect here, which forgets
// it.
//
// The earlier OAuth app's tokens ("gho_…") are refused the artifacts: token()
// reads one as none, so the next opening asks to sign in again.
//
// The token is kept in its own file, sd:/switch/playguard/github_token,
// never in config.json, which "Send a report online" sends.
//
// Parsing and the token file are plain C++ for the host tests
// (tests/dev_builds); start() and poll() are in github_auth_net.cpp.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace github_auth
{

// The GitHub App's client ID (public: the device flow has no secret).
constexpr const char* CLIENT_ID = "Iv23lipQVAoOXRkdiIc1";

struct DeviceCode
{
    std::string device_code;        // kept by PlayGuard, sent back when polling
    std::string user_code;          // "ABCD-1234", shown
    std::string verification_uri;   // "https://github.com/login/device"
    int         interval = 5;       // seconds between polls
    int         expires_in = 900;   // seconds the code is valid
};

enum class Poll
{
    Token,     // signed in: *token set
    Pending,   // not authorised yet: poll again after the interval
    SlowDown,  // too fast: the interval grows by 5 s
    Expired,   // the code expired: start again
    Denied,    // the developer refused
    Failed,    // anything else (*error set)
};

// POST https://github.com/login/device/code's answer.
bool parse_device_code(const std::string& json, DeviceCode* out);
// POST https://github.com/login/oauth/access_token's answer.
Poll parse_poll(const std::string& json, std::string* token, std::string* error);

// The saved token, or empty. A file holding anything but a token's
// characters, or an OAuth app's token ("gho_…"), reads as none.
std::string token();
bool        save_token(const std::string& token, std::string* error);
void        forget_token();
std::string token_file();

// "Authorization: Bearer <token>" plus the API's media type and version, for
// http::get; only the media type without a token.
std::vector<std::string> api_headers(const std::string& token);

// Asks GitHub for a code. Blocking (network). On the desktop build
// PLAYGUARD_SIM_GITHUB_LOGIN ("ok", "denied" or "offline") replaces it.
bool start(DeviceCode* out, std::string* error);
// Asks whether the code was authorised. Blocking.
Poll poll(const DeviceCode& code, std::string* token, std::string* error);

}   // namespace github_auth
