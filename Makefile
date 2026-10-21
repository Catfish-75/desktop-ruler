CXX ?= g++
CXXFLAGS += -std=c++17 -O2 -fPIC -Wall -Wextra -Wpedantic -MMD -MP $(shell pkg-config --cflags Qt6Widgets Qt6DBus)
LDLIBS += $(shell pkg-config --libs Qt6Widgets Qt6DBus)
TEST_CFLAGS := $(shell pkg-config --cflags Qt6Test)
TEST_LIBS := $(shell pkg-config --libs Qt6Test)

.PHONY: all test native-test clean
all: desktop-ruler
desktop-ruler: main.o window.o canvas.o geometry.o shortcuts.o global_shortcuts.o
	$(CXX) $^ -o $@ $(LDLIBS)
tests/test_ui.o tests/test_capture.o tests/test_canvas_regression.o tests/test_workflow.o tests/test_shortcuts.o tests/test_capture_errors.o: CXXFLAGS += $(TEST_CFLAGS)
test_ui: tests/test_ui.o window.o canvas.o geometry.o shortcuts.o global_shortcuts.o
	$(CXX) $^ -o $@ $(LDLIBS) $(TEST_LIBS)
test_geometry: tests/test_geometry.o geometry.o
	$(CXX) $^ -o $@ $(LDLIBS)
test_capture: tests/test_capture.o window.o canvas.o geometry.o shortcuts.o global_shortcuts.o
	$(CXX) $^ -o $@ $(LDLIBS) $(TEST_LIBS)
global_shortcuts.moc: global_shortcuts.h
	$(shell pkg-config --variable=libexecdir Qt6Core)/moc $< -o $@
global_shortcuts.o: global_shortcuts.moc
test_canvas_regression: tests/test_canvas_regression.o canvas.o geometry.o
	$(CXX) $^ -o $@ $(LDLIBS) $(TEST_LIBS)
test_workflow: tests/test_workflow.o window.o canvas.o geometry.o shortcuts.o global_shortcuts.o
	$(CXX) $^ -o $@ $(LDLIBS) $(TEST_LIBS)
test_capture_errors: tests/test_capture_errors.o window.o canvas.o geometry.o shortcuts.o global_shortcuts.o
	$(CXX) $^ -o $@ $(LDLIBS) $(TEST_LIBS)
tests/test_shortcuts.moc: tests/test_shortcuts.cpp
	$(shell pkg-config --variable=libexecdir Qt6Core)/moc $< -o $@
tests/test_shortcuts.o: tests/test_shortcuts.moc
test_shortcuts: tests/test_shortcuts.o shortcuts.o global_shortcuts.o
	$(CXX) $^ -o $@ $(LDLIBS) $(TEST_LIBS)
test: test_geometry test_ui test_canvas_regression test_workflow test_shortcuts test_capture_errors
	./test_geometry
	QT_QPA_PLATFORM=offscreen ./test_ui
	QT_QPA_PLATFORM=offscreen ./test_canvas_regression
	QT_QPA_PLATFORM=offscreen ./test_workflow
	QT_QPA_PLATFORM=offscreen ./test_shortcuts
	QT_QPA_PLATFORM=offscreen ./test_capture_errors
native-test: test_capture
	QT_QPA_PLATFORM=wayland ./test_capture
clean:
	rm -f *.o tests/*.o *.d tests/*.d *.moc tests/*.moc desktop-ruler test_ui test_geometry test_capture test_canvas_regression test_workflow test_shortcuts test_capture_errors

-include $(wildcard *.d tests/*.d)
