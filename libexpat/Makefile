CC ?= gcc
EXPATOBJ := loadlibrary.o xmlparse.o xmlrole.o xmltok.o xmltok_impl.o xmltok_ns.o
EXPATTARGET := libexpat.a
EXPATCLEAN := clean
EXPATCLEANFILE := *.o *.lo *.a
EXPAT_CFLAGS := -DHAVE_EXPAT_CONFIG_H

ifneq ($(notdir $(PWD)),libexpat)
	EXPATOBJ := $(addprefix libexpat/,$(EXPATOBJ))
	EXPATTARGET := libexpat/libexpat.a
	EXPATCLEAN := libexpatclean
	EXPATCLEANFILE := libexpat/*.o libexpat/*.lo libexpat/*.a
	EXPAT_CFLAGS += -Ilibexpat
endif

$(EXPATTARGET):$(EXPATOBJ)
	$(AR) q $@ $(EXPATOBJ)

%.o:%.c
	$(CC) $(EXPAT_CFLAGS) $(CFLAGS) -c $^ -o $@

$(EXPATCLEAN):
	rm -vf $(EXPATCLEANFILE)
