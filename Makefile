CXX = g++
CXXFLAGS = -std=c++17 -Wall -Isrc/utils/headers

SRC = \
    src/main.cpp \
    src/MiniSQL.cpp \
    src/utils/helperFuncs/csv_utils.cpp \
    src/utils/helperFuncs/parser_utils.cpp \
    src/utils/helperFuncs/string_utils.cpp \
    src/utils/helperFuncs/table_print.cpp

OBJ = $(SRC:.cpp=.o)

minisql: $(OBJ)
	$(CXX) $(CXXFLAGS) -o minisql $(OBJ)

# Compile rule for ALL .cpp → .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	del /Q src\*.o src\utils\helperFuncs\*.o minisql.exe 2>nul
