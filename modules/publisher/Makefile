CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
LDLIBS += -lcurl -ljson-c
PREFIX ?= /opt/digit
TARGET := bin/publisher.so
.PHONY: all test clean install test-login test-announcement test-poemei-announcement test-poemei-post test-poemei-followup test-other-sites-posts
all: $(TARGET)
$(TARGET): publisher.c ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared publisher.c -o $@ $(LDLIBS)
test: $(TARGET)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_publisher.c -o bin/test_publisher -ldl
	./bin/test_publisher ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(PREFIX)/modules/publisher/bin"
	install -m 0644 module.conf "$(DESTDIR)$(PREFIX)/modules/publisher/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(PREFIX)/modules/publisher/bin/publisher.so"
clean:
	rm -rf bin

# Read-only live login acceptance test. Requires private credentials on Digit host.
bin/test_site_login: test_site_login.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_site_login.c -o $@ -lcurl
test-login: bin/test_site_login
	./bin/test_site_login


# Live two-domain announcement acceptance test (creates publicly visible entries).
bin/test_site_announcement: test_site_announcement.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_site_announcement.c -o $@ -lcurl
test-announcement: bin/test_site_announcement
	./bin/test_site_announcement

# Live poemei.com-only witty announcement acceptance test.
bin/test_poemei_announcement: test_poemei_announcement.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_poemei_announcement.c -o $@ -lcurl
test-poemei-announcement: bin/test_poemei_announcement
	./bin/test_poemei_announcement

# Live Core Posts acceptance test for poemei.com; submits one public post.
bin/test_poemei_post: test_poemei_post.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_poemei_post.c -o $@ -lcurl
test-poemei-post: bin/test_poemei_post
	./bin/test_poemei_post

# Separate Episode 002; never overwrites the introduction.
bin/test_poemei_followup: test_poemei_followup.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_poemei_followup.c -o $@ -lcurl
test-poemei-followup: bin/test_poemei_followup
	./bin/test_poemei_followup

# Live Core Posts tests for chaos-mvc.org and STN-LABZ (distinct public posts).
bin/test_other_sites_posts: test_other_sites_posts.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_other_sites_posts.c -o $@ -lcurl
test-other-sites-posts: bin/test_other_sites_posts
	./bin/test_other_sites_posts
