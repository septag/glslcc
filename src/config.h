//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/glslcc#license-bsd-2-clause
//
#pragma once

#include "ResourceLimits.h"
#include <string>

extern const TBuiltInResource k_default_conf;

// Returns the DefaultTBuiltInResource as a human-readable string.
std::string get_default_conf_str();