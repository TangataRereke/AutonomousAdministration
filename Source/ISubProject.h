#ifndef ISUBPROJECT_H
    #define ISUBPROJECT_H
    
    #include "IWorker.h"
#include <list>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include "APIRequest.h"
    class ISubProject : public IWorker{
        public:
            void initialise();
            void runTask();
            void finalise();
            virtual std::string getWorkerName() = 0;
            ISubProject(std::string setupJson, std::string projectPath);
            ~ISubProject();
            const std::string COMMAND_RESIZE_IMAGE = "resizeIMAGE";
            const std::string COMMAND_SPLIT_TILE_IMAGE = "splitTileIMAGE";
            const std::string COMMAND_PARAMETER_INPUTS = "inputs";
            const std::string COMMAND_PARAMETER_OUTPUTS = "outputs";
            const std::string COMMAND_PARAMETER_DESCRIPTION = "description";
        protected:
            virtual std::string getPlanningInstructions() = 0;
            virtual void getSubCommands(nlohmann::json &commandTypesJson) = 0;
            virtual std::string fuzzyLogicSubCheckPlanning(short unsigned expectedNumber, std::string taskCommand, nlohmann::json element) = 0;
        private:
            struct ProcessingFile {
                std::string fileName = "";
                std::string description = "";
            };
            std::list<ProcessingFile*> allInputs;
            std::list<ProcessingFile*> allOutputs;
            std::string requirementPrompt = "";
            bool planProject(std::string errors = "", std::string lastResponse = "");
            std::string getCommands();
            std::string checkPlanning(std::string response);
            std::string fuzzyLogicCheckPlanning(nlohmann::json responseJson);
    };

#endif