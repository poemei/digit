#!/usr/bin/env python3
"""Core source-contract regression checks.

These checks are NOT a replacement for the mandatory Core runtime qualification
suite. They deliberately fail when critical source-level invariants disappear.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
def source(name):
    return (ROOT / "src" / name).read_text(encoding="utf-8")
def header(name):
    return (ROOT / "include" / name).read_text(encoding="utf-8")

class CoreSourceContracts(unittest.TestCase):
    def require(self, file, *fragments):
        body = source(file)
        for fragment in fragments:
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, body)

    def test_01_core_root_validation(self):
        self.require("digit.c", "stnlabz_agent_root_validate(root)")
    def test_02_root_rejection_fails_closed(self):
        self.require("digit.c", "Startup denied", "return 1;")
    def test_03_core_logging_required(self):
        self.require("digit.c", "if (!stnlabz_log_open())")
    def test_04_module_manager_is_core_owned(self):
        self.require("digit.c", "stnlabz_hotload_create(", "stnlabz_hotload_poll(watch)")
    def test_05_discovery_does_not_authorize(self):
        self.require("core_discovery.c", "Discovery never confers qualification")
    def test_06_module_qualification_uses_registry(self):
        self.require("core_discovery.c", "stnlabz_module_registry_qualify(")
    def test_07_module_qualification_audit(self):
        self.require("core_discovery.c", "QUALIFICATION_FAILED", "negative_executed")
    def test_08_activation_requires_qualified(self):
        self.require("core_activation.c", "STNLABZ_MODULE_STATE_QUALIFIED",
                     "STNLABZ_MODULE_ERR_NOT_QUALIFIED")
    def test_09_explicit_activation_requires_authority(self):
        self.require("core_activation.c", "STNLABZ_MODULE_ERR_NOT_AUTHORIZED",
                     "stnlabz_core_policy_evaluate(")
    def test_10_stopping_uses_abi(self):
        self.require("core_activation.c", "stnlabz_module_abi_stop(")
    def test_11_stop_all_reverses_order(self):
        self.require("core_activation.c", "registry->count; i > 0; --i")
    def test_12_snapshot_uses_evp_sha256(self):
        self.require("core_hotload.c", "EVP_DigestInit_ex(", "EVP_DigestFinal_ex(")
    def test_13_snapshot_rejects_changes(self):
        self.require("core_hotload.c", "candidate_changed_during_snapshot")
    def test_14_hotload_checks_version(self):
        self.require("core_hotload.c", "version_newer(known, descriptor)")
    def test_15_hotload_qualifies_in_isolation(self):
        self.require("core_hotload.c", "stnlabz_module_registry_init(&isolated)",
                     "stnlabz_module_registry_qualify(&isolated")
    def test_16_failed_qual_retains_incumbent(self):
        self.require("core_hotload.c", "qualification_failed_incumbent_retained")
    def test_17_replacement_attempts_restoration(self):
        self.require("core_hotload.c", "restore:", "incumbent_restoration_failed",
                     "HOTLOAD_ROLLBACK_ACTIVE")
    def test_18_qualification_audit_has_counts(self):
        self.require("core_hotload.c", "candidate_sha256", "negative_passed=%d",
                     "tests_failed")
    def test_19_module_log_destination(self):
        self.assertIn("/opt/digit/logs/modules.log", header("logging.h"))
    def test_20_module_log_retains_failure_evidence(self):
        self.require("logging.c", "QUALIFICATION_GREEN", '" evidence="',
                     "fsync(fileno(module_log_stream))")
if __name__ == "__main__":
    unittest.main(verbosity=2)
