PREFIX ?= /opt/lesson-exporter
SYSTEMD_DIR ?= /etc/systemd/system
STATE_DIR ?= /var/lib/lesson-exporter
MODULE_USER ?= lesson-exporter

.PHONY: all test install enable disable status clean

all:
	@python3 -m py_compile lesson_exporter.py

test:
	@python3 -m py_compile lesson_exporter.py
	@echo "Syntax check only; integration testing requires real lesson dirs and Ollama"

install: all
	@id -u $(MODULE_USER) >/dev/null 2>&1 || useradd --system --home-dir $(STATE_DIR) --shell /usr/sbin/nologin $(MODULE_USER)
	install -d -m 0755 $(PREFIX)
	install -d -o $(MODULE_USER) -g $(MODULE_USER) -m 0700 $(STATE_DIR)
	install -m 0755 lesson_exporter.py $(PREFIX)/lesson_exporter.py
	install -m 0644 lesson-exporter.service $(SYSTEMD_DIR)/lesson-exporter.service
	systemctl daemon-reload
	@echo 'Installed; run: sudo systemctl enable --now lesson-exporter.service'

enable:
	systemctl enable --now lesson-exporter.service

disable:
	systemctl disable --now lesson-exporter.service

status:
	systemctl status lesson-exporter.service

clean:
	rm -rf __pycache__
