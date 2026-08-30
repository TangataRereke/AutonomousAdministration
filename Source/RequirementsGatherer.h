#ifndef REQUIREMENTS_GATHERER_H
    #define REQUIREMENTS_GATHERER_H
    #include "IWorker.h"
#include "IWorker.h"
#include <nlohmann/json_fwd.hpp>
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
            void gatherRequirements();
            std::string fuzzyLogicCheckGatherRequirements(std::string response);
            bool askQuestions(std::string feedback = "", std::string lastResponse = "");
            std::string vetQuestions(std::string request, std::string response);
            bool vetQuestionsVet(std::string request, std::string response, std::string vetResponse);
            nlohmann::json questionsJson;
            std::string instructions = "";
    };
#endif