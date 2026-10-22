CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
LDLIBS += -lcurl -ljson-c
PREFIX ?= /opt/digit
TARGET := bin/publisher.so
.PHONY: all test clean install test-login test-announcement test-poemei-announcement test-poemei-post test-poemei-followup test-other-sites-posts test-stnlabz-direct-image fix-stnlabz-existing-posts
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

# STN-LABZ Core Posts: direct image path in Markdown body.
bin/test_stnlabz_direct_image: test_stnlabz_direct_image.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_stnlabz_direct_image.c -o $@ -lcurl
test-stnlabz-direct-image: bin/test_stnlabz_direct_image
	./bin/test_stnlabz_direct_image

# Edits only the two existing STN-LABZ posts to select digit.png (Media ID 1).
bin/fix_stnlabz_existing_posts: fix_stnlabz_existing_posts.c
	@mkdir -p bin
	$(CC) $(CFLAGS) fix_stnlabz_existing_posts.c -o $@ -lcurl
fix-stnlabz-existing-posts: bin/fix_stnlabz_existing_posts
	./bin/fix_stnlabz_existing_posts

# Live Core Pages acceptance test, chaos-mvc.org only.
bin/test_core_page: test_core_page.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_core_page.c -o $@ -lcurl
test-core-page: bin/test_core_page
	./bin/test_core_page

# Publish distinct Core Pages on STN-LABZ and poemei.com.
bin/test_other_sites_pages: test_other_sites_pages.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_other_sites_pages.c -o $@ -lcurl
test-other-sites-pages: bin/test_other_sites_pages
	./bin/test_other_sites_pages

# Read-only News form discovery; never submits News content.
bin/inspect_news_forms: inspect_news_forms.c
	@mkdir -p bin
	$(CC) $(CFLAGS) inspect_news_forms.c -o $@ -lcurl
inspect-news: bin/inspect_news_forms
	./bin/inspect_news_forms
