EXE = metal

SOURCES = main.cpp

OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))

CXXFLAGS = -std=c++17 -Wall -Wformat
LIBS += -framework Metal -framework Foundation

INCLUDE += -Imetal-cpp

# rules
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o $@ $<

all: clean $(EXE)
run: $(EXE)
	./$(EXE)

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(INCLUDE) $(LIBS)
	rm -f $(OBJS)

clean:
	rm -f $(EXE) $(OBJS)