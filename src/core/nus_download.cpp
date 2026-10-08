// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <httplib.h>
#include "common/logging/log.h"
#include "core/nus_download.h"

namespace Core::NUS {

std::optional<std::vector<u8>> Download(const std::string& path) {
    LOG_INFO(WebService, "NUS Download disabled in this build to force NAND dump usage");
    return {};
}

} // namespace Core::NUS
