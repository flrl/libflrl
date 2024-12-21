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

LIBOBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(wildcard $(SRCDIR)/*.c))
TESTOBJS := $(patsubst $(TESTDIR)/%.test,$(BUILDDIR)/test-%.o,$(wildcard $(TESTDIR)/*.test))
TESTS := $(patsubst $(TESTDIR)/%.test,%,$(wildcard $(TESTDIR)/*.test))
TESTTARGETOBJS := $(patsubst $(TESTDIR)/%.c,$(BUILDDIR)/%.o,$(wildcard $(TESTDIR)/*.c))
TESTTARGETS := $(patsubst $(TESTDIR)/%.test,$(TESTDIR)/%,$(wildcard $(TESTDIR)/*.test))

DEPS := $(patsubst %.o,%.d,$(LIBOBJS))
DEPS += $(patsubst %.o,%.d,$(TESTOBJS))
DEPS += $(patsubst %.o,%.d,$(TESTTARGETOBJS))

TARGET = libflrl.a

all: $(TARGET)

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
	$(RM) $(TARGET) $(LIBOBJS) $(TESTOBJS) $(TESTTARGETOBJS) $(TESTTARGETS) $(DEPS)

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

$(LIBOBJS) $(TESTOBJS) $(TESTTARGETOBJS) $(DEPS): | $(BUILDDIR)

$(LIBOBJS) $(TESTOBJS) $(TESTTARGETOBJS): CPPFLAGS += -I.

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/%.o: $(TESTDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(BUILDDIR)/test-%.o: $(TESTDIR)/%.test $(BUILDDIR)/test-%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/test-$*.d -o $@ -x c -c $<

$(DEPS):
-include $(DEPS)
