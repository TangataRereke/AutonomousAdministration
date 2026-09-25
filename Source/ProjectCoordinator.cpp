#include "ProjectCoordinator.h"
#include "RequirementsGatherer.h"
#include <exception>
#include <iostream>
#include <nlohmann/json_fwd.hpp>
#include <string>

ProjectCoordinator::ProjectCoordinator(std::string projectName, std::string instructions, bool topLevel){
    this->projectName = projectName;
    this->instructions = instructions;
    this->topLevel = topLevel;
}

void ProjectCoordinator::initialise(){

}

void ProjectCoordinator::finalise(){

}

void ProjectCoordinator::runTask(){
    
    if(topLevel){
        RequirementsGatherer requirementsGatherer(instructions);
        requirementsGatherer.run(projectPath);
    }
    runProjects();
}

void ProjectCoordinator::runProjects(){
    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& entry : std::filesystem::directory_iterator(projectPath)) {
        if (entry.is_directory()) {
            entries.push_back(entry);
        }
    }

    std::sort(entries.begin(), entries.end(),
        [](const std::filesystem::directory_entry& a,
           const std::filesystem::directory_entry& b){
            return a.path().filename().string() < b.path().filename().string();
    });

        for(const auto& entry : entries){
        if(entry.is_directory()){
            std::string instructionPath = entry.path();
            instructionPath.append("/requirements.json");
            if(std::filesystem::exists(instructionPath)){
                std::fstream inFile(instructionPath, std::ios::in);
                if(!inFile.is_open()){
                    std::cerr << "Could not open file " << instructionPath << std::endl;
                    exit(1);
                }
                std::stringstream instructionStream;
                instructionStream << inFile.rdbuf();
                try{
                    inFile.close();
                }catch(std::exception excp){
                    std::cerr << "File close error running project for " << instructionPath << ", " << excp.what() << std::endl;
                    exit(1);
                }catch(...){
                    std::cerr << "File close error running project for " << instructionPath  << std::endl;
                    exit(1);
                }

                nlohmann::json json;
                try{
                    json = nlohmann::json::parse(instructionStream.str());
                }catch(...){
                    std::cout << "Error running the project " << instructionPath << " just skipping" << std::endl;
                    continue;
                }

                if(!json.contains("outputFiles")||!json["outputFiles"].is_array()||json["outputFiles"].size()==0){
                    std::cerr << "Output files is invalid for running a project " << instructionPath << std::endl;
                    exit(1);
                }

                if(!json.contains("requirementPrompt")){
                    std::cerr << "Requirement prompt is invalid for running a project " << instructionPath << std::endl;
                    exit(1);
                }

                std::string requirementType = "";
                try{
                    requirementType = json["requirementType"];
                }catch(...){
                    std::cerr << "Json with no requirement type in run project, skipping" << std::endl;
                    continue;
                }
                

                ISubProject *subProject = 0;
                if(requirementType == "Research Project"){
                    subProject = new SubResearchProject(instructionStream.str(), entry.path());
                
                }

                if(!subProject){
                    std::cerr << "Invalid project type to run of " << requirementType << std::endl;
                    exit(1);
                }

                subProject->run(entry.path());

                if(subProject){
                    delete subProject;
                }
                //ProjectCoordinator coordinator(std::string(entry.path().filename()) + "/", instructionStream.str(), true);
                //coordinator.run(std::string(entry.path()) + "/");
            }
        }
    }
}
    
std::string ProjectCoordinator::getProjectTypes(){
    nlohmann::json returnJson;
    for(ProjectType projectType : ProjectTypes){
        returnJson[projectType.name] = projectType.description;

    }
    return returnJson.dump();
}