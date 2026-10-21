CXX ?= g++
CXXFLAGS += -std=c++17 -O2 -fPIC -Wall -Wextra -Wpedantic -MMD -MP $(shell pkg-config --cflags Qt6Widgets)
LDLIBS += $(shell pkg-config --libs Qt6Widgets)
TEST_CFLAGS := $(shell pkg-config --cflags Qt6Test)
TEST_LIBS := $(shell pkg-config --libs Qt6Test)

.PHONY: all test native-test clean
all: desktop-ruler
desktop-ruler: main.o window.o canvas.o geometry.o
	$(CXX) $^ -o $@ $(LDLIBS)
tests/test_ui.o tests/test_capture.o: CXXFLAGS += $(TEST_CFLAGS)
test_ui: tests/test_ui.o window.o canvas.o geometry.o
	$(CXX) $^ -o $@ $(LDLIBS) $(TEST_LIBS)
test_geometry: tests/test_geometry.o geometry.o
	$(CXX) $^ -o $@ $(LDLIBS)
test_capture: tests/test_capture.o window.o canvas.o geometry.o
	$(CXX) $^ -o $@ $(LDLIBS) $(TEST_LIBS)
test: test_geometry test_ui
	./test_geometry
	QT_QPA_PLATFORM=offscreen ./test_ui
native-test: test_capture
	QT_QPA_PLATFORM=wayland ./test_capture
clean:
	rm -f *.o tests/*.o *.d tests/*.d desktop-ruler test_ui test_geometry test_capture

-include $(wildcard *.d tests/*.d)
