"""Focused cold-start validation for every Escape-close UI contract."""


def sequence(ctx):
    subsystem = ctx.test_subsystem()
    if not ctx.guard("UI test subsystem exists", subsystem is not None):
        return

    names = [name for name in subsystem.get_all_test_names() if name.endswith(".CloseEscape")]
    if not ctx.guard("Escape-close tests are registered", len(names) > 0):
        return
    if not ctx.guard("Escape-close batch started", subsystem.start_tests_matching("*.CloseEscape")):
        return

    for _ in range(360):
        subsystem = ctx.test_subsystem()
        if subsystem is None or not subsystem.is_running_tests():
            break
        yield 1

    subsystem = ctx.test_subsystem()
    if not ctx.guard("UI test subsystem survived", subsystem is not None):
        return
    if not ctx.assert_true("Escape-close batch completed", not subsystem.is_running_tests()):
        return

    summary = subsystem.get_last_test_summary()
    for result in summary.results:
        if not result.passed:
            ctx.out(f"[UICloseEscape] FAIL {result.test_name}: {result.error_message}")

    ctx.assert_eq("Escape-close test count", summary.total_tests, len(names))
    ctx.assert_eq("Escape-close failures", summary.failed_tests, 0)
