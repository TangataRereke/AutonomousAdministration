#include "APIRequest.h"
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <nlohmann/json_fwd.hpp>

void APIRequest::setup(std::string request, std::string role, std::string projectPath, std::string requestName, std::string model, int contextSize){
    this->requestName = requestName;
    this->stringRequest = request;
    this->model = model;
    this->role = role;
    this->projectPath = projectPath;
    this->requestName = requestName;
    this->requestFileName = projectPath + requestName + "_request.json";
    this->responseFileName = projectPath + requestName + "_response.json";
    this->contextSize = contextSize;
}

void APIRequest::initialise(){
    if(this->model==""){
        std::cerr << "Error APIRequest needs a model";
        exit(1);
    }
    if(this->role==""){
        std::cerr << "Error APIRequest needs a role";
        exit(1);
    }
    curl = curl_easy_init();
    if(!curl){
        std::cerr << "Error APIRequest curl did not initialise";
        exit(1);
    }
}

void APIRequest::finalise(){

}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void APIRequest::extract_stats(const nlohmann::json& response, GlobalStats& stats) {
    // Store them separately, exactly as they come

    GlobalStats newStats;
    newStats.prompt_tokens = response["prompt_eval_count"];
    newStats.response_tokens = response["eval_count"];
    newStats.prompt_duration_ns = response["prompt_eval_duration"];
    newStats.response_duration_ns = response["eval_duration"];
    newStats.total_duration_ns = response["total_duration"];
    stats.prompt_tokens += newStats.prompt_tokens;
    stats.response_tokens += newStats.response_tokens;
    stats.prompt_duration_ns += newStats.prompt_duration_ns;
    stats.response_duration_ns += newStats.response_duration_ns;
    stats.total_duration_ns += newStats.total_duration_ns;

    std::string statsPath = projectPath + "_stats.csv";
    if(!std::filesystem::exists(statsPath)){
        std::ofstream header(statsPath, std::ios_base::out);
        header << "prompt_type,model,attempt,prompt_tokens,response_tokens,prompt_ns,prompt_ms,prompt_s,prompt_m,response_ns,response_ms,response_s,response_m" << std::endl;
        header.close();
    }
    std::ofstream statsFile(statsPath, std::ios_base::out | std::ios_base::app);
    statsFile << requestName << "," << model << "," << attempts << "," << newStats.prompt_tokens << "," << newStats.response_tokens << "," << newStats.prompt_duration_ns << "," << (double)newStats.prompt_duration_ns / 1000000 << "," << (double)newStats.prompt_duration_ns / (1000000*1000) << "," << (double)newStats.prompt_duration_ns / ((unsigned int)10000000*60000) << "," << newStats.response_duration_ns << "," << (double)newStats.response_duration_ns / 1000000 << "," << (double)newStats.response_duration_ns / (1000000*1000) << "," << (double)newStats.response_duration_ns / ((unsigned int)10000000*60000) << std::endl;
    statsFile.close();
}

/*std::string exec(const char* cmd) {
     std::array<char, 128> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result; 
}*/

void APIRequest::stopAllOllamaModels() {
/*     std::string ps = exec("ollama ps");

    std::istringstream iss(ps);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.find("running") != std::string::npos ||
            line.find("idle") != std::string::npos) {

            // Extract model name (first column)
            std::string model = line.substr(0, line.find_first_of(" \t"));
            std::string cmd = "ollama stop " + model;
            exec(cmd.c_str());
        }
    } */
     std::cout << "STOP ALL MODELS DOES NOT NEED TO RUN NOW AS OLLAMA DOES THIS AUTOMATICALLY" << std::endl;
}

void APIRequest::runTask(){
    attempts++;
    if(std::filesystem::exists(responseFileName)&&std::filesystem::exists((requestFileName))){
        std::ifstream requestFile(requestFileName);
        std::stringstream requestBuffer;
        requestBuffer << requestFile.rdbuf();
        if(requestBuffer.str()==stringRequest){
            std::ifstream responseFile(responseFileName);
            std::stringstream responseBuffer;
            responseBuffer << responseFile.rdbuf();
            stringResponse = responseBuffer.str();
            return;
        }

    }

    std::ofstream requestFile(requestFileName, std::ios_base::out);
    requestFile << stringRequest;
    requestFile.close();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    nlohmann::json instructions;
    instructions["your_role"] = role;
    instructions["response_type"] = "json";
    instructions["request"] = stringRequest;

    std::cout << "REQUEST NAME:" << requestName << std::endl;
    std::cout << "Request file:  " << requestFileName << std::endl;
    std::cout << "Response file: " << responseFileName << std::endl;
    std::cout << "StringRequest size: " << stringRequest.size() << std::endl;
    std::cout << "MODEL: " << model << std::endl;

    nlohmann::json payload;
    payload["model"] = model;
    payload["stream"] = false;
    if(contextSize>0){
        payload["options"]["num_ctx"] = contextSize;
    }

    // Add these memory-saving options
    payload["options"]["num_gpu"] = 0;      // Force CPU only (uses system RAM, not VRAM)
    payload["options"]["f16_kv"] = true;    // Half-precision KV cache (saves ~50% cache RAM)
    payload["options"]["num_threads"] = 8;  // Adjust to your physical core count
    payload["options"]["batch_size"] = 256; // Smaller batch = less memory

    payload["messages"] = nlohmann::json::array({
        {
            {"role", "user"},
            {"content", instructions.dump()}
        }
    });
    std::string json_str = payload.dump();
    
    curl_easy_setopt(curl, CURLOPT_URL, apiAddress.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stringResponse);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6000L);
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
        //backoff_ms *= 2;
    }

    // Parse the response JSON
    nlohmann::json response;
    try {
        response = nlohmann::json::parse(stringResponse);
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << e.what() << " in APIRequest::runTask, retrying. Attempt: " << attempts << std::endl;
        std::cerr << "FULL RESPONSE: " << stringResponse << std::endl;
        runTask();
        return;
    }

    try{
        stringResponse = extractInnerJson(stringResponse);
    } catch (const std::exception& e) {
        std::cerr << "Request file:  " << requestFileName << std::endl;
        std::cerr << "Response file: " << responseFileName << std::endl;
        std::cerr << "StringRequest size: " << stringRequest.size() << std::endl;

        std::cerr << e.what() << " in APIRequest::runTask, retrying. Attempt: " << attempts << std::endl;
        std::cout << "String response: " << stringResponse << std::endl;
        runTask();
        return;
    }catch(...){
        std::cerr << "Request file:  " << requestFileName << std::endl;
        std::cerr << "Response file: " << responseFileName << std::endl;
        std::cerr << "StringRequest size: " << stringRequest.size() << std::endl;

        std::cerr << "Unknonwn error  in APIRequest::runTask, retrying. Attempt: " << attempts << std::endl;
        std::cout << "String response: " << stringResponse << std::endl;
        runTask();
        return;
    }
    extract_stats(response, globalStats);

    // Extract stats into the global variable
    std::ofstream responseFile(this->responseFileName, std::ios_base::out);
    responseFile << stringResponse;
    responseFile.close();

}

std::string APIRequest::extractInnerJson(std::string rawResponse) {
    nlohmann::json outer = nlohmann::json::parse(rawResponse);
    std::string content = outer["message"]["content"];

    size_t start = content.find('{');
    if (start == std::string::npos) {
        throw std::runtime_error("No JSON object found");
    }

    int depth = 0;
    size_t end = std::string::npos;

    for (size_t i = start; i < content.size(); ++i) {
        if (content[i] == '{') depth++;
        if (content[i] == '}') depth--;
        if (depth == 0) {
            end = i;
            break;
        }
    }

    if (end == std::string::npos) {
        throw std::runtime_error("JSON braces not balanced");
    }

    return content.substr(start, end - start + 1);
}
