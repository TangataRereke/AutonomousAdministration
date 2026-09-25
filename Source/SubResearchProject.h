#ifndef SUBRESEARCHPROJECT_H
    #define SUBRESEARCHPROJECT_H
    #include "ISubProject.h"

    class SubResearchProject : public ISubProject{
        public:
            SubResearchProject(std::string pJson, std::string path) : ISubProject(pJson, path){};
            std::string getWorkerName(){return "Research Project";}
            ~SubResearchProject(){}

            const std::string COMMAND_RESEARCH_IMAGE_FOR_DRAWING = "researchImageForDrawing";
            const std::string COMMAND_RESEARCH_IMAGE_PLANNING = "researchImageForPlanning";
            const std::string COMMAND_RESEARCH_MERGE_RESEARCH = "researchMergeResearch";

        protected:
            std::string getPlanningInstructions();
            void getSubCommands(nlohmann::json &commandTypesJson);
            std::string fuzzyLogicSubCheckPlanning(short unsigned expectedNumber, std::string taskCommand, nlohmann::json element);
        private:
    };

#endif