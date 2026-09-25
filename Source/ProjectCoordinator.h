#ifndef PROJECT_COORDINATOR_H
    #define PROJECT_COORDINATOR_H
    #include "IWorker.h"
#include "IWorker.h"
#include <string>
#include <fstream>
#include "ISubProject.h"
#include "SubResearchProject.h"

    class ProjectCoordinator : public IWorker{
        public:
            struct ProjectType{
                const char* name;
                const char* description;
            };
            static constexpr ProjectType ProjectTypes[] = {
                {"Research Project", "Projects that perform research and usually output a text file or json. Including AI agents using internal data, web searches, accessing web-pages, analysing data/text/code or using vision AI's."},
                {"Design Project", "Projects that do the design/layout work output as a json. Usually performed after research projects if there are any, it feeds designs into the other projects by way of input files. E.g. Graphics will determine the size, and what to put what where by position. Code will be data layouts, structs, classes, method signatures and descriptions. Web pages layouts, styling, and code design. Music the sections, lengths etc. Videos the segments of time and how long it will take. Even text based projects will plan the ordering and what goes in what paragraph."},
                {"Coding Project", "Projects that have an output of code, executables or other run time files. NOTE: Website Projects are different. This includes the design, testing, guideliness, class maps, UML and code itself."},
                {"Website Project", "Projects that build websites such as html, php, javascript etc. This includes the style sheets and design layouts of the page. It does NOT include graphics and other media as these need a more generic project. Media should be used as an input."},
                {"Graphics Project", "These are projects that create graphics SVG, PNG, OBJ etc. It includes the design and layout of the graphics itself. It is recommended to use research projects to gather information such as shapes and proportions however. AI agents are not very good at creating graphics without research or very good instructions from the customer."},
            {"Music Project", "These are projects where music is created in the form of either notation and/or sound itself. It includes the design. Research might need to be an input here."},
            {"Sound Effect Project", "These are projects where general sound is created but not in the form of a music project. It could feed however into the music project, e.g. an instrument."},
            {"Video Project", "These are projects that generate video files, it is recommended graphics music or sound effect projects take place before hand if generating a new video. However it might be a more generic video task that is fine, such as trimming a video."},
            {"Documentation Project", "These are projects that create any type of document such as a word document or a text file. It includes aspects of planning a document and formalising styling if required. Research might be good as an input here for certain types of documents."},
            {"Data Project", "These are projects that deal with data and often create tabular formats like spreadsheets. This is only the spreadsheet/data aspect so research or other input files may be useful."},
            {"Translation Project", "These are projects that translate data and output it. It is recommended to have an input and an output here in text format. The other projects can format accordingly."}


            };
            static constexpr char PROJECT_TYPE_JSON_NAME[] = "ProjectTypes";

            static std::string getProjectTypes();
            ProjectCoordinator(std::string projectName, std::string instructions, bool topLevel = false);
        protected:
            void initialise();
            void runTask();
            void finalise();
            std::string getWorkerName() { return "Project Coordinator";};        
        private:
            std::string projectName = "";
            std::string instructions = "";
            bool topLevel = false;
            void runProjects();
            void runProject(std::string path);
    };
#endif