#pragma once

// bientypes.h
// This file contains any types that might be generally needed.
// For instance types that are present on one platform and not another.
// dbien: 16MAR2020

// This type is present in MacOS but not in linux.
// We will leave it generally compiled in case somehow the MacOS version changed.
typedef char uuid_string_t[37];
