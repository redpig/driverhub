# DriverHub host build — for development and testing.
# The real Fuchsia build uses BUILD.gn.

CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -I. -Isrc/shim/include -fno-pie
LDFLAGS := -no-pie

# Core library sources (everything except main.cc).
LIB_SRCS := \
	src/bus_driver/bus_driver.cc \
	src/module_node/module_node.cc \
	src/loader/module_loader.cc \
	src/loader/dependency_sort.cc \
	src/symbols/symbol_registry.cc \
	src/dt/dt_provider.cc \
	src/shim/kernel/memory.cc \
	src/shim/kernel/print.cc \
	src/shim/kernel/sync.cc \
	src/shim/kernel/module.cc \
	src/shim/kernel/device.cc \
	src/shim/kernel/platform.cc \
	src/shim/kernel/time.cc \
	src/shim/kernel/timer.cc \
	src/shim/kernel/workqueue.cc \
	src/shim/kernel/irq.cc \
	src/shim/kernel/io.cc \
	src/shim/kernel/bug.cc \
	src/shim/subsystem/rtc.cc \
	src/shim/subsystem/i2c.cc \
	src/shim/subsystem/spi.cc \
	src/shim/subsystem/gpio.cc \
	src/shim/subsystem/usb.cc \
	src/shim/subsystem/drm.cc \
	src/shim/subsystem/input.cc \
	src/shim/subsystem/cfg80211.cc \
	src/shim/subsystem/fs.cc

SRCS := src/main.cc $(LIB_SRCS)
OBJS := $(SRCS:.cc=.o)
LIB_OBJS := $(LIB_SRCS:.cc=.o)
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
	src/loader/dependency_sort.cc \
	src/symbols/symbol_registry.cc \
	src/dt/dt_provider.cc \
	src/shim/kernel/memory.cc \
	src/shim/kernel/print.cc \
	src/shim/kernel/sync.cc \
	src/shim/kernel/module.cc \
	src/shim/kernel/time.cc \
	src/shim/kernel/workqueue.cc \
	src/shim/kernel/irq.cc \
	src/shim/kernel/io.cc \
	src/shim/kernel/bug.cc \
	src/shim/abi/platform_abi.cc \
	src/shim/abi/rtc_abi.cc \
	src/shim/abi/timer_abi.cc \
	src/shim/subsystem/i2c.cc \
	src/shim/subsystem/spi.cc \
	src/shim/subsystem/gpio.cc \
	src/shim/subsystem/usb.cc \
	src/shim/subsystem/drm.cc \
	src/shim/subsystem/input.cc \
	src/shim/subsystem/cfg80211.cc \
	src/shim/subsystem/fs.cc

GKI_OBJS := $(GKI_SRCS:.cc=.gki.o)
GKI_TARGET := driverhub-gki

CC ?= gcc
SHIM_INCLUDES := -Isrc/shim/include
TEST_KO := tests/test_module.ko
RTC_TEST_KO := third_party/linux/rtc-test/rtc-test.ko
GKI_KO := third_party/linux/rtc-test/rtc-test-gki.ko
EXPORT_KO := tests/export_module.ko
IMPORT_KO := tests/import_module.ko
GPIO_KO := tests/gpio_test_module.ko
RTC_OPS_TEST := tests/rtc_ops_test
DEP_SORT_TEST := tests/dependency_sort_test

.PHONY: all clean test test-rtc test-gki test-rtc-ops test-intermodule test-gpio test-dep-sort test-all

all: $(TARGET) $(GKI_TARGET)

test: $(TARGET) $(TEST_KO)
	echo | ./$(TARGET) $(TEST_KO)

test-rtc: $(TARGET) $(RTC_TEST_KO)
	echo | ./$(TARGET) $(RTC_TEST_KO)

test-gki: $(GKI_TARGET)
	echo | ./$(GKI_TARGET) $(GKI_KO)

test-rtc-ops: $(RTC_OPS_TEST) $(RTC_TEST_KO)
	./$(RTC_OPS_TEST) $(RTC_TEST_KO)

test-intermodule: $(TARGET) $(EXPORT_KO) $(IMPORT_KO)
	echo | ./$(TARGET) $(EXPORT_KO) $(IMPORT_KO)

test-gpio: $(TARGET) $(GPIO_KO)
	echo | ./$(TARGET) $(GPIO_KO)

test-dep-sort: $(DEP_SORT_TEST)
	./$(DEP_SORT_TEST)

test-all: test test-rtc test-rtc-ops test-intermodule test-gpio test-dep-sort test-gki
	@echo ""
	@echo "=== All tests passed ==="

$(TEST_KO): tests/test_module.c
	$(CC) -c -o $@ $< -fno-stack-protector -fno-pie

$(RTC_TEST_KO): third_party/linux/rtc-test/rtc-test.c
	$(CC) -c -o $@ $< $(SHIM_INCLUDES) -fno-stack-protector -fno-pie

$(EXPORT_KO): tests/export_module.c
	$(CC) -c -o $@ $< $(SHIM_INCLUDES) -fno-stack-protector -fno-pie

$(IMPORT_KO): tests/import_module.c
	$(CC) -c -o $@ $< $(SHIM_INCLUDES) -fno-stack-protector -fno-pie

$(GPIO_KO): tests/gpio_test_module.c
	$(CC) -c -o $@ $< $(SHIM_INCLUDES) -fno-stack-protector -fno-pie

# Test binaries: compile test .cc without -Isrc/shim/include to avoid
# Linux kernel type conflicts with gtest's system header includes.
TEST_CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -I. -fno-pie

$(DEP_SORT_TEST): tests/dependency_sort_test.cc src/loader/dependency_sort.o
	$(CXX) $(TEST_CXXFLAGS) $(LDFLAGS) -o $@ $^ -lgtest -lgtest_main -lpthread

$(RTC_OPS_TEST): tests/rtc_ops_test.cc $(LIB_OBJS)
	$(CXX) $(TEST_CXXFLAGS) $(LDFLAGS) -o $@ $^ -lgtest -lpthread

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -lpthread

$(GKI_TARGET): $(GKI_OBJS)
	$(CXX) $(GKI_CXXFLAGS) $(LDFLAGS) -o $@ $^ -lpthread

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.gki.o: %.cc
	$(CXX) $(GKI_CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(GKI_OBJS) $(LIB_OBJS) $(TARGET) $(GKI_TARGET) \
		$(TEST_KO) $(RTC_TEST_KO) $(EXPORT_KO) $(IMPORT_KO) $(GPIO_KO) \
		$(RTC_OPS_TEST) $(DEP_SORT_TEST)
