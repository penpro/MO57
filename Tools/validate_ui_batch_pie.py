"""Tick-driven validation for the supported frame-stepped UI batch command."""


def sequence(ctx):
    test_subsystem = ctx.test_subsystem()
    if not ctx.guard("UI test subsystem exists", test_subsystem is not None):
        return

    expected_count = len(test_subsystem.get_all_test_names())
    unreal.SystemLibrary.execute_console_command(ctx.world, "MO.UI.RunAllTests")
    if not ctx.guard("aggregate command started asynchronous run", test_subsystem.is_running_tests()):
        return

    # The game-thread test runner intentionally reserves clean settling frames
    # between cases. Low editor FPS during procedural-world PIE can be ~3 Hz,
    # so budget enough frames for the full isolated suite without weakening any
    # per-action timeout inside the C++ harness.
    max_frames = max(1200, expected_count * 16)
    for _ in range(max_frames):
        current_subsystem = ctx.test_subsystem()
        if current_subsystem is None or not current_subsystem.is_running_tests():
            break
        yield 1

    test_subsystem = ctx.test_subsystem()
    if not ctx.guard("UI test subsystem survived aggregate", test_subsystem is not None):
        return
    if not ctx.assert_true("aggregate completed before timeout", not test_subsystem.is_running_tests()):
        return

    summary = test_subsystem.get_last_test_summary()
    failed = [
        f"{result.test_name}: {result.error_message}"
        for result in summary.results
        if not result.passed
    ]
    ctx.out(
        f"[UIBatch] complete passed={summary.passed_tests} "
        f"failed={summary.failed_tests} total={summary.total_tests}"
    )
    for failure in failed:
        ctx.out(f"[UIBatch] FAIL {failure}")

    ctx.assert_eq("aggregate covered every registered test", summary.total_tests, expected_count)
    ctx.assert_eq("aggregate produced one result per test", len(summary.results), expected_count)
    ctx.assert_eq("aggregate has zero failures", summary.failed_tests, 0)
