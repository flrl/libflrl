CC := gcc
AR := ar
WARNINGS := -Wall -Wextra -Werror -Wsuggest-attribute=format -Wwrite-strings
FEATURES := -fstrict-aliasing
LCOVEXCLUDE := misc/* src/xoshiro*.c src/splitmix64.c
UTMUX := $(shell which utmux 2>/dev/null)
COVERAGE :=

REQUIRES := cmocka

FLRL_CFLAGS := -Og -ggdb3 $(WARNINGS) $(FEATURES) $(COVERAGE) $(CFLAGS)
FLRL_CXXFLAGS := -Og -ggdb3 -std=c++2b -ffreestanding -fno-exceptions \
				 $(WARNINGS) $(FEATURES) $(COVERAGE) $(CXXFLAGS)
FLRL_LDFLAGS := $(LDFLAGS)
FLRL_CPPFLAGS := $(shell pkg-config --cflags $(REQUIRES)) $(CPPFLAGS)
FLRL_LDLIBS := $(shell pkg-config --libs $(REQUIRES)) $(LDLIBS)

.PHONY: all check clean
.PHONY: coverage coverage-setup coverage-report

SRCDIR := src
TESTDIR := test
BUILDDIR := build
MISCDIR := misc
COVERDIR := coverage

LIBCOBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(wildcard $(SRCDIR)/*.c))
LIBCXXOBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(wildcard $(SRCDIR)/*.cpp))

TESTOBJS := $(patsubst $(TESTDIR)/%.test,$(BUILDDIR)/test-%.o,$(wildcard $(TESTDIR)/*.test))
TESTTARGETS := $(patsubst $(BUILDDIR)/test-%.o,$(TESTDIR)/%,$(TESTOBJS))
TESTS := $(patsubst $(TESTDIR)/%,%,$(TESTTARGETS))
TESTCOMMONOBJS := $(patsubst $(TESTDIR)/%.c,$(BUILDDIR)/%.o,$(wildcard $(TESTDIR)/*.c))

MISCTARGETOBJS := $(patsubst $(MISCDIR)/%.c,$(BUILDDIR)/misc-%.o,$(wildcard $(MISCDIR)/*.c))
MISCTARGETS := $(patsubst $(MISCDIR)/%.c,$(MISCDIR)/%,$(wildcard $(MISCDIR)/*.c))

OBJS := $(LIBCOBJS) $(LIBCXXOBJS) $(TESTOBJS) $(TESTCOMMONOBJS) $(MISCTARGETOBJS)
DEPS := $(patsubst %.o,%.d,$(OBJS))

COVEROBJS := $(OBJS)
COVERFILES := $(patsubst %.o,%.gcda,$(COVEROBJS))
COVERFILES += $(patsubst %.o,%.gcno,$(COVEROBJS))

TARGET = libflrl.a

all: $(TARGET) $(MISCTARGETS)

ifneq ($(strip $(UTMUX)),)
check-%: $(TESTDIR)/%
	$(UTMUX) $<

vcheck-%: $(TESTDIR)/%
	$(UTMUX) -v $<

check: all $(TESTTARGETS)
	$(UTMUX) $(TESTTARGETS)

vcheck: all $(TESTTARGETS)
	$(UTMUX) -v $(TESTTARGETS)
else
check-%: $(TESTDIR)/%
	$< 2>&1

vcheck-%: $(TESTDIR)/%
	$< -v 2>&1

check: all $(foreach t,$(TESTS),check-$(t))

vcheck: all $(foreach t,$(TESTS),vcheck-$(t))
endif

clean:
	$(RM) $(TARGET) $(LIBCOBJS) $(LIBCXXOBJS)
	$(RM) $(TESTOBJS) $(TESTTARGETS) $(TESTCOMMONOBJS)
	$(RM) $(MISCTARGETOBJS) $(MISCTARGETS)
	$(RM) $(DEPS)
	$(RM) $(COVERFILES) app_base.info app_test.info

clean-coverage:
	$(RM) -r $(COVERDIR)

tests: $(TESTTARGETS)

$(BUILDDIR):
	mkdir -p $@

ifeq ($(OS),Windows_NT)
%.exe: %
	@touch $@
endif

$(TARGET): $(LIBCOBJS) $(LIBCXXOBJS)
	$(AR) -rcsD $@ $^

$(TESTOBJS): FLRL_CPPFLAGS += -DUNIT_TESTING

filt_flrl = $(filter-out $(BUILDDIR)/$(1).o, $(2))

$(TESTTARGETS): $(TESTDIR)/%: $(BUILDDIR)/test-%.o $(TESTCOMMONOBJS) $(LIBCOBJS) $(LIBCXXOBJS)
	$(CC) $(FLRL_CFLAGS) $(FLRL_LDFLAGS) -o $@ $(call filt_flrl,$*,$^) $(FLRL_LDLIBS)

$(MISCTARGETS): $(MISCDIR)/%: $(BUILDDIR)/misc-%.o $(TARGET)
	$(CC) $(FLRL_CFLAGS) $(FLRL_LDFLAGS) -o $@ $+

$(OBJS) $(DEPS): | $(BUILDDIR)

$(OBJS): FLRL_CPPFLAGS += -I.

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(FLRL_CFLAGS) $(FLRL_CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(BUILDDIR)/%.d
	$(CXX) $(FLRL_CXXFLAGS) $(FLRL_CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/%.o: $(TESTDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(FLRL_CFLAGS) $(FLRL_CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/test-%.o: $(TESTDIR)/%.test $(BUILDDIR)/test-%.d
	$(CC) $(FLRL_CFLAGS) $(FLRL_CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/test-$*.d -o $@ -x c -c $<

$(BUILDDIR)/misc-%.o: $(MISCDIR)/%.c $(BUILDDIR)/misc-%.d
	$(CC) $(FLRL_CFLAGS) $(FLRL_CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/misc-$*.d -o $@ -c $<

ifeq ($(OS),Windows_NT)
# lcov doesn't handle / paths on windows well, replace / with \.
LCOVEXCLUDE := $(subst /,\,$(LCOVEXCLUDE))
LCOVFLAGS := $(patsubst %,--exclude "*\\%",$(LCOVEXCLUDE))
else
LCOVFLAGS := $(patsubst %,--exclude "*/%",$(LCOVEXCLUDE))
endif

app_base.info: all tests
	lcov --config-file=lcovrc $(LCOVFLAGS) --directory . --capture --initial -o $@

app_test.info:
	lcov --config-file=lcovrc $(LCOVFLAGS) --directory . --capture -o $@

coverage-setup:
	$(MAKE) clean clean-coverage
	$(MAKE) COVERAGE=--coverage app_base.info

coverage-report:
	$(MAKE) -B app_test.info
	genhtml --config-file=lcovrc -o $(COVERDIR) app_base.info app_test.info

coverage:
	$(MAKE) coverage-setup
	$(MAKE) vcheck
	$(MAKE) coverage-report

$(DEPS):
-include $(DEPS)
