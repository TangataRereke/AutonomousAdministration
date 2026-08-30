#ifndef IWORKER_H
    #define IWORKER_H
    #include <string>
    #include <iostream>
class IWorker{
    public:
        void run(std::string projectPath);
    protected:
        virtual void initialise() = 0;
        virtual void runTask() = 0;
        virtual void finalise() = 0;
        virtual std::string getWorkerName() = 0;
        std::string projectPath = "";
    private:
};
#endif