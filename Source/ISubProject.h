#ifndef ISUBPROJECT_H
    #define ISUBPROJECT_H
    
    #include "IWorker.h"
#include <nlohmann/json_fwd.hpp>
    class ISubProject : public IWorker{
        public:
            virtual void initialise() = 0;
            virtual void runTask() = 0;
            virtual void finalise() = 0;
            virtual std::string getWorkerName() = 0;
            void setup(std::string setupJson);
            
        protected:

        private:

    };

#endif