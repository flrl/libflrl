CC := gcc
AR := ar
CFLAGS := -O0 -g -Wall -Wextra -Werror -Wsuggest-attribute=format $(CFLAGS)
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

LIBOBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(wildcard $(SRCDIR)/*.c))
TESTOBJS := $(patsubst $(TESTDIR)/%.test,$(BUILDDIR)/test-%.o,$(wildcard $(TESTDIR)/*.test))
TESTS := $(patsubst $(TESTDIR)/%.test,%,$(wildcard $(TESTDIR)/*.test))
TESTTARGETOBJS := $(patsubst $(TESTDIR)/%.c,$(BUILDDIR)/%.o,$(wildcard $(TESTDIR)/*.c))
TESTTARGETS := $(patsubst $(TESTDIR)/%.test,$(TESTDIR)/%,$(wildcard $(TESTDIR)/*.test))
MISCTARGETOBJS := $(patsubst $(MISCDIR)/%.c,$(BUILDDIR)/misc-%.o,$(wildcard $(MISCDIR)/*.c))
MISCTARGETS := $(patsubst $(MISCDIR)/%.c,$(MISCDIR)/%,$(wildcard $(MISCDIR)/*.c))

OBJS := $(LIBOBJS) $(TESTOBJS) $(TESTTARGETOBJS) $(MISCTARGETOBJS)

DEPS := $(patsubst %.o,%.d,$(LIBOBJS))
DEPS += $(patsubst %.o,%.d,$(TESTOBJS))
DEPS += $(patsubst %.o,%.d,$(TESTTARGETOBJS))
DEPS += $(patsubst %.o,%.d,$(MISCTARGETOBJS))

TARGET = libflrl.a

all: $(TARGET) $(MISCTARGETS)

ifneq ($(strip $(UTMUX)),)
check-%: $(TESTDIR)/%
	$(UTMUX) $<

vcheck-%: $(TESTDIR)/%
	$(UTMUX) -v $<

check: $(TESTTARGETS)
	$(UTMUX) $(TESTTARGETS)

vcheck: $(TESTTARGETS)
	$(UTMUX) -v $(TESTTARGETS)
else
check-%: $(TESTDIR)/%
	$< 2>&1

vcheck-%: $(TESTDIR)/%
	$< -v 2>&1

check: $(foreach t,$(TESTS),check-$(t))

vcheck: $(foreach t,$(TESTS),vcheck-$(t))
endif

clean:
	$(RM) $(TARGET) $(LIBOBJS)
	$(RM) $(TESTOBJS) $(TESTTARGETOBJS) $(TESTTARGETS)
	$(RM) $(MISCTARGETOBJS) $(MISCTARGETS)
	$(RM) $(DEPS)

tests: $(TESTTARGETS)

$(BUILDDIR):
	mkdir -p $@

ifeq ($(OS),Windows_NT)
%.exe: %
	@touch $@
endif

$(TARGET): $(LIBOBJS)
	$(AR) -rcsD $@ $^

$(TESTOBJS): CPPFLAGS += -DUNIT_TESTING

filt_flrl = $(filter-out $(BUILDDIR)/$(1).o, $(2))

$(TESTTARGETS): $(TESTDIR)/%: $(BUILDDIR)/test-%.o $(TESTTARGETOBJS) $(LIBOBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(call filt_flrl,$*,$^) $(LDLIBS)

$(MISCTARGETS): $(MISCDIR)/%: $(BUILDDIR)/misc-%.o $(TARGET)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

$(OBJS) $(DEPS): | $(BUILDDIR)

$(OBJS): CPPFLAGS += -I.

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/%.o: $(TESTDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/test-%.o: $(TESTDIR)/%.test $(BUILDDIR)/test-%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/test-$*.d -o $@ -x c -c $<

$(BUILDDIR)/misc-%.o: $(MISCDIR)/%.c $(BUILDDIR)/misc-%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/misc-$*.d -o $@ -c $<

$(DEPS):
-include $(DEPS)
