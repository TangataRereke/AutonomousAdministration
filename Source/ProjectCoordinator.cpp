#include "ProjectCoordinator.h"
#include "RequirementsGatherer.h"
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
}

std::string ProjectCoordinator::getProjectTypes(){
    nlohmann::json returnJson;
    for(ProjectType projectType : ProjectTypes){
        returnJson[projectType.name] = projectType.description;

    }
    return returnJson.dump();
}