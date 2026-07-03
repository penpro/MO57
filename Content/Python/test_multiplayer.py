"""2-client co-op PIE harness (charter Move 2, tasks #165/#168).

A claude_seq sequence: run via

    python Tools/ue.py mptest            (wraps: ue.py seq Content/Python/test_multiplayer.py)

Phases:
  0  ConfigurePIE(2, listen-server, one process)   [MOEditorTestHelper]
  1  begin PIE, wait for BOTH PIE worlds (host=ListenServer, client=Client)
  2  drive the HOST through the menu flow (skip intro -> new game seed 4242)
  3  wait for host pawn possession; report what the CLIENT world did meanwhile
  4  client-world smoke: MO.Test.State executed ON the client world
  5  RPC round-trip (best effort): grant mats to the client's HOST-side proxy
     pawn (authority), then enqueue a craft FROM THE CLIENT WORLD -- the
     component forwards non-authority calls through ServerRequestEnqueueCraft,
     so this exercises the real client->server transport -- and assert the
     host-side queue grew.

Results stream to ue_out.txt as [seq:test_multiplayer] lines; final line is
DONE pass=<n> fail=<n> (ue.py seq turns that into an exit code).

NOTE: with the editor window unfocused PIE runs ~3 fps -- yields below are
sized for that (yield 10 ~= 3 s wall time).
"""
import unreal


SEED = 4242


def _pc(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_controller(world, idx)
    except Exception:
        return None  # stale world wrapper (torn down by travel) — treat as absent


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None  # stale world wrapper (torn down by travel) — treat as absent


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _out_summary(ctx, helper):
    # one ctx.out per line: multi-line out() loses the [seq:] prefix on
    # continuation lines, which grep-based runners then drop
    for line in helper.get_pie_worlds_summary().splitlines():
        ctx.out(line)


def sequence(ctx):
    helper = unreal.MOEditorTestHelper

    # ---- Phase 0: configure 2-player listen-server PIE --------------------
    ok = helper.configure_pie(2, True)
    if not ctx.guard("ConfigurePIE(2, listen) succeeded", ok):
        return
    yield 1

    # ---- Phase 1: begin PIE, wait for both worlds -------------------------
    if ctx.atl:
        ctx.atl.begin_pie(ctx.out)
    host = client = None
    for i in range(90):                      # up to ~4.5 min at 3 fps
        yield 10
        host = helper.find_pie_world_by_net_mode("ListenServer")
        client = helper.find_pie_world_by_net_mode("Client")
        if host and client:
            break
    ctx.assert_true("host (ListenServer) world exists", host is not None)
    ctx.assert_true("client (Client) world exists", client is not None)
    _out_summary(ctx, helper)
    if not ctx.guard("both PIE worlds resolved", host and client):
        return

    # ---- Phase 2: drive the HOST through the menu flow --------------------
    if ctx.atl:
        ctx.atl.skip_intro(host, ctx.out)
        yield 10
        ctx.atl.start_new_game(host, ctx.out, seed=SEED, survivor_name="MPHost")

    # ---- Phase 3: wait for host pawn; observe the client ------------------
    # StartNewGame OPENS A NEW LEVEL: every cached UWorld reference goes stale
    # (first run failed exactly here). Re-resolve worlds EVERY poll iteration.
    # KNOWN GAP (discovered by this harness, 2026-07-03): the new-game flow
    # uses a plain local OpenLevel, so the traveled world comes back
    # NM_Standalone — the listen-server role AND the connected client are
    # dropped. Until the flow uses ServerTravel (charter Pillar 1A), the pawn
    # search accepts Standalone, and the "still a listen server" assert below
    # is the precise, expected-red co-op gap marker.
    host_pawn = None
    for i in range(60):                      # world gen can take a while
        yield 10
        for mode in ("ListenServer", "Standalone"):
            w = helper.find_pie_world_by_net_mode(mode)
            p = _pawn(w, 0) if w else None
            if p and p.get_component_by_class(unreal.MOInventoryComponent):
                host_pawn = p
                break
        if host_pawn:
            break
    ctx.assert_true("host pawn possessed with inventory (any PIE world)", host_pawn is not None)
    ctx.assert_true("host is STILL a listen server after new-game travel (co-op survives start)",
                    helper.find_pie_world_by_net_mode("ListenServer") is not None)
    ctx.out("post-newgame world state:")
    _out_summary(ctx, helper)

    # The client may have been dropped / left behind by a non-seamless map
    # open -- that exact behavior is a Move-2 DISCOVERY output, so re-resolve
    # and report rather than assume.
    client = helper.find_pie_world_by_net_mode("Client")
    ctx.assert_true("client world still exists after host travel", client is not None)
    client_pawn = None
    if client:
        for i in range(60):
            yield 10
            client = helper.find_pie_world_by_net_mode("Client")   # re-resolve: client travels too
            p = _pawn(client, 0) if client else None
            if p and p.get_component_by_class(unreal.MOInventoryComponent):
                client_pawn = p
                break
        ctx.assert_true("client pawn possessed with inventory", client_pawn is not None)

    # ---- Phase 4: run a MO.Test command ON the client world ---------------
    if client:
        _exec(client, "MO.Test.State")       # logs [MOQUERY] STATE netmode=Client...
        yield 5
        ctx.out("MO.Test.State executed on client world (grep [MOQUERY] STATE for netmode=Client)")

    # ---- Phase 5 (best effort): client->server craft RPC round-trip -------
    host = helper.find_pie_world_by_net_mode("ListenServer")        # fresh post-travel
    if client_pawn and host:
        try:
            # Authority-side setup: the client player's pawn AS SEEN BY THE
            # HOST is player index 1 on the host world.
            proxy = _pawn(host, 1)
            if proxy:
                inv = proxy.get_component_by_class(unreal.MOInventoryComponent)
                skills = proxy.get_component_by_class(unreal.MOSkillsComponent)
                if inv:
                    inv.add_item_by_guid(unreal.Guid.new_guid(), "Flint01", 1)
                if skills:
                    skills.set_skill_level("Stoneworking", 3)
                queue_before = 0
                qc = proxy.get_component_by_class(unreal.MOCraftingQueueComponent)
                if qc:
                    try:
                        queue_before = len(qc.get_editor_property("queue").get_editor_property("entries"))
                    except Exception:
                        queue_before = -1
                # Client-side action: enqueue from the CLIENT world. The
                # component forwards to ServerRequestEnqueueCraft (real RPC).
                cq = client_pawn.get_component_by_class(unreal.MOCraftingQueueComponent)
                if cq:
                    cq.enqueue_craft("KnapFlintFlakes", 1, unreal.MOCraftingStation.NONE)
                    ctx.out("client EnqueueCraft dispatched")
                yield 15                      # RPC + host tick + replication
                queue_after = -1
                if qc:
                    try:
                        queue_after = len(qc.get_editor_property("queue").get_editor_property("entries"))
                    except Exception:
                        queue_after = -1
                ctx.out("host-side proxy queue entries: before=%s after=%s" % (queue_before, queue_after))
                if queue_before >= 0 and queue_after >= 0:
                    ctx.assert_true("client craft RPC landed on host queue", queue_after > queue_before)
                else:
                    ctx.out("queue not reflection-readable -- RPC verdict deferred to log inspection")
            else:
                ctx.out("no player-1 proxy pawn on host world -- client never fully joined")
                ctx.assert_true("client proxy pawn on host", False)
        except Exception as e:  # noqa: BLE001 - report, don't crash the tick
            ctx.out("phase 5 exception: %s" % e)
            ctx.assert_true("craft RPC phase ran without exception", False)

    ctx.out("2-client smoke complete")
