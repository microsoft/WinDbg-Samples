// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Fallbacks.h"

#include <filesystem>
#include <system_error>
#include <vector>

std::error_code WriteFallbackStatsFile(std::vector<FallbackInfo> const& fallbackInfo, std::filesystem::path const& outputFile);

std::error_code ReadFallbackStatsFile(std::filesystem::path const& inputFile, std::vector<FallbackInfo>& fallbackInfo);
