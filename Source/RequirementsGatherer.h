#ifndef REQUIREMENTS_GATHERER_H
    #define REQUIREMENTS_GATHERER_H
    #include "IWorker.h"
#include "IWorker.h"
#include <nlohmann/json_fwd.hpp>
#include <map>
#include <unordered_set>
#include <string>
    #include "APIRequest.h"

    class RequirementsGatherer : public IWorker{
        public:
            RequirementsGatherer(std::string instructions);
        protected:
            void initialise();
            void runTask();
            void finalise();
            std::string getWorkerName() { return "Requirements Gatherer";};        
        private:
            void gatherRequirements(std::string lastResponse = "", std::string vettedResponse = "");
            std::string fuzzyLogicCheckGatherRequirements(const std::string& response);
            bool askQuestions(std::string feedback = "", std::string lastResponse = "");
            std::string vetQuestions(std::string request, std::string response);
            std::string vetRequirements(std::string request, std::string response);
            bool vetQuestionsVet(std::string request, std::string response, std::string vetResponse);
            bool vetRequirementsVet(std::string request, std::string response, std::string vetResponse);
            bool textAppearsInRequirements(const std::string& needle,  const nlohmann::json& requirements);
            nlohmann::json questionsJson;
            std::string instructions = "";
            void processSubProjects();
            std::string standardiseFolderName(std::string pNumber, std::string pName);
            bool isValidProjectType(std::string projectType);
    };
#endif