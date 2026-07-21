"""Tick-driven Stage-3 queue-renderer contract: LIVE progress + completion-as-removal.

    python Tools/ue.py seq Tools/validate_ui_queue_pie.py

Requires PIE already booted (same convention as the other validate_ui_*_pie
scripts). Covers the two Stage-3 checklist items that need real frames — the
rest (initial rows, cancel-one/all, empty state, source swap, reconstruct) are
the Queue.* entries in the MO.UI live suite.

  (a) LIVE PROGRESS: with a craft running and the crafting menu open, the
      renderer's polled progress advances over real frames.
  (b) COMPLETION/REMOVAL: when the active craft finishes its repeats, the row
      set shrinks (completion presents as removal, never a terminal-state row).
"""


def sequence(ctx):
    test_subsystem = ctx.test_subsystem()
    if not ctx.guard("UI test subsystem exists", test_subsystem is not None):
        return

    # Self-healing start: a prior aborted run may have left menus open (OpenMenu
    # dispatches through Toggle*, so opening an already-open menu closes it).
    test_subsystem.close_all_menus()
    for _ in range(3):
        yield 1

    # Fixture: 2 entries (1x active + 2x queued) via the shared C++ fixture.
    if not ctx.guard("queue fixture enqueued", bool(test_subsystem.setup_queue_fixture(2))):
        return
    yield 1

    if not ctx.guard("crafting menu opened", bool(test_subsystem.open_menu("Crafting"))):
        return
    yield 2

    widget = None
    for candidate in ctx.all_widgets(ctx.find_class("MOCraftingQueueWidget")):
        if candidate.has_queue_source():
            widget = candidate
            break
    if not ctx.guard("live crafting queue widget found", widget is not None):
        return

    rows0 = widget.get_row_count()
    ctx.assert_true("initial rows present", rows0 >= 1)
    p_start = widget.get_current_progress()

    # (a) live progress: poll across real frames until progress advances.
    advanced = False
    p_seen = p_start
    for _ in range(600):  # ~10s at 60fps; procedural PIE can run slow
        yield 1
        p_now = widget.get_current_progress()
        if p_now > p_seen + 0.0005:
            advanced = True
            p_seen = p_now
            break
    ctx.assert_true(
        "LIVE PROGRESS: renderer progress advanced over frames (%.4f -> %.4f)" % (p_start, p_seen),
        advanced)

    # (b) completion-as-removal: wait for the active entry to finish all repeats
    # and leave the queue; the row set must shrink with no terminal-state row.
    removed = False
    for i in range(2400):  # generous: repeats * CraftTime real seconds
        yield 1
        if widget.get_row_count() < rows0:
            removed = True
            break
    ctx.assert_true("COMPLETION/REMOVAL: finished craft left the row set", removed)
    if removed:
        # No terminal ghost rows: the surviving row set mirrors the component's
        # queue exactly. (Row-id validity is asserted by the live Queue.CraftingRows
        # test; unreal.Guid exposes no validity check to Python.)
        pawn0 = unreal.GameplayStatics.get_player_pawn(ctx.world, 0)
        qc0 = pawn0.get_component_by_class(unreal.MOCraftingQueueComponent) if pawn0 else None
        ctx.assert_true("surviving rows mirror the component queue",
                        qc0 is not None and widget.get_row_count() == qc0.get_queue_length())

    # (c) F18 MENU-ROUND-TRIP RECONSTRUCT: close + reopen the menu across real
    # frames (CommonUI reconciles close/push next-tick — F21), then ONE cancel
    # intent must cancel exactly one entry (no doubled/torn bindings).
    test_subsystem.cleanup_queue_fixture()
    for _ in range(5):
        yield 1
    if not ctx.guard("reconstruct fixture enqueued", bool(test_subsystem.setup_queue_fixture(2))):
        return
    yield 1
    if not ctx.guard("crafting menu reopened", bool(test_subsystem.open_menu("Crafting"))):
        return
    for _ in range(3):
        yield 1

    widget2 = None
    for candidate in ctx.all_widgets(ctx.find_class("MOCraftingQueueWidget")):
        if candidate.has_queue_source():
            widget2 = candidate
            break
    if not ctx.guard("live widget found after reconstruct", widget2 is not None):
        return

    pawn = unreal.GameplayStatics.get_player_pawn(ctx.world, 0)
    queue_comp = pawn.get_component_by_class(unreal.MOCraftingQueueComponent) if pawn else None
    if not ctx.guard("pawn crafting queue resolved", queue_comp is not None):
        return

    len_before = queue_comp.get_queue_length()
    rows_before = widget2.get_row_count()
    ctx.assert_true("reconstructed widget rebuilt its rows", rows_before == len_before and rows_before >= 2)
    last_row = widget2.get_row_widget_at(rows_before - 1)
    if not ctx.guard("queued row present post-reconstruct", last_row is not None):
        return
    last_row.request_cancel()
    yield 1
    ctx.assert_eq("RECONSTRUCT (F18): one intent cancelled exactly one entry",
                  queue_comp.get_queue_length(), len_before - 1)

    test_subsystem.cleanup_queue_fixture()
    ctx.out("[UIQueue] Stage-3 live progress + completion + reconstruct contract complete")
