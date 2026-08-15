/**
 * AutonomousAdministration - Recursive Stateless AI-Driven Prompt Orchestrator (MoE)
 *
 * This program implements a recursive orchestrator that lets the AI decide dynamically
 * at runtime whether to decompose a step further (up to 3 tiers or more) to inject verbose
 * details, ask clarifying questions, or directly generate leaf file contents.
 *
 * Authors: Jules (Senior Software Engineer)
 */
#include <set>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <regex>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <zip.h>
#include <libssh/libssh.h>
#include <png.h>

namespace fs = std::filesystem;

// ============================================================================
// STATE TRACKING STRUCTURES
// ============================================================================

struct PromptStep {
    std::string description;
    std::string prompt;
    std::string output_file;
    nlohmann::json commands;
    int tier = 1; // Tracks decomposition depth (Tier 1, Tier 2, Tier 3)
};

// Global Configuration
struct Config {
    std::string model = "huihui_ai/qwen3-coder-abliterated:latest";
    std::string api_address = "http://localhost:11434/api/chat";
    bool dummy = false;
};

Config g_config;

// Global log file for the currently processed project
std::string g_current_log_path;

// Global stats
int g_total_projects_processed = 0;
int g_total_files_generated = 0;
int g_total_files_failed = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Append a message to a log file
void log_to_file(const fs::path& log_path, const std::string& message) {
    std::ofstream log(log_path, std::ios::app);
    if (log.is_open()) {
        log << message << std::endl;
    }
}

static const std::set<std::string> allowed_commands = {
    "join", "move", "copy", "zip", "ftp", "sftp", "search", "insert"
};

// Libcurl Write Callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Strip markdown code block markers from text
std::string strip_markdown_code_blocks(const std::string& input) {
    // 1. Try to extract <content>...</content>
    std::regex content_tag("<content>\\s*([\\s\\S]*?)\\s*</content>");
    std::smatch match;
    if (std::regex_search(input, match, content_tag)) {
        std::string result = match[1].str();
        // Trim whitespace
        result.erase(0, result.find_first_not_of(" \t\n\r"));
        result.erase(result.find_last_not_of(" \t\n\r") + 1);
        return result;
    }

    // 2. Try markdown code blocks: ```language? ... ```
    std::regex code_pattern("```(?:\\w+)?\\s*([\\s\\S]*?)\\s*```");
    if (std::regex_search(input, match, code_pattern)) {
        std::string result = match[1].str();
        // Trim whitespace
        result.erase(0, result.find_first_not_of(" \t\n\r"));
        result.erase(result.find_last_not_of(" \t\n\r") + 1);
        return result;
    }

    // 3. Fallback: if the whole string is wrapped in triple backticks
    std::string trimmed = input;
    // Trim leading/trailing whitespace first
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    if (trimmed.size() >= 6 &&
        trimmed.substr(0, 3) == "```" &&
        trimmed.substr(trimmed.size() - 3) == "```") {
        // Remove the backticks, possibly with a language spec on the first line
        size_t first_newline = trimmed.find('\n');
        if (first_newline != std::string::npos) {
            // Remove first line (language spec) and the closing backticks
            std::string inner = trimmed.substr(first_newline + 1);
            size_t last_backtick = inner.rfind("```");
            if (last_backtick != std::string::npos) {
                inner = inner.substr(0, last_backtick);
            }
            // Trim again
            inner.erase(0, inner.find_first_not_of(" \t\n\r"));
            inner.erase(inner.find_last_not_of(" \t\n\r") + 1);
            return inner;
        }
        // If no newline, just strip the outer backticks
        return trimmed.substr(3, trimmed.size() - 6);
    }

    // 4. Nothing found – return original (but trimmed)
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    return trimmed;
}
// Convert string to lower case
std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
    return str;
}

// Sanitize arguments to prevent Remote Code Execution / Command Injection
std::string sanitize_argument(const std::string& arg) {
    std::string safe_arg;
    for (char c : arg) {
        if (std::isalnum(c) || c == '/' || c == '.' || c == '_' || c == '-' || c == ':') {
            safe_arg += c;
        }
    }
    return safe_arg;
}

// ============================================================================
// COMMAND EXECUTION FRAMEWORK (Step 8)
// ============================================================================

std::string execute_http_get(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return "Error: Failed to initialize curl";
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        return "Error: " + std::string(curl_easy_strerror(res));
    }
    return response;
}

std::string execute_http_post(const std::string& url, const std::string& payload) {
    CURL* curl = curl_easy_init();
    if (!curl) return "Error: Failed to initialize curl";
    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        return "Error: " + std::string(curl_easy_strerror(res));
    }
    return response;
}

std::string execute_search(const std::string& dir, const std::string& pattern) {
    std::string result;
    try {
        fs::path p(dir);
        if (!fs::exists(p)) {
            return "Directory does not exist: " + dir;
        }
        for (const auto& entry : fs::recursive_directory_iterator(p)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.find(pattern) != std::string::npos) {
                    result += entry.path().string() + "\n";
                }
            }
        }
    } catch (const std::exception& e) {
        return "Search error: " + std::string(e.what());
    }
    return result.empty() ? "No files found matching pattern." : result;
}

std::string execute_zip(const std::string& zip_file, const std::string& source_path) {
    std::string cmd = "zip -r " + zip_file + " " + source_path;
    int ret = std::system(cmd.c_str());
    if (ret == 0) {
        return "Successfully created zip archive: " + zip_file;
    } else {
        return "Failed to create zip archive. Return code: " + std::to_string(ret);
    }
}

std::string execute_png_ops(const std::string& op, const std::string& src, const std::string& dst, const std::string& args) {
    std::string cmd;
    if (op == "resize") {
        cmd = "convert " + src + " -resize " + args + " " + dst;
    } else if (op == "crop") {
        cmd = "convert " + src + " -crop " + args + " " + dst;
    } else if (op == "convert") {
        cmd = "convert " + src + " " + dst;
    } else if (op == "optimize") {
        cmd = "convert " + src + " -quality 85 " + dst;
    } else {
        return "Error: Unsupported PNG operation '" + op + "'";
    }

    int ret = std::system(cmd.c_str());
    if (ret == 0) {
        return "Successfully executed PNG operation: " + op;
    } else {
        return "Failed PNG operation. ImageMagick return code: " + std::to_string(ret);
    }
}

// Safely joins file contents in C++ memory
std::string execute_join(const std::string& out_file, const std::vector<std::string>& in_files) {
    std::ofstream out(out_file);
    if (!out.is_open()) return "Error: Failed to open output file for join: " + out_file;

    for (const auto& in_file : in_files) {
        std::ifstream in(in_file);
        if (in.is_open()) {
            out << in.rdbuf() << "\n";
        } else {
            return "Error: Failed to open input file for join: " + in_file;
        }
    }
    out.close();
    return "Successfully joined " + std::to_string(in_files.size()) + " files into " + out_file;
}

// Safely inserts text into a file after a target pattern
std::string execute_insert(const std::string& target_file, const std::string& source_file, const std::string& pattern = "") {
    // Read source file content
    std::ifstream src(source_file);
    if (!src.is_open()) return "Error: Failed to open source file: " + source_file;
    std::string content((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
    src.close();

    // Read target file
    std::ifstream in(target_file);
    if (!in.is_open()) {
        // If target doesn't exist, create it with the source content
        std::ofstream out(target_file);
        if (!out.is_open()) return "Error: Failed to create target file: " + target_file;
        out << content;
        out.close();
        return "Created target file with source content.";
    }

    // Read target lines
    std::vector<std::string> lines;
    std::string line;
    bool found = false;
    while (std::getline(in, line)) {
        lines.push_back(line);
        if (!found && !pattern.empty() && line.find(pattern) != std::string::npos) {
            lines.push_back(content);
            found = true;
        }
    }
    in.close();

    if (!found && !pattern.empty()) {
        // Pattern not found - append to end
        lines.push_back(content);
    } else if (pattern.empty()) {
        // No pattern - insert at end
        lines.push_back(content);
    }

    std::ofstream out(target_file);
    for (const auto& l : lines) {
        out << l << "\n";
    }
    out.close();

    return found ? "Successfully inserted source file after pattern." : "Pattern not found (or no pattern) – appended source content to target.";
}

// Unified Command Execution (Step 8)
std::string run_external_command(const std::string& name, const std::vector<std::string>& args) {
    std::string normalized_name = to_lower(name);

    std::vector<std::string> safe_args;
    for (const auto& arg : args) {
        safe_args.push_back(sanitize_argument(arg));
    }

    if (normalized_name == "get" || normalized_name == "get_url" || normalized_name == "http") {
        if (safe_args.empty()) return "Error: Missing URL for GET command";
        return execute_http_get(safe_args[0]);
    }
    else if (normalized_name == "post" || normalized_name == "post_url") {
        if (safe_args.size() < 2) return "Error: Missing URL or body for POST command";
        return execute_http_post(safe_args[0], safe_args[1]);
    }
    else if (normalized_name == "search" || normalized_name == "web_search") {
        std::string pattern = safe_args.empty() ? "" : safe_args[0];
        std::string dir = (safe_args.size() > 1) ? safe_args[1] : ".";
        return execute_search(dir, pattern);
    }
    else if (normalized_name == "zip") {
        if (safe_args.size() < 2) return "Error: Missing zip destination or source path";
        return execute_zip(safe_args[0], safe_args[1]);
    }
    else if (normalized_name == "png") {
        if (safe_args.size() < 3) return "Error: Missing PNG arguments (op, src, dst, [args])";
        std::string op = safe_args[0];
        std::string src = safe_args[1];
        std::string dst = safe_args[2];
        std::string extra_args = (safe_args.size() > 3) ? safe_args[3] : "";
        return execute_png_ops(op, src, dst, extra_args);
    }
    else if (normalized_name == "move") {
        if (safe_args.size() < 2) return "Error: Missing source or destination for move";
        try {
            fs::rename(safe_args[0], safe_args[1]);
            return "Successfully moved " + safe_args[0] + " to " + safe_args[1];
        } catch (const std::exception& e) {
            return "Move error: " + std::string(e.what());
        }
    }
    else if (normalized_name == "copy") {
        if (safe_args.size() < 2) return "Error: Missing source or destination for copy";
        try {
            fs::copy(safe_args[0], safe_args[1], fs::copy_options::overwrite_existing);
            return "Successfully copied " + safe_args[0] + " to " + safe_args[1];
        } catch (const std::exception& e) {
            return "Copy error: " + std::string(e.what());
        }
    }
    else if (normalized_name == "join") {
        if (safe_args.size() < 2) return "Error: Missing output file or input files for join";
        std::string out_file = safe_args[0];
        std::vector<std::string> in_files(safe_args.begin() + 1, safe_args.end());
        return execute_join(out_file, in_files);
    }
    else if (normalized_name == "insert") {
        if (safe_args.size() < 2) return "Error: Missing target file or source file for insert";
        std::string target = safe_args[0];
        std::string source = safe_args[1];
        std::string pattern = (safe_args.size() > 2) ? safe_args[2] : "";
        return execute_insert(target, source, pattern);
    }
    else if (normalized_name == "ftp") {
        if (safe_args.size() < 2) return "Error: Missing ftp file path or ftp target url";
        std::string local_file = safe_args[0];
        std::string ftp_url = safe_args[1];
        std::string cmd = "curl -T " + local_file + " " + ftp_url;
        int ret = std::system(cmd.c_str());
        return (ret == 0) ? "FTP upload succeeded." : "FTP upload failed with code: " + std::to_string(ret);
    }
    else if (normalized_name == "sftp") {
        if (safe_args.size() < 2) return "Error: Missing sftp file path or sftp target url";
        std::string local_file = safe_args[0];
        std::string sftp_url = safe_args[1];
        std::string cmd = "curl -u user:pass sftp://" + sftp_url + " -T " + local_file;
        int ret = std::system(cmd.c_str());
        return (ret == 0) ? "SFTP upload succeeded." : "SFTP upload failed with code: " + std::to_string(ret);
    }

    return "Error: Command '" + name + "' is not implemented.";
}

// Intercepts any command pattern in AI text and runs it
std::string handle_embedded_commands(const std::string& response_text) {
    std::regex cmd_pattern("\\[\\[COMMAND:\\s*(\\w+)\\s*([^\\]]+)\\]\\]");
    std::string text_copy = response_text;
    std::smatch match;

    while (std::regex_search(text_copy, match, cmd_pattern)) {
        std::string cmd_name = match[1].str();
        std::string cmd_args_str = match[2].str();

        std::vector<std::string> args;
        std::istringstream iss(cmd_args_str);
        std::string arg;
        while (iss >> arg) {
            args.push_back(arg);
        }

        std::cout << "[Command Frame] Executing AI requested command: " << cmd_name << "\n";
        std::string cmd_result = run_external_command(cmd_name, args);
        std::cout << "[Command Frame] Result: " << cmd_result << "\n";

        std::string full_match = match[0].str();
        size_t pos = text_copy.find(full_match);
        if (pos != std::string::npos) {
            text_copy.replace(pos, full_match.length(), "\n[Command Execution Result:\n" + cmd_result + "\n]\n");
        }
    }
    return text_copy;
}

// ============================================================================
// HIGH-FIDELITY MOCK OLLAMA RESPONDER (Testing/Dummy Mode)
// ============================================================================

std::string generate_mock_response(const std::string& prompt, int& sim_state, const std::string& current_filename) {
    std::string prompt_lower = to_lower(prompt);

    // PRIORITY 1: Correction Prompt
    if (prompt_lower.find("fix this content to fulfill the request") != std::string::npos) {
        if (current_filename == "action.json") {
            return R"raw({
                "action": "decompose",
                "substeps": [
                    {
                        "description": "Decompose into basic arithmetic details (Tier 3)",
                        "prompt": "Fulfill basic operations math.h and math.cpp. Context: Basic arithmetic addition/subtraction. Contains simulate-retry."
                    }
                ]
            })raw";
        }
        if (current_filename == "math.h") {
            return R"raw(#pragma once
// Basic Math Library Header (Corrected)
int add(int a, int b);
int subtract(int a, int b);
)raw";
        }
        if (current_filename == "math.cpp") {
            return R"raw(#include "math.h"
// Implementation of math functions (Corrected)
int add(int a, int b) { return a + b; }
int subtract(int a, int b) { return a - b; }
)raw";
        }
        return "/* Corrected content for " + current_filename + " */\n// Corrected successfully.";
    }

    // PRIORITY 2: Validation Prompt
    if (prompt_lower.find("does this content fully and correctly fulfill this request") != std::string::npos) {
        if (current_filename == "action.json") {
            return "YES. It is completely correct, structured perfectly, and fulfills all requirements.";
        }
        if (prompt_lower.find("simulate-retry") != std::string::npos && sim_state == 0) {
            sim_state = 1;
            return "NO. The content lacks stateless documentation.";
        }
        return "YES. It is completely correct, structured perfectly, and fulfills all requirements.";
    }

    // PRIORITY 3: Project Manager recursive coordinator
    if (prompt_lower.find("recursive mixture of experts") != std::string::npos) {
        // 1. Math Library - Simulation (Tier 1 -> Ask questions, Tier 2 -> Decompose, Tier 3 -> Generate)
        if (prompt_lower.find("math") != std::string::npos) {
            fs::path sentinel = fs::path("projects/math_library/.questions_asked");
            if (!fs::exists(sentinel)) {
                std::ofstream f(sentinel);
                f << "done";
                f.close();

                return R"raw({
                    "action": "questions",
                    "questions": [
                        "What standard of C++ should we build against?",
                        "Do you need multiplication/division, or just addition/subtraction?"
                    ]
                })raw";
            } else if (sim_state == 0) {
                sim_state = 1; // Move to next tier
                return R"raw({
                    "action": "decompose",
                    "substeps": [
                        {
                            "description": "Decompose into basic arithmetic details (Tier 3)",
                            "prompt": "Fulfill basic operations math_library_leaf. Context: Basic arithmetic addition/subtraction. Contains simulate-retry."
                        }
                    ]
                })raw";
            } else {
                return R"raw({
                    "action": "generate",
                    "files": [
                        {"filename": "math.h", "type": "header", "description": "Basic math header."},
                        {"filename": "math.cpp", "type": "source", "description": "Basic math implementations."}
                    ]
                })raw";
            }
        }

        // 2. Admin Dashboard - Simulation (Tier 1 -> Decompose, Tier 2 -> Generate HTML/CSS, Tier 3 -> Generate JS)
        if (prompt_lower.find("admin") != std::string::npos || prompt_lower.find("website") != std::string::npos) {
            if (sim_state == 0) {
                sim_state = 1;
                return R"raw({
                    "action": "decompose",
                    "substeps": [
                        {
                            "description": "Generate Web Structures (Tier 2)",
                            "prompt": "Generate index.html and style.css for admin. Context: Admin dashboard frontend layout."
                        },
                        {
                            "description": "Generate Web Scripts (Tier 2)",
                            "prompt": "Generate script.js admin actions. Context: Admin dashboard interaction behaviors."
                        }
                    ]
                })raw";
            } else if (prompt_lower.find("frontend") != std::string::npos || prompt_lower.find("styles") != std::string::npos || prompt_lower.find("html") != std::string::npos) {
                return R"raw({
                    "action": "generate",
                    "files": [
                        {"filename": "index.html", "type": "html", "description": "Landing structure."},
                        {"filename": "style.css", "type": "css", "description": "Colors and grids."}
                    ]
                })raw";
            } else {
                return R"raw({
                    "action": "generate",
                    "files": [
                        {"filename": "script.js", "type": "js", "description": "Dashboard interaction scripts."}
                    ]
                })raw";
            }
        }

        // 3. New Commands Test (Insert & Join)
        if (prompt_lower.find("new commands") != std::string::npos || prompt_lower.find("new_commands") != std::string::npos) {
            return R"raw({
                "action": "generate",
                "files": [
                    {"filename": "a.txt", "type": "source", "description": "Source A content."},
                    {"filename": "b.txt", "type": "source", "description": "Source B content."}
                ],
                "substeps": [
                    {
                        "description": "Run Insert and Join utilities",
                        "commands": [
                            {"name": "insert", "args": ["a.txt", "Hello"]},
                            {"name": "insert", "args": ["b.txt", "World!"]},
                            {"name": "join", "args": ["joined.txt", "a.txt", "b.txt"]}
                        ],
                        "prompt": "Document that join succeeded.",
                        "output_file": "joined_summary.txt"
                    }
                ]
            })raw";
        }

        // 4. Vector Art - SVG
        if (prompt_lower.find("svg") != std::string::npos || prompt_lower.find("vector") != std::string::npos) {
            return R"raw({
                "action": "generate",
                "files": [
                    {"filename": "image.svg", "type": "svg", "description": "Circles SVG graphic."}
                ]
            })raw";
        }

        // 5. Binary Image
        if (prompt_lower.find("binary") != std::string::npos || prompt_lower.find("png") != std::string::npos) {
            return R"raw({
                "action": "generate",
                "files": [
                    {"filename": "generate_binary.py", "type": "source", "description": "Python PIL PNG binary image creator."}
                ]
            })raw";
        }

        // 6. Command Test - Zip
        if (prompt_lower.find("command execution") != std::string::npos || prompt_lower.find("command_test") != std::string::npos) {
            return R"raw({
                "action": "generate",
                "files": [
                    {"filename": "run_command_test.txt", "type": "source", "description": "Summary file."}
                ],
                "substeps": [
                    {
                        "description": "Run Zip Command",
                        "commands": [
                            {"name": "zip", "args": ["test.zip", "projects/command_test"]}
                        ],
                        "prompt": "Verify Zip run."
                    }
                ]
            })raw";
        }

        // Default Fallback
        return R"raw({
            "action": "generate",
            "files": [
                {"filename": "output.txt", "type": "source", "description": "Simple default task output."}
            ]
        })raw";
    }

    // PRIORITY 4: Leaf File Generation prompts (simulated by stateless runner)
    if (current_filename == "index.html") {
        return R"raw(<!DOCTYPE html>
<html>
<head>
    <title>AI Assisted Admin Page</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <div class="container">
        <h1>Welcome to Administrative Generator Dashboard</h1>
        <p>This is generated iteratively by our smart prompt orchestrator.</p>
        <div id="content">Loading dashboard components...</div>
    </div>
    <script src="script.js"></script>
</body>
</html>)raw";
    }
    else if (current_filename == "style.css") {
        return R"raw(body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background-color: #f4f6f9;
    color: #333;
    margin: 0;
    padding: 20px;
}
.container {
    max-width: 800px;
    margin: 50px auto;
    background: #fff;
    padding: 30px;
    border-radius: 8px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.1);
})raw";
    }
    else if (current_filename == "script.js") {
        return R"raw(document.addEventListener("DOMContentLoaded", () => {
    console.log("Admin Dashboard Loaded.");
    document.getElementById("content").innerText = "Systems normal. Generated iteratively by stateless prompt loop.";
});)raw";
    }
    else if (current_filename == "image.svg") {
        return R"raw(<svg width="200" height="200" xmlns="http://www.w3.org/2000/svg">
    <rect width="100%" height="100%" fill="#eceff1"/>
    <circle cx="100" cy="100" r="70" fill="#37474f"/>
    <text x="100" y="105" text-anchor="middle" fill="#ffffff" font-family="Arial" font-size="16">Layer 1</text>
</svg>)raw";
    }
    else if (current_filename == "math.h") {
        return R"raw(#pragma once
// Basic Math Library Header
int add(int a, int b);
int subtract(int a, int b);
)raw";
    }
    else if (current_filename == "math.cpp") {
        return R"raw(#include "math.h"
// Implementation of math functions
int add(int a, int b) {
    return a + b;
}
int subtract(int a, int b) {
    return a - b;
}
)raw";
    }
    else if (current_filename == "generate_binary.py") {
        return R"raw(#!/usr/bin/env python3
# Script to generate binary PNG file using Pillow
import sys
from PIL import Image, ImageDraw

def main():
    img = Image.new('RGB', (300, 300), color = (73, 109, 137))
    d = ImageDraw.Draw(img)
    d.text((50,130), "Binary Generation Success", fill=(255,255,0))
    img.save('output_image.png')
    print("PNG image created successfully via python script.")

if __name__ == '__main__':
    main()
)raw";
    }

    return "/* Generated output content for " + current_filename + " */\n";
}

// ============================================================================
// OLLAMA API CLIENT WITH RETRY LOGIC (Step 11)
// ============================================================================

std::string call_ollama_api(const std::string& prompt, const std::string& current_filename, int& sim_state) {
    if (g_config.dummy) {
        std::string mock_res = generate_mock_response(prompt, sim_state, current_filename);
        return handle_embedded_commands(mock_res);
    }

    // New (Matches Qt structure):
    nlohmann::json payload;
    payload["model"] = g_config.model;
    payload["stream"] = false;
    payload["messages"] = nlohmann::json::array({
        {
            {"role", "user"},
            {"content", prompt}
        }
    });

    std::string json_str = payload.dump();
    std::string url = g_config.api_address;

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[Error] Failed to initialize Curl.\n";
        return "[Error: Failed to initialize curl]";
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if(!g_current_log_path.empty()){
        std::ofstream log(g_current_log_path, std::ios::app);
        if (log.is_open()) {
            log << "=== PROMPT ===\n" << prompt;
        }
    }

    std::string response_string;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    int backoff_ms = 1000;
    CURLcode res = CURLE_FAILED_INIT;
    bool success = false;

    for (int attempt = 0; attempt < 3000; ++attempt) {
        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            success = true;
            break;
        }

        std::cerr << "[API Warning] Call failed (" << curl_easy_strerror(res)
                  << "). Retrying in " << backoff_ms << "ms...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
        backoff_ms *= 2;
    }


    long http_code = 0;
    if (success) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (!success) {
        std::string err_msg = "[Error: Connection failed completely after 3 retries. cURL code: " + std::to_string(res) + " (" + curl_easy_strerror(res) + "). Is Ollama running on " + url + "?]";
        std::cerr << err_msg << "\n";
        return err_msg;
    }

    if (http_code != 200) {
        std::string err_msg = "[Error: HTTP response code " + std::to_string(http_code) + " (expected 200 OK)]";
        std::cerr << err_msg << "\n";
        // After the request, before checking http_code
        std::cout << "DEBUG: Response body: " << response_string << std::endl;

        return err_msg;
    }

    try {
        auto j = nlohmann::json::parse(response_string);
        // New:
        if (j.contains("message") && j["message"].contains("content")) {
            std::string text_output = j["message"]["content"].get<std::string>();
            if(!g_current_log_path.empty()){
                std::ofstream log(g_current_log_path, std::ios::app);
                if (log.is_open()) {
                    log << "=== RESPONSE ===\n" << text_output;
                }
            }            
            return handle_embedded_commands(text_output);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Error] JSON parsing error on API response: " << e.what() << "\n";
        std::cerr << "[Error] Raw response text was: " << response_string << "\n";
        return "[Error: JSON parsing failed. Raw response: " + response_string + "]";
    }

    return "[Error: Response missing 'response' text field]";
}

// ============================================================================
// TWO-STEP VALIDATION LOOP (Step V2, V3, Validation Rules)
// ============================================================================

bool run_two_step_validation(const std::string& request, std::string& content, const std::string& filename, int& retry_count) {
    int sim_val_state_1 = 0;
    int sim_val_state_2 = 0;

    while (retry_count <= 1000) {
        // Step V2 - Validate First Time
        // Detect if we are validating a PM decision (plan) or a file content
        bool is_pm_decision = (request.find("Project Manager") != std::string::npos ||
                       request.find("recursive Mixture of Experts") != std::string::npos);

        std::string v1_prompt;
        if (is_pm_decision) {
            v1_prompt = "Does this JSON decision correctly handle the step (generate, decompose, or questions) as requested by the Project Manager instructions?\n"
                        "<decision>\n" + content + "\n</decision>\n"
                        "<request>\n" + request + "\n</request>\n"
                        "Your job is to ensure all steps are broken down logically, there is ample summary information and if something needs to be generated it is generated.\n"
                        "DO NOT say NO IF the steps are broken down, and there is no impact on quality or outcome. e.g. If you think something can be achieved in less steps or less files, DO NOT say NO based on that alone. Saying NO when the outcome is fine will ONLY slow down the delivery process.\n"
                        "Answer only YES or NO with a brief reason.";        
        } else {
            v1_prompt = "Does this content fully and correctly fulfill this request? Request is " + request + ". Content is " + content + ". Answer only YES or NO with a brief reason.";
        }
        std::string v1_response = call_ollama_api(v1_prompt, filename, sim_val_state_1);

        bool v1_ok = false;
        std::string v1_reason = v1_response;

        std::string v1_lower = to_lower(v1_response);
        if (v1_lower.find("yes") != std::string::npos && (v1_lower.find("no") == std::string::npos || v1_lower.find("yes") < v1_lower.find("no"))) {
            v1_ok = true;
        }

        if (v1_ok) {
            std::cout << "File: " << filename << " - validation round 1: YES\n";

            // Step V3 - Validate Second Time
            // Detect if we are validating a PM decision (plan) or a file content
            bool is_pm_decision = (request.find("Project Manager") != std::string::npos ||
                       request.find("recursive Mixture of Experts") != std::string::npos);

            std::string v2_prompt;
            if (is_pm_decision) {
                v2_prompt = "Does this JSON decision correctly handle the step (generate, decompose, or questions) as requested by the Project Manager instructions?\n"
                            "<decision>\n" + content + "\n</decision>\n"
                            "<request>\n" + request + "\n</request>\n"
                            "Your job is to ensure all steps are broken down logically, there is ample summary information and if something needs to be generated it is generated.\n"
                            "DO NOT say NO IF the steps are broken down, and there is no impact on quality or outcome. e.g. If you think something can be achieved in less steps or less files, DO NOT say NO based on that alone. Saying NO when the outcome is fine will ONLY slow down the delivery process.\n"
                            "Answer only YES or NO with a brief reason.";
            }   else {
                v2_prompt = "Does this content fully and correctly fulfill this request? Request is " + request + ". Content is " + content + ". Answer only YES or NO with a brief reason.";
            }
            std::string v2_response = call_ollama_api(v2_prompt, filename, sim_val_state_2);

            bool v2_ok = false;
            std::string v2_lower = to_lower(v2_response);
            if (v2_lower.find("yes") != std::string::npos && (v2_lower.find("no") == std::string::npos || v2_lower.find("yes") < v2_lower.find("no"))) {
                v2_ok = true;
            }

            if (v2_ok) {
                std::cout << "File: " << filename << " - validation round 2: YES\n";
                return true; // Content accepted! Two consecutive YES!
            } else {
                std::cout << "File: " << filename << " - validation round 2: NO (reason: " << v2_response << ")\n";
                v1_reason = v2_response;
            }
        } else {
            std::cout << "File: " << filename << " - validation round 1: NO (reason: " << v1_response << ")\n";
        }

        retry_count++;
        if (retry_count > 3000) {
            break;
        }

        std::cout << "File: " << filename << " - retrying with corrections...\n";
        std::string correction_prompt = "Fix this content to fulfill the request. Request is " + request + ". Current content is " + content + ". Issue is " + v1_reason + ". Return the complete corrected content.";
        content = call_ollama_api(correction_prompt, filename, sim_val_state_1);
    }

    return false; // Validation failed after 3 retries
}

// ============================================================================
// STATELESS RECURSIVE PROMPT ORCHESTRATOR
// ============================================================================

bool validate_pm_structure(const nlohmann::json& pm_json, std::string& error_msg) {
    if (!pm_json.is_object()) {
        error_msg = "PM response is not a JSON object.";
        return false;
    }
    if (!pm_json.contains("action") || !pm_json["action"].is_string()) {
        error_msg = "Missing or invalid 'action' field.";
        return false;
    }
    std::string action = pm_json["action"];
    if (action != "generate" && action != "decompose" && action != "questions") {
        error_msg = "Invalid action: " + action;
        return false;
    }

    if (action == "generate") {
        if (!pm_json.contains("files") || !pm_json["files"].is_array() || pm_json["files"].empty()) {
            error_msg = "'generate' action requires non-empty 'files' array.";
            return false;
        }
        for (const auto& f : pm_json["files"]) {
            if (!f.is_object()) {
                error_msg = "Each file entry must be an object.";
                return false;
            }
            if (!f.contains("filename") || !f["filename"].is_string() || f["filename"].get<std::string>().empty()) {
                error_msg = "File entry missing 'filename' or empty.";
                return false;
            }
            if (!f.contains("type") || !f["type"].is_string() || f["type"].get<std::string>().empty()) {
                error_msg = "File entry missing 'type' or empty.";
                return false;
            }
            if (!f.contains("description") || !f["description"].is_string() || f["description"].get<std::string>().empty()) {
                error_msg = "File entry missing 'description' or empty.";
                return false;
            }
            std::string fname = f["filename"];
            if (fname.find('/') != std::string::npos || fname.find('\\') != std::string::npos) {
                error_msg = "Filename contains path separators: " + fname;
                return false;
            }
        }
    } else if (action == "decompose") {
        if (!pm_json.contains("substeps") || !pm_json["substeps"].is_array() || pm_json["substeps"].empty()) {
            error_msg = "'decompose' action requires non-empty 'substeps' array.";
            return false;
        }
        for (const auto& s : pm_json["substeps"]) {
            if (!s.is_object()) {
                error_msg = "Each substep must be an object.";
                return false;
            }
            if (!s.contains("description") || !s["description"].is_string() || s["description"].get<std::string>().empty()) {
                error_msg = "Substep missing 'description' or empty.";
                return false;
            }
            if (!s.contains("prompt") || !s["prompt"].is_string() || s["prompt"].get<std::string>().empty()) {
                error_msg = "Substep missing 'prompt' or empty.";
                return false;
            }
            if (!s.contains("output_file") || !s["output_file"].is_string() || s["output_file"].get<std::string>().empty()) {
                error_msg = "Substep missing 'output_file' or empty.";
                return false;
            }
            std::string out_file = s["output_file"];
            if (out_file.find('/') != std::string::npos || out_file.find('\\') != std::string::npos) {
                error_msg = "output_file contains path separators: " + out_file;
                return false;
            }
            
            if (s.contains("commands")) {
                if (!s["commands"].is_array()) {
                    error_msg = "Commands must be an array.";
                    return false;
                }
                for (const auto& cmd : s["commands"]) {
                    if (!cmd.is_object()) {
                        error_msg = "Each command must be an object.";
                        return false;
                    }
                    if (!cmd.contains("name") || !cmd["name"].is_string() || cmd["name"].get<std::string>().empty()) {
                        error_msg = "Command missing 'name' or empty.";
                        return false;
                    }
                    std::string name = cmd["name"];
                    if (allowed_commands.find(name) == allowed_commands.end()) {
                        error_msg = "Illegal command: " + name + ". Allowed: join, move, copy, zip, ftp, sftp, search, insert.";
                        return false;
                    }
                    if (cmd.contains("args")) {
                        if (!cmd["args"].is_array()) {
                            error_msg = "Command 'args' must be an array.";
                            return false;
                        }
                        for (const auto& arg : cmd["args"]) {
                            if (!arg.is_string()) {
                                error_msg = "Command argument must be a string.";
                                return false;
                            }
                            std::string arg_str = arg.get<std::string>();
                            if (arg_str.find('/') != std::string::npos || arg_str.find('\\') != std::string::npos) {
                                error_msg = "Command argument contains path separators: " + arg_str;
                                return false;
                            }
                        }
                    }
                }
            }
        }
    } else if (action == "questions") {
        if (!pm_json.contains("questions") || !pm_json["questions"].is_array() || pm_json["questions"].empty()) {
            error_msg = "'questions' action requires non-empty 'questions' array.";
            return false;
        }
        for (const auto& q : pm_json["questions"]) {
            if (!q.is_string() || q.get<std::string>().empty()) {
                error_msg = "Each question must be a non-empty string.";
                return false;
            }
        }
    }
    return true;
}

void process_project(const fs::path& project_dir) {
    std::string project_name = project_dir.filename().string();
    std::cout << "Processing project: " << project_name << "\n";

    fs::path inst_path = project_dir / "instructions.txt";
    if (!fs::exists(inst_path)) {
        std::cerr << "[Warning] instructions.txt is missing in: " << project_name << ". Skipping.\n";
        return;
    }

    // Step 1 - Read Instructions
    std::ifstream inst_file(inst_path);
    std::string instructions((std::istreambuf_iterator<char>(inst_file)), std::istreambuf_iterator<char>());
    inst_file.close();

    // Check for clarifying questions waiting for manual answers
    fs::path questions_path = project_dir / "questions.txt";
    if (fs::exists(questions_path)) {
        std::cout << "[Clarification Pending] Waiting for manual answers on questions.txt under " << project_name << "\n";
        return;
    }

    // Output Directory Setup
    fs::path output_dir = fs::path("output") / project_name;
    fs::create_directories(output_dir);

    fs::path log_file = output_dir / "conversation.log";
    g_current_log_path = log_file.string();
    // Clear any previous log
    std::ofstream(log_file.string(), std::ios::trunc).close();

    // Dynamic work queue of prompts to execute (MoE Recursive Coordination)
    std::vector<PromptStep> steps_queue;
    PromptStep root_step;
    root_step.description = "Overall project completion";
    root_step.prompt = instructions;
    root_step.output_file = "";
    root_step.tier = 1;
    steps_queue.push_back(root_step);

    std::string accumulated_context = "Initial Project Instructions:\n" + instructions + "\n\n";
    size_t current_index = 0;
    int dummy_sim = 0;
    g_total_projects_processed++;

    while (current_index < steps_queue.size()) {
        PromptStep& step = steps_queue[current_index];
        std::cout << "[Orchestrator] Processing Step " << current_index + 1 << "/" << steps_queue.size()
                  << " (Tier " << step.tier << "): " << step.description << "\n";

        // Query AI PM expert to decide the next recursive MoE action
        std::string pm_expert_prompt =
    "You are the Project Manager expert coordinating a recursive Mixture of Experts (MoE) pipeline.\n"
    "Your job is to break a broad request into a set of independent, verifiable sub‑tasks.\n"
    "For each sub‑task, you decide whether it should be 'generate', 'decompose', or 'questions'.\n\n"

    "RULES:\n"
    "- Use 'generate' when the sub‑task can be completed by creating a single file.\n"
    "  The file can be any type (e.g., .txt, .svg, .json, .py). The 'description' field must clearly\n"
    "  define what the file should contain – the expert will generate the actual content later.\n"
    "- Use 'decompose' only when the sub‑task is genuinely broad and cannot be captured in a single file.\n"
    "  Break it into smaller, independent sub‑tasks that can each be handled by a separate 'generate'.\n"
    "- Do NOT decompose a step that can be done in one file (e.g., 'determine a colour' is a single file).\n"
    "- 'questions' is for ambiguous steps that need more details before proceeding.\n\n"

    "GOOD EXAMPLES:\n"
    "  'generate' for: 'Create a head for Lisa Simpson' – description: a round head with eyes, nose, mouth, and ponytail.\n"
    "  'generate' for: 'Determine earthy colours for logo' – description: a text file listing the two chosen colours.\n"
    "  'decompose' for: 'Create the full website' – this is broad; split into 'generate' tasks for HTML, CSS, JS, etc.\n\n"

    "BAD EXAMPLES:\n"
    "  Decomposing a single file task into multiple steps (e.g., splitting 'determine colour' into 'pick colour' and 'write file').\n"
    "  Using 'generate' for a genuinely broad task without breaking it down (e.g., 'generate full website' with one file).\n\n"

    "*** DO NOT include any code, colours, or actual content in your JSON. Only provide descriptions. ***\n"
    "*** The expert will generate the file content later. ***\n\n"

    "Analyze this active step:\n"
    "Description: " + step.description + "\n" +
    "Prompt: " + step.prompt + "\n\n"
    "Remember that the AI API is stateless, so describe any context needed.\n"
    "You have the authority to decide if this step should be:\n"
    "- 'generate': Detailed enough to directly generate and write the output files.\n"
    "- 'decompose': Too broad or needs more verbose, incremental details. Decompose it into a list of smaller, highly-specific sub-steps.\n"
    "- 'questions': Ambiguous or missing critical details. Ask clarifying questions.\n\n"
    "Return a JSON object ONLY with the field \"action\": \"generate\" | \"decompose\" | \"questions\".\n"
    "If 'generate', include:\n"
    "- \"files\": [ {\"filename\": \"name\", \"type\": \"html|css|js|source|header|svg\", \"description\": \"detailed description\"} ]\n"
    "If 'decompose', include:\n"
    "- \"substeps\": [ {\"description\": \"short description\", \"prompt\": \"specific sub-prompt to resolve\", \"commands\": [ {\"name\": \"cmd\", \"args\": []} ], \"output_file\": \"filename.ext\"} ]\n"
    "If 'questions', include:\n"
    "- \"questions\": [ \"clarifying question 1\", ... ]\n\n"
    "AVAILABLE COMMANDS (Project Manager may ONLY use these for file management; DO NOT use them to create or modify file content):\n"
    "- join: merge existing files. Usage: {\"name\": \"join\", \"args\": [\"output.txt\", \"input1.txt\", \"input2.txt\"]}\n"
    "- move: move/rename files. Usage: {\"name\": \"move\", \"args\": [\"source\", \"dest\"]}\n"
    "- copy: copy files. Usage: {\"name\": \"copy\", \"args\": [\"source\", \"dest\"]}\n"
    "- zip: create archive of existing files. Usage: {\"name\": \"zip\", \"args\": [\"archive.zip\", \"source_path\"]}\n"
    "- ftp, sftp: upload existing files. Usage: {\"name\": \"ftp\", \"args\": [\"local_file\", \"ftp_url\"]}\n"
    "- search: list files matching pattern. Usage: {\"name\": \"search\", \"args\": [\"pattern\", \"optional_dir\"]}\n"
    "- insert: copy content from a source file into a target file. Usage: {\"name\": \"insert\", \"args\": [\"target.txt\", \"source.txt\", \"optional_pattern\"]}\n\n"
    "You must NOT use echo or any shell command. The only commands you may use are: join, move, copy, zip, ftp, sftp, search, insert. All content creation must be delegated to the leaf generation step (via generate).\n"
    "Note: You may only reference filenames in these commands – never embed actual code, colours, or SVG. All content comes from generated files, this ensures you split tasks up correctly.\n\n"
    "If you choose 'generate', you MUST include a \"files\" array. Do NOT include \"substeps\" or any other fields besides \"files\". The \"files\" array must contain objects with \"filename\" and \"description\" (and optionally \"content\" if you want to embed the file content directly).\n"
    "Accumulated Project Context:\n" + accumulated_context;

        std::cout << "  Querying AI PM for recursive coordination action...\n";
        std::string api_raw = call_ollama_api(pm_expert_prompt, "action.json", dummy_sim);
        std::string api_clean = strip_markdown_code_blocks(api_raw);

        // Check if the API call failed (returned an error string)
        if (api_clean.find("[Error:") != std::string::npos || api_clean.find("Connection failed") != std::string::npos) {
            std::cerr << "  [Critical Error] API call failed. Skipping step.\n";
            current_index++;
            continue;
        }

        int val_retry = 0;
        std::string request_for_validation;
        if (current_index == 0) {
            // Root step: use the full PM instructions (which include "break into verifiable elements")
            request_for_validation = pm_expert_prompt;
        } else {
            // Sub-step: only the step's own description and prompt – no decomposition mandate
            request_for_validation = "Step description: " + step.description + ". Prompt: " + step.prompt;
        }
        // ---- STRUCTURAL VALIDATION ----


        // ---- NOW api_clean may have been corrected by the validation loop ----
        nlohmann::json pm_json;
        try {
            pm_json = nlohmann::json::parse(api_clean);
        } catch (const std::exception& e) {
            std::cerr << "  [Critical Error] JSON parse failure for AI PM action: " << e.what() << "\n";
            current_index++;
            continue;
        }

        std::string struct_error;
        if (!validate_pm_structure(pm_json, struct_error)) {
            std::cerr << "  [Structural Validation Error] " << struct_error << "\n";
            current_index++;
            continue;
        }


        if (!run_two_step_validation(request_for_validation, api_clean, "action.json", val_retry)) {            std::cerr << "  [Critical Error] AI PM action decision failed validation. Skipping step.\n";
            current_index++;
            continue;
        }

        // ---- RE-VALIDATE AFTER CORRECTION ----
        
        try {
            pm_json = nlohmann::json::parse(api_clean);
        } catch (const std::exception& e) {
            std::cerr << "  [Critical Error] JSON parse failure after correction: " << e.what() << "\n";
            current_index++;
            continue;
        }
        
        if (!validate_pm_structure(pm_json, struct_error)) {
            std::cerr << "  [Structural Validation Error after correction] " << struct_error << "\n";
            current_index++;
            continue;
        }

        std::string action = pm_json.value("action", "generate");

        if (action == "questions") {
            std::cout << "  [Action Questions] Project " << project_name << " has pending questions. Writing questions.txt...\n";
            std::ofstream q_file(questions_path);
            if (pm_json.contains("questions") && pm_json["questions"].is_array()) {
                for (const auto& q : pm_json["questions"]) {
                    q_file << "- " << q.get<std::string>() << "\n";
                    std::cout << "    ? " << q.get<std::string>() << "\n";
                }
            } else {
                q_file << "- Please clarify the target file parameters.\n";
            }
            q_file.close();
            return; // Pause execution for manual answer
        }
        else if (action == "decompose") {
            std::cout << "  [Action Decompose] Decomposing step dynamically into Tier " << step.tier + 1 << " sub-steps...\n";
            std::vector<PromptStep> new_substeps;
            if (pm_json.contains("substeps") && pm_json["substeps"].is_array()) {
                for (const auto& s : pm_json["substeps"]) {
                    PromptStep sub;
                    sub.description = s.value("description", "");
                    sub.prompt = s.value("prompt", "");
                    sub.output_file = s.value("output_file", "");
                    sub.tier = step.tier + 1;
                    if (s.contains("commands")) {
                        sub.commands = s["commands"];
                    }
                    new_substeps.push_back(sub);
                }
            }

            if (!new_substeps.empty()) {
                // Dynamically insert sub-steps immediately after current position in the work queue (Depth-First order!)
                steps_queue.insert(steps_queue.begin() + current_index + 1, new_substeps.begin(), new_substeps.end());
                std::cout << "    Inserted " << new_substeps.size() << " sub-steps recursively.\n";
            }
            current_index++; // Decomposed parent is completed, proceed to first child sub-step
        }
        else { // generate
            std::cout << "  [Action Generate] Detailed leaf task identified. Generating file contents...\n";

            // 1. Intercept and run any pre-command lists requested by AI
            std::string command_execution_results = "";
            if (!step.commands.is_null() && step.commands.is_array()) {
                for (const auto& cmd_obj : step.commands) {
                    std::string cmd_name = cmd_obj.value("name", "");
                    std::vector<std::string> cmd_args;
                    if (cmd_obj.contains("args") && cmd_obj["args"].is_array()) {
                        for (const auto& a : cmd_obj["args"]) {
                            cmd_args.push_back(a.get<std::string>());
                        }
                    }

                    // Prefix file paths for join and insert to generate cleanly inside output_dir
                    if ((cmd_name == "insert" || cmd_name == "join") && !cmd_args.empty()) {
                        cmd_args[0] = (output_dir / cmd_args[0]).string();
                        if (cmd_name == "join") {
                            for (size_t k = 1; k < cmd_args.size(); ++k) {
                                cmd_args[k] = (output_dir / cmd_args[k]).string();
                            }
                        }
                    }

                    std::cout << "    [Intercepted Command] Running AI requested command: " << cmd_name << "\n";
                    std::string cmd_res = run_external_command(cmd_name, cmd_args);
                    std::cout << "    [Intercepted Command] Result: " << cmd_res << "\n";
                    if (cmd_res.find("Error:") == std::string::npos) {
                        command_execution_results += "Command '" + cmd_name + "' output:\n" + cmd_res + "\n";
                    }
                }
            }

            // 2. Fulfill generation for each requested file
            nlohmann::json files_list;
            if (pm_json.contains("files") && pm_json["files"].is_array()) {
                files_list = pm_json["files"];
            } else if (!step.output_file.empty()) {
                nlohmann::json single_f;
                single_f["filename"] = step.output_file;
                single_f["description"] = step.prompt;
                files_list.push_back(single_f);
            }

            for (const auto& f : files_list) {
                if (!f.is_object()) {
                    std::cerr << "Warning: files entry is not an object, skipping." << std::endl;
                    continue;
                }                
                std::string fname = f.value("filename", "");
                std::string fdesc = f.value("description", "");
                if (fname.empty()) continue;

                std::cout << "    File: " << fname << " - generating...\n";

                // Build stateless query prompt
                std::string gen_prompt = accumulated_context;
                if (!command_execution_results.empty()) {
                    gen_prompt += "Latest Command Interceptions Results:\n" + command_execution_results + "\n";
                }
                gen_prompt += "Generate the full content for the file '" + fname + "'. Wrap ONLY the file content inside <content> tags. Do NOT include any other text outside the tags. Description: " + fdesc;

                std::string generated_content = call_ollama_api(gen_prompt, fname, dummy_sim);
                generated_content = strip_markdown_code_blocks(generated_content);

                // Two-step validation
                int file_retry = 0;
                bool is_binary = (fname.find(".png") != std::string::npos ||
                                   fname.find(".wav") != std::string::npos ||
                                   fname.find(".mp4") != std::string::npos);

                if (is_binary) {
                    std::cout << "    File " << fname << " is binary. Writing Python generator instead.\n";
                    std::string py_generator = "generate_" + fname.substr(0, fname.find('.')) + ".py";

                    std::string binary_prompt = "Generate a Python script named " + py_generator + " to create binary " + fname;
                    std::string py_code = call_ollama_api(binary_prompt, py_generator, dummy_sim);

                    int py_retry = 0;
                    std::string concise_gen_request = "Generate file " + fname + " with description: " + fdesc;
                    if (run_two_step_validation(concise_gen_request, generated_content, fname, file_retry)) {
                        std::ofstream py_file(output_dir / py_generator);
                        py_file << py_code;
                        py_file.close();

                        std::ofstream readme(output_dir / "README.md", std::ios_base::app);
                        readme << "# How to generate binary file " << fname << "\n";
                        readme << "Run the python generator script:\n";
                        readme << "```bash\npython3 " << py_generator << "\n```\n\n";
                        readme.close();

                        g_total_files_generated++;
                        std::cout << "    File " << fname << " complete (Py script generated)\n";
                    } else {
                        std::cerr << "    [Error] Failed to validate Py script for binary: " << fname << "\n";
                        g_total_files_failed++;
                    }
                } else {
                    std::string concise_gen_request = "File: " + fname + " – " + fdesc;
                    if (run_two_step_validation(concise_gen_request, generated_content, fname, file_retry)) {
//                    if (run_two_step_validation(gen_prompt, generated_content, fname, file_retry)) {
                        // Write file content
                        std::ofstream out_file(output_dir / fname);
                        out_file << generated_content;
                        out_file.close();

                        // After writing the file, if it's small, read and add to context
                        if (fs::file_size(output_dir / fname) < 2000) {
                            std::ifstream summary_file(output_dir / fname);
                            std::string content((std::istreambuf_iterator<char>(summary_file)),
                                                std::istreambuf_iterator<char>());
                            if (!content.empty()) {
                                accumulated_context += "Content of " + fname + ":\n" + content + "\n\n";
                            }
                        }

                        g_total_files_generated++;
                        std::cout << "    File " << fname << " complete (" << file_retry << " retries)\n";

                        accumulated_context += "File: " + fname + " was successfully created with contents.\n\n";
                    } else {
                        std::cerr << "    [Error] File " << fname << " failed validation retries.\n";
                        g_total_files_failed++;
                    }
                }
            }

            current_index++;
        }
    }

    // Final Project Summary
    std::cout << "Project " << project_name << " complete.\n";
    std::ofstream summary_out(output_dir / "generation_summary.txt");
    summary_out << "Project: " << project_name << "\n";
    summary_out << "Recursive steps processed: " << steps_queue.size() << "\n";
    summary_out.close();
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    // 1. Configuration File Loading
    std::string config_path = "config.json";
    if (fs::exists(config_path)) {
        try {
            std::ifstream config_file(config_path);
            nlohmann::json cfg_json = nlohmann::json::parse(config_file);
            config_file.close();

            g_config.model = cfg_json.value("model", g_config.model);
            g_config.api_address = cfg_json.value("api_address", g_config.api_address);
            g_config.dummy = cfg_json.value("dummy", g_config.dummy);
        } catch (const std::exception& e) {
            std::cerr << "[Warning] Could not parse config.json, using default configurations: " << e.what() << "\n";
        }
    } else {
        nlohmann::json cfg_json;
        cfg_json["model"] = g_config.model;
        cfg_json["api_address"] = g_config.api_address;
        cfg_json["dummy"] = g_config.dummy;

        std::ofstream config_file(config_path);
        if (config_file.is_open()) {
            config_file << cfg_json.dump(4);
            config_file.close();
        }
    }

    // 2. Command Line Argument Support
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--model" || arg == "-m") && i + 1 < argc) {
            g_config.model = argv[++i];
        } else if ((arg == "--api" || arg == "-a") && i + 1 < argc) {
            g_config.api_address = argv[++i];
        } else if (arg == "--dummy" || arg == "-d") {
            g_config.dummy = true;
        }
    }

    std::cout << "AutonomousAdministration Stateless AI Orchestrator Initialized\n";
    std::cout << "Model: " << g_config.model << "\n";
    std::cout << "API Address: " << g_config.api_address << "\n";
    std::cout << "Dummy/Mock Mode: " << (g_config.dummy ? "ENABLED" : "DISABLED") << "\n\n";

    curl_global_init(CURL_GLOBAL_ALL);

    // 3. Project Scanning & Processing
    fs::path projects_dir("projects");
    if (!fs::exists(projects_dir)) {
        fs::create_directories(projects_dir);
    }

    try {
        for (const auto& entry : fs::directory_iterator(projects_dir)) {
            if (entry.is_directory()) {
                process_project(entry.path());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Critical Error] Failed recursively scanning projects: " << e.what() << "\n";
        curl_global_cleanup();
        return 1;
    }

    // ============================================================================
    // SUMMARY GENERATION (Step 12)
    // ============================================================================
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Projects processed: " << g_total_projects_processed << "\n";
    std::cout << "Total files generated: " << g_total_files_generated << "\n";
    std::cout << "Total files failed: " << g_total_files_failed << "\n";

    curl_global_cleanup();
    return 0;
}
