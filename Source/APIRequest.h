#ifndef APIREQUEST_H
    #define APIREQUEST_H
    #include "IWorker.h"
    #include <string>
    #include <curl/curl.h>
    #include <nlohmann/json.hpp>
    #include <thread>

    class APIRequest : public IWorker{
        public:
            inline static std::string MODEL_CODER{"codestral:22b-v0.1-q8_0"};
            inline static std::string MODEL_GENERAL{"hf.co/bartowski/mistralai_Mistral-Small-3.2-24B-Instruct-2506-GGUF:Q6_K"};
            //inline static std::string MODEL_REQUIREMENTS{"qwen3:30b-a3b-instruct-2507-q4_K_M"};
            inline static std::string MODEL_REQUIREMENTS{"hf.co/bartowski/mistralai_Mistral-Small-3.2-24B-Instruct-2506-GGUF:Q6_K"};
            inline static std::string MODEL_VISION{"llava:13b-v1.6-vicuna-q8_0"};
            inline static std::string ROLE_REQUIREMENTS_GATHERING = "Project Requirements Gatherer";
            inline static std::string ROLE_REQUIREMENTS_VETTER = "Project Requirements Vetter";
            inline static std::string ROLE_PROJECT_PLANNER = "Project Planner";
            inline static std::string ROLE_PROJECT_PLANNER_VETTER = "Project Planner Vetter";
            void stopAllOllamaModels(/*NOT REQUIRED AUTOMATIC*/);
            std::string getResponse() { return stringResponse;}
            void setup(std::string request, std::string role, std::string projectPath, std::string requestName,  std::string model = MODEL_GENERAL, int contextSize = -1);
        protected:
            void initialise();
            void finalise();
            void runTask();
            std::string getWorkerName() { return "API Request";}
        private:
            std::string requestFileName = "";
            std::string responseFileName = "";
            std::string projectPath = "";
            std::string requestName = "";
            std::string stringRequest = "";
            std::string stringResponse = "";
            std::string model = MODEL_GENERAL;
            std::string role = "";
            short unsigned attempts = 0;
            CURL *curl = 0;
            inline static std::string apiAddress = "http://localhost:11434/api/chat";
            int contextSize = -1;


            struct GlobalStats {
                // Token counts (separate)
                long long prompt_tokens;
                long long response_tokens;

                // Durations in nanoseconds (separate)
                long long prompt_duration_ns;    // Time to process input
                long long response_duration_ns;  // Time to generate output
                long long total_duration_ns;     // Wall-clock (optional, but keep for sanity)
            };
            void extract_stats(const nlohmann::json& response, GlobalStats& stats);
            GlobalStats globalStats;

            std::string extractInnerJson(std::string rawResponse);
    };

#endif