CC := gcc
AR := ar
CFLAGS := -O0 -g -Wall -Wextra -Werror -Wsuggest-attribute=format $(CFLAGS)
LDFLAGS := $(LDFLAGS)

.PHONY: all check clean

SRCDIR := src
BUILDDIR := build

OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(wildcard $(SRCDIR)/*.c))

DEPS := $(patsubst %.o,%.d,$(OBJS))

TARGET = libflrl.a

all: $(TARGET)

check:

clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

$(BUILDDIR):
	mkdir -p $@

$(TARGET): $(OBJS)
	$(AR) -rcsD $@ $^

$(OBJS) $(DEPS): | $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BUILDDIR)/%.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d -o $@ -c $<

$(DEPS):
-include $(DEPS)
