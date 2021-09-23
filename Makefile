RM = /bin/rm
SDCC_PREFIX = /opt/sdcc
SDCC = $(SDCC_PREFIX)/bin/sdcc
PACKIHX = $(SDCC_PREFIX)/bin/packihx

PROJNAME = CH559USB
SDCCFLAGS = -V -mmcs51 --xstack --stack-auto --model-large --xram-size 0x0c00 --xram-loc 0x0400 --code-size 0xefff -DFREQ_SYS=48000000

OBJS = main.rel util.rel USBHost.rel uart0.rel uart1.rel kbdparse.rel keymap.rel ringbuf.rel udev_hid.rel udev_hub.rel udev_util.rel
HEADERS = util.h USBHost.h uart0.h uart1.h kbdparse.h keymap.h ringbuf.h sdcc_keywords.h udev_hid.h udev_hub.h udev_util.h

all: $(PROJNAME).hex

%.rel: %.c $(HEADERS)
	$(SDCC) $(SDCCFLAGS) -c $<
#	$(RM) -f $(<:%.c=%.lst) $(<:%.c=%.lst)

$(PROJNAME).ihx: $(OBJS)
	$(SDCC) $(SDCCFLAGS) -o $@ $(OBJS)
#	$(foreach ext, lst rst asm sym, $(RM) -f $(<:%.c=%.$(ext));)

$(PROJNAME).hex: $(PROJNAME).ihx
	$(PACKIHX) $< >$(PROJNAME).hex.lf
	sed -e 's/$$/\r/' $(PROJNAME).hex.lf > $@

.PHONY: clean

clean:
	$(RM) -f $(OBJS)
	$(RM) -f $(PROJNAME).hex $(PROJNAME).ihx
	$(RM) -f $(PROJNAME).hex.lf $(PROJNAME).lk $(PROJNAME).map $(PROJNAME).mem
	$(RM) -f `for obj in $(OBJS); do \
		for ext in lst rst asm sym; do \
			echo \`basename $$obj rel\`$$ext; \
		done \
	done`
	