#include "ProjectScanner.h"
#include "ProjectCoordinator.h"
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
void ProjectScanner::initialise(){
    if(!std::filesystem::exists(PATH)){
        std::filesystem::create_directories(PATH);
    }
}
void ProjectScanner::finalise(){

}
void ProjectScanner::runTask(){
    for(const auto& entry : std::filesystem::directory_iterator(projectPath)){
        if(entry.is_directory()){
            std::string instructionPath = entry.path();
            instructionPath.append("/instructions.txt");
            if(std::filesystem::exists(instructionPath)){
                std::fstream inFile(instructionPath, std::ios::in);
                if(!inFile.is_open()){
                    std::cerr << "Could not open file " << instructionPath << std::endl;
                    exit(1);
                }
                std::stringstream instructionStream;
                instructionStream << inFile.rdbuf();
                inFile.close();
                ProjectCoordinator coordinator(std::string(entry.path().filename()) + "/", instructionStream.str(), true);
                coordinator.run(std::string(entry.path()) + "/");
            }
        }
    }
}
