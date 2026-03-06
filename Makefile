# DriverHub host build — for development and testing.
# The real Fuchsia build uses BUILD.gn.

CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -I. -Isrc/shim/include -fno-pie
LDFLAGS := -no-pie

SRCS := \
	src/main.cc \
	src/bus_driver/bus_driver.cc \
	src/module_node/module_node.cc \
	src/loader/module_loader.cc \
	src/symbols/symbol_registry.cc \
	src/shim/kernel/memory.cc \
	src/shim/kernel/print.cc \
	src/shim/kernel/sync.cc \
	src/shim/kernel/module.cc \
	src/shim/kernel/device.cc \
	src/shim/kernel/platform.cc \
	src/shim/kernel/time.cc \
	src/shim/kernel/timer.cc \
	src/shim/subsystem/rtc.cc

OBJS := $(SRCS:.cc=.o)
TARGET := driverhub

CC ?= gcc
SHIM_INCLUDES := -Isrc/shim/include
TEST_KO := tests/test_module.ko
RTC_TEST_KO := third_party/linux/rtc-test/rtc-test.ko

.PHONY: all clean test test-rtc

all: $(TARGET)

test: $(TARGET) $(TEST_KO)
	echo | ./$(TARGET) $(TEST_KO)

test-rtc: $(TARGET) $(RTC_TEST_KO)
	echo | ./$(TARGET) $(RTC_TEST_KO)

$(TEST_KO): tests/test_module.c
	$(CC) -c -o $@ $< -fno-stack-protector -fno-pie

$(RTC_TEST_KO): third_party/linux/rtc-test/rtc-test.c
	$(CC) -c -o $@ $< $(SHIM_INCLUDES) -fno-stack-protector -fno-pie

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -lpthread

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_KO) $(RTC_TEST_KO)
