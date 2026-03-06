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

# GKI ABI-compatible build — replaces small-struct shims with ABI shims.
# -mstackrealign: kernel .ko modules are compiled with -mno-sse and may not
# maintain 16-byte stack alignment.  When they call into our shims which then
# call glibc (which uses SSE), this causes SIGSEGV.  -mstackrealign adds a
# stack-realignment prologue to every function.
GKI_CXXFLAGS := $(CXXFLAGS) -mstackrealign
GKI_SRCS := \
	src/main.cc \
	src/bus_driver/bus_driver.cc \
	src/module_node/module_node.cc \
	src/loader/module_loader.cc \
	src/symbols/symbol_registry.cc \
	src/shim/kernel/memory.cc \
	src/shim/kernel/print.cc \
	src/shim/kernel/sync.cc \
	src/shim/kernel/module.cc \
	src/shim/kernel/time.cc \
	src/shim/abi/platform_abi.cc \
	src/shim/abi/rtc_abi.cc \
	src/shim/abi/timer_abi.cc

GKI_OBJS := $(GKI_SRCS:.cc=.gki.o)
GKI_TARGET := driverhub-gki

CC ?= gcc
SHIM_INCLUDES := -Isrc/shim/include
TEST_KO := tests/test_module.ko
RTC_TEST_KO := third_party/linux/rtc-test/rtc-test.ko
GKI_KO := third_party/linux/rtc-test/rtc-test-gki.ko

.PHONY: all clean test test-rtc test-gki

all: $(TARGET) $(GKI_TARGET)

test: $(TARGET) $(TEST_KO)
	echo | ./$(TARGET) $(TEST_KO)

test-rtc: $(TARGET) $(RTC_TEST_KO)
	echo | ./$(TARGET) $(RTC_TEST_KO)

test-gki: $(GKI_TARGET)
	echo | ./$(GKI_TARGET) $(GKI_KO)

$(TEST_KO): tests/test_module.c
	$(CC) -c -o $@ $< -fno-stack-protector -fno-pie

$(RTC_TEST_KO): third_party/linux/rtc-test/rtc-test.c
	$(CC) -c -o $@ $< $(SHIM_INCLUDES) -fno-stack-protector -fno-pie

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -lpthread

$(GKI_TARGET): $(GKI_OBJS)
	$(CXX) $(GKI_CXXFLAGS) $(LDFLAGS) -o $@ $^ -lpthread

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.gki.o: %.cc
	$(CXX) $(GKI_CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(GKI_OBJS) $(TARGET) $(GKI_TARGET) $(TEST_KO) $(RTC_TEST_KO)
