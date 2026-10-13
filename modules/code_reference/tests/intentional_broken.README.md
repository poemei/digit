# Intentional C failure fixture

This file is deliberately invalid and is **not** built into Digit or installed as a module.
Run `diagnose source modules/code_reference/tests/intentional_broken.c` in the Digit console to request DSD -> Core ABI -> Code Reference compiler diagnostics.

The compiler should identify an undeclared `missing_amount` symbol. A reasonable repair is to use the existing `tax` parameter if that matches the intended computation; the test cannot prove business intent.
