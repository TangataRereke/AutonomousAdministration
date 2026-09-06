#include "RequirementsGatherer.h"
#include "APIRequest.h"
#include "ProjectCoordinator.h"
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <nlohmann/json_fwd.hpp>
#include <sstream>
#include <string>
RequirementsGatherer::RequirementsGatherer(std::string instructions){
    this->instructions = instructions;
}

void RequirementsGatherer::initialise(){

}

void RequirementsGatherer::finalise(){

}

void RequirementsGatherer::gatherRequirements(std::string lastResponse, std::string vettedResponse){
    APIRequest request;
    nlohmann::json fullInstructions;
    std::string instructionsTag = "instructions";
    if(vettedResponse!=""){
        instructionsTag = "originalInstructions";
        fullInstructions["instructions"] =
            std::string("Previously requirements gathering instructions were sent as per " + instructionsTag + " however a vet of this failed. Please look at the lastResponse tag and apply the vettedResponse instructions to ensure it complies.\n"
                        "Respond with the new full json as per the " + instructionsTag + " instructions.\n");

        fullInstructions["lastResponse"] = lastResponse;
        fullInstructions["vettedResponse"] = vettedResponse;
    }
    fullInstructions[instructionsTag] = 
    std::string("You are the requirements gatherer. Your role is to split all of the requirements into sub-projects, a sub-project is a type of project and should be one single subject. e.g. If researching two things, then it most likely will be two sub-projects. \n"
    "The projects must be in a logical order, all research projects should be done first and design projects are done prior to the main output projects. e.g. A a research project might be required for multiple requirements. The design is generally done before the output, such as a website would have a design element first.\n"
    "requirements - These are the requirements provided by the end-user, these are what the project must do. Take care to follow everything is followed as requested and ensure it is compelete."
    "requirementNumber - MUST be incremental, 1, 2, 3, etc\n"
    "requirementType - MUST be one of ") + ProjectCoordinator::PROJECT_TYPE_JSON_NAME + std::string(". It must be a single goal and a single project type. You must never combine requirementTypes.\n"
    "requirementPrompt - The requirement prompt will be passed on to the sub-project AI agent. The requirements are ***NOT*** presented to sub-projects. So it MUST specifically say in details what the requirement does. You must refer to input and output files. You must not used generalised terms based on the instructions. i.e. If it is a new company you can't put as per <company name> as that AI agent hasn't read instructions, instead be specific. The goal is to be succinct, to the point and in details but don't include other details that don't apply.\n"
    "inputFiles - This is an array of input file names and will be fed to the sub-project depending on the type of AI agent. So referring to this in the instructions is important. IF it isn't supplied by the user then it should be an output of another process\n"
    "outputFiles - This is an array of output file names. There must be at least one outputFile and if a generic outputFile is used such as a zip you still need a specify the main output file. e.g. An SVG, PNG, mov etc.\n\n"
    );

    fullInstructions["requirements"] = instructions;

    fullInstructions[ProjectCoordinator::PROJECT_TYPE_JSON_NAME] = ProjectCoordinator::getProjectTypes();

    // FIXED: Single JSON object with all keys, not multiple separate objects
    fullInstructions["return_format"] =
        std::string(R"(
    Here is the return format in JSON:
    "requirements":[
    {"requirementNumber": "numeric only, the incremental requirement",
        "requirementName": "name of the requirement, please use a file safe format, e.g. Instead of spaces use an underscore",
        "requirementType": "The type of project it must be one of )") +
        ProjectCoordinator::PROJECT_TYPE_JSON_NAME +
        R"(",
        "requirementPrompt": "convert the requirement into a prompt for the next project manager for the sub-project",
        "inputFiles": [{"fileName": "file name but must come from an outputFiles previously", "description": "describe file purpose"}],
        "outputFiles": [{"fileName": "file name", "description": "describe file purpose"}]
    }
    ]
    )";



    // FIXED: Valid JSON array with proper commas and single objects per requirement
    fullInstructions["requirementJsonExample"] = 
    "Requirements json example:\n"
    "{\"requirements\":["
    "{"
    "  \"requirementType\": \"Research\","
    "  \"requirementNumber\": \"1\","
    "  \"requirementName\": \"Research_Dog\","
    "  \"requirementPrompt\": \"Using the image dog_1.png, dog_2.png and dog_3.png find the dog. Look at each dog part and analyze the shapes of each body part as though you were telling a blind person how to draw the dog when they have never seen one. Assume they only know the main shapes. Use a json containing the part name and a list of what shape to draw where to have a complete dog. List the dog colours in each image in a separate json array.\","
    "  \"inputFiles\": [],"
    "  \"outputFiles\": [{\"fileName\": \"dog_shapes.txt\", \"description\": \"A text file containing the research of a dog for visual\"}]"
    "},"
    "{"
    "  \"requirementType\": \"Research\","
    "  \"requirementNumber\": \"2\","
    "  \"requirementName\": \"Research_Website_Styles\","
    "  \"requirementPrompt\": \"For a doggy day care website perform research on the hex colours to use for foreground, backgrounds, hover and logos etc. Determine the best layout to use. Ensure the colours suit dark and white backgrounds. Also determine the best way to do a logo, what colours to use, what should be in the image. NOTE: Dog research has been performed to draw a dog if required.\","
    "  \"inputFiles\": [],"
    "  \"outputFiles\": [{\"fileName\": \"doggy_day_care_research.txt\", \"description\": \"A text file containing research results\"}]"
    "},"
    "{"
    "  \"requirementType\": \"Graphics\","
    "  \"requirementNumber\": \"3\","
    "  \"requirementName\": \"Create_Logo\","
    "  \"requirementPrompt\": \"Based on dog_shapes.txt and research in doggy_day_care_research.txt create a logo with the dog as a mascott.\","
    "  \"inputFiles\": ["
    "    {\"fileName\": \"dog_shapes.txt\", \"description\": \"A text file containing the research of a dog for visual\"},"
    "    {\"fileName\": \"doggy_day_care_research.txt\", \"description\": \"A text file containing the research for colours to use for a logo and a website.\"}"
    "  ],"
    "  \"outputFiles\": [{\"fileName\": \"doggy_day_care.svg\", \"description\": \"The doggy day care logo\"}]"
    "}"
    "]}";

    //request.stopAllOllamaModels();
    request.setup(fullInstructions.dump(), APIRequest::ROLE_REQUIREMENTS_GATHERING, projectPath, "requirementsGathering", APIRequest::MODEL_REQUIREMENTS, 8192);
    request.run(projectPath);

    std::string fuzzyLogicCheck = fuzzyLogicCheckGatherRequirements(request.getResponse());
    if(fuzzyLogicCheck!=""){
        return gatherRequirements(request.getResponse(), fuzzyLogicCheck);
    }
    std::string vetCheck = vetRequirements(fullInstructions.dump(), request.getResponse());
    if(vetCheck!=""){
        return gatherRequirements(request.getResponse(), vetCheck);
    }

}

bool RequirementsGatherer::askQuestions(std::string feedback, std::string lastResponse){
    APIRequest request;
    nlohmann::json fullInstructions;
    std::string feedbackRequest = "";
    if(feedback!=""){
        feedbackRequest += "AI API has provided feedback for the previousResponse. Please look at the previousResponse and applied feedbackComments.\n";
    }

    fullInstructions["instructions"] = 
    "I will soon send the requirements to be someone to be turned into a project plan. Your job is to ensure we have all questions asked before we do project planning.\n"
    "The questions to ask on the requirements are about things we are likely to assume on, OR, something we just do not know clearly to build the project. We don't want to however ask questions that are already defined, OR, we need to answer ourselves.\n"
    "For example a question about the styling or how to do something is something that should be done with our OWN research. e.g. Encryption mechanism, we would obviously use the best encryption we can do. OR questions on how to style a website should be done with our own research if we know enough about the company. e.g. If we don't know about the company then we ask about that. If we know about the company but do not know the style then we research how to style based on the company.\n\n"
    "ENSURE you look at any questions already answered to see if that is satisfactory!\n"
    "DO ***NOT*** PUT QUESTIONS ALREADY ANSWERED. These are additional questions, we DO NOT want to annoy the customer with redundant information.\n"
    "Don't ask confirmation based questions, such as 'Are there any other ...' or 'Should we proceed ...' or 'Are you sure ...' \n"
    "***ENSURE*** your questions are NOT dumb and will not piss off our client. If the the requirements ask you to research something, then there should ***NEVER*** be a question about it. If the customer asks for something such as 2 colours then ensure you never ask about gradients as that is going against the requirest\n"
    "Do ***NOT*** FEATURE/SCOPE CREEP if you have been asked to do something, do not ask questions about things not considered that are additional to what they asked. The questions ARE about the requirements, NOT about what is NOT in the requirements.\n"
    "Please return a json of questions we NEED to ask before we do project planning.\n" + feedbackRequest;

    if(feedback!=""){
        fullInstructions["feedbackCommnets"] = feedback;
    }

    if(lastResponse!=""){
        fullInstructions["previousResponse"] = lastResponse;
    }

    fullInstructions["requirements"] = instructions;

    // FIXED: Single JSON object with all keys, not multiple separate objects
    fullInstructions["return_format"] = 
    "Here is the return format in JSON:\n"
    "{\"questions\":["
    "   {\"questionNumber\": \"numeric only, the incremental question\","
    "    \"questionName\": \"name of the question, please use a file safe format, e.g. Instead of spaces use an underscore\","
    "    \"question\": \"The actual question to be asked. Please ensure it is understandable, especially to a business user.\","
    "    \"assumption\": \"If you can make assumption on the answer, this will be presented to the business user. If have an inkling about something then put that here. e.g. If everything mentions a website except for one requirement which could be software based, then you'd put I think this might be website in <language>?\","
    "    \"answer\": \"Leave this empty ALWAYS. This is to be filled out by the business user. If it is not \"\" then the program will assume it is answered!\""
    "   }"
    "]";
    

    // FIXED: Valid JSON array with proper commas and single objects per requirement
    fullInstructions["requirementJsonExample"] = 
    "Requirements json example:\n"
    "{\"questions\":["
    "{"
    "  \"questionNumber\": \"1\","
    "  \"questionName\": \"CustomerEnvironment\","
    "  \"question\": \"What operating system is the customer's environment in?\","
    "  \"assumption\": \"Windows is the most likely environment.\","
    "  \"answer\": \"\","
    "},"
    "{"
    "  \"questionNumber\": \"2\","
    "  \"questionName\": \"ServerEnvironment\","
    "  \"question\": \"What operating system is the server environment in?\","
    "  \"assumption\": \"Linux/PHP is a cheaper option.\","
    "  \"answer\": \"\","
    "},"
    "{"
    "  \"questionNumber\": \"3\","
    "  \"questionName\": \"Company\","
    "  \"question\": \"Please tell me more about the company so we can do research on the correct styles to use\","
    "  \"assumption\": \"Most companies have a mission and a vision, if you can tell me these then this would be enough to work off.\","
    "  \"answer\": \"\","
    "}"
    "]}";

    
    
    request.setup(fullInstructions.dump(), APIRequest::ROLE_REQUIREMENTS_GATHERING, projectPath, "questions");
    request.run(projectPath);

    std::string vettedFeedback = vetQuestions(fullInstructions.dump(), request.getResponse());
    if(!vettedFeedback.empty()){
        return askQuestions(vettedFeedback, request.getResponse());
    }   
    return true;
}

bool get_bool_flexible(const nlohmann::json& j, const std::string& key, bool default_val = false) {
    // If key doesn't exist, return default
    if (!j.contains(key)) {
        return default_val;
    }

    const auto& val = j[key];

    // If it's already a boolean
    if (val.is_boolean()) {
        return val.get<bool>();
    }

    // If it's a number (0 = false, anything else = true)
    if (val.is_number()) {
        return val.get<double>() != 0.0;
    }

    // If it's a string, check for truthy values
    if (val.is_string()) {
        std::string str = val.get<std::string>();
        // Convert to lowercase
        for (auto& c : str) {
            c = std::tolower(static_cast<unsigned char>(c));
        }
        // Truthy strings
        if (str == "yes" || str == "y" || str == "1" || str == "true" || str == "on") {
            return true;
        }
        // Falsy strings
        if (str == "no" || str == "n" || str == "0" || str == "false" || str == "off" || str.empty()) {
            return false;
        }
        // Unknown string: default to false or throw? Let's default to false.
        return false;
    }

    // Fallback: return default for any other type
    return default_val;
}

bool RequirementsGatherer::vetQuestionsVet(std::string request, std::string response, std::string vettedRespons){
    APIRequest apiRequest;
    nlohmann::json fullInstructions;

    fullInstructions["instructions"] = 
    "AI has asked questions to ensure we have all answers for the requirements before doing project planning.\n"
    "A seperate AI vetting process has taken place. Your job is to ensure the vetting took place correctly. IF the vetting is incorrect then we will redo the vetting.\n"
    "Please ENSURE the vetting process gave the correct response. Answer Yes if you agree or No if you disagree.\n"
    "The originalRequest is the original requirements questions. The aiResponse is the response to asking what questions to answer. The vetResponse is the resulting vetting results. Remember your job is to see if you agree with the vetResponse only.\n";

    fullInstructions["originalRequest"] = request;
    fullInstructions["aiResponse"] = response;
    fullInstructions["vetResponse"] = vettedRespons;

    fullInstructions["return_format"] = 
    "{\"vettingApproved\": \"Yes or No. Yes if the vetted response is correct. Otherwise it must be No\"}";
    
    

    fullInstructions["return_success_example"] = 
    "Requirements json example when successful:\n"
    "{\"vettingApproved\":\"Yes\"}";

    fullInstructions["return_unsuccessful_example"] = 
    "Requirements json example when successful:\n"
    "{\"vettingApproved\":\"No\"}";
    
    apiRequest.setup(fullInstructions.dump(), APIRequest::ROLE_REQUIREMENTS_VETTER, projectPath, "questionVetVet");
    apiRequest.run(projectPath);
    std::string vetResponseText = apiRequest.getResponse();

    nlohmann::json responseJson;
    try {
        responseJson = nlohmann::json::parse(vetResponseText);
    } catch (const std::exception& e) {
        return vetQuestionsVet(request, response, vettedRespons);
    }
    

    return get_bool_flexible(responseJson, "vettingApproved", false);
 
}

bool RequirementsGatherer::vetRequirementsVet(std::string request, std::string response, std::string vettedRespons){
    APIRequest apiRequest;
    nlohmann::json fullInstructions;

    fullInstructions["instructions"] = 
    "AI has asked for requirements to be broken down into sub-projects.\n"
    "A seperate AI vetting process has taken place. Your job is to ensure the vetting took place correctly. IF the vetting is incorrect then we will redo the vetting.\n"
    "Please ENSURE the vetting process gave the correct response. Answer Yes if you agree or No if you disagree.\n"
    "The originalRequest is the original requirements being split up. The aiResponse is the response to asking what requirements to be broken down are. The vetResponse is the resulting vetting results. Remember your job is to see if you agree with the vetResponse only.\n";

    fullInstructions["originalRequest"] = request;
    fullInstructions["aiResponse"] = response;
    fullInstructions["vetResponse"] = vettedRespons;

    fullInstructions["return_format"] = 
    "{\"vettingApproved\": \"Yes or No. Yes if the vetted response is correct. Otherwise it must be No\"}";
    
    

    fullInstructions["return_success_example"] = 
    "Requirements json example when successful:\n"
    "{\"vettingApproved\":\"Yes\"}";

    fullInstructions["return_unsuccessful_example"] = 
    "Requirements json example when successful:\n"
    "{\"vettingApproved\":\"No\"}";
    
    apiRequest.setup(fullInstructions.dump(), APIRequest::ROLE_REQUIREMENTS_VETTER, projectPath, "requirementsVetVet");
    apiRequest.run(projectPath);
    std::string vetResponseText = apiRequest.getResponse();

    nlohmann::json responseJson;
    try {
        responseJson = nlohmann::json::parse(vetResponseText);
    } catch (const std::exception& e) {
        return vetRequirementsVet(request, response, vettedRespons);
    }
    
    return get_bool_flexible(responseJson, "vettingApproved", false);
 
}

auto getBasename = [](const std::string& file) -> std::string {
    auto pos = file.find_last_of('.');
    if (pos == std::string::npos) return file;
    return file.substr(0, pos);
};

bool RequirementsGatherer::textAppearsInRequirements(
    const std::string& needle,
    const nlohmann::json& requirements)
{
    for (const auto& r : requirements) {
        if (r.contains("requirementName") &&
            r["requirementName"].is_string() &&
            r["requirementName"].get<std::string>().find(needle) != std::string::npos)
            return true;

        if (r.contains("requirementPrompt") &&
            r["requirementPrompt"].is_string() &&
            r["requirementPrompt"].get<std::string>().find(needle) != std::string::npos)
            return true;

        if (r.contains("outputFiles") && r["outputFiles"].is_array()) {
            for (const auto& out : r["outputFiles"]) {
                if (out.contains("description") &&
                    out["description"].is_string() &&
                    out["description"].get<std::string>().find(needle) != std::string::npos)
                    return true;
            }
        }
    }
    return false;
}

std::string RequirementsGatherer::fuzzyLogicCheckGatherRequirements(const std::string& response) {
    nlohmann::json responseJSON;
    try {
        responseJSON = nlohmann::json::parse(response);
    } catch (const std::exception& exception) {
        nlohmann::json err;
        err["errors"] = nlohmann::json::array();
        err["errors"].push_back({
            {"requirementNumber", -1},
            {"error", std::string("JSON parse error: ") + exception.what()}
        });
        return err.dump();
    } catch (...) {
        nlohmann::json err;
        err["errors"] = nlohmann::json::array();
        err["errors"].push_back({
            {"requirementNumber", -1},
            {"error", "Unknown JSON parse error."}
        });
        return err.dump();
    }

    if (!responseJSON.contains("requirements") || !responseJSON["requirements"].is_array()) {
        nlohmann::json err;
        err["errors"] = nlohmann::json::array();
        err["errors"].push_back({
            {"requirementNumber", -1},
            {"error", "'requirements' array missing or not an array."}
        });
        return err.dump();
    }

    const auto& requirements = responseJSON["requirements"];
    if (requirements.empty()) {
        nlohmann::json err;
        err["errors"] = nlohmann::json::array();
        err["errors"].push_back({
            {"requirementNumber", -1},
            {"error", "'requirements' array is empty."}
        });
        return err.dump();
    }

    // Allowed project types
    std::unordered_set<std::string> allowedTypes;
    for (const auto& pt : ProjectCoordinator::ProjectTypes) {
        allowedTypes.insert(pt.name);
    }

    // Track produced files
    std::unordered_set<std::string> producedFiles;

    // Instructions length
    const std::size_t instructionsLen = this->instructions.size();

    // Accumulated requirementPrompt length
    std::size_t totalPromptLen = 0;

    auto getExtension = [](const std::string& file) -> std::string {
        auto pos = file.find_last_of('.');
        if (pos == std::string::npos) return "";
        return file.substr(pos + 1);
    };

    const std::unordered_map<std::string, std::unordered_set<std::string>> typeExtensions = {
        {"Video Project", {"mp4","mov","avi","mkv","flv"}},
        {"Design Project", {"json"}},
        {"Graphics Project", {"svg","png","jpg","jpeg","obj"}},
        {"Music Project", {"wav","mp3","flac","midi"}},
        {"Sound Effect Project", {"wav","mp3","flac"}},
        {"Documentation Project", {"txt","docx","md","pdf"}},
        {"Data Project", {"csv","xlsx","json"}},
        {"Coding Project", {"cpp","py","js","java","exe","dll","so","c"}},
        {"Website Project", {"html","css","js","php"}}
    };

    nlohmann::json err;
    err["errors"] = nlohmann::json::array();

    // Iterate requirements
    for (std::size_t i = 0; i < requirements.size(); ++i) {
        const auto& req = requirements[i];

        std::string reqName   = req.value("requirementName", "");
        std::string reqType   = req.value("requirementType", "");
        std::string reqPrompt = req.value("requirementPrompt", "");

        totalPromptLen += reqPrompt.size();

        // Rule 5: requirementType must be valid
        if (allowedTypes.find(reqType) == allowedTypes.end()) {
            err["errors"].push_back({
                {"requirementNumber", static_cast<int>(i)},
                {"error", "Invalid requirementType '" + reqType + "' in '" + reqName + "'."}
            });
            return err.dump();
        }

        // Rule 1: input files must be valid
        if (req.contains("inputFiles") && req["inputFiles"].is_array()) {
            for (const auto& input : req["inputFiles"]) {
                std::string fileName = input.value("fileName", "");
                if (fileName.empty()) continue;

                bool mentionedInInstructions =
                    (this->instructions.find(fileName) != std::string::npos);
                bool producedEarlier =
                    (producedFiles.find(fileName) != producedFiles.end());

                if (!mentionedInInstructions && !producedEarlier) {

                    std::string basename = getBasename(fileName);
                    bool appearsInRequirements =
                        textAppearsInRequirements(basename, requirements);

                    if (!appearsInRequirements) {
                        err["errors"].push_back({
                            {"requirementNumber", static_cast<int>(i)},
                            {"error", "Input file '" + fileName +
                                      "' not mentioned in instructions, not produced earlier, "
                                      "and basename '" + basename + "' not found in any requirement."}
                        });
                        return err.dump();
                    }
                }
            }
        }

        // Rule 2: must have outputFiles
        if (!req.contains("outputFiles") || !req["outputFiles"].is_array() || req["outputFiles"].empty()) {
            err["errors"].push_back({
                {"requirementNumber", static_cast<int>(i)},
                {"error", "Requirement '" + reqName + "' has no outputFiles."}
            });
            return err.dump();
        }

        bool hasNonZipOutput = false;

        for (const auto& output : req["outputFiles"]) {
            std::string outName = output.value("fileName", "");
            if (outName.empty()) continue;

            producedFiles.insert(outName);

            std::string ext = getExtension(outName);
            if (ext != "zip") {
                hasNonZipOutput = true;
            }

            auto it = typeExtensions.find(reqType);
            if (it != typeExtensions.end()) {
                const auto& allowedExts = it->second;
                if (!allowedExts.empty() && allowedExts.find(ext) == allowedExts.end()) {
                    err["errors"].push_back({
                        {"requirementNumber", static_cast<int>(i)},
                        {"error", "Output file '" + outName +
                                  "' has invalid extension '" + ext +
                                  "' for project type '" + reqType + "'."}
                    });
                    return err.dump();
                }
            }
        }

        if (!hasNonZipOutput) {
            err["errors"].push_back({
                {"requirementNumber", static_cast<int>(i)},
                {"error", "Requirement '" + reqName +
                          "' only produces .zip files or none at all."}
            });
            return err.dump();
        }
    }

    // NEW Rule 4: accumulated prompt length
    if (((double)totalPromptLen / (double)instructionsLen) < .4) {
        err["errors"].push_back({
            {"requirementNumber", -1},
            {"error", "Total requirementPrompt length is too short relative to instructions. Please expand the requirementPrompts to ensure the requirements are fully provided for each sub-project."}
        });
        return err.dump();
    }

    // Rule 3: last step cannot be Research Project
    const auto& lastReq = requirements.back();
    std::string lastType = lastReq.value("requirementType", "");
    std::string lastName = lastReq.value("requirementName", "");

    if (lastType == "Research Project") {
        err["errors"].push_back({
            {"requirementNumber", static_cast<int>(requirements.size() - 1)},
            {"error", "Last requirement '" + lastName +
                      "' is a Research Project. Final step must produce a concrete output."}
        });
        return err.dump();
    }

    return "";
}


std::string RequirementsGatherer::vetQuestions(std::string request, std::string response){
    APIRequest apiRequest;
    nlohmann::json fullInstructions;

    fullInstructions["instructions"] = 
    "AI has asked questions to ensure we have all answers for the requirements before doing project planning.\n"
    "Please ENSURE the right questions are being asked to fulfill the requirements, but also ensure we are not asking any dumb questions. Such as, if a question is already answered in the requirements then we do not need it. OR if a question has already been asked. OR as an expert we should do our own research rather than picking up on the business user with questions.\n"
    "When responding please ensure a full valid json response is returned. e.g. If you start a json array make sure it is ended so it can be parsed correctly.\n"
    "Answer Yes if the questions asked are satisfactory (or none are asked if there are no questions to ask), Answer No if the questions are not satisfactory. Provide any feedback that will be passed on to the original AI agent so they can adjust accordingly.\n";

    fullInstructions["originalRequest"] = request;
    fullInstructions["aiResponse"] = response;

    fullInstructions["return_format"] = 
    "Here is the return format in JSON:\n"
    "{\"questionsApproved\": \"Yes or No. Yes if the questions are satisfactory, No if the qustions are NOT satisfactory\","
    "\"general_vetting_feedback\": \"Enter any feedback here that doesn't apply to any particular questions. e.g. If something is missing or with the response in general.\","
    " \"feedback\":["
    "   {\"questionNumber\": \"ONLY return the question number IF it is has the issue\","
    "    \"questionName\": \"name of the question\","
    "    \"issue_during_vet\": \"Explain the issue and what is wrong with the question\""
    "   }"
    "]";
    

    fullInstructions["return_success_example"] = 
    "Requirements json example when successful:\n"
    "{\"questionsApproved\":\"Yes\"}";

    fullInstructions["return_unsuccessful_example"] = 
    "Requirements json example when successful:\n"
    "{\"questionsApproved\":\"No\"\n"
    " \"general_vetting_feedback\":\"We do not know where the software is, is it on a website, is it on a computer, is it php, is it web based, is it standard computer code in windows?\",\n"
    " \"feedback\":["
    "   {\"questionNumber\": \"4\","
    "    \"questionName\": \"Style_Use\","
    "    \"issue_during_vet\": \"The requirements already explicitly stay do your own research. We should not be asking them this question when they want us to do the research ourselves.\""
    "   },"
    "   {\"questionNumber\": \"5\","
    "    \"questionName\": \"Font_Style\","
    "    \"issue_during_vet\": \"The requirements already explicilty mention using Courier New for all fonts, so we should not ask this question.\""
    "   },"
    "   {\"questionNumber\": \"8\","
    "    \"questionName\": \"Encryption_Mechanism\","
    "    \"issue_during_vet\": \"The requirements state to encrypt the file and it is in PHP, this should be obvious to us to use the best encryption free for PHP.\""
    "   },"
    "]\n"
    "}";
    
    apiRequest.setup(fullInstructions.dump(), APIRequest::ROLE_REQUIREMENTS_VETTER, projectPath, "questionVet");
    apiRequest.run(projectPath);
    std::string apiResponseText = apiRequest.getResponse();

    nlohmann::json responseJson;
    try {
        responseJson = nlohmann::json::parse(apiResponseText);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse vet response: " << e.what() << std::endl;
        exit(1);
    }
    std::string rawResponse = apiResponseText;
    if(!vetQuestionsVet(request, response, rawResponse)){
        return vetQuestions(request, response);
    }else{

        bool questionsApproved = get_bool_flexible(responseJson, "questionsApproved", false);
        if(questionsApproved){
            return "";
        }
        return responseJson.dump();
    }
}


std::string RequirementsGatherer::vetRequirements(std::string request, std::string response){
    APIRequest apiRequest;
    nlohmann::json fullInstructions;

    fullInstructions["instructions"] = 
    "AI has asked to breakdown a project into smaller sub-projects. This is the most crucial part of the requirements gathering.\n"
    "Please ensure the project is broken down correctly. Eact project should be one goal and there should be no mixing of project outcomes. Research Projects are the only partial exception as the research could be used acrossed many projects.\n"
    "When responding please ensure a full valid json response is returned. e.g. If you start a json array make sure it is ended so it can be parsed correctly.\n"
    "The most essential part is each requirementPrompt is exclusively sent to the project manager. It DOES NOT know about the original instructions as it is likely to cause hallucinations. Please ensure these are complete.\n"
    "Answer Yes if the requirements are satisfactory, Answer No if the requirements are not satisfactory. Provide any feedback that will be passed on to the original AI agent so they can adjust accordingly.\n";

    fullInstructions["originalRequest"] = request;
    fullInstructions["aiResponse"] = response;

    fullInstructions["return_format"] = 
    "Here is the return format in JSON:\n"
    "{\"requirementsApproved\": \"Yes or No. Yes if the requirements are satisfactory, No if the requirements are NOT satisfactory\","
    "\"general_vetting_feedback\": \"Enter any feedback here that doesn't apply to any particular requirements. e.g. If something is missing or with the response in general.\","
    " \"feedback\":["
    "   {\"requirementNumber\": \"ONLY return the requirement number IF it is has the issue\","
    "    \"requirementName\": \"name of the requirement\","
    "    \"issue_during_vet\": \"Explain the issue and what is wrong with the requirement\""
    "   }"
    "]";
    

    fullInstructions["return_success_example"] = 
    "Requirements json example when successful:\n"
    "{\"requirementsApproved\":\"Yes\"}";

    fullInstructions["return_unsuccessful_example"] = 
    "Requirements json example when successful:\n"
    "{\"requirementsApproved\":\"No\"\n"
    " \"general_vetting_feedback\":\"You missed the requirement on researching the graphics\",\n"
    " \"feedback\":["
    "   {\"requirementNumber\": \"4\","
    "    \"requirementName\": \"Research_Birds_And_Mountains\","
    "    \"issue_during_vet\": \"This requirement should be split into two as it is researching two different types of things.\""
    "   },"
    "   {\"requirementNumber\": \"5\","
    "    \"requirementName\": \"Generate_Code\","
    "    \"issue_during_vet\": \"The requirementPrompt does not mention the language nor what is trying to be achieved with the code. Remember instructions are not passed on to each requirement.\""
    "   },"
    "   {\"requirementNumber\": \"8\","
    "    \"requirementName\": \"Generate_3D_Object\","
    "    \"issue_during_vet\": \"The research requirements should be used as an input and mentioned in the requirementPrompt.\""
    "   },"
    "]\n"
    "}";
    
    apiRequest.setup(fullInstructions.dump(), APIRequest::ROLE_REQUIREMENTS_VETTER, projectPath, "requirementsVet", APIRequest::MODEL_REQUIREMENTS);
    apiRequest.run(projectPath);
    std::string apiResponseText = apiRequest.getResponse();

    nlohmann::json responseJson;
    try {
        responseJson = nlohmann::json::parse(apiResponseText);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse vet response: " << e.what() << std::endl;
        exit(1);
    }
    std::string rawResponse = apiResponseText;
    if(!vetRequirementsVet(request, response, rawResponse)){
        return vetRequirements(request, response);
    }else{

        bool questionsApproved = get_bool_flexible(responseJson, "requirementsApproved", false);
        if(questionsApproved){
            return "";
        }
        return responseJson.dump();
    }
}
void RequirementsGatherer::runTask(){
    if(askQuestions()){
        gatherRequirements();
    }
    processSubProjects();
}

void RequirementsGatherer::processSubProjects(){
    nlohmann::json jsonObject;
    std::string requirementsFileName = projectPath + "requirementsGathering_response.json";
    if(!std::filesystem::exists(requirementsFileName)){
        std::cerr << "Cannot find " << requirementsFileName << " for processSubProjects. It should be there!" << std::endl;
        exit(1);
    }
    std::stringstream requirementsAsText;
    try{
        std::fstream fileRequirements(projectPath + "requirementsGathering_response.json", std::ios_base::in);
        requirementsAsText << fileRequirements.rdbuf();
        fileRequirements.close();
    }catch(std::exception exception){
        std::cerr << "Error reading file in processSubProjects " << exception.what() << std::endl;
        exit(1);
    }catch(...){
        std::cerr << "Error reading file in processSubProjects"  << std::endl;
        exit(1);
    }

    nlohmann::json requirementsAsJson;
    try{
        requirementsAsJson = nlohmann::json::parse(requirementsAsText);
    }catch(std::exception exception){
        std::cerr << "Error with the requirements file in processSubProjects. " << exception.what() << std::endl;
        exit(1);
    }catch(...){
        std::cerr << "Error with the requirements file in processSubProjects." << std::endl;
        exit(1);
    }
    if(!requirementsAsJson.contains("requirements")){
        std::cerr << "Expected requirements in processSubProjects" << std::endl;
        exit(1);
    }
    if(!requirementsAsJson["requirements"].is_array()){
        std::cerr << "Expected requirements as array in processSubProjects" << std::endl;
        exit(1);
    }
    for(const auto& element : requirementsAsJson["requirements"]){
        if(element.is_object()){
            if(!element.contains("requirementNumber")){
                std::cerr << "An object in processSubProjects does not contain requirementNumber" << std::endl;
                exit(1);
            }
            if(!element.contains("requirementName")){
                std::cerr << "An object in processSubProjects does not contain requirementName" << std::endl;
                exit(1);
            }
            if(!element.contains("requirementType")){
                std::cerr << "An object in processSubProjects does not contain requirementType" << std::endl;
                exit(1);
            }
            if(!element.contains("requirementPrompt")){
                std::cerr << "An object in processSubProjects does not contain requirementPrompt" << std::endl;
                exit(1);
            }
            if(!element.contains("inputFiles")){
                std::cerr << "An object in processSubProjects does not contain inputFiles" << std::endl;
                exit(1);
            }
            if(!element.contains("outputFiles")){
                std::cerr << "An object in processSubProjects does not contain outputFiles" << std::endl;
                exit(1);
            }
            if(!element["outputFiles"].is_array()){
                std::cerr << "An object in processSubProjects does not contain outputFiles as an array" << std::endl;
                exit(1);
            }
            std::string requirementNumber = element["requirementNumber"];
            std::string requirementName = element["requirementName"];
            std::string requirementType = element["requirementType"];
            if(!isValidProjectType(requirementType)){
                std::cerr << requirementType << " is not a valid requirementType for processSubProject" << std::endl;
                exit(1);
            }
            std::string requirementPrompt = element["requirementPrompt"];
            std::cout << requirementNumber << " - " << requirementName << " - " << requirementType << " - " << " - " << requirementPrompt << std::endl << std::endl;

            std::string newFolderName = standardiseFolderName(requirementNumber, requirementName);
            newFolderName = projectPath + newFolderName;
            if(!std::filesystem::exists(newFolderName)){
                try{
                    std::filesystem::create_directory(newFolderName);
                }catch(std::exception exception){
                    std::cerr << "Failed to create the sub project folder " << newFolderName << ". " << exception.what() << std::endl;
                    exit(1);
                }catch(...){
                    std::cerr << "Failed to create the sub project folder " << newFolderName << "." << std::endl;
                    exit(1);

                }
            }
            std::string newJsonName = newFolderName + "/requirements.json";
            std::fstream outputJsonFile(newJsonName, std::ios_base::out);
            try{
                outputJsonFile << element.dump();
            }catch(std::exception exception){
                std::cerr << "Error creating requirements file " << newJsonName << " in processSubProject. " << exception.what() << std::endl;
                exit(1);
            }catch(...){
                std::cerr << "Error creating requirements file " << newJsonName << " in processSubProject." << std::endl;
                exit(1);

            }
            outputJsonFile.close();

        }else{
            std::cerr << "We have a none object in requirements of processSubProjects" << std::endl;
            exit(1);
        }
        

    }
}

bool RequirementsGatherer::isValidProjectType(std::string projectType){
    for(ProjectCoordinator::ProjectType type : ProjectCoordinator::ProjectTypes){
        if(type.name==projectType){
            return true;
        }
    }
    return false;
}

std::string RequirementsGatherer::standardiseFolderName(std::string pNumber, std::string pName){
    std::stringstream newFname;
    for(char pCharacter : pNumber){
        if(pCharacter >= '0' && pCharacter <= '9'){
            newFname << pCharacter;
        }
    }
    newFname << "_";
    for(char pCharacter : pName){
        if(
            (pCharacter >= '0' && pCharacter <= '9')||
            (pCharacter >= 'A' && pCharacter <= 'Z')||
            (pCharacter >= 'a' && pCharacter <= 'z')||
            pCharacter == '_'
        ){
            newFname << pCharacter;
        }else if(pCharacter == ' '){
            newFname << "_";
        }
    }
    return newFname.str();
}