.msc.svg:
	mscgen -T svg -i $^ -o $@

MSG_SRCS = resource_management.msc
MSG_TARGETS = $(MSG_SRCS:msc=svg)

EXTRA_DIST = $(MSG_SRCS)

if HAVE_MSCGEN
TARGETS = $(MSG_TARGETS)
endif

all-local: $(TARGETS)

clean-local:
	rm -f $(MSG_TARGETS)
