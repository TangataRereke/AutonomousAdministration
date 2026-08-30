#ifndef PROJECTSCANNER_H
    #define PROJECTSCANNER_H
#include "IWorker.h"
#include <filesystem>
class ProjectScanner : public IWorker{

    public:
        //const std::string PATH = "/home/james/source/AutonomousAdministration/Projects/";
        const std::string PATH = "/home/james/source/AutonomousAdministration/projects/";
    protected:
        void initialise();
        void runTask();
        void finalise();
        std::string getWorkerName() { return "Project Scanner";};
    private:
        std::filesystem::path projectPath{PATH};
};

#endif
