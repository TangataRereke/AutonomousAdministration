#include "SubResearchProject.h"
#include <string>

std::string SubResearchProject::getPlanningInstructions(){
    return "";
}

void SubResearchProject::getSubCommands(nlohmann::json &commandTypesJson){
    {
        std::string prompt = "After converting the image to 336 x 336 use " + COMMAND_RESEARCH_IMAGE_PLANNING + " to explain about the image or parts of the image. You ***MUST*** only do one image at a time. The image will be researched in 9 tiles after this to explain the outline. Ask in the prompt the following things.\n"
        "1. What exactly is to be examined in the image.\n"
        "2. What the body/object parts are.\n"
        "3. The angle of each body/object part.\n"
        "4. Visually split the image into a 3x3 grid, list what body/object parts are in each.\n";

        commandTypesJson[COMMAND_RESEARCH_IMAGE_PLANNING][COMMAND_PARAMETER_DESCRIPTION] = prompt;
        commandTypesJson[COMMAND_RESEARCH_IMAGE_PLANNING][COMMAND_PARAMETER_INPUTS]["imageFileName"] = "Name of the image to analyse (withouth the path).";
        commandTypesJson[COMMAND_RESEARCH_IMAGE_PLANNING][COMMAND_PARAMETER_OUTPUTS]["outputJson"] = "Name of the output JSON file without paths.";

    }

    {
        std::string prompt = "After tiling the image into nine images (3 by 3 grid) a maximum of 336x336 in each; use " + COMMAND_RESEARCH_IMAGE_FOR_DRAWING + " to explain in detail how to each shape. Explain what tile it is of the 9 and pass in json information of any surrounding tiles. This is going to be programmatically drawn so the detail must know when to move to what angles for each body part in the tile. Ensure there is a task for viewing the image as a whole so AI has a full map, use the " + COMMAND_RESEARCH_IMAGE_PLANNING + " command for this. For each body/object part ask in the prompt the following things.\n"
        "1. The other tile positions for the end point. e.g. If the top left and the top right have been done, where does the top left end and where does the top right start.\n"
        "2. How far does line/points go and to what angle. If the angle, curve, point changes it will be a new point to draw.\n"
        "3. What colours are being used.\n\n";

        prompt += "For every drawing task, the prompt must NAME the tile's exact coordinate (e.g. 'tile (0,1), top-centre') and NAME the exact file names of every adjacent tile it shares an edge with. "
"Do not write generic phrases like 'surrounding tiles' — the drawing AI has no way to know which tiles those are unless you name them.\n"
"Outputs from researchImageForDrawing on ADJACENT tiles (not the planning json — that goes in map.json). Only include tiles already referenced by their drawing tasks.";



        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_DESCRIPTION] = prompt;
        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_INPUTS]["imageFileName"] = "Name of the without a path to draw. It ***MUST*** be of the same image, ***USE*** a separate " + COMMAND_RESEARCH_IMAGE_FOR_DRAWING + " for each image.";
        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_INPUTS]["map.json"] = "Use the map output from " + COMMAND_RESEARCH_IMAGE_PLANNING + " so it has the full map of the image as context.";
        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_INPUTS]["tile(n)(n).json"] = "If any other surrounding json's are supplied place them here. e.g. tile00.json and tile20.json for top centre json.";
        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_OUTPUTS]["outputJson"] = "Name of the output JSON file.";
    }

    {
        std::string prompt = COMMAND_RESEARCH_MERGE_RESEARCH + " is used to analysis different input files (normally json) and output a final file. This is ideal when researching data from different images or sources, or combining data. Ensure you put in the prompt exactly what it should do with the multiple pieces of data, and how it should output. It MUST have the same kind of output as the inputs, unless specified. \nHere are some examples\n"
        "1. If merging data just ask it to merge the files, but use some reasoning to allow for different parameters. Such as 3 images, but they might be the left/back/top images.\n"
        "2. If analysing multiple data sets tell it to pick the most consistent information. Such as if it is explaining an image, and two out of three images have the same information then use those two. If all are different then take an average but ensure the data is still accurate. e.g. We don't want chopped off shapes if drawing.\n";

        commandTypesJson[COMMAND_RESEARCH_MERGE_RESEARCH][COMMAND_PARAMETER_DESCRIPTION] = prompt;
        commandTypesJson[COMMAND_RESEARCH_MERGE_RESEARCH][COMMAND_PARAMETER_INPUTS]["research(n).json"] = "For each file to be researched, name it here. Do not include the path.";
        commandTypesJson[COMMAND_RESEARCH_MERGE_RESEARCH][COMMAND_PARAMETER_OUTPUTS]["outputJson"] = "Name of the output research file.";
    }

    


}

std::string SubResearchProject::fuzzyLogicSubCheckPlanning(short unsigned expectedNumber, std::string taskCommand, nlohmann::json element){
    std::string errors = "";

    if(taskCommand==COMMAND_RESEARCH_MERGE_RESEARCH){

    }else if(taskCommand==COMMAND_RESEARCH_IMAGE_FOR_DRAWING){

    }else if(taskCommand==COMMAND_RESEARCH_IMAGE_PLANNING){

    }else{
        errors += std::to_string(expectedNumber) + " has an unknown taskCommand of " + taskCommand + "\n";
    }
    /*    {
        std::string prompt = "After converting the image to 336 x 336 use " + COMMAND_RESEARCH_IMAGE_PLANNING + " to explain about the image or parts of the image. You ***MUST*** only do one image at a time. The image will be researched in 9 tiles after this to explain the outline. Ask in the prompt the following things.\n"
        "1. What exactly is to be examined in the image.\n"
        "2. What the body/object parts are.\n"
        "3. The angle of each body/object part.\n"
        "4. Visually split the image into a 3x3 grid, list what body/object parts are in each.\n";

        commandTypesJson[COMMAND_RESEARCH_IMAGE_PLANNING][COMMAND_PARAMETER_DESCRIPTION] = prompt;
        commandTypesJson[COMMAND_RESEARCH_IMAGE_PLANNING][COMMAND_PARAMETER_INPUTS]["imageFileName"] = "Name of the image to analyse (withouth the path).";
        commandTypesJson[COMMAND_RESEARCH_IMAGE_PLANNING][COMMAND_PARAMETER_OUTPUTS]["outputJson"] = "Name of the output JSON file without paths.";

    }

    {
        std::string prompt = "After tiling the image into nine images (3 by 3 grid) a maximum of 336x336 in each; use " + COMMAND_RESEARCH_IMAGE_FOR_DRAWING + " to explain in detail how to each shape. Explain what tile it is of the 9 and pass in json information of any surrounding tiles. This is going to be programmatically drawn so the detail must know when to move to what angles for each body part in the tile. Ensure there is a task for viewing the image as a whole so AI has a full map, use the " + COMMAND_RESEARCH_IMAGE_PLANNING + " command for this. For each body/object part ask in the prompt the following things.\n"
        "1. The other tile positions for the end point. e.g. If the top left and the top right have been done, where does the top left end and where does the top right start.\n"
        "2. How far does line/points go and to what angle. If the angle, curve, point changes it will be a new point to draw.\n"
        "3. What colours are being used.\n\n";

        prompt += "For every drawing task, the prompt must NAME the tile's exact coordinate (e.g. 'tile (0,1), top-centre') and NAME the exact file names of every adjacent tile it shares an edge with. "
"Do not write generic phrases like 'surrounding tiles' — the drawing AI has no way to know which tiles those are unless you name them.\n"
"Outputs from researchImageForDrawing on ADJACENT tiles (not the planning json — that goes in map.json). Only include tiles already referenced by their drawing tasks.";



        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_DESCRIPTION] = prompt;
        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_INPUTS]["imageFileName"] = "Name of the without a path to draw. It ***MUST*** be of the same image, ***USE*** a separate " + COMMAND_RESEARCH_IMAGE_FOR_DRAWING + " for each image.";
        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_INPUTS]["map.json"] = "Use the map output from " + COMMAND_RESEARCH_IMAGE_PLANNING + " so it has the full map of the image as context.";
        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_INPUTS]["tile(n)(n).json"] = "If any other surrounding json's are supplied place them here. e.g. tile00.json and tile20.json for top centre json.";
        commandTypesJson[COMMAND_RESEARCH_IMAGE_FOR_DRAWING][COMMAND_PARAMETER_OUTPUTS]["outputJson"] = "Name of the output JSON file.";
    }

    {
        std::string prompt = COMMAND_RESEARCH_MERGE_RESEARCH + " is used to analysis different input files (normally json) and output a final file. This is ideal when researching data from different images or sources, or combining data. Ensure you put in the prompt exactly what it should do with the multiple pieces of data, and how it should output. It MUST have the same kind of output as the inputs, unless specified. \nHere are some examples\n"
        "1. If merging data just ask it to merge the files, but use some reasoning to allow for different parameters. Such as 3 images, but they might be the left/back/top images.\n"
        "2. If analysing multiple data sets tell it to pick the most consistent information. Such as if it is explaining an image, and two out of three images have the same information then use those two. If all are different then take an average but ensure the data is still accurate. e.g. We don't want chopped off shapes if drawing.\n";

        commandTypesJson[COMMAND_RESEARCH_MERGE_RESEARCH][COMMAND_PARAMETER_DESCRIPTION] = prompt;
        commandTypesJson[COMMAND_RESEARCH_MERGE_RESEARCH][COMMAND_PARAMETER_INPUTS]["research(n).json"] = "For each file to be researched, name it here. Do not include the path.";
        commandTypesJson[COMMAND_RESEARCH_MERGE_RESEARCH][COMMAND_PARAMETER_OUTPUTS]["outputJson"] = "Name of the output research file.";
    }*/
    return errors;
}