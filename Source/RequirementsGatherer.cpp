#include "RequirementsGatherer.h"
#include "APIRequest.h"
#include "ProjectCoordinator.h"
#include <nlohmann/json_fwd.hpp>
#include <string>
RequirementsGatherer::RequirementsGatherer(std::string instructions){
    this->instructions = instructions;
}

void RequirementsGatherer::initialise(){

}

void RequirementsGatherer::finalise(){

}

void RequirementsGatherer::gatherRequirements(){
    APIRequest request;
    nlohmann::json fullInstructions;
    fullInstructions["instructions"] = 
    "Please turn the requirements into a json of full requirements. Break it down into sub-projects, where a sub-project will have a different kind of output. e.g. Graphics, website, data collation etc. Ensure they are only high level requirements as each sub-project will refine more requirements into a higher level of detail.\n"
    "1. The requirement prompt must contain the full context of the requirements for the sub-project, sub-projects DO NOT know about the requirements for the overall project to avoid hallucinations with too much unrequired detail.\n"
    "2. The ordering MUST be logically so pre-requsites are fulfilled. e.g. If an image is required this must be done before it is required. IF it requires research it should be done first, not afterwards.\n"
    "3. There should always be at least one outputFile but the purpose of outputFile is to pass it in as inputFile's to other projects. e.g. If it is an SVG output then this will be used for the icon. Or a CSS common across files. ZIP files are okay for outputFiles IF it is not used as an input, otherwise ensure you have multiple files.\n"
    "4. Focus on splitting into logical sub-project. e.g. If there are two different research topics or research is performed on different sets of files like images, then each set should have it's own sub-project. This is to keep the AI Agent's focussed without hallucinating.\n"
    "To emphasize 3 - multiple outputFile's are essential AND it should contain the extension of the proposed input for other processes, zip files are only to be used for ADDITIONAL files. If you just use .zip then how does the input process know details about the individual files!\n"
    "To emphasize 2 - ORDERING is ESSENTIAL. You CANNOT do SEO research for example AFTER you have built a site. You CANNOT create classes AFTER the code uses it. You CANNOT create an image, AFTER you have used them!\n"
    "To emphasize 1 - We are NOT sending the requirements tag to each sub-project itself. Please instead ensure the requirementPrompt includes the full picture of that requirement. e.g. YOU have to ensure the sub-projects have FULL context of the requirements. You can't just use the term \"the website\" or \"the company\" you have to explain everything IT needs to do the job\n\n";

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
    "  \"requirementPrompt\": \"Using three downloaded images of a dog provided, research the dog colours and body part shapes in order to draw the shape at a later date.\","
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

    
    request.setup(fullInstructions.dump(), APIRequest::ROLE_REQUIREMENTS_GATHERING, projectPath, "requirementsGathering");
    request.run(projectPath);
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


std::string RequirementsGatherer::fuzzyLogicCheckGatherRequirements(std::string response){
   nlohmann::json responseJSON;
   
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

        std::cout << "VET QUESTIONS: " << get_bool_flexible(responseJson, "questionsApproved", false) << std::endl;
        std::cout << "  RESPONSE JSON: " << responseJson.dump() << std::endl;
        std::cout << "  RAW RESPONSE: " << apiResponseText << std::endl;
        bool questionsApproved = get_bool_flexible(responseJson, "questionsApproved", false);
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
}