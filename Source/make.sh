clear
# 1. Create a separate build folder (keeps your source folder clean)
mkdir build
cd build

# 2. Generate the Makefiles / Project files
cmake ..

# 3. Compile the application
cmake --build .
./AutonomousAdministration

cd ..
