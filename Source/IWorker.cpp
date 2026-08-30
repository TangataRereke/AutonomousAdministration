#include "IWorker.h"
#include <exception>
#include <iostream>

void IWorker::run(std::string projectPath){
    this->projectPath = projectPath;
    try{
        initialise();
    }catch(const std::exception& exception){
        std::cerr << "EXCEPTION INITIALISING " << getWorkerName() << " of " << exception.what() << std::endl;
        exit(1);
    }catch(...){
        std::cerr << "UNKNOWN INITIALISING " << getWorkerName() << std::endl;
        exit(1);
    }

    try{
        runTask();
    }catch(const std::exception& exception){
        std::cerr << "EXCEPTION RUNNING " << getWorkerName() << " of " << exception.what() << std::endl;
        exit(1);
    }catch(...){
        std::cerr << "UNKNOWN RUNNING " << getWorkerName() << std::endl;
        exit(1);
    }

    try{
        finalise();
    }catch(const std::exception& exception){
        std::cerr << "EXCEPTION FINISHING " << getWorkerName() << " of " << exception.what() << std::endl;
        exit(1);
    }catch(...){
        std::cerr << "UNKNOWN FINISHING " << getWorkerName() << std::endl;
        exit(1);
    }

}