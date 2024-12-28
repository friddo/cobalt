EXE = cobalt

SOURCES = main.cpp mtl_implementation.cpp GLFWBridge.mm

IMGUI_DIR = imgui
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(IMGUI_DIR)/imgui_impl_glfw.cpp $(IMGUI_DIR)/imgui_impl_metal.mm

OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))

CXXFLAGS = -std=c++17 -Wall -Wformat
LIBS += -framework Metal -framework Foundation -framework QuartzCore
LIBS += -L/usr/local/lib -L/opt/homebrew/lib
LIBS += -lglfw

INCLUDE += -Imetal-cpp -Iimgui -I/opt/homebrew/include

# rules
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.mm
	$(CXX) $(CXXFLAGS) $(INCLUDE) -ObjC++ -fobjc-weak -fobjc-arc -c -o $@ $<

%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o $@ $<

%.o:$(IMGUI_DIR)/%.mm
	$(CXX) $(CXXFLAGS) $(INCLUDE) -ObjC++ -fobjc-weak -fobjc-arc -c -o $@ $<

all: $(EXE)
run: $(EXE)
	./$(EXE)

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(INCLUDE) $(LIBS)

clean:
	rm -f $(EXE) $(OBJS)