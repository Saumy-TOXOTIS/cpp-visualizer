#ifndef LIBRARIES_HPP
#define LIBRARIES_HPP

// ########## The Ultimate Common C++ Standard Library Includes ##########
// This file acts as a custom, portable <bits/stdc++.h> for the v-cpp project,
// ensuring a vast range of standard library features are available out-of-the-box.

// ==========================================================
// ==                  CORE UTILITIES                      ==
// ==========================================================
#include <iostream>   // For std::cout, std::cin (console debugging)
#include <string>     // For std::string
#include <vector>     // For std::vector
#include <utility>    // For std::pair, std::move, std::forward
#include <stdexcept>  // For standard exception classes like std::runtime_error
#include <chrono>     // For time-related operations and benchmarking
#include <memory>     // For smart pointers like std::unique_ptr, std::shared_ptr
#include <functional> // For std::function and other functional utilities

// ==========================================================
// ==               ALGORITHMS & NUMERICS                  ==
// ==========================================================
#include <algorithm>  // For std::sort, std::find, std::min, std::max, etc.
#include <numeric>    // For std::accumulate, std::iota, std::gcd, std::lcm
#include <cmath>      // For math functions (sqrt, pow, sin, cos, etc.)
#include <cstdlib>    // For general utilities like abs(), rand(), srand()
#include <cctype>     // For character functions (toupper, isdigit, isalpha)
#include <limits>     // For std::numeric_limits to get properties of types
#include <random>     // For modern C++ random number generation

// ==========================================================
// ==                 DATA STRUCTURES                      ==
// ==========================================================
// --- Sequential Containers ---
#include <list>
#include <deque>
#include <array> // For fixed-size arrays

// --- Container Adaptors ---
#include <stack>
#include <queue>

// --- Associative Containers (Ordered) ---
#include <map>
#include <set>

// --- Associative Containers (Unordered / Hash-based) ---
#include <unordered_map>
#include <unordered_set>

// --- Other Structures ---
#include <tuple>
#include <bitset>     // For efficient fixed-size sequence of bits

// ==========================================================
// ==               INPUT / OUTPUT & STREAMS               ==
// ==========================================================
#include <sstream>    // For std::stringstream to parse strings
#include <fstream>    // For file I/O (less common in Wasm, but good practice)
#include <iomanip>    // For stream manipulators like std::setprecision

// ==========================================================
// ==            MISC / C-STYLE HEADERS                    ==
// ==========================================================
#include <climits>    // For C-style integer limits (INT_MAX, etc.)
#include <cstring>    // For C-style string functions like strlen, strcpy

#endif // LIBRARIES_HPP