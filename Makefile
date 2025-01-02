CC := gcc
AR := ar
WARNINGS := -Wall -Wextra -Werror -Wsuggest-attribute=format
CFLAGS := -O0 -ggdb3 $(WARNINGS) $(CFLAGS)
CXXFLAGS := -O0 -ggdb3 -std=c++2b $(WARNINGS) $(CXXFLAGS)
LDFLAGS := $(LDFLAGS)
UTMUX := $(shell which utmux 2>/dev/null)

REQUIRES := cmocka

CPPFLAGS += $(shell pkg-config --cflags $(REQUIRES))
LDLIBS += $(shell pkg-config --libs $(REQUIRES))

.PHONY: all check clean

SRCDIR := src
TESTDIR := test
BUILDDIR := build
MISCDIR := misc

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

tests: $(TESTTARGETS)

$(BUILDDIR):
	mkdir -p $@

ifeq ($(OS),Windows_NT)
%.exe: %
	@touch $@
endif

$(TARGET): $(LIBCOBJS) $(LIBCXXOBJS)
	$(AR) -rcsD $@ $^

$(TESTOBJS): CPPFLAGS += -DUNIT_TESTING

filt_flrl = $(filter-out $(BUILDDIR)/$(1).o, $(2))

$(TESTTARGETS): $(TESTDIR)/%: $(BUILDDIR)/test-%.o $(TESTCOMMONOBJS) $(LIBCOBJS) $(LIBCXXOBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(call filt_flrl,$*,$^) $(LDLIBS)

$(MISCTARGETS): $(MISCDIR)/%: $(BUILDDIR)/misc-%.o $(TARGET)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

$(OBJS) $(DEPS): | $(BUILDDIR)

$(OBJS): CPPFLAGS += -I.

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(BUILDDIR)/%.d
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/%.o: $(TESTDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/test-%.o: $(TESTDIR)/%.test $(BUILDDIR)/test-%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/test-$*.d -o $@ -x c -c $<

$(BUILDDIR)/misc-%.o: $(MISCDIR)/%.c $(BUILDDIR)/misc-%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/misc-$*.d -o $@ -c $<

$(DEPS):
-include $(DEPS)
