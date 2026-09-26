#include "ISubProject.h"
#include "APIRequest.h"
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <nlohmann/json_fwd.hpp>
#include <string>

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

ISubProject::ISubProject(std::string setupJson, std::string projectPath){
    this->projectPath = projectPath;
    nlohmann::json json;
    try{
        json = nlohmann::json::parse(setupJson);
        requirementPrompt = json["requirementPrompt"];
        if(json.contains("inputFiles")){
            for(auto element : json["inputFiles"]){
                ProcessingFile *newFile = new ProcessingFile;
                newFile->fileName = std::string(element["fileName"]);
                newFile->description = element["description"];
                allInputs.push_back(newFile);
            }
        }
        for(auto element : json["outputFiles"]){
            ProcessingFile *newFile = new ProcessingFile;
            newFile->fileName = std::string(element["fileName"]);
            newFile->description = element["description"];
            allOutputs.push_back(newFile);
        }        
    }catch(std::exception exc){
        std::cerr << "Critical error creating ISubProject " << exc.what() << std::endl;
        std::cout << setupJson << std::endl;
        exit(1);
    }catch(...){
        std::cerr << "Critical error creating ISubProject " << std::endl;
        std::cout << setupJson << std::endl;
        exit(1);
    }

}

void ISubProject::initialise(){
    if(allInputs.size()){
        try{
            std::filesystem::create_directory(projectPath + "/Input");
        }catch(std::exception exc){
            std::cerr << exc.what() << "| ISubProject::initialise could not create " + projectPath + "/Input" << std::endl;
            exit(1);

        }catch(...){
            std::cerr << "ISubProject::initialise could not create " + projectPath + "/Input" << std::endl;
            exit(1);
        }
        for(ProcessingFile *file : allInputs){
            std::filesystem::path leaf(file->fileName);
            leaf = leaf.filename();
            if(!std::filesystem::exists(projectPath + "/Input/" + leaf.string())){
                std::filesystem::copy_file(projectPath + "/../InputFiles/" + leaf.string(), projectPath + "/Input/" + leaf.string());
                std::cout << projectPath << " - " << leaf.string() << std::endl;

            }
        }
    }
    std::filesystem::create_directory(projectPath + "/Output");
}

bool ISubProject::planProject(std::string errors, std::string lastResponse){
    std::string instructions = "You are the project planner for a " + getWorkerName() + " project. Your job is to split the project into the smallest tasks to produce the output files. Use the requirementPrompt to determine what the actual requirement is that needs to be broken down. Follow the outputFormat on how to format the response. Look at outputExample for an example. You ***MUST*** use ONLY commands provided, using the commandParameters. This is for an API agent pipeline and will fail otherwise.\n";
    instructions += "You must thing through methodically, split into as many tasks as required. If there is multiple files to analyse then ensure you have separate tasks for each file. Do ***NOT*** skip steps. IF You do not think through each step methodically then the project will be ruined, and all the time will be wasted on a broken project!\n";
    nlohmann::json apiJson;
    if(allInputs.size()>0){
        instructions += "The inputFiles contains inputFiles that will help for figuring out the project.\n";
        for(ProcessingFile *file : allInputs){
            nlohmann::json subJson;
            subJson["fileName"] = file->fileName;
            subJson["description"] = file->description;
            apiJson["inputFiles"].push_back(subJson);
        }
        
    }
        for(ProcessingFile *file : allOutputs){
            nlohmann::json subJson;
            subJson["fileName"] = file->fileName;
            subJson["description"] = file->description;
            apiJson["outputFiles"].push_back(subJson);
        }

    instructions += "The outputFiles contains the overall outputFiles for the process. These files MUST be created as they will be used by the next API project.\n";
    
    if(errors.length()){
        instructions += "***IMPORTANT*** This has already been processed and has errors. Please fix the errors mentioned in the errors tag. Use the lastResponse tag to fix these errors.\n";
        apiJson["errors"] = errors;
        apiJson["lastResponse"] = lastResponse;
    }

    apiJson["commands"] = getCommands();
    apiJson["instructions"] = instructions;
    apiJson["requirementPrompt"] = requirementPrompt;
    apiJson["outputFormat"] =
        std::string(R"(
    Here is the return format in JSON:
    "tasks":[
    {"taskNumber": "numeric only, the incremental requirement",
        "taskName": "name of the task, please use a file safe format, e.g. Instead of spaces use an underscore",
        "taskCommand": "Must be a taskCommand",
        "prompt": "This is the prompt that is to be sent to the AI pipeline for processing. Ensure it is explicit and detailed enough for the AI API agent to understand to fulfill its job.",
        "inputs": [An array of inputs, please refer to taskCommand details on what these are],
        "outputs": [An array of outputs, please refer to taskCommand details on what these are]
    }
    ])");
    apiJson["outputExample"] = "";
    
    std::ofstream testDump(projectPath + "/test.json");
    testDump << apiJson.dump();
    testDump.close();


    APIRequest request;
    request.setup(apiJson.dump(), APIRequest::ROLE_PROJECT_PLANNER, projectPath + "/", "planTasks", APIRequest::MODEL_REQUIREMENTS, 16384);
    request.run(projectPath);
    std::string newErrors = checkPlanning(request.getResponse());
    if(newErrors.length()>0){
        return planProject(newErrors, request.getResponse());
    }
    return true;
}

std::string ISubProject::getCommands(){
    nlohmann::json commandJson;
    commandJson[COMMAND_RESIZE_IMAGE][COMMAND_PARAMETER_DESCRIPTION] = "Resizes the inputImage to inputX and inputY and outputs the resulting image to outputImage. Use scaleType to be any of the scaleType.";
    commandJson[COMMAND_RESIZE_IMAGE]["scaleType"]["chop"] = "Chops the edges evenly of the longest size to fit.";
    commandJson[COMMAND_RESIZE_IMAGE]["scaleType"]["border"] = "Adds a border evenly on the shortest size, please use use a new input of borderColour which contains the RGB colour to add the border with. e.g. #000000 is black.";
    commandJson[COMMAND_RESIZE_IMAGE]["scaleType"]["stretch"] = "Image will be stretched.";
    commandJson[COMMAND_RESIZE_IMAGE][COMMAND_PARAMETER_INPUTS]["inputImage"] = "The file name (without path) to be resized.";
    commandJson[COMMAND_RESIZE_IMAGE][COMMAND_PARAMETER_INPUTS]["inputX"] = "The new width of the image.";
    commandJson[COMMAND_RESIZE_IMAGE][COMMAND_PARAMETER_INPUTS]["inputY"] = "The new height of the image.";
    commandJson[COMMAND_RESIZE_IMAGE][COMMAND_PARAMETER_INPUTS]["scaleType"] = "The type of scaling to be used. This MUST be one of the scaleType, it also MUST be specified..";
    commandJson[COMMAND_RESIZE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImage"] = "The file name (without path) of the output file.";

    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_DESCRIPTION] = "Splits the single image of inputImage into tilesX and tilesY and save it to outputImages. ***DO NOT*** use separate images as an input, separate images need a different task! Remember the maximum size is 336x336 SO ensure you resize the image to be split to be 1008x1008 first. This will ensure each TILE is 336 rather than 112x112 tiles.\n";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_INPUTS]["inputImage"] = "The file name (without path) to be split.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_INPUTS]["tilesX"] = "How many columns to split the image into.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_INPUTS]["tilesY"] = "How many rows to split the image into.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["description"] = "An array of files (without path) to be split. There ***MUST*** be an image for each cell, so a 3x3 split will have 9 output files.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["x"] = "The column of the image, start at 0. e.g. 3 tiles will have 0, 1 and 2.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["y"] = "The row of the image, start at 0. e.g. 3 tiles will have 0, 1 and 2.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["fileName"] = "The output file name of each image tile.";

    getSubCommands(commandJson);
    return commandJson.dump();
}


std::string ISubProject::checkPlanning(std::string response){
    nlohmann::json responseAsJson;
    std::string errors = "";
    try{
        responseAsJson = nlohmann::json::parse(response);
    }catch(...){
        return "The response was an invalid JSON";
    }
    errors += fuzzyLogicCheckPlanning(responseAsJson);
    return errors;
}
std::string ISubProject::fuzzyLogicCheckPlanning(nlohmann::json responseJson){

    if(!responseJson.contains("tasks")){
        return "The response does not contain a tasks array.";
    }
    if(!responseJson["tasks"].is_array()){
        return "The response must contain tasks as an array";
    }
    std::string errors = "";
    short unsigned expectedNumber = 0;
    for(auto element : responseJson["tasks"]){
        expectedNumber++;
        if(!element.contains("taskNumber")){
            errors += "A task does not contain 'taskNumber' at taskNumber " + std::to_string(expectedNumber) + "\n";
        }else if(element["taskNumber"].is_string()&&element["taskNumber"]!=std::to_string(expectedNumber)){
            errors += "taskNumber " + std::string(element["taskNumber"]) + " expected to be " + std::to_string(expectedNumber) + "\n";
        }else if(element["taskNumber"].is_number()&&element["taskNumber"]!=expectedNumber){
            errors += "taskNumber " + std::string(element["taskNumber"]) + " expected to be " + std::to_string(expectedNumber) + "\n";
        }
        if(!element.contains("taskCommand")){
            errors += "task Number " + std::to_string(expectedNumber) + " does not have a task command!\n";
            continue;
        }
        std::string taskCommand = element["taskCommand"];


        if(taskCommand==COMMAND_RESIZE_IMAGE){
            if(!element.contains("scaleType")){
                errors += "Task number " + std::to_string(expectedNumber) + " does not have a scaleType\n";
            }else{
                std::string scaleType = element["scaleType"];
                if((!(scaleType=="chop"||scaleType=="border"||scaleType=="stretch"))){
                    errors += "Task number " + std::to_string(expectedNumber) + " scaleType of " + scaleType + " is invalid.\n";
                }
            }
            std::string inputImage = "";
            if(!element.contains("inputImage")){
                errors += "Task number " + std::to_string(expectedNumber) + " does not have an inputImage.\n";
            }else{
                inputImage = element["inputImage"];
                std::string ext = lower(inputImage);
                if(!(inputImage.ends_with(".png")!=std::string::npos||
                    inputImage.ends_with(".jpg")!=std::string::npos||
                    inputImage.ends_with(".jpeg")!=std::string::npos||
                    inputImage.ends_with(".gif")!=std::string::npos
                )){
                    errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid inputImage.\n";
                }
                if(inputImage.find("/")!=std::string::npos||inputImage.find("\\")!=std::string::npos){
                    errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid inputImage as it contains a path.\n";
                }
            }
            int inputX = 0;
            if(!element.contains("inputX")){
                errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid inputX.\n";                
            }else{
                if(element["inputX"].is_number()){
                    inputX = element["inputX"];
                }else{
                    std::string inputXS = element["inputX"];
                    inputX = stoi(inputXS);
                }
                if(inputX <= 10){
                    errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid inputX of " + std::to_string(inputX) + ".\n";                
                }
            }
            int inputY = 0;
            if(!element.contains("inputY")){
                errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid inputY.\n";                
            }else{
                if(element["inputY"].is_number()){
                    inputX = element["inputY"];
                }else{
                    std::string inputYS = element["inputY"];
                    inputY = stoi(inputYS);
                }
                if(inputY <= 10){
                    errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid inputY of " + std::to_string(inputY) + ".\n";                
                }
            }
            std::string outputImage = "";
            if(!element.contains("outputImage")){
                errors += "Task number " + std::to_string(expectedNumber) + " does not have an outputImage.\n";
            }else{
                outputImage = element["outputImage"];
                std::string ext = lower(outputImage);
                if(!(outputImage.ends_with(".png")!=std::string::npos||
                    outputImage.ends_with(".jpg")!=std::string::npos||
                    outputImage.ends_with(".jpeg")!=std::string::npos||
                    outputImage.ends_with(".gif")!=std::string::npos
                )){
                    errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid outputImage.\n";
                }
                if(outputImage.find("/")!=std::string::npos||outputImage.find("\\")!=std::string::npos){
                    errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid outputImage as it contains a path.\n";
                }
                if(inputImage==outputImage){
                    errors += "Task number " + std::to_string(expectedNumber) + " cannot have the same outputImage as the inputImage.\n";
                }
            }
        }else if(taskCommand==COMMAND_SPLIT_TILE_IMAGE){
            std::string inputImage = "";
            if(!element.contains("inputImage")){
                errors += "Task number " + std::to_string(expectedNumber) + " does not have an inputImage.\n";
            }else{
                inputImage = element["inputImage"];
                std::string ext = lower(inputImage);
                if(!(inputImage.ends_with(".png")!=std::string::npos||
                    inputImage.ends_with(".jpg")!=std::string::npos||
                    inputImage.ends_with(".jpeg")!=std::string::npos||
                    inputImage.ends_with(".gif")!=std::string::npos
                )){
                    errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid inputImage.\n";
                }
                if(inputImage.find("/")!=std::string::npos||inputImage.find("\\")!=std::string::npos){
                    errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid inputImage as it contains a path.\n";
                }
            }
            int tilesX = 0;
            if(!element.contains("tilesX")){
                errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid tilesX.\n";                
            }else{
                if(element["tilesX"].is_number()){
                    tilesX = element["tilesX"];
                }else{
                    std::string inputXS = element["tilesX"];
                    tilesX = stoi(inputXS);
                }
            }
            int tilesY = 0;
            if(!element.contains("tilesY")){
                errors += "Task number " + std::to_string(expectedNumber) + " does not have a valid tilesY.\n";                
            }else{
                if(element["tilesY"].is_number()){
                    inputX = element["tilesY"];
                }else{
                    std::string inputYS = element["tilesY"];
                    tilesY = stoi(inputYS);
                }
                if(tilesY+tilesX <= 1){
                    errors += "Task number " + std::to_string(expectedNumber) + " seem to have invalid tilesX or tilesY.\n";                
                }
            }            
        }else{
            std::string subErrors = fuzzyLogicSubCheckPlanning(expectedNumber, taskCommand, element);
            if(subErrors.length()>0){
                errors += subErrors;
            }
        }
        if(tilesX>0&&tilesY>0){
            f
        }
    }

    /*
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_DESCRIPTION] = "Splits the single image of inputImage into tilesX and tilesY and save it to outputImages. ***DO NOT*** use separate images as an input, separate images need a different task!";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_INPUTS]["inputImage"] = "The file name (without path) to be split.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_INPUTS]["tilesX"] = "How many columns to split the image into.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_INPUTS]["tilesY"] = "How many rows to split the image into.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["description"] = "An array of files (without path) to be split. There ***MUST*** be an image for each cell, so a 3x3 split will have 9 output files.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["x"] = "The column of the image, start at 0. e.g. 3 tiles will have 0, 1 and 2.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["y"] = "The row of the image, start at 0. e.g. 3 tiles will have 0, 1 and 2.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["fileName"] = "The output file name of each image tile.";
    commandJson[COMMAND_SPLIT_TILE_IMAGE][COMMAND_PARAMETER_OUTPUTS]["outputImages"]["description"] = "The output file description.";

    getSubCommands(commandJson);*/
    return "";
}

void ISubProject::runTask(){
    if(planProject()){

    }
}

ISubProject::~ISubProject(){
    for(ProcessingFile *file : allInputs){
        delete file;
    }
    for(ProcessingFile *file : allOutputs){
        delete file;
    }

}

void ISubProject::finalise(){

}